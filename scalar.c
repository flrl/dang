/*
 *  scalar.c
 *  dang
 *
 *  Created by Ellie on 8/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "scalar.h"

#define POOL_ITEM(x)        g_scalar_pool.m_items[(x) - 1]

static scalar_pool_t g_scalar_pool;

const size_t scalar_pool_initial_size = 1024;   // FIXME arbitrary number
const size_t scalar_pool_grow_size = 1024;      // FIXME arbitrary number

void _scalar_pool_add_to_free_list(scalar_handle_t);

int scalar_pool_init(void) {
    if (NULL != (g_scalar_pool.m_items = calloc(scalar_pool_initial_size, sizeof(g_scalar_pool.m_items[0])))) {
        g_scalar_pool.m_allocated_count = g_scalar_pool.m_free_count = scalar_pool_initial_size;
        g_scalar_pool.m_count = 0;
        if (0 == pthread_mutex_init(&g_scalar_pool.m_free_list_mutex, NULL)) {
            g_scalar_pool.m_free_list_head = 1;
            for (scalar_handle_t i = 2; i < g_scalar_pool.m_allocated_count - 1; i++) {
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

void scalar_pool_increase_refcount(scalar_handle_t handle) {
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

scalar_handle_t scalar_pool_allocate_scalar(uint32_t flags) {
    if (0 == pthread_mutex_lock(&g_scalar_pool.m_free_list_mutex)) {
        scalar_handle_t handle;
        
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
            for (scalar_handle_t i = g_scalar_pool.m_free_list_head; i < new_size - 1; i++) {
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

void scalar_pool_release_scalar(scalar_handle_t handle) {
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

void _scalar_pool_add_to_free_list(scalar_handle_t handle) {
    assert(handle != 0);

    scalar_handle_t prev = handle;

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


static inline int _scalar_lock(scalar_handle_t handle) {
    if (POOL_ITEM(handle).m_flags & SCALAR_FLAG_SHARED) {
        return pthread_mutex_lock(POOL_ITEM(handle).m_mutex);
    }
    else {
        return 0;
    }
}

static inline int _scalar_unlock(scalar_handle_t handle) {
    if (POOL_ITEM(handle).m_flags & SCALAR_FLAG_SHARED) {
        return pthread_mutex_unlock(POOL_ITEM(handle).m_mutex);
    }
    else {
        return 0;
    }
}

static inline void _scalar_reset_unlocked(scalar_handle_t handle) {
    assert(handle != 0);
    
    // clean up allocated memory, if anything
    if (POOL_ITEM(handle).m_flags & SCALAR_FLAG_PTR) {
        switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                free(POOL_ITEM(handle).m_value.as_string);
                POOL_ITEM(handle).m_flags &= ~SCALAR_FLAG_PTR;
                break;
        }
    }
    
    // FIXME if it was a reference type, decrease ref counts on referenced object here
    
    // set up default values
    POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
    POOL_ITEM(handle).m_flags |= SCALAR_UNDEF;
    POOL_ITEM(handle).m_value.as_int = 0;    
}


void scalar_reset(scalar_handle_t handle) {
    if (0 == _scalar_lock(handle)) {
        _scalar_reset_unlocked(handle);
        _scalar_unlock(handle);
    }
}

void scalar_set_int_value(scalar_handle_t handle, intptr_t ival) {
    assert(handle != 0);
    
    if (0 == _scalar_lock(handle)) {
        _scalar_reset_unlocked(handle);
        POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
        POOL_ITEM(handle).m_flags |= SCALAR_INT;
        POOL_ITEM(handle).m_value.as_int = ival;
        _scalar_unlock(handle);
    }    
}

void scalar_set_float_value(scalar_handle_t handle, float fval) {
    assert(handle != 0);
    
    if (0 == _scalar_lock(handle)) {
        _scalar_reset_unlocked(handle);
        POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
        POOL_ITEM(handle).m_flags |= SCALAR_INT;
        POOL_ITEM(handle).m_value.as_float = fval;
        _scalar_unlock(handle);
    }
}

void scalar_set_string_value(scalar_handle_t handle, const char *sval) {
    assert(handle != 0);
    assert(sval != NULL);
    
    if (0 == _scalar_lock(handle)) {
        _scalar_reset_unlocked(handle);
        POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
        POOL_ITEM(handle).m_flags |= SCALAR_STRING | SCALAR_FLAG_PTR;
        POOL_ITEM(handle).m_value.as_string = strdup(sval);
        _scalar_unlock(handle);
    }
}

void scalar_set_value(scalar_handle_t handle, const anon_scalar_t *val) {  // FIXME implement this
    assert(handle != 0);
    assert(val != NULL);
    
    if (0 == _scalar_lock(handle)) {
        switch (val->m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                POOL_ITEM(handle).m_value.as_string = strdup(val->m_value.as_string);
                POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
                POOL_ITEM(handle).m_flags |= SCALAR_STRING | SCALAR_FLAG_PTR;
                break;
            default:
                memcpy(&POOL_ITEM(handle).m_value, &val->m_value, sizeof(POOL_ITEM(handle).m_value));
                POOL_ITEM(handle).m_flags &= ~SCALAR_TYPE_MASK;
                POOL_ITEM(handle).m_flags |= val->m_flags & SCALAR_TYPE_MASK;
                break;
        }
        _scalar_lock(handle);
    }
}


intptr_t scalar_get_int_value(scalar_handle_t handle) {
    assert(handle != 0);
    
    intptr_t value;
    
    if (0 == _scalar_lock(handle)) {
        switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_INT:
                value = POOL_ITEM(handle).m_value.as_int;
                break;
            case SCALAR_FLOAT:
                value = (intptr_t) POOL_ITEM(handle).m_value.as_float;
                break;
            case SCALAR_STRING:
                value = POOL_ITEM(handle).m_value.as_string != NULL ? strtol(POOL_ITEM(handle).m_value.as_string, NULL, 0) : 0;
                break;
            case SCALAR_UNDEF:
                value = 0;
                break;
            default:
                debug("unexpected type value %"PRIu32"\n", POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK);
                value = 0;
                break;
        }
        _scalar_unlock(handle);
    }
    
    return value;    
}

float scalar_get_float_value(scalar_handle_t handle) {
    assert(handle != 0);

    float value;
    
    if (0 == _scalar_lock(handle)) {
        switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_INT:
                value = (float) POOL_ITEM(handle).m_value.as_int;
                break;
            case SCALAR_FLOAT:
                value = POOL_ITEM(handle).m_value.as_float;
                break;
            case SCALAR_STRING:
                value = POOL_ITEM(handle).m_value.as_string != NULL ? strtof(POOL_ITEM(handle).m_value.as_string, NULL) : 0.0f;
                break;
            case SCALAR_UNDEF:
                value = 0;
                break;
            default:
                debug("unexpected type value %"PRIu32"\n", POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK);
                value = 0;
                break;
        }
        _scalar_unlock(handle);
    }
    
    return value;    
}

void scalar_get_string_value(scalar_handle_t handle, char **result) {
    assert(handle != 0);

    char numeric[100];
    
    if (0 == _scalar_lock(handle)) {
        switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_INT:
                snprintf(numeric, sizeof(numeric), "%"PRIiPTR"", POOL_ITEM(handle).m_value.as_int);
                *result = strdup(numeric);
                break;
            case SCALAR_FLOAT:
                snprintf(numeric, sizeof(numeric), "%f", POOL_ITEM(handle).m_value.as_float);
                *result = strdup(numeric);
                break;
            case SCALAR_STRING:
                *result = strdup(POOL_ITEM(handle).m_value.as_string);
                break;
            case SCALAR_UNDEF:
                *result = strdup("");
                break;
            default:
                debug("unexpected type value %"PRIu32"\n", POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK);
                *result = strdup("");
                break;
        }
        _scalar_unlock(handle);
    }
}

void scalar_get_value(scalar_handle_t handle, anon_scalar_t *result) {  // FIXME Implement this
    assert(handle != 0);
    assert(result != NULL);
    
    if ((result->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(result);
    
    if (0 == _scalar_lock(handle)) {
        switch(POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                result->m_value.as_string = strdup(POOL_ITEM(handle).m_value.as_string);
                result->m_flags = SCALAR_STRING | SCALAR_FLAG_PTR;
                break;
            default:
                memcpy(&result->m_value, &POOL_ITEM(handle).m_value, sizeof(result->m_value));
                result->m_flags = POOL_ITEM(handle).m_flags & SCALAR_TYPE_MASK;
                break;
        }
        _scalar_unlock(handle);
    }
}

// anon scalar functions
void anon_scalar_init(anon_scalar_t *self) {
    assert(self != NULL);
    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
}

void anon_scalar_destroy(anon_scalar_t *self) {
    assert(self != NULL);
    if (self->m_flags & SCALAR_FLAG_PTR) {
        switch (self->m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                if (self->m_value.as_string)  free(self->m_value.as_string);
                break;
            default:
                debug("unexpected anon scalar type: %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
                break;
        }
    }
    // FIXME other cleanup stuff
    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
}

void anon_scalar_clone(anon_scalar_t * restrict self, const anon_scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return;
    
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    switch(other->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_STRING:
            self->m_flags = SCALAR_FLAG_PTR | SCALAR_STRING;
            self->m_value.as_string = strdup(other->m_value.as_string);
            break;
        // FIXME other setup stuff
        default:
            memcpy(self, other, sizeof(*self));
    }
}

void anon_scalar_assign(anon_scalar_t * restrict self, const anon_scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return;
    
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    memcpy(self, other, sizeof(anon_scalar_t));
}

void anon_scalar_set_int_value(anon_scalar_t *self, intptr_t ival) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_INT;
    self->m_value.as_int = ival;
}

void anon_scalar_set_float_value(anon_scalar_t *self, float fval) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_FLOAT;
    self->m_value.as_float = fval;
}

void anon_scalar_set_string_value(anon_scalar_t *self, const char *sval) {
    assert(self != NULL);
    assert(sval != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_FLAG_PTR | SCALAR_STRING;
    self->m_value.as_string = strdup(sval);
}

intptr_t anon_scalar_get_int_value(const anon_scalar_t *self) {
    assert(self != NULL);
    intptr_t value;
    switch(self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_INT:
            value = self->m_value.as_int;
            break;
        case SCALAR_FLOAT:
            value = (intptr_t) self->m_value.as_float;
            break;
        case SCALAR_STRING:
            value = self->m_value.as_string != NULL ? strtol(self->m_value.as_string, NULL, 0) : 0;
            break;
        case SCALAR_UNDEF:
            value = 0;
            break;
        default:
            debug("unexpected type value %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
            value = 0;
            break;
    }
    return value;
}

float anon_scalar_get_float_value(const anon_scalar_t *self) {
    assert(self != NULL);
    float value;
    
    switch(self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_INT:
            value = (float) self->m_value.as_int;
            break;
        case SCALAR_FLOAT:
            value = self->m_value.as_float;
            break;
        case SCALAR_STRING:
            value = self->m_value.as_string != NULL ? strtof(self->m_value.as_string, NULL) : 0.0f;
            break;
        case SCALAR_UNDEF:
            value = 0;
            break;
        default:
            debug("unexpected type value %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
            value = 0;
            break;
    }
    
    return value;    
}

void anon_scalar_get_string_value(const anon_scalar_t *self, char **result) {
    assert(self != NULL);
    
    char numeric[100];
    
    switch(self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_INT:
            snprintf(numeric, sizeof(numeric), "%"PRIiPTR"", self->m_value.as_int);
            *result = strdup(numeric);
            break;
        case SCALAR_FLOAT:
            snprintf(numeric, sizeof(numeric), "%f", self->m_value.as_float);
            *result = strdup(numeric);
            break;
        case SCALAR_STRING:
            *result = strdup(self->m_value.as_string);
            break;
        case SCALAR_UNDEF:
            *result = strdup("");
            break;
        default:
            debug("unexpected type value %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
            *result = strdup("");
            break;
    }

    return;
}


