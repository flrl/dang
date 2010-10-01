/*
 *  array.c
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"

//typedef struct array_t {
//    size_t m_allocated_count;
//    size_t m_count;
//    struct scalar_t *m_items;
//} array_t;
//
//array_t *array_create(size_t initial_size);
//array_t *array_destroy(array_t *self);
//
//scalar_t array_item_at(array_t *self, size_t index);
//
//int  array_push(array_t *self, scalar_t value);
//int  array_unshift(array_t *self, scalar_t value);
//
//scalar_t array_pop(array_t *self);
//scalar_t array_shift(array_t *self);
//
//array_t *array_splice(array_t *self, size_t index, size_t count);
//

#define ARRAY_GROW_SIZE 10

array_t *array_create(size_t initial_count) {
    array_t *self = calloc(1, sizeof(array_t));
    if (!self)  return NULL;

    if (initial_count == 0)  initial_count = ARRAY_GROW_SIZE;
    self->m_items = calloc(initial_count, sizeof(scalar_t));
    if (!self->m_items) {
        free(self);
        return NULL;
    }

    self->m_allocated_count = initial_count;
    self->m_count = 0;
    
    return self;
}

array_t *array_destroy(array_t *self) {
    assert(self != NULL);
    for (size_t i = 0; i < self->m_count; i++) {
        scalar_destroy(&self->m_items[i]);
    }
    free(self->m_items);
    free(self);
    return NULL;
}

scalar_t array_item_at(array_t *self, size_t index) {
    assert(self != NULL);
    assert(index < self->m_count);
    return self->m_items[index];
}

int array_push(array_t *self, scalar_t value) {
    assert(self != NULL);
    if (self->m_count == self->m_allocated_count) {
        scalar_t *tmp = self->m_items;
        self->m_items = calloc(self->m_allocated_count + ARRAY_GROW_SIZE, sizeof(scalar_t));
        if (self->m_items == NULL) {
            self->m_items = tmp;
            return -1;
        }
        self->m_allocated_count += ARRAY_GROW_SIZE;
        memcpy(self->m_items, tmp, self->m_count * sizeof(scalar_t));
        free(tmp);
    }
    self->m_items[self->m_count++] = value;
    return 0;
}

int array_unshift(array_t *self, scalar_t value) {
    assert(self != NULL);
    if (self->m_count == self->m_allocated_count) {
        scalar_t *tmp = self->m_items;
        self->m_items = calloc(self->m_allocated_count + ARRAY_GROW_SIZE, sizeof(scalar_t));
        if (self->m_items == NULL) {
            self->m_items = tmp;
            return -1;
        }
        self->m_allocated_count += ARRAY_GROW_SIZE;
        self->m_items[0] = value;
        memcpy(&self->m_items[1], tmp, self->m_count * sizeof(scalar_t));
        self->m_count++;
        free(tmp);
        return 0;
    }
    else if (self->m_count > 0) {
        memmove(&self->m_items[1], &self->m_items[0], self->m_count * sizeof(scalar_t));
        self->m_items[0] = value;
        self->m_count++;
        return 0;
    }
    else {
        self->m_items[0] = value;
        self->m_count++;
        return 0;
    }
}

scalar_t array_pop(array_t *self) {
    // FIXME what if it's empty
    assert(self != NULL);
    return self->m_items[--self->m_count];
}


scalar_t array_shift(array_t *self) {
    // FIXME what if it's empty
    assert(self != NULL);
    scalar_t value = self->m_items[0];
    --self->m_count;
    memmove(&self->m_items[0], &self->m_items[1], self->m_count * sizeof(scalar_t));
    return value;
}

array_t *array_splice(array_t *self, size_t index, size_t count) {
    assert(self != NULL);
    array_t *spliced = array_create(self->m_count);
    if (spliced == NULL)  return NULL;
    for (size_t i = 0; i < count; i++) {
        spliced->m_items[i] = self->m_items[i + index];
    }
    return spliced;
}

