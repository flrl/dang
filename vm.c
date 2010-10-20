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
=head1 VM

=head1 INTRODUCTION

=head1 PUBLIC INTERFACE

=cut
*/

STACK_DEFINITIONS(anon_scalar_t, anon_scalar_init, anon_scalar_destroy, anon_scalar_clone, anon_scalar_assign);
STACK_DEFINITIONS(size_t, STACK_basic_init, STACK_basic_destroy, STACK_basic_clone, STACK_basic_assign);

typedef struct vm_symboltable_registry_node_t {
    struct vm_symboltable_registry_node_t *m_next;
    vm_symboltable_t *m_table;
} vm_symboltable_registry_node_t;

static vm_symboltable_registry_node_t *_vm_symboltable_registry = NULL;
static pthread_mutex_t _vm_symboltable_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

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

Data stack management functions

=cut
 */
int vm_ds_push(vm_context_t *context, const anon_scalar_t *value) {
    assert(context != NULL);
    assert(value != NULL);
    
    return STACK_PUSH(anon_scalar_t, &context->m_data_stack, value);
}

int vm_ds_pop(vm_context_t *context, anon_scalar_t *result) {
    assert(context != NULL);
    
    return STACK_POP(anon_scalar_t, &context->m_data_stack, result);
}

int vm_ds_top(vm_context_t *context, anon_scalar_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    return STACK_TOP(anon_scalar_t, &context->m_data_stack, result);
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
    
    vm_symboltable_t *new_table = calloc(1, sizeof(*new_table));
    if (new_table == NULL)  return -1;
    
    vm_symboltable_init(new_table, context->m_symboltable);
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
    
    vm_symboltable_t *old_table = context->m_symboltable;
    context->m_symboltable = context->m_symboltable->m_parent;
    
    if (0 == vm_symboltable_destroy(old_table)) {
        free(old_table);
    }
    
    return 0;
}

/*
=item vm_symbol_define()

Define a symbol within the current scope.

=cut
 */
int vm_symbol_define(vm_context_t *context, identifier_t identifier, uint32_t flags) {
    assert(context != NULL);
    vm_symbol_t *symbol = calloc(1, sizeof(*symbol));
    vm_symbol_init(symbol);
    symbol->m_identifier = identifier;
    symbol->m_flags = flags;  // FIXME validate this
    symbol->m_referent.as_scalar = scalar_allocate(flags);
    // FIXME need two sets of flags here: one set for the symbol (to say whether it's a scalar, array, etc), and one set
    // FIXME for the thing being defined (to say whether it's shared, etc)
    
    if (context->m_symboltable->m_symbols == NULL) {
        context->m_symboltable->m_symbols = symbol;
        return 0;
    }
    else {
        vm_symbol_t *parent = context->m_symboltable->m_symbols;
        do {
            if (identifier < parent->m_identifier) {
                if (parent->m_left_child == NULL) {
                    parent->m_left_child = symbol;
                    symbol->m_parent = parent;
                    return 0;
                }
                else {
                    parent = parent->m_left_child;
                }
            }
            else if (identifier > parent->m_identifier) {
                if (parent->m_right_child == NULL) {
                    parent->m_right_child = symbol;
                    symbol->m_parent = parent;
                    return 0;
                }
                else {
                    parent = parent->m_right_child;
                }
            }
            else {
                debug("identifier %"PRIuPTR" is already defined in current scope\n", identifier);
                vm_symbol_destroy(symbol);
                free(symbol);
                return -1;
            }
        } while (parent != NULL);
        
        debug("not supposed to get here\n");
        return -1;
    }
}

/*
=item vm_symbol_lookup()

Look for a symbol in the current scope.  If no such symbol is found, search parent scopes, then grandparent scopes, etc,
until either a matching symbol is found, or the top level ("global") scope has been unsuccessfully searched.

Returns a pointer to the vm_symbol_t object found, or NULL if the symbol is not defined.

=cut
 */
const vm_symbol_t *vm_symbol_lookup(vm_context_t *context, identifier_t identifier) {
    assert(context != NULL);
    vm_symboltable_t *scope = context->m_symboltable;
    while (scope != NULL) {
        vm_symbol_t *symbol = scope->m_symbols;
        while (symbol != NULL) {
            if (identifier < symbol->m_identifier) {
                symbol = symbol->m_left_child;
            }
            else if (identifier >  symbol->m_identifier) {
                symbol = symbol->m_right_child;
            }
            else {
                return symbol;
            }
        }
        scope = scope->m_parent;
    }
    return NULL;
}

/*
=item vm_symbol_undefine()

Removes a symbol from the current scope.  This does I<not> search upwards through ancestor scopes.

Returns 0 when the requested symbol no longer exists (or never did exist) at the current scope.  Other
return values indicate an error undefining the symbol, and that it may still exist at the current scope.

=cut
 */
int vm_symbol_undefine(vm_context_t *context, identifier_t identifier) {
    assert(context != NULL);
    
    vm_symbol_t *symbol = (vm_symbol_t *) vm_symbol_lookup(context, identifier);
    if (symbol == NULL)  return 0;
    
    if (symbol->m_left_child != NULL) {
        if (symbol->m_right_child != NULL) {
            // FIXME two child nodes - tricky bit goes here
            vm_symbol_t *replacement = symbol;
            vm_symbol_t *join;
            if (rand() & 0x4000) {
                // reconnect from left
                replacement = symbol->m_left_child;
                while (replacement != NULL && replacement->m_right_child != NULL)  replacement = replacement->m_right_child;
                join = replacement->m_parent;
                join->m_right_child = replacement->m_left_child;
                join->m_right_child->m_parent = join;
                
            }
            else {
                // reconnect from right
                replacement = symbol->m_right_child;
                while (replacement != NULL && replacement->m_left_child != NULL)  replacement = replacement->m_left_child;
                join = replacement->m_parent;
                join->m_left_child = replacement->m_right_child;
                join->m_left_child->m_parent = join;
            }
            
            replacement->m_left_child = symbol->m_left_child;
            if (replacement->m_left_child != NULL)  replacement->m_left_child->m_parent = replacement;
            replacement->m_right_child = symbol->m_right_child;
            if (replacement->m_right_child != NULL)  replacement->m_right_child->m_parent = replacement;
            
            replacement->m_parent = symbol->m_parent;
            if (replacement->m_parent == NULL) {
                context->m_symboltable->m_symbols = replacement;   
            }
            else if (replacement->m_parent->m_left_child == symbol) {
                replacement->m_parent->m_left_child = replacement;
            }
            else if (replacement->m_parent->m_right_child == symbol) {
                replacement->m_parent->m_right_child = replacement;
            }
            else {
                debug("symbol's parent doesn't claim it as a child\n");
            }
            
            vm_symbol_destroy(symbol);
            free(symbol);
            return 0;                
        }
        else {
            if (symbol->m_parent->m_left_child == symbol) {
                symbol->m_parent->m_left_child = symbol->m_left_child;
            }
            else if (symbol->m_parent->m_right_child == symbol) {
                symbol->m_parent->m_right_child = symbol->m_left_child;
            }
            else {
                debug("symbol's parent doesn't claim it as a child\n");
            }
            vm_symbol_destroy(symbol);
            free(symbol);
            return 0;
        }
    }
    else if (symbol->m_right_child != NULL) {
        if (symbol->m_parent->m_left_child == symbol) {
            symbol->m_parent->m_left_child = symbol->m_right_child;
        }
        else if (symbol->m_parent->m_right_child == symbol) {
            symbol->m_parent->m_right_child = symbol->m_right_child;
        }
        else {
            debug("symbol's parent doesn't claim it as a child\n");
        }
        vm_symbol_destroy(symbol);
        free(symbol);
        return 0;
    }
    else {
        if (symbol->m_parent->m_left_child == symbol) {
            symbol->m_parent->m_left_child = NULL;
        }
        else if (symbol->m_parent->m_right_child == symbol) {
            symbol->m_parent->m_right_child = NULL;
        }
        else {
            debug("symbol's parent doesn't claim it as a child\n");
        }
        vm_symbol_destroy(symbol);
        free(symbol);
        return 0;
    }
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
    
    if (0 == STACK_INIT(anon_scalar_t, &self->m_data_stack)) {
        if (0 == STACK_INIT(size_t, &self->m_return_stack)) {
            return 0;
        }
        else {
            STACK_DESTROY(anon_scalar_t, &self->m_data_stack);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int vm_context_destroy(vm_context_t *self) {
    assert(self != NULL);
    
    STACK_DESTROY(anon_scalar_t, &self->m_data_stack);
    STACK_DESTROY(size_t, &self->m_return_stack);
    
    if (0 == vm_symboltable_destroy(self->m_symboltable)) {
        free(self->m_symboltable);
    }
    
    return 0;
}


/*
=item vm_symbol_init()

=item vm_symbol_destroy()

Setup and teardown functions for individual vm_symbol_t objects

=cut
 */
int vm_symbol_init(vm_symbol_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    return 0;
}

int vm_symbol_destroy(vm_symbol_t *self) {
    assert(self != NULL);
    scalar_release(self->m_referent.as_scalar);  // FIXME handle different types of symbol table entry
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item vm_symbol_reap()

Recursively destroys a vm_symbol_t object and all of its children

=cut
 */
int vm_symbol_reap(vm_symbol_t *self) {
    assert(self != NULL);
    if (self->m_left_child != NULL) {
        vm_symbol_reap(self->m_left_child);
        free(self->m_left_child);
    }
    if (self->m_right_child != NULL) {
        vm_symbol_reap(self->m_right_child);
        free(self->m_right_child);
    }
    return vm_symbol_destroy(self);
}

/*
=item vm_symboltable_init() 

Initialises a symbol table and associates it with its parent.  The symbol table is also registered globally for later
garbage collection.

=cut
 */
int vm_symboltable_init(vm_symboltable_t *restrict self, vm_symboltable_t *restrict parent) {
    assert(self != NULL);
    assert(self != parent);
    
    self->m_references = 1;
    self->m_symbols = NULL;
    self->m_parent = parent;
    if (self->m_parent != NULL)  ++self->m_parent->m_references;
    
    vm_symboltable_registry_node_t *reg_entry;
    if (NULL != (reg_entry = calloc(1, sizeof(*reg_entry)))) {
        if (0 == pthread_mutex_lock(&_vm_symboltable_registry_mutex)) {
            reg_entry->m_table = self;
            reg_entry->m_next = _vm_symboltable_registry;
            _vm_symboltable_registry = reg_entry;
            pthread_mutex_unlock(&_vm_symboltable_registry_mutex);
        }
    }
    
    return 0;
}

/*
=item vm_symboltable_destroy()

Decrements a symbol table's reference count.  If the reference count reaches zero, destroys the symbol table, removes it from
the global registry, and decrements its parent's reference count.  If the reference count does not reach zero, the symbol table
is left in the global registry for later garbage collection.

=cut
 */
int vm_symboltable_destroy(vm_symboltable_t *self) {
    assert(self != NULL);
    assert(self->m_references > 0);
    
    if (--self->m_references == 0) {
        if (self->m_parent != NULL)  --self->m_parent->m_references;
        if (self->m_symbols != NULL) {
            vm_symbol_reap(self->m_symbols);
            free(self->m_symbols);
        } 
        
        if (0 == pthread_mutex_lock(&_vm_symboltable_registry_mutex)) {
            for (vm_symboltable_registry_node_t *i = _vm_symboltable_registry; i != NULL && i->m_next != NULL; i = i->m_next) {
                if (i->m_next->m_table == self) {
                    vm_symboltable_registry_node_t *reg = i->m_next;
                    i->m_next = i->m_next->m_next;
                    free(reg);
                }
            }
            
            pthread_mutex_unlock(&_vm_symboltable_registry_mutex);
        }
        
        memset(self, 0, sizeof(*self));
        return 0;
    }
    else {
        return -1;
    }
}

/*
=item vm_symboltable_garbage_collect()

Checks the global symbol table registry for entries with a zero reference count, and destroys them.

This situation arises when a scope ends while it is still referenced elsewhere (e.g. by a child scope running in a different
thread of execution).  When the child scope ends it decrements the parent's reference count, but by this time the parent scope 
has already ended.  Periodically calling vm_symboltable_garbage_collect allows the resources held by these scopes to be reclaimed
by the system.

=cut
 */
int vm_symboltable_garbage_collect(void) {
    if (0 == pthread_mutex_lock(&_vm_symboltable_registry_mutex)) {
        while (_vm_symboltable_registry != NULL) {
            assert(_vm_symboltable_registry->m_table != NULL);
            if (_vm_symboltable_registry->m_table->m_references > 0) {
                break;  
            } 
            else {
                vm_symboltable_registry_node_t *old_reg = _vm_symboltable_registry;
                _vm_symboltable_registry = _vm_symboltable_registry->m_next;
                
                vm_symboltable_t *old_table = old_reg->m_table;
                memset(old_reg, 0, sizeof(*old_reg));
                free(old_reg);
                
                if (old_table->m_parent != NULL)  --old_table->m_parent->m_references;
                if (old_table->m_symbols != NULL) {
                    vm_symbol_reap(old_table->m_symbols);
                    free(old_table->m_symbols);
                }
                
                memset(old_table, 0, sizeof(*old_table));
                free(old_table);
            }
        }
        pthread_mutex_unlock(&_vm_symboltable_registry_mutex);
        return 0;
    }
    else {
        return -1;
    }
}


/*
=back

=cut
 */
