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

#include <stdint.h>

#include "scalar.h"

typedef struct array_t {
    size_t m_allocated_count;
    size_t m_count;
    scalar_t *m_items;
} array_t;

int array_init(array_t *);
int array_destroy(array_t *);
int array_reserve(array_t *, size_t);

int array_item_at(array_t *, size_t, scalar_t *);

int array_push(array_t *, const scalar_t *);
int array_unshift(array_t *, const scalar_t *);

int array_pop(array_t *, scalar_t *);
int array_shift(array_t *, scalar_t *);

#endif
