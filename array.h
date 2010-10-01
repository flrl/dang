/*
 *  array.h
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>

#include "scalar.h"

typedef struct array_t {
    size_t m_allocated_count;
    size_t m_count;
    struct scalar_t *m_items;
} array_t;

array_t *array_create(size_t initial_size);
array_t *array_destroy(array_t *self);

scalar_t array_item_at(array_t *self, size_t index);

int  array_push(array_t *self, scalar_t value);
int  array_unshift(array_t *self, scalar_t value);

scalar_t array_pop(array_t *self);
scalar_t array_shift(array_t *self);

array_t *array_splice(array_t *self, size_t index, size_t count);

#endif
