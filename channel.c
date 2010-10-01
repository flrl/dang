/*
 *  channel.c
 *  dang
 *
 *  Created by Ellie on 1/10/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "channel.h"

//typedef struct channel_t {
//    size_t m_bufsize;
//    scalar_t *m_ringbuf;
//    size_t m_start;
//    size_t m_count;
//    pthread_mutex_t m_mutex;
//    pthread_cond_t m_has_items;
//    pthread_cond_t m_has_space;
//} channel_t;


static int _channel_resize_nonlocking(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size >= self->m_count);
    assert(new_size != self->m_bufsize);
    
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

int channel_grow_buffer(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size > 0);

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
    assert(new_size > 0);
    
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

int channel_init(channel_t *self, size_t bufsize) {
    assert(self != NULL);
    assert(bufsize > 0);  // FIXME make bufsize a power of 2
    
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

    scalar_t value;
    scalar_init(&value);
    
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        while (self->m_count == 0) {
            pthread_cond_wait(&self->m_has_items, &self->m_mutex);
        }
        value = self->m_ringbuf[self->m_start];
        self->m_start = (self->m_start + 1) % self->m_bufsize;
        self->m_count--;
    pthread_mutex_unlock(&self->m_mutex);        

    pthread_cond_signal(&self->m_has_space);    

    return value;
}

void channel_write(channel_t *self, scalar_t value) {
    assert(self != NULL);

    assert(0 == pthread_mutex_lock(&self->m_mutex));
        while (self->m_count >= self->m_bufsize) {
            pthread_cond_wait(&self->m_has_space, &self->m_mutex);
        }
        size_t index = (self->m_start + self->m_count) % self->m_bufsize;
        self->m_ringbuf[index] = value;
        self->m_count++;
    pthread_mutex_unlock(&self->m_mutex);

    pthread_cond_signal(&self->m_has_items);
}


