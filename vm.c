/*
 *  vm.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "vm.h"

//typedef struct vm_context_t {
//    uint8_t *m_bytecode;
//    size_t  m_bytecode_length;
//    size_t  m_counter;
//    size_t  m_data_stack_alloc_count;
//    size_t  m_data_stack_count;
//    anon_scalar_t *m_data_stack;
//    size_t  m_return_stack_alloc_count;
//    size_t  m_return_stack_count;
//    size_t  *m_return_stack;
//    vm_symboltable_t *m_symboltable;
//} vm_context_t;
//

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
    
    return -1;  // FIXME walk the symbol table and clean that up too?
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
    
    // FIXME set up a registry of these
    
    context->m_symboltable->m_subscope_count++;
    new_table->m_parent = context->m_symboltable;
    context->m_symboltable = new_table;
    return 0;
}

int vm_end_scope(vm_context_t *context) {
    assert(context != NULL);
    
    vm_symboltable_t *old_table = context->m_symboltable;
    context->m_symboltable = context->m_symboltable->m_parent;    // FIXME deal with NULL somehow
    
    if (old_table->m_subscope_count == 0) {
        // FIXME walk the table and clean it up, then free it
    }
    
    return 0;
}

