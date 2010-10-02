/*
 *  data_stack.c
 *  dang
 *
 *  Created by Ellie on 2/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "data_stack.h"

//typedef struct data_stack_scope_t {
//    struct data_stack_scope_t *m_parent;
//    size_t m_allocated_count;
//    size_t m_count;
//    scalar_t *m_items;
//    pthread_mutex_t m_mutex;
//} data_stack_scope_t;

static const int data_stack_scope_initial_reserve_size = 10;
static const int data_stack_scope_grow_size = 10;

int data_stack_scope_init(data_stack_scope_t *self) {
    assert(self != NULL);
    
    assert(0 == pthread_mutex_init(&self->m_mutex, NULL));

    int status = 0;
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        if (NULL != (self->m_items = calloc(data_stack_scope_initial_reserve_size, sizeof(scalar_t)))) {
            self->m_parent = NULL;
            self->m_allocated_count = data_stack_scope_initial_reserve_size;
            self->m_count = 0;
            status = 0;
        }
        else {
            status = -1;
        }
    pthread_mutex_unlock(&self->m_mutex);
    
    return status;
}

int data_stack_scope_destroy(data_stack_scope_t *self) {
    assert(self != NULL);
    
    assert(0 == pthread_mutex_lock(&self->m_mutex));
    pthread_mutex_destroy(&self->m_mutex);  // FIXME with mutex locked, or not?

    free(self->m_items);
    memset(self, 0, sizeof(*self));
    return 0;
}

int data_stack_scope_reserve(data_stack_scope_t *self, size_t new_size) {
    assert(self != NULL);
    
    int status;
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        if (new_size > self->m_allocated_count) {
            scalar_t *tmp = self->m_items;
            if (NULL != (self->m_items = calloc(new_size, sizeof(scalar_t)))) {
                self->m_allocated_count = new_size;
                memcpy(self->m_items, tmp, self->m_count * sizeof(scalar_t));
                free(tmp);
                status = 0;
            }
            else {
                self->m_items = tmp;
                status = -1;
            }
        }
        else {
            status = 0; // do nothing
        }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_scope_push(data_stack_scope_t *self, const scalar_t *value) {
    assert(self != NULL);
    assert(value != NULL);
    
    int status = 0;
    assert(0 == pthread_mutex_lock(&self->m_mutex));
        if (self->m_count == self->m_allocated_count) {
            size_t new_size = self->m_allocated_count + data_stack_scope_grow_size;
            pthread_mutex_unlock(&self->m_mutex);
            status = data_stack_scope_reserve(self, new_size);
            assert(0 == pthread_mutex_lock(&self->m_mutex));
        }
        
        if (status == 0)  scalar_clone(&self->m_items[self->m_count++], value);
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_scope_pop(data_stack_scope_t *self, scalar_t *result) {
    assert(self != NULL);
    
    int status = 0;
    assert(0 == pthread_mutex_lock(&self->m_mutex));
    if (self->m_count > 0) {
        scalar_t value = self->m_items[--self->m_count];
        if (result != NULL) {
            *result = value;   
        }
        else {
            scalar_destroy(&value);
        }
        status = 0;
    }
    else {
        status = -1;
    }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_start_scope(data_stack_scope_t **data_stack) { 
    // FIXME thread safety?
    // FIXME think about this more
    data_stack_scope_t *new_scope;
    if (NULL != (new_scope = calloc(1, sizeof(data_stack_scope_t)))) {
        if (0 == data_stack_scope_init(new_scope)) {
            new_scope->m_parent = *data_stack;
            *data_stack = new_scope;
            return 0;
        }
        else {
            free(new_scope);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int data_stack_end_scope(data_stack_scope_t **data_stack) { 
    // FIXME thread safety?
    // FIXME think about this more
    data_stack_scope_t *old_scope = *data_stack;
    *data_stack = old_scope->m_parent;
    return -0;
}
