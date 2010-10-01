/*
 *  scalar.c
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scalar.h"

void scalar_init(scalar_t *self) {
    assert(self != NULL);
    self->m_type = ScUNDEFINED;
    self->m_value.int_value = 0;
}

void scalar_destroy(scalar_t *self) {
    assert(self != NULL);
    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
    self->m_type = ScUNDEFINED;
    self->m_value.int_value = 0;
}

void scalar_clone(scalar_t *self, const scalar_t *other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
    
    switch(other->m_type) {
        case ScSTRING:
            self->m_type = ScSTRING;
            self->m_value.string_value = strdup(other->m_value.string_value);
            break;
        default:
            memcpy(self, other, sizeof(*self));
    }
}

void scalar_set_int_value(scalar_t *self, intptr_t ival) {
    assert(self != NULL);
    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
    self->m_type = ScINT;
    self->m_value.int_value = ival;
}

void scalar_set_double_value(scalar_t *self, double dval) {
    assert(self != NULL);
    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
    self->m_type = ScDOUBLE;
    self->m_value.double_value = dval;
}

void scalar_set_string_value(scalar_t *self, const char *sval) {
    assert(self != NULL);
    assert(sval != NULL);
    if (self->m_type == ScSTRING && self->m_value.string_value != NULL)  free(self->m_value.string_value);
    self->m_type = ScSTRING;
    self->m_value.string_value = strdup(sval);
}

intptr_t scalar_get_int_value(const scalar_t *self) {
    assert(self != NULL);
    switch(self->m_type) {
        case ScINT:
            return self->m_value.int_value;
        case ScDOUBLE:
            return (intptr_t) self->m_value.double_value;
        case ScSTRING:
            return self->m_value.string_value != NULL ? strtol(self->m_value.string_value, NULL, 0) : 0;
        default:
            return 0;
    }
}

double scalar_get_double_value(const scalar_t *self) {
    assert(self != NULL);
    switch(self->m_type) {
        case ScINT:
            return (double) self->m_value.int_value;
        case ScDOUBLE:
            return self->m_value.double_value;
        case ScSTRING:
            return self->m_value.string_value != NULL ? strtod(self->m_value.string_value, NULL) : 0.0;
        default:
            return 0;
    }    
}

const char *scalar_get_string_value(const scalar_t *self) {
    assert(self != NULL);
    // FIXME write this
    return NULL;
}
