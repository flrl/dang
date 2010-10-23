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
    size_t m_first;
    scalar_handle_t *m_items;
};

static const size_t array_initial_reserve_size = 10; // FIXME 
static const size_t array_grow_size = 10; // FIXME

int array_init(array_t *self) {
    assert(self != NULL);
    
    memset(self, 0, sizeof(*self));
    self->m_items = calloc(array_initial_reserve_size, sizeof(*self->m_items));

    return (self->m_items != NULL ? 0 : -1);
}

int array_destroy(array_t *self) {
    assert(self != NULL);

    for (size_t i = self->m_first; i < self->m_first + self->m_count; i++) {
        scalar_release(self->m_items[i]);
    }
    free(self->m_items);

    memset(self, 0, sizeof(*self));
    return 0;
}

// this is sort of obsolete
int array_reserve(array_t *self, size_t new_size) {
    assert(self != NULL);
    if (self->m_allocated_count < new_size) {
        scalar_handle_t *new_items = calloc(new_size, sizeof(*new_items));
        if (NULL != new_items) {
            memcpy(new_items, self->m_items, self->m_allocated_count * sizeof(*self->m_items));
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

int array_grow_back(array_t *self, size_t n) {
    assert(self != NULL);
    scalar_handle_t *new_items = calloc(self->m_allocated_count + n, sizeof(*new_items));
    if (new_items != NULL) {
        memcpy(new_items, self->m_items, self->m_allocated_count * sizeof(*self->m_items));
        scalar_handle_t *tmp = self->m_items;
        self->m_items = new_items;
        self->m_allocated_count += n;
        free(tmp);
        return 0;
    }
    else {
        return -1;
    }
}

int array_grow_front(array_t *self, size_t n) {
    assert(self != NULL);
    scalar_handle_t *new_items = calloc(self->m_allocated_count + n, sizeof(*new_items));
    if (new_items != NULL) {
        memcpy(&new_items[n], self->m_items, self->m_allocated_count * sizeof(*self->m_items));
        scalar_handle_t *tmp = self->m_items;
        self->m_items = new_items;
        self->m_first += n;
        self->m_allocated_count += n;
        free(tmp);
        return 0;
    }
    else {
        return -1;
    }
}

scalar_handle_t array_item_at(array_t *self, size_t index) {
    assert(self != NULL);
    assert(index < self->m_count);
    
    return scalar_increase_refcount(self->m_items[self->m_first + index]);
}

int array_push(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);

    if (self->m_first + self->m_count == self->m_allocated_count) {
        if (0 != array_grow_back(self, self->m_count))  return -1;
    }

    self->m_items[self->m_count++] = scalar_increase_refcount(handle);
    return 0;
}

int array_unshift(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);
    
    if (self->m_first == 0) {
        if (0 != array_grow_front(self, self->m_count))  return -1;
    }
    
    self->m_items[--self->m_first] = scalar_increase_refcount(handle);
    return 0;
}

scalar_handle_t array_pop(array_t *self) {
    assert(self != NULL);

    if (self->m_count > 0) {
        return self->m_items[self->m_first + --self->m_count];
    }
    else {
        return 0;
    }
}

scalar_handle_t array_shift(array_t *self) {
    assert(self != NULL);

    if (self->m_count > 0) {
        --self->m_count;
        return self->m_items[self->m_first++];
    }
    else {
        return 0;
    }
}
