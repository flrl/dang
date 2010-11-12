/*
 *  vmtypes.h
 *  dang
 *
 *  Created by Ellie on 30/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef VMTYPES_H
#define VMTYPES_H

#include <stdint.h>

#include "floatptr_t.h"

typedef uintptr_t handle_t;
typedef handle_t scalar_handle_t;
typedef handle_t array_handle_t;
typedef handle_t hash_handle_t;
typedef handle_t channel_handle_t;
typedef handle_t function_handle_t;
typedef handle_t stream_handle_t;

typedef uint32_t flags_t;
typedef uintptr_t identifier_t;

#endif
