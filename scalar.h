/*
 *  scalar.h
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef SCALAR_H
#define SCALAR_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum scalar_type_t {
    ScUNDEFINED = 0,
    ScINT       = 1,
    ScDOUBLE    = 2,
    ScSTRING    = 4,
    ScREFERENCE = 8,
} scalar_type_t;

typedef struct scalar_t {
    scalar_type_t type;
    union {
        intptr_t int_value;
        double   double_value;
        /* string_t *string_value; */
        /* reference_t *reference_value; */
    } value;
} scalar_t;

void scalar_init(scalar_t *);
void scalar_destroy(scalar_t *);




#endif
