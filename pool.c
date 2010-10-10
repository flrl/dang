/*
 *  pool.c
 *  dang
 *
 *  Created by Ellie on 8/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pool.h"



scalar_t pool_get_scalar_handle(scalar_pool_t *self, scalar_t handle) {
    assert(self != NULL);
    
    if (handle >= 0) {
        // create an additional reference to an already existing scalar
        assert(handle <= self->m_allocated_count);
        assert(self->m_items[handle].m_references > 0);
        ++self->m_items[handle].m_references;
        return handle;
    }
    else if (self->m_free_list_head >= 0) {
        // allocate a new one from the free list
        handle = self->m_free_list_head;
        self->m_free_list_head = self->m_items[handle].m_value.next_free;
        self->m_items[handle].m_flags = SCALAR_UNDEF; // FIXME
        self->m_items[handle].m_value.as_int = 0;
        self->m_count++;
        return handle;
    }
    else {
        size_t new_size = self->m_allocated_count + self->m_allocated_count;
        pooled_scalar_t *tmp = calloc(new_size, sizeof(pooled_scalar_t));
        if (tmp != NULL) {
            memcpy(tmp, self->m_items, self->m_allocated_count * sizeof(pooled_scalar_t));
            free(self->m_items);
            self->m_items = tmp;
            self->m_allocated_count = new_size;
            // FIXME add the newly allocated items to the free list, and then set one of them up and use it
            
            return -1;
        }
        else {
            return -1;
        }
    }
}

void pool_release_scalar(scalar_pool_t *self, scalar_t handle) {
    assert(self != NULL);
    assert(handle >= 0);
    assert(handle <= self->m_allocated_count);
    assert(self->m_items[handle].m_references > 0);
    
    if (--self->m_items[handle].m_references == 0) {
        if (self->m_items[handle].m_flags & SCALAR_FLAG_PTR) {
            switch(self->m_items[handle].m_flags & SCALAR_TYPEMASK) {
                case SCALAR_STRING:
                    free(self->m_items[handle].m_value.as_string);
                    break;
            }
        }
        self->m_items[handle].m_flags = SCALAR_FLAG_ISFREE | SCALAR_UNDEF;
        self->m_items[handle].m_value.next_free = self->m_free_list_head;
        self->m_free_list_head = handle;
    }
}

