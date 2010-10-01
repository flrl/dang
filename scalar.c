/*
 *  scalar.c
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include "scalar.h"

void scalar_init(scalar_t *self) {
    assert(self != NULL);
    self->type = ScUNDEFINED;
    self->value.int_value = 0;
}

void scalar_destroy(scalar_t *self) {
    assert(self != NULL);
}

