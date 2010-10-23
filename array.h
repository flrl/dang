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

#include "pool.h"

#ifndef HAVE_ARRAY_HANDLE_T
#define HAVE_ARRAY_HANDLE_T
typedef POOL_HANDLE(array_t) array_handle_t;
#endif

#ifndef HAVE_SCALAR_HANDLE_T
#define HAVE_SCALAR_HANDLE_T
typedef POOL_HANDLE(scalar_t) scalar_handle_t;
#endif

typedef struct array_t array_t;
struct scalar_t;

int array_pool_init(void);
int array_pool_destroy(void);

array_handle_t array_allocate(void);
array_handle_t array_allocate_many(size_t);
array_handle_t array_reference(array_handle_t);
int array_release(array_handle_t);

scalar_handle_t array_item_at(array_handle_t, size_t);

int array_push(array_handle_t, const struct scalar_t *);
int array_unshift(array_handle_t, const struct scalar_t *);

int array_pop(array_handle_t, struct scalar_t *);
int array_shift(array_handle_t, struct scalar_t *);

#endif
