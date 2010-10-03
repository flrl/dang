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

// ( a b -- a+b )
int instruction_add(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    // FIXME haven't defined maths operators for scalar_t yet, whoops!
    return 1;
}

// ( a b -- a-b )
int instruction_subt(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    // FIXME haven't defined maths operators for scalar_t yet, whoops!
    return 1;
}

// ( a b -- a*b )
int instruction_mult(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    // FIXME haven't defined maths operators for scalar_t yet, whoops!
    return 1;
}

// ( a b -- a/b )
int instruction_div(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    // FIXME haven't defined maths operators for scalar_t yet, whoops!
    return 1;
}

// ( a b -- a%b )
int instruction_mod(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a, b;
    data_stack_scope_pop(data_stack, &b);
    data_stack_scope_pop(data_stack, &a);
    // FIXME haven't defined maths operators for scalar_t yet, whoops!
    return 1;
}

// ( -- )
int instruction_branch(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    const scalar_t *offset = (const scalar_t *) (instruction_ptr + 1);
    return scalar_get_int_value(offset);
}

// ( a -- )
int instruction_0branch(const uint8_t *instruction_ptr, data_stack_scope_t *data_stack, void *return_stack) {
    const scalar_t *branch_offset = (const scalar_t *) (instruction_ptr + 1);
    int incr = 0;

    scalar_t a;
    data_stack_scope_pop(data_stack, &a);

    if (scalar_get_int_value(&a) == 0) {
        // branch by offset
        incr = scalar_get_int_value(branch_offset);
    }
    else {
        // skip to the next instruction
        incr = 1 + sizeof(scalar_t);
    }
    
    scalar_destroy(&a);
    return incr;
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
    &instruction_add,
    &instruction_subt,
    &instruction_mult,
    &instruction_div,
    &instruction_mod,
    &instruction_branch,
    &instruction_0branch,
};


