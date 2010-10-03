/*
 *  bytecode.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>

#include "bytecode.h"
#include "scalar.h"

// dummy function for lookup table -- should not be executed!
int instruction_exit(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// dummy function for lookup table -- should not be executed!
int instruction_noop(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// ( a -- )
int instruction_drop(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    data_stack_scope_pop(data_stack, NULL);
    return 1;
}

// ( a b -- b a )
int instruction_swap(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    data_stack_scope_push(data_stack, &b);
    data_stack_scope_push(data_stack, &b);
    scalar_destroy(&a);
    scalar_destroy(&b);
    return 1;
}

// ( a -- a a )
int instruction_dup(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a;
    data_stack_scope_top(data_stack, &a);
    data_stack_scope_push(data_stack, &a);
    scalar_destroy(&a);
    return 1;
}

// ( -- a ) 
int instruction_lit(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    const scalar_t *a = (const scalar_t *) (instruction_ptr + 1);
    data_stack_scope_push(data_stack, a);
    return 1 + sizeof(scalar_t);
}

// N.B This needs to match the order of instruction_t (in bytecode.h)
// FIXME possibly break this out into its own file (e.g. instruction_table.c) and generate it
const instruction_func instruction_table[] = {
    &instruction_exit,
    &instruction_noop,
    &instruction_drop,
    &instruction_swap,
    &instruction_dup,
    &instruction_lit,
};

