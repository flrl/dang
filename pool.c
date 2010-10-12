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

#define POOL_ITEM(x)        g_scalar_pool.m_items[(x) - 1]

static scalar_pool_t g_scalar_pool;

const size_t scalar_pool_initial_size = 1024;   // FIXME arbitrary number
const size_t scalar_pool_grow_size = 1024;      // FIXME arbitrary number

void _scalar_pool_add_to_free_list(scalar_t);

int scalar_pool_init(void) {
    if (NULL != (g_scalar_pool.m_items = calloc(scalar_pool_initial_size, sizeof(g_scalar_pool.m_items[0])))) {
        g_scalar_pool.m_allocated_count = g_scalar_pool.m_free_count = scalar_pool_initial_size;
        g_scalar_pool.m_count = 0;
        if (0 == pthread_mutex_init(&g_scalar_pool.m_free_list_mutex, NULL)) {
            g_scalar_pool.m_free_list_head = 1;
            for (scalar_t i = 2; i < g_scalar_pool.m_allocated_count - 1; i++) {
                POOL_ITEM(i).m_value.next_free = i;
            }
            POOL_ITEM(g_scalar_pool.m_allocated_count).m_value.next_free = 0;
            return 0;
        }
        else {
            free(g_scalar_pool.m_items);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int scalar_pool_destroy(void) {
    if (0 == pthread_mutex_lock(&g_scalar_pool.m_free_list_mutex)) {
        free(g_scalar_pool.m_items);
        g_scalar_pool.m_allocated_count = g_scalar_pool.m_count = 0;
        pthread_mutex_destroy(&g_scalar_pool.m_free_list_mutex);
        // FIXME loop over and properly destroy all the currently defined pool items
        return 0;
    }
    else {
        return -1;
    }
}

void scalar_pool_increase_refcount(scalar_t handle) {
    assert(handle != 0);
    assert(handle <= g_scalar_pool.m_allocated_count);
    assert(SCALAR_UNALLOC != (POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK));
    assert(POOL_ITEM(handle).m_references > 0);

    const uint32_t shared = POOL_ITEM(handle).m_flags & SCALAR_FLAG_SHARED;
    if (shared) {
        assert(POOL_ITEM(handle).m_mutex != NULL);
        pthread_mutex_lock(POOL_ITEM(handle).m_mutex);
    }

    ++POOL_ITEM(handle).m_references;

    if (shared)  pthread_mutex_unlock(POOL_ITEM(handle).m_mutex);
}

scalar_t scalar_pool_allocate_scalar(uint32_t flags) {
    if (0 == pthread_mutex_lock(&g_scalar_pool.m_free_list_mutex)) {
        scalar_t handle;
        
        if (g_scalar_pool.m_free_count > 0) {
            // allocate a new one from the free list
            assert(g_scalar_pool.m_free_list_head != 0);
            handle = g_scalar_pool.m_free_list_head;        
            g_scalar_pool.m_free_list_head = POOL_ITEM(handle).m_value.next_free;
            g_scalar_pool.m_free_count--;

            pthread_mutex_unlock(&g_scalar_pool.m_free_list_mutex);
        }
        else {
            // grow the pool and allocate a new one from the increased free list
            handle = g_scalar_pool.m_allocated_count + 1;
            
            size_t new_size = g_scalar_pool.m_allocated_count + scalar_pool_grow_size;
            pooled_scalar_t *tmp = calloc(new_size, sizeof(pooled_scalar_t));
            if (tmp != NULL)  {
                pthread_mutex_unlock(&g_scalar_pool.m_free_list_mutex);
                return -1;   
            }
            memcpy(tmp, g_scalar_pool.m_items, g_scalar_pool.m_allocated_count * sizeof(pooled_scalar_t));
            free(g_scalar_pool.m_items);
            g_scalar_pool.m_items = tmp;
            g_scalar_pool.m_allocated_count = new_size;
            
            g_scalar_pool.m_free_list_head = handle + 1;            
            for (scalar_t i = g_scalar_pool.m_free_list_head; i < new_size - 1; i++) {
                POOL_ITEM(i).m_value.next_free = i;
            }
            g_scalar_pool.m_items[new_size - 1].m_value.next_free = 0;
            g_scalar_pool.m_free_count = scalar_pool_grow_size - 1;
            
            pthread_mutex_unlock(&g_scalar_pool.m_free_list_mutex);
        }
        
        POOL_ITEM(handle).m_flags = SCALAR_UNDEF; // FIXME sanitise flags argument and use it, eg set up mutex for shared ones
        POOL_ITEM(handle).m_value.as_int = 0;
        g_scalar_pool.m_count++;
        return handle;
    }
    else {
        return -1;
    }
}

void scalar_pool_release_scalar(scalar_t handle) {
    assert(handle != 0);
    assert(handle <= g_scalar_pool.m_allocated_count);
    assert(POOL_ITEM(handle).m_references > 0);
    
    if (--POOL_ITEM(handle).m_references == 0) {
        // FIXME should this just call scalar_destroy?
        if (POOL_ITEM(handle).m_flags & SCALAR_FLAG_PTR) {
            switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
                case SCALAR_STRING:
                    free(POOL_ITEM(handle).m_value.as_string);
                    break;
            }
        }
        _scalar_pool_add_to_free_list(handle);        
        g_scalar_pool.m_count--;
    }
}

void _scalar_pool_add_to_free_list(scalar_t handle) {
    assert(handle != 0);

    scalar_t prev = handle;

    if (0 == pthread_mutex_lock(&g_scalar_pool.m_free_list_mutex)) {
        for ( ; prev > 0 && SCALAR_UNALLOC != (POOL_ITEM(prev).m_flags & SCALAR_TYPE_MASK); --prev) ;
        
        if (prev > 0) {
            POOL_ITEM(handle).m_value.next_free = POOL_ITEM(prev).m_value.next_free;
            POOL_ITEM(prev).m_value.next_free = handle;
        }
        else {
            POOL_ITEM(handle).m_value.next_free = g_scalar_pool.m_free_list_head;
            g_scalar_pool.m_free_list_head = handle;
        }
        
        POOL_ITEM(handle).m_flags = SCALAR_UNALLOC;
        g_scalar_pool.m_free_count++;

        pthread_mutex_unlock(&g_scalar_pool.m_free_list_mutex);
    }
}

// FIXME

void scalar_init(scalar_t handle) {
    assert(handle != 0);
    assert((POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) != SCALAR_UNALLOC);
    
    if ((POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  scalar_destroy(handle);
    
    // FIXME what does this need to actually do now?
//    self->m_type = ScUNDEFINED;
//    self->m_value.int_value = 0;
}

void scalar_destroy(scalar_t handle) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
    // FIXME i guess it sets it back to undef, and cleans up any malloc'd values
//    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
//    self->m_type = ScUNDEFINED;
//    self->m_value.int_value = 0;
}

void scalar_set_int_value(scalar_t handle, intptr_t ival) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
//    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
//    self->m_type = ScINT;
//    self->m_value.int_value = ival;
}

void scalar_set_double_value(scalar_t handle, float dval) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
//    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
//    self->m_type = ScDOUBLE;
//    self->m_value.double_value = dval;
}

void scalar_set_string_value(scalar_t handle, const char *sval) {
    assert(handle != 0);
    assert(sval != NULL);
    // FIXME what does this need to actually do now?
//    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
//    self->m_type = ScSTRING;
//    self->m_value.string_value = strdup(sval);
}

intptr_t scalar_get_int_value(scalar_t handle) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
//    switch(self->m_type) {
//        case ScUNDEFINED:
//            return 0;
//        case ScINT:
//            return self->m_value.int_value;
//        case ScDOUBLE:
//            return (intptr_t) self->m_value.double_value;
//        case ScSTRING:
//            return self->m_value.string_value != NULL ? strtol(self->m_value.string_value, NULL, 0) : 0;
//        default:
//            return 0;
//    }
}

float scalar_get_double_value(scalar_t handle) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
//    switch(self->m_type) {
//        case ScUNDEFINED:
//            return 0.0;
//        case ScINT:
//            return (double) self->m_value.int_value;
//        case ScDOUBLE:
//            return self->m_value.double_value;
//        case ScSTRING:
//            return self->m_value.string_value != NULL ? strtod(self->m_value.string_value, NULL) : 0.0;
//        default:
//            return 0;
//    }    
}

void scalar_get_string_value(scalar_t handle, char **result) {
    assert(handle != 0);
    // FIXME what does this need to actually do now?
//    
//    char buffer[100];
//    
//    switch(self->m_type) {
//        case ScUNDEFINED:
//            *result = strdup("");
//            return;
//        case ScINT:
//            snprintf(buffer, sizeof(buffer), "%"PRIiPTR"", self->m_value.int_value);
//            *result = strdup(buffer);
//            break;
//        case ScDOUBLE:
//            snprintf(buffer, sizeof(buffer), "%g", self->m_value.double_value);
//            *result = strdup(buffer);
//            break;
//        case ScSTRING:
//            *result = strdup(self->m_value.string_value);
//            break;
//        default:
//            ; // do nothing
//    }
//    
//    return;
}
