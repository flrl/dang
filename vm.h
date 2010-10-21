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

#include "scalar.h"
#include "stack.h"

typedef uintptr_t identifier_t;

typedef STACK_STRUCT(scalar_t) data_stack_t;
typedef STACK_STRUCT(size_t) return_stack_t;

typedef struct vm_symbol_t {
    struct vm_symbol_t *m_parent;
    struct vm_symbol_t *m_left_child;
    struct vm_symbol_t *m_right_child;
    identifier_t m_identifier;
    uint32_t m_flags;
    union {
        scalar_handle_t as_scalar;
    } m_referent;
} vm_symbol_t;

typedef struct vm_symboltable_t {
    struct vm_symboltable_t *m_parent;
    vm_symbol_t *m_symbols;
    size_t m_references;
} vm_symboltable_t;

typedef struct vm_context_t {
    uint8_t *m_bytecode;
    size_t  m_bytecode_length;
    size_t  m_counter;
    data_stack_t m_data_stack;
    return_stack_t m_return_stack;
    vm_symboltable_t *m_symboltable;
} vm_context_t;

void *vm_execute(void *);

int vm_ds_push(vm_context_t *, const scalar_t *);
int vm_ds_pop(vm_context_t *, scalar_t *);
int vm_ds_top(vm_context_t *, scalar_t *);

int vm_rs_push(vm_context_t *, size_t);
int vm_rs_pop(vm_context_t *, size_t *);
int vm_rs_top(vm_context_t *, size_t *);

int vm_start_scope(vm_context_t *);
int vm_end_scope(vm_context_t *);

int vm_context_init(vm_context_t *);
int vm_context_destroy(vm_context_t *);

int vm_symboltable_init(vm_symboltable_t *restrict, vm_symboltable_t *restrict);
int vm_symboltable_destroy(vm_symboltable_t *);
int vm_symboltable_garbage_collect(void);

int vm_symbol_init(vm_symbol_t *);
int vm_symbol_destroy(vm_symbol_t *);
int vm_symbol_reap(vm_symbol_t *);

int vm_symbol_define(vm_context_t *, identifier_t, uint32_t);
const vm_symbol_t *vm_symbol_lookup(vm_context_t *, identifier_t);
int vm_symbol_undefine(vm_context_t *, identifier_t);


#endif
