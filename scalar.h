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

typedef enum scalar_type_t {
    ScUNDEFINED = 0,
    ScINT       = 1,
    ScDOUBLE    = 2,
    ScSTRING    = 4,
    ScREFERENCE = 8,
} scalar_type_t;

typedef struct scalar_t {
    scalar_type_t m_type;
    union {
        intptr_t int_value;
        double   double_value;
        char     *string_value;
        /* reference_t *reference_value; */
    } m_value;
} scalar_t;

void scalar_init(scalar_t *);
void scalar_destroy(scalar_t *);
void scalar_clone(scalar_t * restrict, const scalar_t * restrict);
void scalar_assign(scalar_t * restrict, const scalar_t * restrict);

void scalar_set_int_value(scalar_t *, intptr_t);
void scalar_set_double_value(scalar_t *, double);
void scalar_set_string_value(scalar_t *, const char *);

intptr_t scalar_get_int_value(const scalar_t *);
double scalar_get_double_value(const scalar_t *);
const char *scalar_get_string_value(const scalar_t *);

#endif
