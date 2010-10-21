/*
 *  reference.h
 *  dang
 *
 *  Created by Ellie on 21/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef REFERENCE_H
#define REFERENCE_H

#include "scalar.h"
#include "channel.h"
#include "array.h"

int scalar_reference_create(scalar_t *, scalar_handle_t);
int array_reference_create(scalar_t *, uintptr_t); // FIXME
int channel_reference_create(scalar_t *, channel_handle_t);



// FIXME don't return the handle directly -- breaks reference counting!
// instead have "deref read" and "deref write" functions for each type
scalar_handle_t scalar_reference_deref(const scalar_t *);
uintptr_t array_reference_deref(const scalar_t *);  // FIXME
channel_handle_t channel_reference_deref(const scalar_t *);


#endif
