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

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE 64
#include "pool.h"

struct scalar_t;

typedef struct channel_t {
    size_t m_allocated_count;
    size_t m_count;
    struct scalar_t *m_items;
    size_t m_start;
    pthread_cond_t m_has_items;
    pthread_cond_t m_has_space;
} channel_t;

int _channel_init(channel_t *);
int _channel_destroy(channel_t *);

POOL_HEADER_CONTENTS(channel_t, _channel_init, _channel_destroy);

#ifndef HAVE_CHANNEL_HANDLE_T
#define HAVE_CHANNEL_HANDLE_T
typedef POOL_HANDLE(channel_t) channel_handle_t;
#endif

int channel_pool_init(void);
int channel_pool_destroy(void);

channel_handle_t channel_allocate(void);
channel_handle_t channel_allocate_many(size_t);
channel_handle_t channel_reference(channel_handle_t);
int channel_release(channel_handle_t);

int channel_read(channel_handle_t, struct scalar_t *);
int channel_tryread(channel_handle_t, struct scalar_t *);
int channel_write(channel_handle_t, const struct scalar_t *);

#endif
