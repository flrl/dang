/*
 *  bytecode.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>

#include "bytecode.h"
#include "debug.h"
#include "scalar.h"

// dummy function for lookup table -- should not be executed!
int instruction_exit(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// dummy function for lookup table -- should not be executed!
int instruction_noop(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    assert("should not get here" == 0);
    return 0;
}

// ( [params] -- ) ( -- addr )
int instruction_call(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    const size_t argc = *(const uint8_t *)(instruction_ptr + 1);
    const size_t jump_dest = *(const size_t *) (instruction_ptr + 1 + sizeof(uint8_t));
    
    // pop and stash the params off the caller's stack
    scalar_t *argv = calloc(argc, sizeof(scalar_t));
    assert(argv != NULL);
    data_stack_npop(*data_stack, argc, argv);
    
    // push a new stack for the callee
    data_stack_start_scope(data_stack);

    // push the stashed params onto the callee's stack
    data_stack_npush(*data_stack, argc, argv);
    
    // clean up the stash
    for (size_t i = 0; i < argc; i++) {
        scalar_destroy(&argv[i]);
    }
    free(argv);
    
    // hook up return stack
    return_stack_push(return_stack, instruction_index + 1 + sizeof(uint8_t) + sizeof(size_t));
    return jump_dest - instruction_index;
}

// ( -- [results] ) ( addr -- )
int instruction_return(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    const size_t resultc = *(const uint8_t *)(instruction_ptr + 1);

    // pop and stash the result from the returning stack
    scalar_t *resultv = calloc(resultc, sizeof(scalar_t));
    assert(resultv != NULL);
    data_stack_npop(*data_stack, resultc, resultv);

    // pop the returning scope
    data_stack_end_scope(data_stack);

    // push the stashed results onto the caller's stack
    data_stack_npush(*data_stack, resultc, resultv);

    // clean up the stash
    for (size_t i = 0; i < resultc; i++) {
        scalar_destroy(&resultv[i]);
    }
    free(resultv);
    
    // unhook return stack
    size_t jump_dest;
    return_stack_pop(return_stack, &jump_dest);
    return jump_dest - instruction_index;
}

// ( a -- )
int instruction_drop(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    data_stack_pop(*data_stack, NULL);
    return 1;
}

// ( a b -- b a )
int instruction_swap(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);
    data_stack_push(*data_stack, &b);
    data_stack_push(*data_stack, &b);
    scalar_destroy(&a);
    scalar_destroy(&b);
    return 1;
}

// ( a -- a a )
int instruction_dup(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a;
    data_stack_top(*data_stack, &a);
    data_stack_push(*data_stack, &a);
    scalar_destroy(&a);
    return 1;
}

// ( -- a ) 
int instruction_intlit(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    const intptr_t *lit = (const intptr_t *) (instruction_ptr + 1);

    scalar_t a;
    scalar_init(&a);
    scalar_set_int_value(&a, *lit);
    data_stack_push(*data_stack, &a);
    scalar_destroy(&a);
    
    return 1 + sizeof(intptr_t);
}

// ( -- )
int instruction_branch(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    // FIXME why is this using a scalar_t?!
    const scalar_t *offset = (const scalar_t *) (instruction_ptr + 1);
    return scalar_get_int_value(offset);
}

// ( a -- )
int instruction_0branch(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    // FIXME why is this using a scalar_t?!
    const scalar_t *branch_offset = (const scalar_t *) (instruction_ptr + 1);
    int incr = 0;

    scalar_t a;
    data_stack_pop(*data_stack, &a);

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

// ( a b -- a+b )
int instruction_intadd(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);

    scalar_t c;
    scalar_init(&c);
    scalar_set_int_value(&c, scalar_get_int_value(&a) + scalar_get_int_value(&b));
    data_stack_push(*data_stack, &c);
    debug("%ld + %ld = %ld\n", scalar_get_int_value(&a), scalar_get_int_value(&b), scalar_get_int_value(&c));
    scalar_destroy(&c);
    return 1;
}

// ( a b -- a-b )
int instruction_intsubt(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);
    
    scalar_t c;
    scalar_init(&c);
    scalar_set_int_value(&c, scalar_get_int_value(&a) - scalar_get_int_value(&b));
    data_stack_push(*data_stack, &c);
    debug("%ld - %ld = %ld\n", scalar_get_int_value(&a), scalar_get_int_value(&b), scalar_get_int_value(&c));
    scalar_destroy(&c);
    return 1;
}

// ( a b -- a*b )
int instruction_intmult(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);

    scalar_t c;
    scalar_init(&c);
    scalar_set_int_value(&c, scalar_get_int_value(&a) * scalar_get_int_value(&b));
    data_stack_push(*data_stack, &c);
    debug("%ld * %ld = %ld\n", scalar_get_int_value(&a), scalar_get_int_value(&b), scalar_get_int_value(&c));
    scalar_destroy(&c);    
    return 1;
}

// ( a b -- a/b )
int instruction_intdiv(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);
    
    scalar_t c;
    scalar_init(&c);
    scalar_set_int_value(&c, scalar_get_int_value(&a) / scalar_get_int_value(&b));
    data_stack_push(*data_stack, &c);
    debug("%ld / %ld = %ld\n", scalar_get_int_value(&a), scalar_get_int_value(&b), scalar_get_int_value(&c));
    scalar_destroy(&c);    
    return 1;
}

// ( a b -- a%b )
int instruction_intmod(const uint8_t *instruction_ptr, size_t instruction_index, data_stack_t **data_stack, return_stack_t *return_stack) {
    scalar_t a, b;
    data_stack_pop(*data_stack, &b);
    data_stack_pop(*data_stack, &a);
    
    scalar_t c;
    scalar_init(&c);
    scalar_set_int_value(&c, scalar_get_int_value(&a) % scalar_get_int_value(&b));
    data_stack_push(*data_stack, &c);
    debug("%ld %% %ld = %ld\n", scalar_get_int_value(&a), scalar_get_int_value(&b), scalar_get_int_value(&c));
    scalar_destroy(&c);
    return 1;
}

// N.B This needs to match the order of instruction_t (in bytecode.h)
// FIXME possibly break this out into its own file (e.g. instruction_table.c) and generate it
const instruction_func instruction_table[] = {
    &instruction_exit,
    &instruction_noop,
    &instruction_call,
    &instruction_return,
    &instruction_drop,
    &instruction_swap,
    &instruction_dup,
    &instruction_branch,
    &instruction_0branch,
    &instruction_intlit,
    &instruction_intadd,
    &instruction_intsubt,
    &instruction_intmult,
    &instruction_intdiv,
    &instruction_intmod,
};


