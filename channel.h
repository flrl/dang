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

#include "pool.h"

struct scalar_t;

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
