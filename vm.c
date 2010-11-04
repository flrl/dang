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

#include "array.h"
#include "bytecode.h"
#include "channel.h"
#include "debug.h"
#include "hash.h"
#include "scalar.h"
#include "stack.h"
#include "vm.h"

pthread_attr_t vm_thread_attributes;

int _vm_context_destroy(vm_context_t *);

/*
=head1 NAME

vm

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=cut
*/

STACK_DEFINITIONS(scalar_t, anon_scalar_init, anon_scalar_destroy, anon_scalar_clone);
STACK_DEFINITIONS(function_handle_t, STACK_basic_init, STACK_basic_destroy, STACK_basic_copy);

/*
=head2 The Virtual Machine

=over 

=item vm_main()

Runs the virtual machine.

=cut
*/
int vm_main(const uint8_t *bytecode, size_t length, size_t start) {
    vm_context_t *context;

    pthread_attr_init(&vm_thread_attributes);
    pthread_attr_setdetachstate(&vm_thread_attributes, PTHREAD_CREATE_DETACHED);
    
    scalar_pool_init();
    array_pool_init();
    hash_pool_init();
    channel_pool_init();
    
    if (NULL != (context = calloc(1, sizeof(*context)))) {
        vm_context_init(context, bytecode, length, start);
        vm_execute(context);
    }
    
    symboltable_garbage_collect();

    channel_pool_destroy();
    hash_pool_destroy();
    array_pool_destroy();
    scalar_pool_destroy();
    
    pthread_attr_destroy(&vm_thread_attributes);

    return 0;
}


/*
=item vm_execute()

Runs a single execution context (thread) within the virtual machine. 

Its sole argument must be a pointer to a heap-allocated vm_context_t object that has been initialised with
C<vm_context_init()>, which it takes ownership of, and cleans up upon completion.  

The caller should B<not> call C<vm_context_destroy()> or otherwise clean up a context_t that has been passed
to C<vm_execute()>.

This function is designed to be callable via C<pthread_create()> with C<vm_thread_attributes> provided to the 
attr argument.

=back

=cut
*/
void *vm_execute(void *ptr) {
    vm_context_t *context = ptr;
    assert(context != NULL);
    
    // set the top of the return stack to point back to the zero'th instruction, which always contains END.
    // this allows the vm entry point to be a normal function that expects to simply return when it's done.
    vm_rs_push(context, 0);
    vm_start_scope(context);
    
    int incr;
    while (context->m_counter < context->m_bytecode_length) {
        uint8_t instruction = context->m_bytecode[context->m_counter];
        assert(instruction < i__MAX);
        switch (instruction) {
            case i_END:
                goto end;
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
    
end:
    if (context->m_symboltable)  vm_end_scope(context);
    _vm_context_destroy(context);
    free(context);
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
    
    return STACK_PUSH(function_handle_t, &context->m_return_stack, &value);
}

int vm_rs_pop(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    
    return STACK_POP(function_handle_t, &context->m_return_stack, result);
}

int vm_rs_top(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    return STACK_TOP(function_handle_t, &context->m_return_stack, result);
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

Initialise a vm_context_t object.

=cut
*/
int vm_context_init(vm_context_t *self, const uint8_t *bytecode, size_t bytecode_len, size_t start) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    
    if (0 == STACK_INIT(scalar_t, &self->m_data_stack)) {
        if (0 == STACK_INIT(function_handle_t, &self->m_return_stack)) {
            self->m_bytecode = bytecode;
            self->m_bytecode_length = bytecode_len;
            self->m_counter = start;
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

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/

/*
=item _vm_context_destroy()

Destroys a vm_context_t object.  This should B<not> be called on objects that have been passed to
C<vm_execute()>, except by C<vm_execute()> itself.

=cut
 */ 
int _vm_context_destroy(vm_context_t *self) {
    assert(self != NULL);
    
    STACK_DESTROY(scalar_t, &self->m_data_stack);
    STACK_DESTROY(function_handle_t, &self->m_return_stack);
    
    if (self->m_symboltable != NULL && 0 == symboltable_destroy(self->m_symboltable)) {
        free(self->m_symboltable);
        self->m_symboltable = NULL;
    }
    
    return 0;
}


/*
=back

=cut
 */
