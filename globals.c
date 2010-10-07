/*
 *  globals.c
 *  dang
 *
 *  Created by Ellie on 7/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>

#include "globals.h"

//typedef struct globals_t {
//    size_t m_allocated_count;
//    size_t m_lock_granularity;
//    scalar_t *m_items;
//    pthread_mutex_t *m_locks;
//} globals_t;


size_t _globals_lock_index_for_item(globals_t *self, size_t index) {
    assert(self != NULL);
    assert(index >= 0 && index < self->m_allocated_count);

    return index / self->m_lock_granularity;  // FIXME is this correct?
}

int _globals_lock_item(globals_t *self, size_t index) {
    assert(self != NULL);
    assert(index >= 0 && index < self->m_allocated_count);

    size_t lock_index = _globals_lock_index_for_item(self, index);
    return pthread_mutex_lock(&self->m_locks[lock_index]);
}

int _globals_unlock_item(globals_t *self, size_t index) {
    assert(self != NULL);
    assert(index >= 0 && index < self->m_allocated_count);

    size_t lock_index = _globals_lock_index_for_item(self, index);
    return pthread_mutex_unlock(&self->m_locks[lock_index]);
}

int globals_init(globals_t *self, size_t count, size_t lock_granularity) {
    assert(self != NULL);
    
    if (NULL != (self->m_items = calloc(count, sizeof(scalar_t)))) {
        self->m_allocated_count = count;
        size_t num_locks = count / lock_granularity;
        if (NULL != (self->m_locks = calloc(num_locks, sizeof(pthread_mutex_t)))) {
            for (size_t i = 0; i < num_locks; i++) {
                pthread_mutex_init(&self->m_locks[i], NULL);
            }
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

int globals_destroy(globals_t *self) {
    assert(self != NULL);
    
    // FIXME write this
    return 0;
}

int globals_read(globals_t *self, size_t index, scalar_t *result) {
    assert(self != NULL);
    assert(result != NULL);
    
    if (0 == _globals_lock_item(self, index)) {
        scalar_clone(result, &self->m_items[index]);
        _globals_unlock_item(self, index);
        return 0;
    }
    else {
        return -1;
    }
}

int globals_write(globals_t *self, size_t index, const scalar_t *value) {
    assert(self != NULL);
    assert(value != NULL);
    
    if (0 == _globals_lock_item(self, index)) {
        scalar_clone(&self->m_items[index], value);
        _globals_unlock_item(self, index);
        return 0;
    }
    else {
        return -1;
    }
}




