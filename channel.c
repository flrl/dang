/*
 *  channel.c
 *  dang
 *
 *  Created by Ellie on 1/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <sys/errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "channel.h"
#include "debug.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE           (64)
#include "pool.h"

#define CHANNEL_FLAG_INUSE          UINTPTR_MAX
#define CHANNEL_POOL_ITEM(handle)   POOL_ITEM(channel_t, (handle))
#define CHANNEL_ISINUSE(c)          ((c).m_next_free == CHANNEL_FLAG_INUSE)
#define CHANNEL_NEXTFREE(c)         ((c).m_next_free)
#define CHANNEL_INIT(c,a)           _channel_init(c)

typedef struct channel_t {
    size_t m_allocated_count;
    size_t m_count;
    anon_scalar_t *m_items;
    size_t m_start;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_has_items;
    pthread_cond_t m_has_space;
    size_t m_references;
    channel_handle_t m_next_free;
} channel_t;

typedef POOL_STRUCT(channel_t) channel_pool_t;

static const size_t _channel_initial_size = 16;

static int _channel_init(channel_t *);
static int _channel_destroy(channel_t *);
static inline int _channel_lock(channel_t *);
static inline int _channel_unlock(channel_t *);
static int _channel_reserve_unlocked(channel_t *, size_t);

POOL_DEFINITIONS(channel_t, CHANNEL_ISINUSE, CHANNEL_NEXTFREE, CHANNEL_INIT, void*, _channel_destroy, _channel_lock, _channel_unlock);

int channel_pool_init(void) {
    return POOL_INIT(channel_t);
}

int channel_pool_destroy(void) {
    return POOL_DESTROY(channel_t);
}

channel_handle_t channel_allocate(void) {
    return POOL_ALLOCATE(channel_t, NULL);
}

int channel_release(channel_handle_t handle) {
    return POOL_RELEASE(channel_t, handle);
}

int channel_increase_refcount(channel_handle_t handle) {
    return POOL_INCREASE_REFCOUNT(channel_t, handle);
}

int channel_read(channel_handle_t handle, anon_scalar_t *result) {
    assert(POOL_VALID_HANDLE(channel_t, handle));

    assert(CHANNEL_POOL_ITEM(handle).m_next_free == CHANNEL_FLAG_INUSE);
    assert(result != NULL);
    
    if (0 == _channel_lock(&CHANNEL_POOL_ITEM(handle))) {
        while (CHANNEL_POOL_ITEM(handle).m_count == 0) {
            pthread_cond_wait(&CHANNEL_POOL_ITEM(handle).m_has_items, &CHANNEL_POOL_ITEM(handle).m_mutex);
        }
        anon_scalar_assign(result, &CHANNEL_POOL_ITEM(handle).m_items[CHANNEL_POOL_ITEM(handle).m_start]);
        CHANNEL_POOL_ITEM(handle).m_start = (CHANNEL_POOL_ITEM(handle).m_start + 1) % CHANNEL_POOL_ITEM(handle).m_allocated_count;
        CHANNEL_POOL_ITEM(handle).m_count--;
        _channel_unlock(&CHANNEL_POOL_ITEM(handle));
        
        pthread_cond_signal(&CHANNEL_POOL_ITEM(handle).m_has_space);
        return 0;
    }
    else {
        return -1;
    }
}

int channel_tryread(channel_handle_t handle, anon_scalar_t *result) {
    assert(POOL_VALID_HANDLE(channel_t, handle));
    assert(CHANNEL_POOL_ITEM(handle).m_next_free == CHANNEL_FLAG_INUSE);
    assert(result != NULL);
    
    if (0 == _channel_lock(&CHANNEL_POOL_ITEM(handle))) {
        int status;
        if (CHANNEL_POOL_ITEM(handle).m_count > 0) {
            anon_scalar_assign(result, &CHANNEL_POOL_ITEM(handle).m_items[CHANNEL_POOL_ITEM(handle).m_start]);
            CHANNEL_POOL_ITEM(handle).m_start = (CHANNEL_POOL_ITEM(handle).m_start + 1) % CHANNEL_POOL_ITEM(handle).m_allocated_count;
            CHANNEL_POOL_ITEM(handle).m_count--;
            status = 0;
        }
        else {
            status = EWOULDBLOCK;
        }
        _channel_unlock(&CHANNEL_POOL_ITEM(handle));
        return status;
    }
    else {
        return -1;
    }
}

int channel_write(channel_handle_t handle, const anon_scalar_t *value) {
    assert(POOL_VALID_HANDLE(channel_t, handle));
    assert(CHANNEL_POOL_ITEM(handle).m_next_free == CHANNEL_FLAG_INUSE);
    assert(value != NULL);
    
    static const struct timespec wait_timeout = { 0, 250000 };
    
    if (0 == _channel_lock(&CHANNEL_POOL_ITEM(handle))) {
        while (CHANNEL_POOL_ITEM(handle).m_count >= CHANNEL_POOL_ITEM(handle).m_allocated_count) {
            if (ETIMEDOUT == pthread_cond_timedwait(&CHANNEL_POOL_ITEM(handle).m_has_space, &CHANNEL_POOL_ITEM(handle).m_mutex, &wait_timeout)) {
                _channel_reserve_unlocked(&CHANNEL_POOL_ITEM(handle), CHANNEL_POOL_ITEM(handle).m_allocated_count * 2);
            }
        }
        size_t index = (CHANNEL_POOL_ITEM(handle).m_start + CHANNEL_POOL_ITEM(handle).m_count) % CHANNEL_POOL_ITEM(handle).m_allocated_count;
        anon_scalar_clone(&CHANNEL_POOL_ITEM(handle).m_items[index], value);
        CHANNEL_POOL_ITEM(handle).m_count++;
        _channel_unlock(&CHANNEL_POOL_ITEM(handle));
        
        pthread_cond_signal(&CHANNEL_POOL_ITEM(handle).m_has_items);
        return 0;        
    }
    else {
        return -1;
    }
}

static int _channel_init(channel_t *self) {
    assert(self != NULL);
    
    if (0 == pthread_mutex_init(&self->m_mutex, NULL)) {    
        if (0 == pthread_mutex_lock(&self->m_mutex)) {
            if (0 == pthread_cond_init(&self->m_has_items, NULL)) {
                if (0 == pthread_cond_init(&self->m_has_space, NULL)) {
                    if (NULL != (self->m_items = calloc(_channel_initial_size, sizeof(anon_scalar_t)))) {
                        self->m_allocated_count = _channel_initial_size;
                        self->m_start = 0;
                        self->m_count = 0;
                        self->m_references = 0;
                        pthread_mutex_unlock(&self->m_mutex);
                        return 0;
                    }
                }
            }
        }
    }
    
    debug("couldn't initialise channel");
    return -1;
}

static int _channel_destroy(channel_t *self) { // FIXME implement this
    assert(self != NULL);
    return 0;
}

static inline int _channel_lock(channel_t *self) {
    assert(self != NULL);
    assert(self->m_next_free == CHANNEL_FLAG_INUSE);
    
    return pthread_mutex_lock(&self->m_mutex);
}

static inline int _channel_unlock(channel_t *self) {
    assert(self != NULL);
    assert(self->m_next_free == CHANNEL_FLAG_INUSE);
    
    return pthread_mutex_unlock(&self->m_mutex);
}

static int _channel_reserve_unlocked(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size != 0);

    if (self->m_allocated_count >= new_size)  return 0;
    
    anon_scalar_t *new_ringbuf = calloc(new_size, sizeof(*new_ringbuf));
    if (new_ringbuf == NULL)  return -1;
    
    size_t straight_count = self->m_allocated_count - self->m_start > self->m_count ? self->m_count : self->m_allocated_count - self->m_start;
    size_t rotated_count = self->m_count - straight_count;
    memcpy(&new_ringbuf[0], &self->m_items[self->m_start], straight_count * sizeof(anon_scalar_t));
    memcpy(&new_ringbuf[straight_count], &self->m_items[0], rotated_count * sizeof(anon_scalar_t));
    free(self->m_items);
    self->m_allocated_count = new_size;
    self->m_items = new_ringbuf;
    self->m_start = 0;
    return 0;
}
