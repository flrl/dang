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

struct array_t;

int array_init(struct array_t *);
int array_destroy(struct array_t *);
int array_reserve(struct array_t *, size_t);

scalar_handle_t array_item_at(struct array_t *, size_t);

int array_push(struct array_t *, scalar_handle_t);
int array_unshift(struct array_t *, scalar_handle_t);

scalar_handle_t array_pop(struct array_t *);
scalar_handle_t array_shift(struct array_t *);

#endif
