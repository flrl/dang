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


#define CHANNEL_FLAG_INUSE          UINTPTR_MAX
#define CHANNEL_POOL_ITEM(handle)   g_channel_pool.m_items[(handle) - 1]

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

typedef struct channel_pool_t {
    size_t m_allocated_count;
    size_t m_count;
    channel_t *m_items;
    size_t m_free_count;
    channel_handle_t m_free_list_head;
    pthread_mutex_t m_free_list_mutex;
} channel_pool_t;


static channel_pool_t g_channel_pool;

const size_t _channel_pool_initial_size = 64;  // FIXME arbitrary number

int channel_pool_init(void) {
    if (NULL != (g_channel_pool.m_items = calloc(_channel_pool_initial_size, sizeof(*g_channel_pool.m_items)))) {
        g_channel_pool.m_allocated_count = g_channel_pool.m_free_count = _channel_pool_initial_size;
        g_channel_pool.m_count = 0;
        if (0 == pthread_mutex_init(&g_channel_pool.m_free_list_mutex, NULL)) {
            g_channel_pool.m_free_list_head = 1;
            for (channel_handle_t i = 2; i < g_channel_pool.m_allocated_count - 1; i++) {
                CHANNEL_POOL_ITEM(i).m_next_free = i;
            }
            CHANNEL_POOL_ITEM(g_channel_pool.m_allocated_count).m_next_free = 0;
            return 0;
        }
        else {
            free(g_channel_pool.m_items);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int channel_pool_destroy(void);

channel_handle_t channel_allocate(void) {
    if (0 == pthread_mutex_lock(&g_channel_pool.m_free_list_mutex)) {
        channel_handle_t handle;
        
        if (g_channel_pool.m_free_count > 0) {
            handle = g_channel_pool.m_free_list_head;
            g_channel_pool.m_free_list_head = CHANNEL_POOL_ITEM(handle).m_next_free;
            --g_channel_pool.m_free_count;
            pthread_mutex_unlock(&g_channel_pool.m_free_list_mutex);
        }
        else {
            handle = g_channel_pool.m_allocated_count + 1;
            size_t new_size = 2 * g_channel_pool.m_allocated_count;
            
            channel_t *tmp = calloc(new_size, sizeof(*tmp));
            if (tmp == NULL) {
                pthread_mutex_unlock(&g_channel_pool.m_free_list_mutex);
                return 0;
            }
            
            memcpy(tmp, g_channel_pool.m_items, g_channel_pool.m_allocated_count * sizeof(*g_channel_pool.m_items));
            free(g_channel_pool.m_items);
            g_channel_pool.m_items = tmp;
            g_channel_pool.m_allocated_count = new_size;
            
            g_channel_pool.m_free_list_head = handle + 1;
            for (channel_handle_t i = handle + 1; i < g_channel_pool.m_allocated_count - 1; i++) {
                CHANNEL_POOL_ITEM(i).m_next_free = i;
            }
            CHANNEL_POOL_ITEM(g_channel_pool.m_allocated_count).m_next_free = 0;
            g_channel_pool.m_free_count = g_channel_pool.m_allocated_count - handle;  // FIXME think about this - is it right?

            pthread_mutex_unlock(&g_channel_pool.m_free_list_mutex);
        }

//        typedef struct channel_t {
//            size_t m_allocated_count;
//            size_t m_count;
//            anon_scalar_t *m_items;
//            size_t m_start;
//            pthread_mutex_t m_mutex;
//            pthread_cond_t m_has_items;
//            pthread_cond_t m_has_space;
//            size_t m_references;
//            channel_handle_t m_next_free;
//        } channel_t;
        
        // FIXME now make sure the channel is set up sanely!

        return handle;
    }
    else {
        return 0;
    }
}

int channel_release(channel_handle_t);
int channel_increase_refcount(channel_handle_t);

int channel_read(channel_handle_t, anon_scalar_t *);
int channel_tryread(channel_handle_t, anon_scalar_t *);
int channel_write(channel_handle_t, const anon_scalar_t *);

/////////////////


static const size_t _channel_initial_size = 16;

static int _channel_reserve_unlocked(channel_t *, size_t);

int channel_init(channel_t *self) {
    assert(self != NULL);
    
    if (0 == pthread_mutex_init(&self->m_mutex, NULL)) {    
        if (0 == pthread_mutex_lock(&self->m_mutex)) {
            if (0 == pthread_cond_init(&self->m_has_items, NULL)) {
                if (0 == pthread_cond_init(&self->m_has_space, NULL)) {
                    if (NULL != (self->m_items = calloc(_channel_initial_size, sizeof(anon_scalar_t)))) {
                        self->m_allocated_count = _channel_initial_size;
                        self->m_start = 0;
                        self->m_count = 0;
                        return 0;
                    }
                }
            }
        }
    }
    
    debug("couldn't initialise channel");
    return -1;
}

int channel_destroy(channel_t *self) { // FIXME implement this
    assert(self != NULL);
    return 0;
}

int channel_read(channel_t *self, anon_scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);
        
    if (0 == pthread_mutex_lock(&self->m_mutex)) {
        while (self->m_count == 0) {
            pthread_cond_wait(&self->m_has_items, &self->m_mutex);
        }
        anon_scalar_assign(result, &self->m_items[self->m_start]);
        self->m_start = (self->m_start + 1) % self->m_allocated_count;
        self->m_count--;
        pthread_mutex_unlock(&self->m_mutex);        

        pthread_cond_signal(&self->m_has_space);
        return 0;
    }
    else {
        return -1;
    }
}

int channel_tryread(channel_t *self, anon_scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);

    if (0 == pthread_mutex_lock(&self->m_mutex)) {
        int status;
        if (self->m_count > 0) {
            anon_scalar_assign(result, &self->m_items[self->m_start]);
            self->m_start = (self->m_start + 1) % self->m_allocated_count;
            self->m_count--;
            status = 0;
        }
        else {
            status = EWOULDBLOCK;
        }
        pthread_mutex_unlock(&self->m_mutex);
        return status;
    }
    else {
        return -1;
    }
}

int channel_write(channel_t *self, const anon_scalar_t *value) {
    assert(self != NULL);
    static const struct timespec wait_timeout = { 0, 250000 };

    if (0 == pthread_mutex_lock(&self->m_mutex)) {
        while (self->m_count >= self->m_allocated_count) {
            if (ETIMEDOUT == pthread_cond_timedwait(&self->m_has_space, &self->m_mutex, &wait_timeout)) {
                _channel_reserve_unlocked(self, self->m_allocated_count * 2);
            }
        }
        size_t index = (self->m_start + self->m_count) % self->m_allocated_count;
        anon_scalar_clone(&self->m_items[index], value);
        self->m_count++;
        pthread_mutex_unlock(&self->m_mutex);
        
        pthread_cond_signal(&self->m_has_items);
        return 0;        
    }
    else {
        return -1;
    }
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
