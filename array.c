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

static const size_t array_initial_reserve_size = 10;
static const size_t array_grow_size = 10;

int array_init(array_t *self) {
    assert(self != NULL);
    
    if (NULL != (self->m_items = calloc(array_initial_reserve_size, sizeof(scalar_t)))) {
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
        scalar_destroy(&self->m_items[i]);
    }
    free(self->m_items);

    memset(self, 0, sizeof(*self));
    return 0;
}

int array_reserve(array_t *self, size_t new_size) {
    assert(self != NULL);
    if (self->m_allocated_count < new_size) {
        scalar_t *new_items = calloc(new_size, sizeof(scalar_t));
        if (NULL != new_items) {
            memcpy(new_items, self->m_items, self->m_count * sizeof(scalar_t));
            free(self->m_items);
            self->m_items = new_items;
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

int array_item_at(array_t *self, size_t index, scalar_t *result) {
    assert(self != NULL);
    assert(index < self->m_count);
    assert(result != NULL);
    
    scalar_clone(result, &self->m_items[index]);
    return 0;
}

int array_push(array_t *self, const scalar_t *value) {
    assert(self != NULL);
    if (self->m_count == self->m_allocated_count) {
        scalar_t *tmp = self->m_items;
        self->m_items = calloc(self->m_allocated_count + array_grow_size, sizeof(scalar_t));
        if (self->m_items == NULL) {
            self->m_items = tmp;
            return -1;
        }
        self->m_allocated_count += array_grow_size;
        memcpy(self->m_items, tmp, self->m_count * sizeof(scalar_t));
        free(tmp);
    }
    scalar_clone(&self->m_items[self->m_count++], value);
    return 0;
}

int array_unshift(array_t *self, const scalar_t *value) {
    assert(self != NULL);
    assert(value != NULL);
    if (self->m_count == self->m_allocated_count) {
        scalar_t *tmp = self->m_items;
        self->m_items = calloc(self->m_allocated_count + array_grow_size, sizeof(scalar_t));
        if (self->m_items == NULL) {
            self->m_items = tmp;
            return -1;
        }
        self->m_allocated_count += array_grow_size;
        scalar_clone(&self->m_items[0], value);
        memcpy(&self->m_items[1], tmp, self->m_count * sizeof(scalar_t));
        self->m_count++;
        free(tmp);
        return 0;
    }
    else if (self->m_count > 0) {
        memmove(&self->m_items[1], &self->m_items[0], self->m_count * sizeof(scalar_t));
        scalar_clone(&self->m_items[0], value);
        self->m_count++;
        return 0;
    }
    else {
        scalar_clone(&self->m_items[0], value);
        self->m_count++;
        return 0;
    }
}

int array_pop(array_t *self, scalar_t *result) {
    assert(self != NULL);

    if (self->m_count > 0) {
        --self->m_count;
        if (result != NULL) {
            scalar_assign(result, &self->m_items[self->m_count]);
        }
        else {
            scalar_destroy(&self->m_items[self->m_count]);
        }
        return 0;
    }
    else {
        return -1;
    }
}

int array_shift(array_t *self, scalar_t *result) {
    assert(self != NULL);

    if (self->m_count > 0) {
        scalar_t value = self->m_items[0];
        --self->m_count;
        memmove(&self->m_items[0], &self->m_items[1], self->m_count * sizeof(scalar_t));
        if (result != NULL) {
            scalar_assign(result, &value);
        }
        else {
            scalar_destroy(&value);
        }
        return 0;
    }
    else {
        return -1;
    }
}
