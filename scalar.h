/*
 *  scalar.h
 *  dang
 *
 *  Created by Ellie on 8/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */


#ifndef SCALAR_H
#define SCALAR_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "floatptr_t.h"
#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE   (1024) /* FIXME arbitrary number */
#include "pool.h"

#define SCALAR(handle)      POOL_OBJECT(scalar_t, handle)

#define SCALAR_UNDEF            0x00000000u
#define SCALAR_INT              0x01u
#define SCALAR_FLOAT            0x02u
#define SCALAR_STRING           0x03u
#define SCALAR_FILEHANDLE       0x04u
// ...
#define SCALAR_SCAREF           0x11u
#define SCALAR_ARRREF           0x12u
#define SCALAR_HASHREF          0x13u
#define SCALAR_CHANREF          0x14u
#define SCALAR_FUNCREF          0x15u

#define SCALAR_TYPE_MASK        0x0000001Fu
#define SCALAR_FLAGS_MASK       0xFFFFFFE0u

#define SCALAR_FLAG_REF         0x00000010u     /* pseudo flag, actually part of the type mask */
// ...
#define SCALAR_FLAG_PTR         0x08000000u

#define SCALAR_ALL_FLAGS        0x0800001Fu     /* keep this up to date */
/*
 0000 1000  0000 0000  0000 0000  0001 1111
      |                              | ''''-- basic types
      |                              '------- value is a reference
      '-------------------------------------- value is a malloc'd pointer, make sure to free it
 */

typedef struct scalar_t {
    uint32_t m_flags;
    union {
        intptr_t as_int;
        floatptr_t as_float;
        char     *as_string;
        scalar_handle_t as_scalar_handle;
        array_handle_t as_array_handle;
        hash_handle_t as_hash_handle;
        channel_handle_t as_channel_handle;
        function_handle_t as_function_handle;
    } m_value;
} scalar_t;

int anon_scalar_init(scalar_t *);
int anon_scalar_destroy(scalar_t *);

POOL_HEADER_CONTENTS(scalar_t, scalar_handle_t, PTHREAD_MUTEX_RECURSIVE, anon_scalar_init, anon_scalar_destroy);

int anon_scalar_clone(scalar_t * restrict, const scalar_t * restrict);
int anon_scalar_assign(scalar_t * restrict, const scalar_t * restrict);

void anon_scalar_set_int_value(scalar_t *, intptr_t);
void anon_scalar_set_float_value(scalar_t *, floatptr_t);
void anon_scalar_set_string_value(scalar_t *, const char *);

intptr_t anon_scalar_is_defined(const scalar_t *);
intptr_t anon_scalar_get_bool_value(const scalar_t *);
intptr_t anon_scalar_get_int_value(const scalar_t *);
floatptr_t anon_scalar_get_float_value(const scalar_t *);
void anon_scalar_get_string_value(const scalar_t *, char **);

void anon_scalar_set_scalar_reference(scalar_t *, scalar_handle_t);
void anon_scalar_set_array_reference(scalar_t *, array_handle_t);
void anon_scalar_set_hash_reference(scalar_t *, hash_handle_t);
void anon_scalar_set_channel_reference(scalar_t *, channel_handle_t);
void anon_scalar_set_function_reference(scalar_t *, function_handle_t);

scalar_handle_t anon_scalar_deref_scalar_reference(const scalar_t *);
array_handle_t anon_scalar_deref_array_reference(const scalar_t *);
hash_handle_t anon_scalar_deref_hash_reference(const scalar_t *);
channel_handle_t anon_scalar_deref_channel_reference(const scalar_t *);
function_handle_t anon_scalar_deref_function_reference(const scalar_t *);


int scalar_pool_init(void);
int scalar_pool_destroy(void);

scalar_handle_t scalar_allocate(uint32_t);
scalar_handle_t scalar_allocate_many(size_t, uint32_t);
scalar_handle_t scalar_reference(scalar_handle_t);
int scalar_release(scalar_handle_t);

/*
=head2 Pooled Scalar Functions

=over

=cut
 */
 
/*
=item scalar_lock()

=item scalar_unlock()

Functions for locking and unlocking pooled scalars.  These do nothing if the scalar was not allocated as shared.

=cut
*/
static inline int scalar_lock(scalar_handle_t handle) {
    return POOL_LOCK(scalar_t, handle);
}

static inline int scalar_unlock(scalar_handle_t handle) {
    return POOL_UNLOCK(scalar_t, handle);
}

/*
=item scalar_set_undef()

=item scalar_set_int_value()

=item scalar_set_float_value()

=item scalar_set_string_value()

=item scalar_set_value()

Functions for setting values on a pooled scalar.  Any previous value is properly cleaned up.

=cut
 */
static inline void scalar_set_undef(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_destroy(&SCALAR(handle));
        scalar_unlock(handle);
    }
}

static inline void scalar_set_int_value(scalar_handle_t handle, intptr_t ival) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_int_value(&SCALAR(handle), ival);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_float_value(scalar_handle_t handle, floatptr_t fval) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_float_value(&SCALAR(handle), fval);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_string_value(scalar_handle_t handle, const char *sval) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    assert(sval != NULL);
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_string_value(&SCALAR(handle), sval);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_value(scalar_handle_t handle, const scalar_t *val) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    assert(val != NULL);
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_clone(&SCALAR(handle), val);
        scalar_unlock(handle);
    }
}

/*
=item scalar_set_scalar_reference()

=item scalar_set_array_reference()

=item scalar_set_hash_reference()

=item scalar_set_channel_reference()

=item scalar_set_function_reference()

Functions for setting up references

=cut
 */
static inline void scalar_set_scalar_reference(scalar_handle_t handle, scalar_handle_t s) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_scalar_reference(&SCALAR(handle), s);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_array_reference(scalar_handle_t handle, array_handle_t a) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_array_reference(&SCALAR(handle), a);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_hash_reference(scalar_handle_t handle, hash_handle_t h) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_hash_reference(&SCALAR(handle), h);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_channel_reference(scalar_handle_t handle, channel_handle_t c) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_channel_reference(&SCALAR(handle), c);
        scalar_unlock(handle);
    }
}

static inline void scalar_set_function_reference(scalar_handle_t handle, function_handle_t f) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_set_function_reference(&SCALAR(handle), f);
        scalar_unlock(handle);
    }
}

/*
=item scalar_is_defined()

Returns 1 if the scalar has some sort of value, or 0 if it is undefined

=cut
 */
static inline intptr_t scalar_is_defined(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    intptr_t value = 0;
    if (0 == scalar_lock(handle)) {
        value = anon_scalar_is_defined(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return value;
}

/*
=item scalar_get_bool_value()

=item scalar_get_int_value()

=item scalar_get_float_value()

=item scalar_get_string_value()

=item scalar_get_value()

Functions for getting values from a pooled scalar.

=cut
 */
static inline intptr_t scalar_get_bool_value(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    intptr_t value = 0;
    if (0 == scalar_lock(handle)) {
        value = anon_scalar_get_bool_value(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return value;
}

static inline intptr_t scalar_get_int_value(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    intptr_t value = 0;
    if (0 == scalar_lock(handle)) {
        value = anon_scalar_get_int_value(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return value;
}

static inline floatptr_t scalar_get_float_value(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    floatptr_t value = 0;
    if (0 == scalar_lock(handle)) {
        value = anon_scalar_get_float_value(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return value;
}

static inline void scalar_get_string_value(scalar_handle_t handle, char **result) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_get_string_value(&SCALAR(handle), result);
        scalar_unlock(handle);
    }
}

static inline void scalar_get_value(scalar_handle_t handle, scalar_t *result) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    assert(result != NULL);
    
    if ((result->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(result);
    
    if (0 == scalar_lock(handle)) {
        anon_scalar_clone(result, &SCALAR(handle));
        scalar_unlock(handle);
    }
}

/*
=item scalar_deref_scalar_reference()

=item scalar_deref_array_reference()

=item scalar_deref_hash_reference()

=item scalar_deref_channel_reference()

=item scalar_deref_function_reference()

Functions for dereferencing references in pooled scalars

=cut
 */
static inline scalar_handle_t scalar_deref_scalar_reference(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    scalar_handle_t ref = 0;
    if (0 == scalar_lock(handle)) {
        ref = anon_scalar_deref_scalar_reference(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return ref;
}

static inline array_handle_t scalar_deref_array_reference(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));

    array_handle_t ref = 0;
    if (0 == scalar_lock(handle)) {
        ref = anon_scalar_deref_array_reference(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return ref;
}

static inline hash_handle_t scalar_deref_hash_reference(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    hash_handle_t ref = 0;
    if (0 == scalar_lock(handle)) {
        ref = anon_scalar_deref_hash_reference(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return ref;
}

static inline channel_handle_t scalar_deref_channel_reference(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    channel_handle_t ref = 0;
    if (0 == scalar_lock(handle)) {
        ref = anon_scalar_deref_channel_reference(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return ref;
}

static inline function_handle_t scalar_deref_function_reference(scalar_handle_t handle) {
    assert(POOL_HANDLE_VALID(scalar_t, handle));
    assert(POOL_HANDLE_IN_USE(scalar_t, handle));
    
    function_handle_t ref = 0;
    if (0 == scalar_lock(handle)) {
        ref = anon_scalar_deref_function_reference(&SCALAR(handle));
        scalar_unlock(handle);
    }
    return ref;
}

#endif
