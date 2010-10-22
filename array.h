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

typedef struct array_t array_t;

int array_init(array_t *);
int array_destroy(array_t *);
int array_reserve(array_t *, size_t);

scalar_handle_t array_item_at(array_t *, size_t);

int array_push(array_t *, scalar_handle_t);
int array_unshift(array_t *, scalar_handle_t);

scalar_handle_t array_pop(array_t *);
scalar_handle_t array_shift(array_t *);

#endif
