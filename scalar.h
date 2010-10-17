/*
 *  scalar.h
 *  dang
 *
 *  Created by Ellie on 8/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */


#ifndef SCALAR_H
#define SCALAR_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "floatptr_t.h"

#define SCALAR_UNDEF            0x00000000u
#define SCALAR_INT              0x01u
#define SCALAR_FLOAT            0x02u
#define SCALAR_STRING           0x03u
#define SCALAR_FILEHANDLE       0x04u
#define SCALAR_CHANNEL          0x05u
// ...
#define SCALAR_SCAREF           0x11u
#define SCALAR_ARRREF           0x12u
#define SCALAR_HASHREF          0x13u

#define SCALAR_TYPE_MASK        0x0000001Fu
#define SCALAR_FLAGS_MASK       0xFFFFFFE0u

#define SCALAR_FLAG_REF         0x00000010u     /* pseudo flag, actually part of the type mask */
// ...
#define SCALAR_FLAG_PTR         0x08000000u
#define SCALAR_FLAG_SHARED      0x40000000u
#define SCALAR_FLAG_INUSE       0x80000000u

#define SCALAR_ALL_FLAGS        0xC800001Fu     /* keep this up to date */
#define ANON_SCALAR_ALL_FLAGS   0x0800001Fu     /* keep this up to date */
/*
 1100 1000  0000 0000  0000 0000  0001 1111
 ||   |                              | ''''-- basic types
 ||   |                              '------- value is a reference
 ||   '-------------------------------------- value is a malloc'd pointer, make sure to free it
 |'------------------------------------------ scalar is shared, lock the mutex
 '------------------------------------------- scalar is in use
 */

#define SCALAR_GUTS             \
    uint32_t m_flags;           \
    union {                     \
        intptr_t as_int;        \
        floatptr_t as_float;    \
        char     *as_string;    \
        intptr_t next_free;     \
    } m_value

typedef struct scalar_t {
    SCALAR_GUTS;
} anon_scalar_t;

typedef struct pooled_scalar_t {
    SCALAR_GUTS;
    size_t m_references;
    pthread_mutex_t *m_mutex;
} pooled_scalar_t;

typedef struct scalar_pool_t {
    size_t m_allocated_count;
    size_t m_count;
    pooled_scalar_t *m_items;
    size_t m_free_count;
    intptr_t m_free_list_head;
    pthread_mutex_t m_free_list_mutex;
} scalar_pool_t;

typedef uintptr_t scalar_handle_t;

int scalar_pool_init(void);
int scalar_pool_destroy(void);

scalar_handle_t scalar_pool_allocate_scalar(uint32_t);
void scalar_pool_release_scalar(scalar_handle_t);
void scalar_pool_increase_refcount(scalar_handle_t);

void scalar_set_undef(scalar_handle_t);
void scalar_set_int_value(scalar_handle_t, intptr_t);
void scalar_set_float_value(scalar_handle_t, floatptr_t);
void scalar_set_string_value(scalar_handle_t, const char *);
void scalar_set_value(scalar_handle_t, const anon_scalar_t *);

intptr_t scalar_get_int_value(scalar_handle_t);
floatptr_t scalar_get_float_value(scalar_handle_t);
void scalar_get_string_value(scalar_handle_t, char **);
void scalar_get_value(scalar_handle_t, anon_scalar_t *);

int anon_scalar_init(anon_scalar_t *);
int anon_scalar_destroy(anon_scalar_t *);

int anon_scalar_clone(anon_scalar_t * restrict, const anon_scalar_t * restrict);
int anon_scalar_assign(anon_scalar_t * restrict, const anon_scalar_t * restrict);

void anon_scalar_set_int_value(anon_scalar_t *, intptr_t);
void anon_scalar_set_float_value(anon_scalar_t *, floatptr_t);
void anon_scalar_set_string_value(anon_scalar_t *, const char *);

intptr_t anon_scalar_get_int_value(const anon_scalar_t *);
floatptr_t anon_scalar_get_float_value(const anon_scalar_t *);
void anon_scalar_get_string_value(const anon_scalar_t *, char **);

#endif
