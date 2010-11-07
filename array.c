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

#include "scalar.h"

#include "array.h"

#define ARRAY(handle)   POOL_OBJECT(array_t, handle)

POOL_SOURCE_CONTENTS(array_t);

static const size_t _array_initial_reserve_size = 16;

static int _array_reserve(array_t *, size_t);
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
=item array_pool_init()

=item array_pool_destroy()

Setup and teardown functions for the array pool.

=cut
 */
int array_pool_init(void) {
    return POOL_INIT(array_t);
}

int array_pool_destroy(void) {
    return POOL_DESTROY(array_t);
}

/*
=item array_allocate()

=item array_allocate_many()

=item array_reference()

=item array_release()

Functions for managing allocation of arrays.

=cut
 */
array_handle_t array_allocate(void) {
    return POOL_ALLOCATE(array_t, 0);
}

array_handle_t array_allocate_many(size_t n) {
    return POOL_ALLOCATE_MANY(array_t, n, 0);
}

array_handle_t array_reference(array_handle_t handle) {
    return POOL_REFERENCE(array_t, handle);
}

int array_release(array_handle_t handle) {
    return POOL_RELEASE(array_t, handle);
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
scalar_handle_t array_item_at(array_handle_t handle, size_t index) {
    assert(POOL_HANDLE_VALID(array_t, handle));

    if (index >= ARRAY(handle).m_first + ARRAY(handle).m_count) {
        if (0 != _array_reserve(&ARRAY(handle), index + 1))  return 0;
        
        size_t need = index - (ARRAY(handle).m_first + ARRAY(handle).m_count - 1);
        scalar_handle_t handle = scalar_allocate_many(need, 0);  FIXME("handle flags\n");
        
        while (ARRAY(handle).m_count <= index - ARRAY(handle).m_first) {
            ARRAY(handle).m_items[ARRAY(handle).m_first + ARRAY(handle).m_count++] = handle + 1;
            ++handle;
        }
    }

    return scalar_reference(ARRAY(handle).m_items[ARRAY(handle).m_first + index]);
}

/*
=item array_push()

Adds an item at the end of the array.

=cut
*/
int array_push(array_handle_t handle, const scalar_t *value) {
    assert(POOL_HANDLE_VALID(array_t, handle));

    if (ARRAY(handle).m_first + ARRAY(handle).m_count == ARRAY(handle).m_allocated_count) {
        if (0 != _array_grow_back(&ARRAY(handle), ARRAY(handle).m_count))  return -1;
    }

    scalar_handle_t s = scalar_allocate(0); FIXME("handle flags\n");
    scalar_set_value(s, value);
    ARRAY(handle).m_items[ARRAY(handle).m_count++] = s;
    return 0;
}

/*
=item array_unshift()

Adds an item at the start of the array.

=cut
*/
int array_unshift(array_handle_t handle, const scalar_t *value) {
    assert(POOL_HANDLE_VALID(array_t, handle));
    
    scalar_handle_t s = scalar_allocate(0);  FIXME("handle flags\n");
    scalar_set_value(s, value);

    if (ARRAY(handle).m_count == 0) {
        ARRAY(handle).m_items[ARRAY(handle).m_first] = s;
        ++ARRAY(handle).m_count;
    }
    else {
        if (ARRAY(handle).m_first == 0) {
            if (0 != _array_grow_front(&ARRAY(handle), ARRAY(handle).m_count)) {
                scalar_release(s);
                return -1;
            }
        }
        
        ARRAY(handle).m_items[--ARRAY(handle).m_first] = s;
        ++ARRAY(handle).m_count;
    }
    return 0;
}

/*
=item array_pop()

Removes an item from the end of the array and returns it.  The caller must release the returned item when they are
done with it.

=cut
*/
int array_pop(array_handle_t handle, scalar_t *result) {
    assert(POOL_HANDLE_VALID(array_t, handle));

    if (ARRAY(handle).m_count > 0) {
        scalar_handle_t s = ARRAY(handle).m_items[ARRAY(handle).m_first + --ARRAY(handle).m_count];
        if (result != NULL)  scalar_get_value(s, result);
        scalar_release(s);
        return 0;
    }
    else {
        return -1;
    }
}

/*
=item array_shift()

Removes an item from the start of the array and returns it.  The caller must release the returned itme when they're
done with it.

=cut
*/
int array_shift(array_handle_t handle, scalar_t *result) {
    assert(POOL_HANDLE_VALID(array_t, handle));

    if (ARRAY(handle).m_count > 0) {
        --ARRAY(handle).m_count;
        scalar_handle_t s = ARRAY(handle).m_items[ARRAY(handle).m_first++];
        if (result != NULL)  scalar_get_value(s, result);
        scalar_release(s);
        return 0;
    }
    else {
        return -1;
    }
}

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/

/*
=item _array_init()

=item _array_destroy()

Setup and teardown functions for array_t objects

=cut
 */
int _array_init(array_t *self) {
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

int _array_destroy(array_t *self) {
    assert(self != NULL);
    
    for (size_t i = self->m_first; i < self->m_first + self->m_count; i++) {
        scalar_release(self->m_items[i]);
    }
    free(self->m_items);
    
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item _array_reserve()

Ensure an array has space for a certain number of items.  Additional space, if any, is allocated at the end of the array.

=cut
 */
static int _array_reserve(array_t *self, size_t target_size) {
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
