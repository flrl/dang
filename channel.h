/*
 *  channel.h
 *  dang
 *
 *  Created by Ellie on 1/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <pthread.h>

#include "scalar.h"

typedef struct channel_t {
    size_t m_bufsize;
    scalar_t *m_ringbuf;
    size_t m_start;
    size_t m_count;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_has_items;
    pthread_cond_t m_has_space;
} channel_t;

int channel_init(channel_t *, size_t);
int channel_destroy(channel_t *);

scalar_t channel_read(channel_t *);
void channel_write(channel_t *, scalar_t);

int channel_grow_buffer(channel_t *, size_t);
int channel_shrink_buffer(channel_t *, size_t);

#endif
