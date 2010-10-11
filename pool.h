/*
 *  pool.h
 *  dang
 *
 *  Created by Ellie on 8/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */


#ifndef POOL_H
#define POOL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define SCALAR_UNALLOC      0x00u
#define SCALAR_INT          0x01u
#define SCALAR_FLOAT        0x02u
#define SCALAR_STRING       0x03u
// ...
#define SCALAR_UNDEF        0xFFu
#define SCALAR_TYPE_MASK    0xFFu

#define SCALAR_FLAG_SHARED  0x40000000u
#define SCALAR_FLAG_PTR     0x80000000u

typedef struct pooled_scalar_t {
    uint32_t m_flags;
    uint32_t m_references;
    union {
        intptr_t as_int;
        float    as_float;
        char     *as_string;
        intptr_t next_free;
    } m_value;
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

typedef intptr_t scalar_t;

int pool_init(scalar_pool_t *);
int pool_destroy(scalar_pool_t *);

scalar_t pool_allocate_scalar(scalar_pool_t *, uint32_t);
void pool_release_scalar(scalar_pool_t *, scalar_t);
void pool_increase_refcount(scalar_pool_t *, scalar_t);

#endif
