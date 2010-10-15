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

//typedef struct data_stack_t {
//    struct data_stack_t *m_parent;
//    size_t m_allocated_count;
//    size_t m_count;
//    anon_scalar_t *m_items;
//    size_t m_subscope_count;
//} data_stack_t;
//
//typedef struct symbol_table_node_t {
//    struct symbol_table_node_t *m_parent;
//    struct symbol_table_node_t *m_left_child;
//    struct symbol_table_node_t *m_right_child;
//    uint32_t m_flags;
//    identifier_t m_identifier;
//    union {
//        scalar_handle_t as_scalar;
//    } m_referent;
//} symbol_table_node_t;
//
//typedef struct return_stack_t {
//    size_t m_allocated_count;
//    size_t m_count;
//    size_t *m_items;
//} return_stack_t;

typedef uintptr_t identifier_t;

typedef struct vm_symbol_t {
    struct vm_symbol_t *m_parent;
    struct vm_symbol_t *m_left_child;
    struct vm_symbol_t *m_right_child;
    uint32_t m_flags;
    identifier_t m_identifier;
    union {
        scalar_handle_t as_scalar;
    } m_referent;
} vm_symbol_t;

typedef struct vm_symboltable_t {
    struct vm_symboltable_t *m_parent;
    vm_symbol_t *m_symbols;
    size_t m_subscope_count;
} vm_symboltable_t;

typedef struct vm_context_t {
    uint8_t *m_bytecode;
    size_t  m_bytecode_length;
    size_t  m_counter;
    size_t  m_data_stack_alloc_count;
    size_t  m_data_stack_count;
    anon_scalar_t *m_data_stack;
    size_t  m_return_stack_alloc_count;
    size_t  m_return_stack_count;
    size_t  *m_return_stack;
    vm_symboltable_t *m_symboltable;
} vm_context_t;


/*
=head1 vm.h

=over 
 
=item vm_execute(context)
 
Executes the given context
 
=cut
 */
void *vm_execute(void *);
//int vm_execute(const uint8_t *, size_t, size_t, data_stack_t *);


/* 
=item vm_context_init(context)

=item vm_context_destroy(context)

Setup and teardown functions for vm_context_t objects
 
=cut
 */
int vm_context_init(vm_context_t *);
int vm_context_destroy(vm_context_t *);

/*
=item vm_ds_push

=item vm_ds_pop

=item vm_ds_top

Data stack management functions

=cut
 */
int vm_ds_push(vm_context_t *, const anon_scalar_t *);
int vm_ds_pop(vm_context_t *, anon_scalar_t *);
int vm_ds_top(vm_context_t *, anon_scalar_t *);

/*
=item vm_rs_push

=item vm_rs_pop

=item vm_rs_top

Return stack management functions

=cut
 */
int vm_rs_push(vm_context_t *, size_t);
int vm_rs_pop(vm_context_t *, size_t *);
int vm_rs_top(vm_context_t *, size_t *);

/*
=item vm_start_scope(context)

=item vm_end_scope(context)

Scope management functions

=cut
 */
int vm_start_scope(vm_context_t *);
int vm_end_scope(vm_context_t *);

/*
=item vm_symboltable_init(symboltable) 

=item vm_symboltable_destroy(symboltable)

Setup and teardown functions for vm_symboltable_t objects

=cut
 */
int vm_symboltable_init(vm_symboltable_t *);
int vm_symboltable_destroy(vm_symboltable_t *);

/*
=item vm_symbol_init(symbol)

=item vm_symbol_destroy(symbol)

Setup and teardown functions for vm_symbol_t objects

=cut
 */
int vm_symbol_init(vm_symbol_t *);
int vm_symbol_destroy(vm_symbol_t *);

/*
=item vm_symboltable_registry_reap()

Garbage collects persistent symbol tables.  Intended for use immediately before exit.
=cut
 */
int vm_symboltable_registry_reap(void);


#endif
/*
 =back
 
 =cut
 */
