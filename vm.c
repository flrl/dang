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
#include "vm.h"

typedef struct vm_symboltable_registry_node_t {
    struct vm_symboltable_registry_node_t *m_next;
    vm_symboltable_t *m_table;
} vm_symboltable_registry_node_t;

static vm_symboltable_registry_node_t *_vm_symboltable_registry = NULL;
static pthread_mutex_t _vm_symboltable_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

static const size_t _vm_context_initial_ds_count = 16;
static const size_t _vm_context_initial_rs_count = 16;

static inline int _vm_ds_reserve(vm_context_t *context, size_t new_size) {
    assert(context != NULL);
    
    if (context->m_data_stack_alloc_count >= new_size)  return 0;

    anon_scalar_t *tmp = calloc(new_size, sizeof(*tmp));
    if (tmp == NULL)  return -1;
    
    memcpy(tmp, context->m_data_stack, context->m_data_stack_count);
    free(context->m_data_stack);
    context->m_data_stack = tmp;
    return 0;
}

static inline int _vm_rs_reserve(vm_context_t *context, size_t new_size) {
    assert(context != NULL);
    
    if (context->m_return_stack_alloc_count >= new_size)  return 0;
    
    size_t *tmp = calloc(new_size, sizeof(*tmp));
    if (tmp == NULL)  return -1;
    
    memcpy(tmp, context->m_return_stack, context->m_return_stack_count);
    free(context->m_return_stack);
    context->m_return_stack = tmp;
    return 0;
}

void *vm_execute(void *ptr) {
    vm_context_t *context = ptr;
    assert(context != NULL);
    
    int incr;
    while (context->m_counter < context->m_bytecode_length) {
        uint8_t instruction = context->m_bytecode[context->m_counter];
        assert(instruction < i__MAX);
        switch (instruction) {
            case iEND:
                return NULL;
            case iNOOP:
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

int vm_context_init(vm_context_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));

    if (NULL != (self->m_data_stack = calloc(_vm_context_initial_ds_count, sizeof(*self->m_data_stack)))) {
        self->m_data_stack_alloc_count = _vm_context_initial_ds_count;
        
        if (NULL != (self->m_return_stack = calloc(_vm_context_initial_rs_count, sizeof(*self->m_return_stack)))) {
            self->m_return_stack_alloc_count = _vm_context_initial_rs_count;
            return 0;
        }
        else {
            free(self->m_data_stack);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int vm_context_destroy(vm_context_t *self) {
    assert(self != NULL);
        
    free(self->m_data_stack);
    free(self->m_return_stack);
    
    if (0 == vm_symboltable_destroy(self->m_symboltable)) {
        free(self->m_symboltable);
    }
    
    return 0;
}


int vm_ds_push(vm_context_t *context, const anon_scalar_t *value) {
    assert(context != NULL);
    assert(value != NULL);
    
    int status = 0;
    if (context->m_data_stack_count == context->m_data_stack_alloc_count) {
        status = _vm_ds_reserve(context, context->m_data_stack_alloc_count * 2);
    }

    if (status == 0) {
        anon_scalar_clone(&context->m_data_stack[context->m_data_stack_count++], value);
    }
    
    return status;
}

int vm_ds_pop(vm_context_t *context, anon_scalar_t *result) {
    assert(context != NULL);
    
    int status = 0;
    if (result != NULL) {
        anon_scalar_assign(result, &context->m_data_stack[--context->m_data_stack_count]);
    }
    else {
        anon_scalar_destroy(&context->m_data_stack[--context->m_data_stack_count]);
    }
    
    return status;
}

int vm_ds_top(vm_context_t *context, anon_scalar_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    anon_scalar_clone(result, &context->m_data_stack[context->m_data_stack_count - 1]);
    
    return 0;
}


int vm_rs_push(vm_context_t *context, size_t value) {
    assert(context != NULL);
    
    int status = 0;
    if (context->m_return_stack_count == context->m_return_stack_alloc_count) {
        status = _vm_rs_reserve(context, context->m_return_stack_alloc_count * 2);
    }
    
    if (status == 0) {
        context->m_return_stack[context->m_return_stack_count++] = value;
    }
    
    return status;
}

int vm_rs_pop(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    
    int status = 0;
    if (result != NULL) {
        *result = context->m_return_stack[--context->m_return_stack_count];
    }
    else {
        --context->m_return_stack_count;
    }
    return status;
}

int vm_rs_top(vm_context_t *context, size_t *result) {
    assert(context != NULL);
    assert(result != NULL);
    
    *result = context->m_return_stack[context->m_return_stack_count - 1];
    return 0;
}

int vm_start_scope(vm_context_t *context) {
    assert(context != NULL);
    
    vm_symboltable_t *new_table = calloc(1, sizeof(*new_table));
    if (new_table == NULL)  return -1;
    
    vm_symboltable_init(new_table, context->m_symboltable);
    context->m_symboltable = new_table;
    
    return 0;
}

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

int vm_symbol_init(vm_symbol_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    return 0;
}

int vm_symbol_destroy(vm_symbol_t *self) {
    assert(self != NULL);
    scalar_pool_release_scalar(self->m_referent.as_scalar);  // FIXME handle different types of symbol table entry
    memset(self, 0, sizeof(*self));
    return 0;
}

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

int vm_symbol_define(vm_context_t *context, identifier_t identifier, uint32_t flags) {
    assert(context != NULL);
    vm_symbol_t *symbol = calloc(1, sizeof(*symbol));
    vm_symbol_init(symbol);
    symbol->m_identifier = identifier;
    symbol->m_flags = flags;  // FIXME validate this
    symbol->m_referent.as_scalar = scalar_pool_allocate_scalar(flags);
    
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
