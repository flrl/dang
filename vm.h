/*
 *  vm.h
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef VM_H
#define VM_H

#include "data_stack.h"

int vm_execute(const uint8_t *, size_t, size_t, data_stack_t *);

#endif
