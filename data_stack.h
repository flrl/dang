/*
 *  data_stack.h
 *  dang
 *
 *  Created by Ellie on 2/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef DATA_STACK_H
#define DATA_STACK_H

#include <pthread.h>

#include "scalar.h"

typedef struct data_stack_t {
    struct data_stack_t *m_parent;
    size_t m_allocated_count;
    size_t m_count;
    scalar_t *m_items;
    pthread_mutex_t m_mutex;
    size_t m_subscope_count;
} data_stack_t;

int data_stack_init(data_stack_t *);
int data_stack_destroy(data_stack_t *);
int data_stack_reserve(data_stack_t *, size_t);
int data_stack_push(data_stack_t *, const scalar_t *);
int data_stack_pop(data_stack_t *, scalar_t *);
int data_stack_top(data_stack_t *, scalar_t *);
int data_stack_read_index(data_stack_t *, size_t, scalar_t *);
int data_stack_write_index(data_stack_t *, size_t, const scalar_t *);

int data_stack_start_scope(data_stack_t **);
int data_stack_end_scope(data_stack_t **);

int data_stack_registry_reap(void);

#endif
