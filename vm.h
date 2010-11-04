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

#include <pthread.h>

#include "scalar.h"
#include "stack.h"
#include "symboltable.h"

typedef STACK_STRUCT(scalar_t) data_stack_t;
typedef STACK_STRUCT(function_handle_t) return_stack_t;

typedef struct vm_context_t {
    const uint8_t *m_bytecode;
    size_t  m_bytecode_length;
    size_t  m_counter;
    data_stack_t m_data_stack;
    return_stack_t m_return_stack;
    symboltable_t *m_symboltable;
} vm_context_t;

extern pthread_attr_t vm_thread_attributes;

int vm_main(const uint8_t *, size_t, size_t);
void *vm_execute(void *);  // n.b. actually takes and returns a vm_context_t*

int vm_context_init(vm_context_t *, const uint8_t *, size_t, size_t);
/* n.b. _vm_context_destroy() is private */


int vm_ds_push(vm_context_t *, const scalar_t *);
int vm_ds_pop(vm_context_t *, scalar_t *);
int vm_ds_top(vm_context_t *, scalar_t *);
int vm_ds_npush(vm_context_t *, size_t, const scalar_t *);
int vm_ds_npop(vm_context_t *, size_t, scalar_t *);
int vm_ds_swap(vm_context_t *);
int vm_ds_dup(vm_context_t *);
int vm_ds_over(vm_context_t *);

int vm_rs_push(vm_context_t *, size_t);
int vm_rs_pop(vm_context_t *, size_t *);
int vm_rs_top(vm_context_t *, size_t *);

int vm_start_scope(vm_context_t *);
int vm_end_scope(vm_context_t *);

#endif
