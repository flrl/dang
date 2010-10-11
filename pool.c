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

//typedef struct scalar_pool_t {
//    size_t m_allocated_count;
//    size_t m_count;
//    pooled_scalar_t *m_items;
//    intptr_t m_free_list_head;
//    pthread_mutex_t m_free_list_mutex;
//} scalar_pool_t;
//
//typedef intptr_t scalar_t;

//typedef struct pooled_scalar_t {
//    uint32_t m_flags;
//    uint32_t m_references;
//    union {
//        intptr_t as_int;
//        float    as_float;
//        char     *as_string;
//        intptr_t next_free;
//    } m_value;
//    pthread_mutex_t *m_mutex;
//} pooled_scalar_t;


const size_t scalar_pool_initial_size = 1024;   // FIXME arbitrary number
const size_t scalar_pool_grow_size = 1024;      // FIXME arbitrary number

void _pool_add_to_free_list(scalar_pool_t *, scalar_t);

int pool_init(scalar_pool_t *self) {
    assert(self != NULL);
    if (NULL != (self->m_items = calloc(scalar_pool_initial_size, sizeof(self->m_items[0])))) {
        self->m_allocated_count = self->m_free_count = scalar_pool_initial_size;
        self->m_count = 0;
        if (0 == pthread_mutex_init(&self->m_free_list_mutex, NULL)) {
            self->m_free_list_head = 0;
            for (size_t i = 1; i < self->m_allocated_count - 1; i++) {
                self->m_items[i - 1].m_value.next_free = i;
            }
            self->m_items[self->m_allocated_count - 1].m_value.next_free = -1;
            return 0;
        }
        else {
            free(self->m_items);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int pool_destroy(scalar_pool_t *self) {
    assert(self != NULL);
    if (0 == pthread_mutex_lock(&self->m_free_list_mutex)) {
        free(self->m_items);
        self->m_allocated_count = self->m_count = 0;
        pthread_mutex_destroy(&self->m_free_list_mutex);
        // FIXME loop over and destroy all the currently defined pool items
        return 0;
    }
    else {
        return -1;
    }
}

void pool_increase_refcount(scalar_pool_t *self, scalar_t handle) {
    assert(self != NULL);
    assert(handle <= self->m_allocated_count);
    assert(SCALAR_UNALLOC != (self->m_items[handle].m_flags & SCALAR_TYPE_MASK));
    assert(self->m_items[handle].m_references > 0);
    ++self->m_items[handle].m_references;
}

scalar_t pool_allocate_scalar(scalar_pool_t *self, uint32_t flags) {
    assert(self != NULL);
    
    if (0 == pthread_mutex_lock(&self->m_free_list_mutex)) {
        scalar_t handle;
        
        if (self->m_free_count > 0) {
            // allocate a new one from the free list
            assert(self->m_free_list_head >= 0);
            handle = self->m_free_list_head;        
            self->m_free_list_head = self->m_items[handle].m_value.next_free;
            self->m_free_count--;

            pthread_mutex_unlock(&self->m_free_list_mutex);
        }
        else {
            // grow the pool and allocate a new one from the increased free list
            handle = self->m_allocated_count;
            
            size_t new_size = self->m_allocated_count + scalar_pool_grow_size;
            pooled_scalar_t *tmp = calloc(new_size, sizeof(pooled_scalar_t));
            if (tmp != NULL)  {
                pthread_mutex_unlock(&self->m_free_list_mutex);
                return -1;   
            }
            memcpy(tmp, self->m_items, self->m_allocated_count * sizeof(pooled_scalar_t));
            free(self->m_items);
            self->m_items = tmp;
            self->m_allocated_count = new_size;
            
            self->m_free_list_head = handle + 1;
            for (size_t i = self->m_free_list_head; i < new_size - 1; i++) {
                self->m_items[i].m_value.next_free = i + 1;
            }
            self->m_items[new_size - 1].m_value.next_free = -1;            
            self->m_free_count = scalar_pool_grow_size - 1;
            
            pthread_mutex_unlock(&self->m_free_list_mutex);
        }
        
        self->m_items[handle].m_flags = SCALAR_UNDEF; // FIXME needs flags as an argument
        self->m_items[handle].m_value.as_int = 0;
        self->m_count++;
        return handle;
    }
    else {
        return -1;
    }
}

void pool_release_scalar(scalar_pool_t *self, scalar_t handle) {
    assert(self != NULL);
    assert(handle >= 0);
    assert(handle <= self->m_allocated_count);
    assert(self->m_items[handle].m_references > 0);
    
    if (--self->m_items[handle].m_references == 0) {
        if (self->m_items[handle].m_flags & SCALAR_FLAG_PTR) {
            switch(self->m_items[handle].m_flags & SCALAR_TYPE_MASK) {
                case SCALAR_STRING:
                    free(self->m_items[handle].m_value.as_string);
                    break;
            }
        }
        _pool_add_to_free_list(self, handle);        
        self->m_count--;
    }
}

void _pool_add_to_free_list(scalar_pool_t *self, scalar_t handle) {
    assert(self != NULL);
    assert(handle >= 0);
    
    scalar_t prev = handle;

    if (0 == pthread_mutex_lock(&self->m_free_list_mutex)) {
        for ( ; prev >= 0 && SCALAR_UNALLOC != (self->m_items[prev].m_flags & SCALAR_TYPE_MASK); --prev) ;
        
        if (prev >= 0) {
            self->m_items[handle].m_value.next_free = self->m_items[prev].m_value.next_free;
            self->m_items[prev].m_value.next_free = handle;
        }
        else {
            self->m_items[handle].m_value.next_free = self->m_free_list_head;
            self->m_free_list_head = handle;
        }
        
        self->m_items[handle].m_flags = SCALAR_UNALLOC;  // FIXME do this here?
        self->m_free_count++;

        pthread_mutex_unlock(&self->m_free_list_mutex);
    }
}
