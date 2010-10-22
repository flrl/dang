/*
 *  array.c
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"

struct array_t {
    size_t m_allocated_count;
    size_t m_count;
    scalar_handle_t *m_items;
};

static const size_t array_initial_reserve_size = 10; // FIXME 
static const size_t array_grow_size = 10; // FIXME

int array_init(array_t *self) {
    assert(self != NULL);
    
    if (NULL != (self->m_items = calloc(array_initial_reserve_size, sizeof(*self->m_items)))) {
        self->m_allocated_count = array_initial_reserve_size;
        self->m_count = 0;
        return 0;
    }
    else {
        self->m_allocated_count = 0;
        self->m_count = 0;
        return -1;
    }
}

int array_destroy(array_t *self) {
    assert(self != NULL);

    for (size_t i = 0; i < self->m_count; i++) {
        scalar_release(self->m_items[i]);
    }
    free(self->m_items);

    memset(self, 0, sizeof(*self));
    return 0;
}

int array_reserve(array_t *self, size_t new_size) {
    assert(self != NULL);
    if (self->m_allocated_count < new_size) {
        scalar_handle_t *new_items = calloc(new_size, sizeof(*new_items));
        if (NULL != new_items) {
            memcpy(new_items, self->m_items, self->m_count * sizeof(*self->m_items));
            scalar_handle_t *tmp = self->m_items;
            self->m_items = new_items;
            free(tmp);
            self->m_allocated_count = new_size;
            return 0;
        }
        else {
            return -1;
        }
    }
    else {
        return 0; // do nothing
    }
}

scalar_handle_t array_item_at(array_t *self, size_t index) {
    assert(self != NULL);
    assert(index < self->m_count);
    
    return scalar_increase_refcount(self->m_items[index]);
}

int array_push(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);
    if (self->m_count == self->m_allocated_count) {
        if (0 != array_reserve(self, 2 * self->m_allocated_count))  return -1;
    }

    self->m_items[self->m_count++] = scalar_increase_refcount(handle);
    return 0;
}

int array_unshift(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);
    if (self->m_count == self->m_allocated_count) {
        // FIXME this can't currently simply call reserve, cause it wants at least one 
        // FIXME of the new items to be at the start rather than the end.
        scalar_handle_t *tmp = self->m_items;
        self->m_items = calloc(self->m_allocated_count + array_grow_size, sizeof(*self->m_items));
        if (self->m_items == NULL) {
            self->m_items = tmp;
            return -1;
        }
        self->m_allocated_count += array_grow_size;
        self->m_items[0] = scalar_increase_refcount(handle);
        memcpy(&self->m_items[1], tmp, self->m_count * sizeof(scalar_t));
        self->m_count++;
        free(tmp);
        return 0;
    }
    else if (self->m_count > 0) {
        memmove(&self->m_items[1], &self->m_items[0], self->m_count * sizeof(scalar_t));
        self->m_items[0] = scalar_increase_refcount(handle);
        self->m_count++;
        return 0;
    }
    else {
        self->m_items[0] = scalar_increase_refcount(handle);
        self->m_count++;
        return 0;
    }
}

scalar_handle_t array_pop(array_t *self) {
    assert(self != NULL);

    if (self->m_count > 0) {
        return self->m_items[--self->m_count];
    }
    else {
        return 0;
    }
}

scalar_handle_t array_shift(array_t *self) {
    assert(self != NULL);

    if (self->m_count > 0) {
        scalar_handle_t handle = self->m_items[0];
        --self->m_count;
        memmove(&self->m_items[0], &self->m_items[1], self->m_count * sizeof(*self->m_items));
        return handle;
    }
    else {
        return 0;
    }
}
