/*
 *  reference.c
 *  dang
 *
 *  Created by Ellie on 21/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>

#include "reference.h"

int scalar_reference_create(scalar_t *ref, scalar_handle_t handle) {
    assert(ref != NULL);
    assert(POOL_VALID_HANDLE(scalar_t, handle));
    
    if ((ref->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(ref);
    
    ref->m_flags = SCALAR_SCAREF;
    ref->m_value.as_scalar_handle = handle;
    
    return 0;
}

int array_reference_create(scalar_t *ref, uintptr_t handle); // FIXME

int channel_reference_create(scalar_t *ref, channel_handle_t handle) {
    assert(ref != NULL);
    assert(POOL_VALID_HANDLE(channel_t, handle));
    
    if ((ref->m_flags & SCALAR_TYPE_MASK) != SCALAR_UNDEF)  anon_scalar_destroy(ref);
    
    ref->m_flags = SCALAR_CHANREF;
    ref->m_value.as_channel_handle = handle;
    
    return 0;    
}

scalar_handle_t scalar_reference_deref(const scalar_t *ref) {
    assert(ref != NULL);
    assert((ref->m_flags & SCALAR_TYPE_MASK) == SCALAR_SCAREF);
    return ref->m_value.as_scalar_handle;
}

uintptr_t array_reference_deref(const scalar_t *ref);  // FIXME

channel_handle_t channel_reference_deref(const scalar_t *ref) {
    assert(ref != NULL);
    assert((ref->m_flags & SCALAR_TYPE_MASK) == SCALAR_CHANREF);
    return ref->m_value.as_channel_handle;
}

