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

static const size_t _array_initial_reserve_size = 16;

static int _array_grow_back(array_t *, size_t);
static int _array_grow_front(array_t *, size_t);

/*
=head1 NAME

array

=head1 INTRODUCTION

=head1 PUBLIC INTERFACE

=over

=cut
*/


/*
=item array_init()

=item array_destroy()

Setup and teardown functions for array_t objects

=cut
*/
int array_init(array_t *self) {
    assert(self != NULL);
    
    memset(self, 0, sizeof(*self));
    self->m_items = calloc(_array_initial_reserve_size, sizeof(*self->m_items));

    if (self->m_items != NULL) {
        self->m_allocated_count = _array_initial_reserve_size;
        return 0;
    }
    else {
        return -1;
    }
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

/*
=item array_reserve()

Ensure an array has space for a certain number of items.  Additional space, if any, is allocated at the end of the array.

=cut
*/
int array_reserve(array_t *self, size_t target_size) {
    assert(self != NULL);

    size_t effective_size = self->m_allocated_count - self->m_first;

    if (effective_size < target_size) {
        return _array_grow_back(self, target_size - effective_size);
    }
    else {
        return 0;
    }
}

/*
=item array_item_at()

Gets a handle to the item in the array at a given index.  If the index is beyond the current 
bounds of the array, the array is grown to accommodate the index, with new items being set
to undefined.

Returns a handle to the item, or 0 on error.  The caller must release the handle when they are
done with it.

=cut
*/
scalar_handle_t array_item_at(array_t *self, size_t index) {
    assert(self != NULL);

    if (index >= self->m_first + self->m_count) {
        if (0 != array_reserve(self, index + 1))  return 0;
        
        size_t need = index - (self->m_first + self->m_count - 1);
        scalar_handle_t handle = scalar_allocate_many(need, 0);  // FIXME flags
        
        while (self->m_count <= index - self->m_first) {
            self->m_items[self->m_first + self->m_count++] = handle++;
        }
    }

    return scalar_reference(self->m_items[self->m_first + index]);
}

/*
=item array_push()

Adds an item at the end of the array.  The caller still needs to release their own copy of the handle.

=cut
*/
int array_push(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);

    if (self->m_first + self->m_count == self->m_allocated_count) {
        if (0 != _array_grow_back(self, self->m_count))  return -1;
    }

    self->m_items[self->m_count++] = scalar_reference(handle);
    return 0;
}

/*
=item array_unshift()

Adds an item before the first in the array.  The caller still needs to release their own copy of the handle.

=cut
*/
int array_unshift(array_t *self, scalar_handle_t handle) {
    assert(self != NULL);
    
    if (self->m_first == 0) {
        if (0 != _array_grow_front(self, self->m_count))  return -1;
    }
    
    self->m_items[--self->m_first] = scalar_reference(handle);
    return 0;
}

/*
=item array_pop()

Removes an item from the end of the array and returns it.  The caller must release the returned item when they are
done with it.

=cut
*/
scalar_handle_t array_pop(array_t *self) {
    assert(self != NULL);

    if (self->m_count > 0) {
        return self->m_items[self->m_first + --self->m_count];
    }
    else {
        return 0;
    }
}

/*
=item array_shift()

Removes an item from the start of the array and returns it.  The caller must release the returned itme when they're
done with it.

=cut
*/
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

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/

/*
=item _array_grow_back()

Allocates space for an additional n items at the end of the current allocation.

=cut
*/
static int _array_grow_back(array_t *self, size_t n) {
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

/*
=item _array_grow_front()

Allocates space for an additional n items at the front of the current allocation.

=cut
*/
static int _array_grow_front(array_t *self, size_t n) {
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

/*
=back

=cut
*/
