/*
 *  bits.h
 *  dang
 *
 *  Created by Ellie on 2/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef BITS_H
#define BITS_H

#include <stdint.h>

#define MAX(a,b) (((b) > (a)) ? (b) : (a))
#define MIN(a,b) (((b) < (a)) ? (b) : (a))

uintptr_t nextupow2(uintptr_t);

#endif
