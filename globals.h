/*
 *  globals.h
 *  dang
 *
 *  Created by Ellie on 7/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>

#include "scalar.h"

typedef struct globals_t {
    size_t m_allocated_count;
    size_t m_lock_granularity;
    scalar_t *m_items;
    pthread_mutex_t *m_locks;
} globals_t;

int globals_init(globals_t *, size_t, size_t);
int globals_destroy(globals_t *);

int globals_read(globals_t *, size_t, scalar_t *);
int globals_write(globals_t *, size_t, const scalar_t *);

#endif
