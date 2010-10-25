/*
 *  vm.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "bytecode.h"
#include "debug.h"
#include "stack.h"
#include "vm.h"

/*
=head1 NAME

vm

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=cut
*/

STACK_DEFINITIONS(scalar_t, anon_scalar_init, anon_scalar_destroy, anon_scalar_clone, anon_scalar_assign);
STACK_DEFINITIONS(size_t, STACK_basic_init, STACK_basic_destroy, STACK_basic_clone, STACK_basic_assign);

/*
=head2 The Virtual Machine

=over 

=item vm_execute()

This function is the core of the virtual machine.

=back

=cut
*/
void *vm_execute(void *ptr) {
    vm_context_t *context = ptr;
    assert(context != NULL);
    
    int incr;
    while (context->m_counter < context->m_bytecode_length) {
        uint8_t instruction = context->m_bytecode[context->m_counter];
        assert(instruction < i__MAX);
        switch (instruction) {
            case i_END:
                return NULL;
            case i_NOOP:
                context->m_counter++;
                break;
            default:
                incr = instruction_table[instruction](context);
                assert(incr != 0);
                context->m_counter += incr;
                break;
        }
    }
    
    return NULL;
}

/*
=head2 Virtual Machine Functions

=over

=item vm_ds_push()

=item vm_ds_pop()

=item vm_ds_top()

=item vm_ds_npush()

=item vm_ds_npop()

=item vm_ds_swap()

=item vm_ds_dup()

=item vm_ds_over()

Data stack management functions

=cut
 */
int vm_ds_push(vm_context_t *context, const scalar_t *value) {
    assert(context != NULL);
    assert(value != NULL);
    
    return STACK_PUSH(scalar_t, &context->m_data_stack, value);
}

int vm_ds_pop(vm_context_t *context, scalar_t *result) {
    assert(context != NULL);
    
    return STACK_POP(scalar_t, &context->m_data_stack, result);
}

int vm_ds_top(vm_context_t *context, scalar_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    return STACK_TOP(scalar_t, &context->m_data_stack, result);
}

int vm_ds_npush(vm_context_t *context, size_t count, const scalar_t *value) {
    assert(context != NULL);
    assert(value != NULL);
    
    return STACK_NPUSH(scalar_t, &context->m_data_stack, count, value);
}

int vm_ds_npop(vm_context_t *context, size_t count, scalar_t *result) {
    assert(context != NULL);
    
    return STACK_NPOP(scalar_t, &context->m_data_stack, count, result);
}

int vm_ds_swap(vm_context_t *context) {
    assert(context != NULL);
    
    return STACK_SWAP(scalar_t, &context->m_data_stack);
}

int vm_ds_dup(vm_context_t *context) {
    assert(context != NULL);
    
    return STACK_DUP(scalar_t, &context->m_data_stack);
}

int vm_ds_over(vm_context_t *context) {
    assert(context != NULL);
    
    return STACK_OVER(scalar_t, &context->m_data_stack);
}


/*
=item vm_rs_push()

=item vm_rs_pop()

=item vm_rs_top()

Return stack management functions

=cut
 */
int vm_rs_push(vm_context_t *context, size_t value) {
    assert(context != NULL);
    
    return STACK_PUSH(size_t, &context->m_return_stack, &value);
}

int vm_rs_pop(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    
    return STACK_POP(size_t, &context->m_return_stack, result);
}

int vm_rs_top(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    return STACK_TOP(size_t, &context->m_return_stack, result);
}

/*
=item vm_start_scope()

Starts a new scope within the current context.  Scope affects availability of symbols: symbols defined
in a scope are only available to the scope itself and any descendant scopes, and cease to exist when the 
scope and all its descendants have ended.

=cut
 */
int vm_start_scope(vm_context_t *context) {
    assert(context != NULL);
    
    symboltable_t *new_table = calloc(1, sizeof(*new_table));
    if (new_table == NULL)  return -1;
    
    symboltable_init(new_table, context->m_symboltable);
    context->m_symboltable = new_table;
    
    return 0;
}

/*
=item vm_end_scope()

Ends a scope within the current context, causing the previous scope to become active again.

=cut
 */
int vm_end_scope(vm_context_t *context) {
    assert(context != NULL);
    assert(context->m_symboltable != NULL);
    
    symboltable_t *old_table = context->m_symboltable;
    context->m_symboltable = context->m_symboltable->m_parent;
    
    if (0 == symboltable_destroy(old_table)) {
        free(old_table);
    }
    
    return 0;
}

/* 
=back

=head2 Virtual Machine Object Management Functions

=over

=item vm_context_init()

=item vm_context_destroy()

Setup and teardown functions for vm_context_t objects

=cut
 */
int vm_context_init(vm_context_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    
    if (0 == STACK_INIT(scalar_t, &self->m_data_stack)) {
        if (0 == STACK_INIT(size_t, &self->m_return_stack)) {
            return 0;
        }
        else {
            STACK_DESTROY(scalar_t, &self->m_data_stack);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int vm_context_destroy(vm_context_t *self) {
    assert(self != NULL);
    
    STACK_DESTROY(scalar_t, &self->m_data_stack);
    STACK_DESTROY(size_t, &self->m_return_stack);
    
    if (0 == symboltable_destroy(self->m_symboltable)) {
        free(self->m_symboltable);
    }
    
    return 0;
}


/*
=back

=cut
 */
