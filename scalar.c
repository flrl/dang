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

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE   (1024) /* FIXME arbitrary number */
#include "pool.h"

#define SCALAR(handle)      POOL_OBJECT(pooled_scalar_t, handle)

static int _scalar_init(pooled_scalar_t *, uint32_t);
static int _scalar_destroy(pooled_scalar_t *);
static inline int _scalar_lock(pooled_scalar_t *);
static inline int _scalar_unlock(pooled_scalar_t *);
static inline void _scalar_set_undef_unlocked(pooled_scalar_t *);

void _scalar_pool_add_to_free_list(scalar_handle_t);

typedef POOL_STRUCT(pooled_scalar_t) scalar_pool_t;

POOL_DEFINITIONS(pooled_scalar_t, _scalar_init, uint32_t, _scalar_destroy, _scalar_lock, _scalar_unlock);

/*
=head1 SCALARS

=head1 INTRODUCTION

=head1 PUBLIC INTERFACE

=cut
*/

/*
=head2 Scalar Pool Functions

=over

=item scalar_pool_init()

=item scalar_pool_destroy()

Scalar pool setup and teardown functions

=cut
*/
int scalar_pool_init(void) {
    return POOL_INIT(pooled_scalar_t);
}

int scalar_pool_destroy(void) {
    return POOL_DESTROY(pooled_scalar_t);
}

/*
=item scalar_allocate()

=item scalar_release()

=item scalar_increase_refcount()

Functions for managing allocation of scalars

=cut
*/
scalar_handle_t scalar_allocate(uint32_t flags) {
    return POOL_ALLOCATE(pooled_scalar_t, flags);
}

int scalar_release(scalar_handle_t handle) {
    return POOL_RELEASE(pooled_scalar_t, handle);
}
int scalar_increase_refcount(scalar_handle_t handle) {
    return POOL_INCREASE_REFCOUNT(pooled_scalar_t, handle);
}


/*
=back

=head2 Pooled Scalar Functions

=over

=cut
*/

/*
=item scalar_set_undef()

=item scalar_set_int_value()

=item scalar_set_float_value()

=item scalar_set_string_value()

=item scalar_set_value()

Functions for setting values on a pooled scalar.  Atomic when the scalar has its SCALAR_FLAG_SHARED flag set.  Any previous
value is properly cleaned up.

=cut
*/
void scalar_set_undef(scalar_handle_t handle) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        _scalar_set_undef_unlocked(&SCALAR(handle));
        _scalar_unlock(&SCALAR(handle));
    }
}

void scalar_set_int_value(scalar_handle_t handle, intptr_t ival) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        _scalar_set_undef_unlocked(&SCALAR(handle));
        SCALAR(handle).m_flags |= SCALAR_INT;
        SCALAR(handle).m_value.as_int = ival;
        _scalar_unlock(&SCALAR(handle));
    }    
}

void scalar_set_float_value(scalar_handle_t handle, floatptr_t fval) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        _scalar_set_undef_unlocked(&SCALAR(handle));
        SCALAR(handle).m_flags |= SCALAR_INT;
        SCALAR(handle).m_value.as_float = fval;
        _scalar_unlock(&SCALAR(handle));
    }
}

void scalar_set_string_value(scalar_handle_t handle, const char *sval) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    assert(sval != NULL);
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        _scalar_set_undef_unlocked(&SCALAR(handle));
        SCALAR(handle).m_flags |= SCALAR_STRING | SCALAR_FLAG_PTR;
        SCALAR(handle).m_value.as_string = strdup(sval);
        _scalar_unlock(&SCALAR(handle));
    }
}

void scalar_set_value(scalar_handle_t handle, const anon_scalar_t *val) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    assert(val != NULL);
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        switch (val->m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                SCALAR(handle).m_value.as_string = strdup(val->m_value.as_string);
                SCALAR(handle).m_flags &= ~SCALAR_TYPE_MASK;
                SCALAR(handle).m_flags |= SCALAR_STRING | SCALAR_FLAG_PTR;
                break;
            default:
                memcpy(&SCALAR(handle).m_value, &val->m_value, sizeof(SCALAR(handle).m_value));
                SCALAR(handle).m_flags &= ~SCALAR_TYPE_MASK;
                SCALAR(handle).m_flags |= val->m_flags & SCALAR_TYPE_MASK;
                break;
        }
        _scalar_unlock(&SCALAR(handle));
    }
}

/*
=item scalar_get_int_value()

=item scalar_get_float_value()

=item scalar_get_string_value()

=item scalar_get_value()

Functions for getting values from a pooled scalar.  Atomic when SCALAR_FLAG_SHARED is set.

=cut
*/
intptr_t scalar_get_int_value(scalar_handle_t handle) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));

    intptr_t value;
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        value = anon_scalar_get_int_value((anon_scalar_t *) &SCALAR(handle));
        _scalar_unlock(&SCALAR(handle));
    }
    else {
        value = 0;
    }
    
    return value;    
}

floatptr_t scalar_get_float_value(scalar_handle_t handle) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));

    floatptr_t value;
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        value = anon_scalar_get_float_value((anon_scalar_t *) &SCALAR(handle));
        _scalar_unlock(&SCALAR(handle));
    }
    else {
        value = 0.0;
    }
    
    return value;    
}

void scalar_get_string_value(scalar_handle_t handle, char **result) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));

    if (0 == _scalar_lock(&SCALAR(handle))) {
        anon_scalar_get_string_value((anon_scalar_t *) &SCALAR(handle), result);
        _scalar_unlock(&SCALAR(handle));
    }
}

void scalar_get_value(scalar_handle_t handle, anon_scalar_t *result) {
    assert(POOL_VALID_HANDLE(pooled_scalar_t, handle));
    assert(POOL_ISINUSE(pooled_scalar_t, handle));
    assert(result != NULL);
    
    if ((result->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(result);
    
    if (0 == _scalar_lock(&SCALAR(handle))) {
        switch(SCALAR(handle).m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                result->m_value.as_string = strdup(SCALAR(handle).m_value.as_string);
                result->m_flags = SCALAR_STRING | SCALAR_FLAG_PTR;
                break;
            default:
                memcpy(&result->m_value, &SCALAR(handle).m_value, sizeof(result->m_value));
                result->m_flags = SCALAR(handle).m_flags & SCALAR_TYPE_MASK;
                break;
        }
        _scalar_unlock(&SCALAR(handle));
    }
}


/*
=back

=head2 Anonymous Scalar Functions

=over

=item anon_scalar_init()

=item anon_scalar_destroy()

Setup and teardown functions for anon_scalar_t objects

=cut
*/
int anon_scalar_init(anon_scalar_t *self) {
    assert(self != NULL);
    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
    return 0;
}

int anon_scalar_destroy(anon_scalar_t *self) {
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
    return 0;
}

/*
=item anon_scalar_clone()

Deep-copy clone of an anon_scalar_t object.  The resulting clone needs to be destroyed independently of the original.

=cut
*/
int anon_scalar_clone(anon_scalar_t * restrict self, const anon_scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return 0;
    
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
    return 0;
}

/*
=item anon_scalar_assign()

Shallow-copy of an anon_scalar_t object.  Only one of dest and original should be destroyed.

=cut
*/
int anon_scalar_assign(anon_scalar_t * restrict self, const anon_scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return 0;
    
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    memcpy(self, other, sizeof(anon_scalar_t));
    return 0;
}

/*
=item anon_scalar_set_int_value()

=item anon_scalar_set_float_value()

=item anon_scalar_set_string_value()

Functions for setting the value of anon_scalar_t objects.  Any previous value is properly cleaned up.

=cut
*/
void anon_scalar_set_int_value(anon_scalar_t *self, intptr_t ival) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_INT;
    self->m_value.as_int = ival;
}

void anon_scalar_set_float_value(anon_scalar_t *self, floatptr_t fval) {
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

/*
=item anon_scalar_get_int_value()

=item anon_scalar_get_float_value()

=item anon_scalar_get_string_value()

Functions for getting values from anon_scalar_t objects.

=cut
*/
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

floatptr_t anon_scalar_get_float_value(const anon_scalar_t *self) {
    assert(self != NULL);
    floatptr_t value;
    
    switch(self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_INT:
            value = (floatptr_t) self->m_value.as_int;
            break;
        case SCALAR_FLOAT:
            value = self->m_value.as_float;
            break;
        case SCALAR_STRING:
            value = self->m_value.as_string != NULL ? strtof(self->m_value.as_string, NULL) : 0.0;
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

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/

/*
=item _scalar_init()

=item _scalar_destroy()

Setup and teardown functions for pooled scalars

=cut
*/
static int _scalar_init(pooled_scalar_t *self, uint32_t flags) {
    assert(self != NULL);
    
    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
    
    if ((flags & SCALAR_FLAG_SHARED)) {
        self->m_mutex = calloc(1, sizeof(pthread_mutex_t));
        assert(self->m_mutex != NULL);
        pthread_mutex_init(self->m_mutex, NULL);
        self->m_flags |= SCALAR_FLAG_SHARED;
    }

    return 0;
}

static int _scalar_destroy(pooled_scalar_t *self) {
    assert(self != NULL);

    if (0 == _scalar_lock(self)) {
        _scalar_set_undef_unlocked(self);
        
        if (self->m_flags & SCALAR_FLAG_SHARED) {
            pthread_mutex_destroy(self->m_mutex);
            free(self->m_mutex);
        }

        // n.b. mutex has been destroyed, so don't try to unlock it
        return 0;        
    }
    else {
        return -1;
    }
}

/*
=item _scalar_lock()

Locks a pooled scalar if it has the SCALAR_FLAG_SHARED flag set, or does nothing otherwise.

Returns 0 on success, or a pthread_mutex_lock error value on failure.

=cut
*/
static inline int _scalar_lock(pooled_scalar_t *self) {
    assert(self != NULL);
    
    if (self->m_flags & SCALAR_FLAG_SHARED) {
        return pthread_mutex_lock(self->m_mutex);
    }
    else {
        return 0;
    }
}

/*
=item _scalar_unlock()

Unlocks a pooled scalar if it has the SCALAR_FLAG_SHARED flag set, or does nothing otherwise.

Returns 0 on success, or a pthread_mutex_unlock error value on failure.

=cut
*/
static inline int _scalar_unlock(pooled_scalar_t *self) {
    assert(self != NULL);
    
    if (self->m_flags & SCALAR_FLAG_SHARED) {
        return pthread_mutex_unlock(self->m_mutex);
    }
    else {
        return 0;
    }
}

/*
=item _scalar_set_undef_unlocked()

Sets a pooled scalar's value and related flags back to the "undefined" state, without consideration
for the SCALAR_FLAG_SHARED flag and without a lock/unlock cycle.

Use this when you already have the scalar locked and need to reset its value to undefined without losing atomicity.

=cut
*/
static inline void _scalar_set_undef_unlocked(pooled_scalar_t *self) {
    assert(self != NULL);
    
    // clean up allocated memory, if anything
    if (self->m_flags & SCALAR_FLAG_PTR) {
        switch(self->m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_STRING:
                free(self->m_value.as_string);
                self->m_flags &= ~SCALAR_FLAG_PTR;
                break;
        }
    }
    
    // FIXME if it was a reference type, decrease ref counts on referenced object here
    
    // set up default values
    self->m_flags &= ~SCALAR_TYPE_MASK;
    self->m_flags |= SCALAR_UNDEF;
    self->m_value.as_int = 0;    
}


/*
 =back
 
 =cut
 */
