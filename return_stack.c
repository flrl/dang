/*
 *  return_stack.c
 *  dang
 *
 *  Created by Ellie on 4/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "return_stack.h"

static const size_t return_stack_initial_size = 10;
static const size_t return_stack_grow_size = 10;

int return_stack_init(return_stack_t *self) {
    assert(self != NULL);
    if (NULL != (self->m_items = calloc(return_stack_initial_size, sizeof(size_t)))) {
        self->m_allocated_count = return_stack_initial_size;
        self->m_count = 0;
        return 0;
    }
    else {
        return -1;
    }
}

int return_stack_destroy(return_stack_t *self) {
    assert(self != NULL);
    if (self->m_items != NULL) free(self->m_items);
    memset(self, 0, sizeof(*self));
    return 0;
}

int return_stack_push(return_stack_t *self, size_t value) {
    assert(self != NULL);
    
    if (self->m_count == self->m_allocated_count) {
        size_t *tmp = self->m_items;
        size_t new_size = self->m_allocated_count + return_stack_grow_size;
        if (NULL != (self->m_items = calloc(new_size, sizeof(size_t)))) {
            self->m_allocated_count = new_size;
            self->m_items[self->m_count++] = value;
            return 0;
        }
        else {
            self->m_items = tmp;
            return -1;
        }
    }
    else {
        self->m_items[self->m_count++] = value;
        return 0;
    }
}

int return_stack_pop(return_stack_t *self, size_t *result) {
    assert(self != NULL);
    
    if (self->m_count > 0) {
        size_t value = self->m_items[--self->m_count];
        if (result != NULL)  *result = value;
        return 0;
    }
    else {
        return -1;
    }
}


