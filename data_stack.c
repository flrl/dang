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

#include "bits.h"
#include "data_stack.h"

//typedef struct data_stack_t {
//    struct data_stack_t *m_parent;
//    size_t m_allocated_count;
//    size_t m_count;
//    scalar_t *m_items;
//    pthread_mutex_t m_mutex;
//    size_t m_subscope_count;
//} data_stack_t;

static const int data_stack_initial_reserve_size = 10;
static const int data_stack_grow_size = 10;

typedef struct data_stack_registry_node_t {
    struct data_stack_registry_node_t *m_next;
    data_stack_t *m_scope;
} data_stack_registry_node_t;

static data_stack_registry_node_t *_data_stack_registry = NULL;
static pthread_mutex_t _data_stack_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

void _data_stack_track_subscopes(data_stack_t *, int);

int data_stack_init(data_stack_t *self) {
    assert(self != NULL);
    
    int status;
    if (0 == (status = pthread_mutex_init(&self->m_mutex, NULL))) {
        int locked = pthread_mutex_lock(&self->m_mutex);
            assert(locked == 0);
            if (NULL != (self->m_items = calloc(data_stack_initial_reserve_size, sizeof(scalar_t)))) {
                self->m_parent = NULL;
                self->m_allocated_count = data_stack_initial_reserve_size;
                self->m_count = 0;
                self->m_subscope_count = 0;
                status = 0;
            }
            else {
                status = -1;
            }
        pthread_mutex_unlock(&self->m_mutex);        
    }
    
    return status;
}

int data_stack_destroy(data_stack_t *self) {
    assert(self != NULL);
    
    int locked = pthread_mutex_lock(&self->m_mutex);
    assert(locked == 0);
    pthread_mutex_destroy(&self->m_mutex);  // FIXME with mutex locked, or not?

    free(self->m_items);
    memset(self, 0, sizeof(*self));
    return 0;
}

int data_stack_reserve(data_stack_t *self, size_t new_size) {
    assert(self != NULL);
    
    int status;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
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

int data_stack_push(data_stack_t *self, const scalar_t *value) {
    assert(self != NULL);
    assert(value != NULL);
    
    int status = 0;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        if (self->m_count == self->m_allocated_count) {
            size_t new_size = self->m_allocated_count + data_stack_grow_size;
            pthread_mutex_unlock(&self->m_mutex);
            status = data_stack_reserve(self, new_size);
            int locked = pthread_mutex_lock(&self->m_mutex);
            assert(locked == 0);
        }
        
        if (status == 0)  scalar_clone(&self->m_items[self->m_count++], value);
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_pop(data_stack_t *self, scalar_t *result) {
    assert(self != NULL);
    
    int status = 0;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        if (self->m_count > 0) {
            --self->m_count;
            if (result != NULL) {
                scalar_assign(result, &self->m_items[self->m_count]);
            }
            else {
                scalar_destroy(&self->m_items[self->m_count]);
            }
            status = 0;
        }
        else {
            status = -1;
        }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_top(data_stack_t *self, scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);
    
    int status = 0;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        if (self->m_count > 0) {
            scalar_clone(result, &self->m_items[self->m_count - 1]);
            status = 0;
        }
        else {
            status = -1;
        }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_read_index(data_stack_t *self, size_t index, scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);
    
    int status = 0;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        if (index < self->m_count) {
            scalar_clone(result, &self->m_items[index]);
            status = 0;
        }
        else {
            status = -1;
        }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}

int data_stack_write_index(data_stack_t *self, size_t index, const scalar_t *value) {
    assert(self != NULL);
    assert(value != NULL);
    
    int status = 0;
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        if (index < self->m_count) {
            scalar_clone(&self->m_items[index], value);
            status = 0;
        }
//        else if (index == self->m_count) {
//            ; // FIXME push it onto the end?
//        }
        else {
            status = -1;
        }
    pthread_mutex_unlock(&self->m_mutex);
    return status;
}


int data_stack_start_scope(data_stack_t **data_stack_top) { 
    // FIXME thread safety?
    // FIXME think about this more
    data_stack_t *new_scope;
    if (NULL != (new_scope = calloc(1, sizeof(data_stack_t)))) {
        if (0 == data_stack_init(new_scope)) {
            new_scope->m_parent = *data_stack_top;
            _data_stack_track_subscopes(*data_stack_top, +1);
            *data_stack_top = new_scope;
            
            // register the new scope for cleanup later
            data_stack_registry_node_t *reg_entry;
            if (NULL != (reg_entry = calloc(1, sizeof(data_stack_registry_node_t)))) {
                int locked = pthread_mutex_lock(&_data_stack_registry_mutex);
                    assert(locked == 0);
                    reg_entry->m_scope = new_scope;
                    reg_entry->m_next = _data_stack_registry;
                    _data_stack_registry = reg_entry;
                pthread_mutex_unlock(&_data_stack_registry_mutex);
            }
            
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

int data_stack_end_scope(data_stack_t **data_stack_top) { 
    // FIXME thread safety?
    // FIXME think about this more
    data_stack_t *old_scope = *data_stack_top;
    *data_stack_top = old_scope->m_parent;
    
    // if nothing else refers to the old scope, it can be cleaned up
    if (old_scope->m_subscope_count == 0) {
        data_stack_destroy(old_scope);
        _data_stack_track_subscopes(*data_stack_top, -1);

        // remove its registry entry
        int locked = pthread_mutex_lock(&_data_stack_registry_mutex);
            assert(locked == 0);
            for (data_stack_registry_node_t *i = _data_stack_registry; i != NULL && i->m_next != NULL; i = i->m_next) {
                if (i->m_next->m_scope == old_scope) {
                    data_stack_registry_node_t *tmp = i->m_next;
                    i->m_next = i->m_next->m_next;
                    free(tmp);
                }
            }
        pthread_mutex_unlock(&_data_stack_registry_mutex);
    
        free(old_scope);
    }
    
    return -0;
}

void _data_stack_track_subscopes(data_stack_t *self, int delta) {
    assert(self != NULL);
    
    int locked = pthread_mutex_lock(&self->m_mutex);
        assert(locked == 0);
        assert(self->m_subscope_count > 0 || delta >= 0);
        self->m_subscope_count += delta;
    pthread_mutex_unlock(&self->m_mutex);
}

int data_stack_registry_reap(void) {
    int locked = pthread_mutex_lock(&_data_stack_registry_mutex);
        assert(locked == 0);
        while(_data_stack_registry != NULL) {
            data_stack_registry_node_t *tmp = _data_stack_registry;
            _data_stack_registry = _data_stack_registry->m_next;
            data_stack_destroy(tmp->m_scope);
            free(tmp->m_scope);
            free(tmp);
        }
    pthread_mutex_unlock(&_data_stack_registry_mutex);
    return 0;
}
