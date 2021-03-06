/*
 *  array.h
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef ARRAY_H
#define ARRAY_H

#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE   (64) /* FIXME arbitrary number */
#include "pool.h"

typedef struct array_t {
    size_t m_allocated_count;
    size_t m_count;
    size_t m_first;
    scalar_handle_t *m_items;
} array_t;

int _array_init(array_t *);
int _array_destroy(array_t *);
POOL_HEADER_CONTENTS(array_t, array_handle_t, PTHREAD_MUTEX_ERRORCHECK, _array_init, _array_destroy);

struct scalar_t;

int array_pool_init(void);
int array_pool_destroy(void);

array_handle_t array_allocate(flags8_t);
array_handle_t array_allocate_many(size_t, flags8_t);
array_handle_t array_reference(array_handle_t);
int array_release(array_handle_t);

size_t array_size(array_handle_t);
scalar_handle_t array_item_at(array_handle_t, intptr_t);

int array_slice(array_handle_t, struct scalar_t *, size_t);
int array_list(array_handle_t, struct scalar_t **restrict, size_t *restrict);
int array_fill(array_handle_t, const struct scalar_t *, size_t);

int array_push(array_handle_t, const struct scalar_t *, size_t);
int array_unshift(array_handle_t, const struct scalar_t *, size_t);

int array_pop(array_handle_t, struct scalar_t *);
int array_shift(array_handle_t, struct scalar_t *);

#endif
