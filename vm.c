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
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "array.h"
#include "bytecode.h"
#include "channel.h"
#include "debug.h"
#include "hash.h"
#include "scalar.h"
#include "stack.h"
#include "stream.h"
#include "vm.h"

typedef struct vm_context_registry_node_t {
    struct vm_context_registry_node_t *m_next;
    vm_context_t *m_context;
} vm_context_registry_node_t;

static struct {
    vm_context_registry_node_t *m_nodes;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_changed;
} _vm_context_registry = { NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

static struct {
    const size_t m_count;
    pthread_mutex_t m_mutex;
    function_handle_t m_handlers[NSIG];
    sigset_t m_sigset;
} _vm_signal_registry = {NSIG, PTHREAD_MUTEX_INITIALIZER, {0}};
/*
=head1 NAME

vm

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=cut
*/

STACK_DEFINITIONS(scalar_t, anon_scalar_init, anon_scalar_destroy, anon_scalar_clone);
STACK_DEFINITIONS(vm_state_t, vm_state_init, vm_state_destroy, vm_state_clone);

/*
=head2 The Virtual Machine

=over 

=item vm_main()

Runs the virtual machine.

=cut
*/
int vm_main(const uint8_t *bytecode, size_t length, size_t start) {
    vm_context_t *context;

    if (0 == pthread_mutex_lock(&_vm_signal_registry.m_mutex)) {
        sigemptyset(&_vm_signal_registry.m_sigset);
        for (size_t i = 0; i < _vm_signal_registry.m_count; i++) {
            _vm_signal_registry.m_handlers[i] = VM_SIGNAL_DEFAULT;
        }
        pthread_sigmask(SIG_SETMASK, &_vm_signal_registry.m_sigset, NULL);
        pthread_mutex_unlock(&_vm_signal_registry.m_mutex);
    }
    else {
        debug("failed to initialise signal handlers\n");
        return -1;
    }

    scalar_pool_init();
    array_pool_init();
    hash_pool_init();
    channel_pool_init();
    stream_pool_init();
    
    if (NULL != (context = calloc(1, sizeof(*context)))) {
        vm_context_init(context, bytecode, length, start);
        vm_execute(context);
    }
    
    debug("waiting for other threads to end...\n");
    if (0 == pthread_mutex_lock(&_vm_context_registry.m_mutex)) {
        while (_vm_context_registry.m_nodes != NULL) {
            pthread_cond_wait(&_vm_context_registry.m_changed, &_vm_context_registry.m_mutex);
        }
        pthread_mutex_unlock(&_vm_context_registry.m_mutex);
    }
    
    if (0 == pthread_mutex_lock(&_vm_signal_registry.m_mutex)) {
        sigemptyset(&_vm_signal_registry.m_sigset);
        for (size_t i = 0; i < _vm_signal_registry.m_count; i++) {
            _vm_signal_registry.m_handlers[i] = VM_SIGNAL_DEFAULT;
        }
        pthread_sigmask(SIG_SETMASK, &_vm_signal_registry.m_sigset, NULL);
        pthread_mutex_unlock(&_vm_signal_registry.m_mutex);
        pthread_mutex_destroy(&_vm_signal_registry.m_mutex);
    }

    debug("about to start cleaning up\n");
    while (symboltable_garbage_collect() > 0) {
        debug("symbol tables still remaining...\n");
        sleep(1);
    }

    stream_pool_destroy();
    channel_pool_destroy();
    hash_pool_destroy();
    array_pool_destroy();
    scalar_pool_destroy();
    
    return 0;
}


/*
=item vm_execute()

Runs a single execution context (thread) within the virtual machine. 

Its sole argument must be a pointer to a heap-allocated vm_context_t object that has been initialised with
C<vm_context_init()>, which it takes ownership of, and cleans up upon completion.  

The caller should B<not> call C<vm_context_destroy()> or otherwise clean up a context_t that has been passed
to C<vm_execute()>.

This function is designed to be callable via C<pthread_create()>.  When doing so, the caller must call
either C<pthread_join()> or C<pthread_detach()> to ensure the thread resources are released upon completion.

=back

=cut
*/
void *vm_execute(void *ptr) {
    vm_context_t *context = ptr;
    assert(context != NULL);
    debug("vm_execute of context %p starting up\n", context);
    
    // set the top of the return stack to point back to the zero'th instruction, which always contains END.
    // this allows the vm entry point to be a normal function that expects to simply return when it's done.
    vm_state_t initial_state = {0};
    vm_state_init(&initial_state, 0, context->m_symboltable);
    vm_rs_push(context, &initial_state);
    vm_state_destroy(&initial_state);
    
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
    debug("vm_execute of context %p about to finish\n", context);
    if (context->m_symboltable)  vm_end_scope(context);
    vm_context_destroy(context);
    debug("context %p destroyed\n", context);
    free(context);
    debug("vm_execute of context %p finished\n", context);
    return NULL;
}


/*
=item vm_signal()

Sends a signal to the virtual machine.

=cut
*/
int vm_signal(int sig) {
    return kill(getpid(), sig);
}

/*
=item vm_set_signal_handler()

Installs a signal handler in the virtual machine.

=cut
*/
int vm_set_signal_handler(int sig, function_handle_t handle) {
    assert(sig >= 0);
    assert(sig < NSIG);
    
    if (0 == pthread_mutex_lock(&_vm_signal_registry.m_mutex)) {
        _vm_signal_registry.m_handlers[sig] = handle;
        switch (handle) {
            case VM_SIGNAL_DEFAULT:
                sigdelset(&_vm_signal_registry.m_sigset, sig);
                break;
            case VM_SIGNAL_IGNORE:
            default:
                sigaddset(&_vm_signal_registry.m_sigset, sig);
                break;
        }
        pthread_sigmask(SIG_SETMASK, &_vm_signal_registry.m_sigset, NULL);
        pthread_mutex_unlock(&_vm_signal_registry.m_mutex);
        return 0;
    }
    else {
        debug("couldn't lock signal registry mutex\n");
        return -1;
    }

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
int vm_rs_push(vm_context_t *context, const vm_state_t *state) {
    assert(context != NULL);
    
    return STACK_PUSH(vm_state_t, &context->m_return_stack, state);
}

int vm_rs_pop(vm_context_t *context, vm_state_t *result) {
    assert(context != NULL);
    
    return STACK_POP(vm_state_t, &context->m_return_stack, result);
}

int vm_rs_top(vm_context_t *context, vm_state_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    return STACK_TOP(vm_state_t, &context->m_return_stack, result);
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
        if (0 == STACK_INIT(vm_state_t, &self->m_return_stack)) {
            self->m_bytecode = bytecode;
            self->m_bytecode_length = bytecode_len;
            self->m_counter = start;
            
            vm_context_registry_node_t *node = calloc(1, sizeof(*node));
            if (node) {
                node->m_context = self;
                if (0 == pthread_mutex_lock(&_vm_context_registry.m_mutex)) {
                    node->m_next = _vm_context_registry.m_nodes;
                    _vm_context_registry.m_nodes = node;
                    pthread_mutex_unlock(&_vm_context_registry.m_mutex);
                    pthread_cond_signal(&_vm_context_registry.m_changed);
                    return 0;
                }
                else {
                    debug("failed to lock _vm_context_registry.m_mutex\n");
                    free(node);
                    STACK_DESTROY(vm_state_t, &self->m_return_stack);
                    STACK_DESTROY(scalar_t, &self->m_data_stack);
                    return -1;
                }
            }
            else {
                debug("couldn't allocate memory for a vm_context_registry_node_t\n");
                STACK_DESTROY(vm_state_t, &self->m_return_stack);
                STACK_DESTROY(scalar_t, &self->m_data_stack);
                return -1;
            }            
        }
        else {
            debug("return stack initialisation failed\n");
            STACK_DESTROY(scalar_t, &self->m_data_stack);
            return -1;
        }
    }
    else {
        debug("data stack initialisation failed\n");
        return -1;
    }
}

/*
=item vm_context_destroy()

Destroys a vm_context_t object.  This should B<not> be called on objects that have been passed to
C<vm_execute()>, except by C<vm_execute()> itself.

=cut
 */ 
int vm_context_destroy(vm_context_t *self) {
    assert(self != NULL);
    
    // clean up
    STACK_DESTROY(scalar_t, &self->m_data_stack);
    STACK_DESTROY(vm_state_t, &self->m_return_stack);
    
    if (self->m_symboltable != NULL && 0 == symboltable_destroy(self->m_symboltable)) {
        free(self->m_symboltable);
        self->m_symboltable = NULL;
    }
    
    // remove from registry
    if (0 == pthread_mutex_lock(&_vm_context_registry.m_mutex)) {
        if (_vm_context_registry.m_nodes != NULL) {
            if (_vm_context_registry.m_nodes->m_context == self) {
                vm_context_registry_node_t *reg = _vm_context_registry.m_nodes;
                _vm_context_registry.m_nodes = _vm_context_registry.m_nodes->m_next;
                memset(reg, 0, sizeof(*reg));
                free(reg);
            }
            else {
                for (vm_context_registry_node_t *i = _vm_context_registry.m_nodes; i != NULL && i->m_next != NULL; i = i->m_next) {
                    if (i->m_next->m_context == self) {
                        vm_context_registry_node_t *reg = i->m_next;
                        i->m_next = i->m_next->m_next;
                        memset(reg, 0, sizeof(*reg));
                        free(reg);
                    }
                }
            }
        }
        else {
            debug("registry has no nodes\n");
        }
        pthread_mutex_unlock(&_vm_context_registry.m_mutex);
        pthread_cond_signal(&_vm_context_registry.m_changed);
    }
    else {
        debug("failed to lock _vm_context_registry.m_mutex - context %p not removed from registry!\n", self);
    }
    
    debug("destroyed %p, returning\n", self);
    return 0;
}

/*
=item vm_state_init()

=item vm_state_destroy()

=item vm_state_clone()

...

=cut
*/
int vm_state_init(vm_state_t *restrict self, function_handle_t position, symboltable_t *restrict symboltable) {
    assert(self != NULL);
    
    self->m_position = position;
    self->m_symboltable_top = symboltable;
    if (self->m_symboltable_top != NULL)  ++self->m_symboltable_top->m_references;
    
    return 0;
}

int vm_state_destroy(vm_state_t *self) {
    assert(self != NULL);
    
    if (self->m_symboltable_top) {
        symboltable_destroy(self->m_symboltable_top);
        self->m_symboltable_top = NULL;
    }
    
    self->m_position = 0;
    return 0;
}

int vm_state_clone(vm_state_t *restrict self, const vm_state_t *restrict other) {
    assert(self != NULL);
    assert(other != NULL);
    assert(self != other);
    
    self->m_position = other->m_position;
    self->m_symboltable_top = other->m_symboltable_top;
    if (self->m_symboltable_top)  ++self->m_symboltable_top->m_references;
    
    return 0;
}


/*
=back

=cut
 */
