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
int instruction_exit(data_stack_scope_t *data_stack, void *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// dummy function for lookup table -- should not be executed!
int instruction_noop(data_stack_scope_t *data_stack, void *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// ( a -- )
int instruction_drop(data_stack_scope_t *data_stack, void *return_stack) {
    data_stack_scope_pop(data_stack, NULL);
    return 1;
}

// ( a b -- b a )
int instruction_swap(data_stack_scope_t *data_stack, void *return_stack) {
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
int instruction_dup(data_stack_scope_t *data_stack, void *return_stack) {
    scalar_t a;
    data_stack_scope_top(data_stack, &a);
    data_stack_scope_push(data_stack, &a);
    scalar_destroy(&a);
    return 1;
}

const instruction_func instruction_table[] = {
    &instruction_exit,
    &instruction_noop,
    &instruction_drop,
    &instruction_swap,
    &instruction_dup,
};


