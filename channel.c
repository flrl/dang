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

static const size_t MIN_BUFSIZE = 1;
static const size_t MAX_BUFSIZE = 4096; // no particular reason

static int _channel_resize_nonlocking(channel_t *, size_t);

int channel_init(channel_t *self, size_t bufsize) {
    assert(self != NULL);
    assert(bufsize >= MIN_BUFSIZE);
    assert(bufsize <= MAX_BUFSIZE);
    // FIXME make bufsize a power of 2
    
    if (0 == pthread_mutex_init(&self->m_mutex, NULL)) {    
        if (0 == pthread_mutex_lock(&self->m_mutex)) {
            if (0 == pthread_cond_init(&self->m_has_items, NULL)) {
                if (0 == pthread_cond_init(&self->m_has_space, NULL)) {
                    if (NULL != (self->m_ringbuf = calloc(bufsize, sizeof(scalar_t)))) {
                        self->m_bufsize = bufsize;
                        self->m_start = 0;
                        self->m_count = 0;
                        return 0;
                    }
                }
            }
        }
    }
    
    perror("channel_init");
    return -1;
}

int channel_destroy(channel_t *self) { // FIXME implement this
    assert(self != NULL);
    return 0;
}

scalar_t channel_read(channel_t *self) {
    assert(self != NULL);
    static const struct timespec wait_timeout = { 10, 0 };
    
    scalar_t value;
    scalar_init(&value);
    
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        while (self->m_count == 0) {
            if (ETIMEDOUT == pthread_cond_timedwait(&self->m_has_items, &self->m_mutex, &wait_timeout)) {
                size_t bufsize = self->m_bufsize;
                if (bufsize > MIN_BUFSIZE) {
                    size_t new_size = MAX(MIN_BUFSIZE, bufsize / 2);
                    pthread_mutex_unlock(&self->m_mutex);
                    channel_shrink_buffer(self, new_size);
                    assert(0 == pthread_mutex_lock(&self->m_mutex));
                }
            }
        }
        value = self->m_ringbuf[self->m_start];
        self->m_start = (self->m_start + 1) % self->m_bufsize;
        self->m_count--;
    pthread_mutex_unlock(&self->m_mutex);        

    pthread_cond_signal(&self->m_has_space);    

    return value;
}

int channel_tryread(channel_t *self, scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);

    int status;
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        if (self->m_count > 0) {
            *result = self->m_ringbuf[self->m_start];
            self->m_start = (self->m_start + 1) % self->m_bufsize;
            self->m_count--;
            status = 0;
        }
        else {
            status = EWOULDBLOCK;
        }
    pthread_mutex_unlock(&self->m_mutex);

    return status;
}

void channel_write(channel_t *self, scalar_t value) {
    assert(self != NULL);
    static const struct timespec wait_timeout = { 0, 250000 };

    assert(0 == pthread_mutex_lock(&self->m_mutex));
        while (self->m_count >= self->m_bufsize) {
            if (ETIMEDOUT == pthread_cond_timedwait(&self->m_has_space, &self->m_mutex, &wait_timeout)) {
                size_t bufsize = self->m_bufsize;
                if (bufsize < MAX_BUFSIZE) {
                    size_t new_size = MIN(MAX_BUFSIZE, bufsize * 2);
                    pthread_mutex_unlock(&self->m_mutex);
                    channel_grow_buffer(self, new_size);
                    assert(0 == pthread_mutex_lock(&self->m_mutex));                    
                }
            }
        }
        size_t index = (self->m_start + self->m_count) % self->m_bufsize;
        self->m_ringbuf[index] = value;
        self->m_count++;
    pthread_mutex_unlock(&self->m_mutex);

    pthread_cond_signal(&self->m_has_items);
}

int channel_grow_buffer(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size >= MIN_BUFSIZE);
    assert(new_size <= MAX_BUFSIZE);
    
    int status = 0;
    int post_has_space = 0;
    
    assert(0 == pthread_mutex_lock(&self->m_mutex));
    if (new_size > self->m_bufsize) {
        if (0 == (status = _channel_resize_nonlocking(self, new_size))) {
            post_has_space = 1;            
        }
    }
    else {
        status = 0; // grow to current size or less requested: do nothing
    }
    pthread_mutex_unlock(&self->m_mutex);
    
    if (post_has_space) {
        pthread_cond_signal(&self->m_has_space);
    }
    
    return status;
}

int channel_shrink_buffer(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size >= MIN_BUFSIZE);
    assert(new_size <= MAX_BUFSIZE);
    
    int status = 0;
    
    assert(0 == pthread_mutex_lock(&self->m_mutex));
    if (new_size < self->m_bufsize) {
        while (self->m_count > new_size) {
            pthread_cond_wait(&self->m_has_space, &self->m_mutex);   
        }
        status = _channel_resize_nonlocking(self, new_size);
    }
    else {
        status = 0; // shrink to current size or greater requested: do nothing
    }
    pthread_mutex_unlock(&self->m_mutex);
    
    return status;
}

static int _channel_resize_nonlocking(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size >= self->m_count);
    assert(new_size != self->m_bufsize);
    assert(new_size >= MIN_BUFSIZE);
    assert(new_size <= MAX_BUFSIZE);
    
    int status = 0;    
    scalar_t *new_ringbuf = calloc(new_size, sizeof(scalar_t));
    if (NULL != new_ringbuf) {
        size_t straight_count = self->m_bufsize - self->m_start > self->m_count ? self->m_count : self->m_bufsize - self->m_start;
        size_t rotated_count = self->m_count - straight_count;
        memcpy(&new_ringbuf[0], &self->m_ringbuf[self->m_start], straight_count * sizeof(scalar_t));
        memcpy(&new_ringbuf[straight_count], &self->m_ringbuf[0], rotated_count * sizeof(scalar_t));
        free(self->m_ringbuf);
        self->m_bufsize = new_size;
        self->m_ringbuf = new_ringbuf;
        self->m_start = 0;
        status = 0;
    }
    else {
        status = -1;
    }    
    
    return status;
}
