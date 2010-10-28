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

#include "array.h"
#include "channel.h"
#include "hash.h"
#include "debug.h"

#include "scalar.h"

POOL_SOURCE_CONTENTS(scalar_t)

/*
=head1 NAME

scalar

=head1 INTRODUCTION

...

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
    return POOL_INIT(scalar_t);
}

int scalar_pool_destroy(void) {
    return POOL_DESTROY(scalar_t);
}

/*
=item scalar_allocate()

=item scalar_allocate_many()

=item scalar_reference()
 
=item scalar_release()

Functions for managing allocation of scalars

=cut
*/
scalar_handle_t scalar_allocate(uint32_t flags) {
    return POOL_ALLOCATE(scalar_t, flags);
}

scalar_handle_t scalar_allocate_many(size_t count, uint32_t flags) {
    return POOL_ALLOCATE_MANY(scalar_t, count, flags);
}

scalar_handle_t scalar_reference(scalar_handle_t handle) {
    return POOL_REFERENCE(scalar_t, handle);
}

int scalar_release(scalar_handle_t handle) {
    return POOL_RELEASE(scalar_t, handle);
}

/*
=back

=cut
*/


/*
=back

=head2 Anonymous Scalar Functions

With the exception of C<anon_scalar_init()>, these functions will all correctly clean up any previous value the object may
have held before setting a new one.

=over

=cut
 */

/*
=item anon_scalar_init()

Initialises a scalar_t object.  

This function is functionally equivalent to setting all its fields to zero, so if the object was allocated with C<calloc()>,
or initialised with {0}, then you do not need to also call C<anon_scalar_init()>

=cut
 */
int anon_scalar_init(scalar_t *self) {
    assert(self != NULL);
    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
    return 0;
}

/*
=item anon_scalar_destroy()

Cleans up a scalar_t object.

This properly releases any references the object may hold.

=cut
 */
int anon_scalar_destroy(scalar_t *self) {
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

    if (self->m_flags & SCALAR_FLAG_REF) {
        switch (self->m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_SCAREF:
                scalar_release(self->m_value.as_scalar_handle);
                break;
            case SCALAR_ARRREF:
                array_release(self->m_value.as_array_handle);
                break;
            //...
            case SCALAR_CHANREF:
                channel_release(self->m_value.as_channel_handle);
                break;
            case SCALAR_FUNCREF:
                /* function references aren't allocated anywhere, and don't need to be released */
                break;
            default:
                debug("unexpected anon scalar type: %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
                break;
        }
    }

    self->m_flags = SCALAR_UNDEF;
    self->m_value.as_int = 0;
    return 0;
}

/*
=item anon_scalar_clone()

Deep-copy clone of an anonymous scalar_t object.  The resulting clone needs to be destroyed independently of the original.

=cut
*/
int anon_scalar_clone(scalar_t * restrict self, const scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return 0;
    
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    switch(other->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_STRING:
            self->m_flags = SCALAR_FLAG_PTR | SCALAR_STRING;
            self->m_value.as_string = strdup(other->m_value.as_string);
            break;
        case SCALAR_SCAREF:
            self->m_flags = SCALAR_SCAREF;
            self->m_value.as_scalar_handle = scalar_reference(other->m_value.as_scalar_handle);
            break;
        case SCALAR_ARRREF:
            self->m_flags = SCALAR_ARRREF;
            self->m_value.as_array_handle = array_reference(other->m_value.as_array_handle);
            break;
        //...
        case SCALAR_CHANREF:
            self->m_flags = SCALAR_CHANREF;
            self->m_value.as_channel_handle = channel_reference(other->m_value.as_channel_handle);
            break;
        case SCALAR_FUNCREF:
            self->m_flags = SCALAR_FUNCREF;
            self->m_value.as_function_handle = other->m_value.as_function_handle;
            break;
        default:
            memcpy(self, other, sizeof(*self));
    }
    return 0;
}

/*
=item anon_scalar_assign()

Shallow-copy of an anonymous scalar_t object.  Only one of dest and original should be destroyed.

=cut
*/
int anon_scalar_assign(scalar_t * restrict self, const scalar_t * restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self == other)  return 0;
    
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    memcpy(self, other, sizeof(scalar_t));
    return 0;
}

/*
=item anon_scalar_set_int_value()

=item anon_scalar_set_float_value()

=item anon_scalar_set_string_value()

Functions for setting the value of anonymous scalar_t objects.  Any previous value is properly cleaned up.

=cut
*/
void anon_scalar_set_int_value(scalar_t *self, intptr_t ival) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_INT;
    self->m_value.as_int = ival;
}

void anon_scalar_set_float_value(scalar_t *self, floatptr_t fval) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_FLOAT;
    self->m_value.as_float = fval;
}

void anon_scalar_set_string_value(scalar_t *self, const char *sval) {
    assert(self != NULL);
    assert(sval != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);

    self->m_flags = SCALAR_FLAG_PTR | SCALAR_STRING;
    self->m_value.as_string = strdup(sval);
}


/*
=item anon_scalar_set_scalar_reference()

=item anon_scalar_set_array_reference()

=item anon_scalar_set_hash_reference()

=item anon_scalar_set_channel_reference()

=item anon_scalar_set_function_reference()

Functions for setting up anonymous scalar_t objects to reference other objects.  Any previous value is properly
cleaned up.

=cut
*/

void anon_scalar_set_scalar_reference(scalar_t *self, scalar_handle_t handle) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    self->m_flags = SCALAR_SCAREF;
    self->m_value.as_scalar_handle = scalar_reference(handle);
}

void anon_scalar_set_array_reference(scalar_t *self, array_handle_t handle) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    self->m_flags = SCALAR_ARRREF;
    self->m_value.as_array_handle = array_reference(handle);
}

void anon_scalar_set_hash_reference(scalar_t *self, hash_handle_t handle) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    self->m_flags = SCALAR_HASHREF;
    self->m_value.as_hash_handle = hash_reference(handle);
}

void anon_scalar_set_channel_reference(scalar_t *self, channel_handle_t handle) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    self->m_flags = SCALAR_CHANREF;
    self->m_value.as_channel_handle = channel_reference(handle);
}

void anon_scalar_set_function_reference(scalar_t *self, function_handle_t handle) {
    assert(self != NULL);
    if ((self->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(self);
    
    self->m_flags = SCALAR_FUNCREF;
    self->m_value.as_function_handle = handle;
}

/*

=item anon_scalar_get_bool_value()

=item anon_scalar_get_int_value()

=item anon_scalar_get_float_value()

=item anon_scalar_get_string_value()

Functions for getting values from anonymous scalar_t objects.

=cut
*/
intptr_t anon_scalar_get_bool_value(const scalar_t *self) {
    assert(self != NULL);
    switch (self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_UNDEF:
            return 0;
        case SCALAR_INT:
        case SCALAR_SCAREF:
        case SCALAR_ARRREF:
        case SCALAR_HASHREF:
        //...
        case SCALAR_CHANREF:
        case SCALAR_FUNCREF:
            return (self->m_value.as_int != 0);
        case SCALAR_FLOAT:
            return (self->m_value.as_float != 0);
            break;
        case SCALAR_STRING:
            if (self->m_value.as_string != NULL && self->m_value.as_string[0] != '\0') {
                if (self->m_value.as_string[0] == 0 && self->m_value.as_string[1] == '\0') {
                    return 0;   
                }
                return 1;
            }
            return 0;
        default:
            debug("unhandled scalar type: %"PRIu32"\n", self->m_flags);
            return 0;
    }
}

intptr_t anon_scalar_get_int_value(const scalar_t *self) {
    assert(self != NULL);
    intptr_t value;
    switch (self->m_flags & SCALAR_TYPE_MASK) {
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

floatptr_t anon_scalar_get_float_value(const scalar_t *self) {
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

void anon_scalar_get_string_value(const scalar_t *self, char **result) {
    assert(self != NULL);
    
    char buffer[100];
    
    switch(self->m_flags & SCALAR_TYPE_MASK) {
        case SCALAR_UNDEF:
            *result = strdup("");
            break;
        case SCALAR_INT:
            snprintf(buffer, sizeof(buffer), "%"PRIiPTR"", self->m_value.as_int);
            *result = strdup(buffer);
            break;
        case SCALAR_FLOAT:
            snprintf(buffer, sizeof(buffer), "%f", self->m_value.as_float);
            *result = strdup(buffer);
            break;
        case SCALAR_STRING:
            *result = strdup(self->m_value.as_string);
            break;

        case SCALAR_SCAREF:
            snprintf(buffer, sizeof(buffer), "SCALAR(%"PRIuPTR"", self->m_value.as_scalar_handle);
            *result = strdup(buffer);
            break;
        case SCALAR_ARRREF:
            snprintf(buffer, sizeof(buffer), "ARRAY(%"PRIuPTR"", self->m_value.as_array_handle);
            *result = strdup(buffer);
            break;
        case SCALAR_HASHREF:
            snprintf(buffer, sizeof(buffer), "HASH(%"PRIuPTR"", self->m_value.as_hash_handle);
            *result = strdup(buffer);
            break;
        //...
        case SCALAR_CHANREF:
            snprintf(buffer, sizeof(buffer), "CHANNEL(%"PRIuPTR"", self->m_value.as_channel_handle);
            *result = strdup(buffer);
            break;
        case SCALAR_FUNCREF:
            snprintf(buffer, sizeof(buffer), "FUNCTION(%"PRIuPTR"", self->m_value.as_function_handle);
            *result = strdup(buffer);
            break;
        
        default:
            debug("unexpected type value %"PRIu32"\n", self->m_flags & SCALAR_TYPE_MASK);
            *result = strdup("");
            break;
    }

    return;
}

/*
=item anon_scalar_deref_scalar_reference()

=item anon_scalar_deref_array_reference()

=item anon_scalar_deref_hash_reference()

=item anon_scalar_deref_channel_reference()

=item anon_scalar_deref_function_reference()

Dereference reference type anonymous scalars

=cut
*/
scalar_handle_t anon_scalar_deref_scalar_reference(const scalar_t *self) {
    assert(self != NULL);
    assert((self->m_flags & SCALAR_TYPE_MASK) == SCALAR_SCAREF);
    
    return self->m_value.as_scalar_handle;
}

array_handle_t anon_scalar_deref_array_reference(const scalar_t *self) {
    assert(self != NULL);
    assert((self->m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    
    return self->m_value.as_array_handle;
}

hash_handle_t anon_scalar_deref_hash_reference(const scalar_t *self) {
    assert(self != NULL);
    assert((self->m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);
    
    return self->m_value.as_hash_handle;
}

channel_handle_t anon_scalar_deref_channel_reference(const scalar_t *self) {
    assert(self != NULL);
    assert((self->m_flags & SCALAR_TYPE_MASK) == SCALAR_CHANREF);
    
    return self->m_value.as_channel_handle;    
}

function_handle_t anon_scalar_deref_function_reference(const scalar_t *self) {
    assert(self != NULL);
    assert((self->m_flags & SCALAR_TYPE_MASK) == SCALAR_FUNCREF);
    
    return self->m_value.as_function_handle;
}

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/


/*
=back

=cut
 */
