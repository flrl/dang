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

#define NEXT_BYTE(x) (const uint8_t*)(&(x)->m_bytecode[(x)->m_counter + 1])

// dummy function for lookup table -- should not be executed!
int instruction_exit(vm_context_t *context) {
    assert("should not get here" == 0);
    return 0;
}

// dummy function for lookup table -- should not be executed!
int instruction_noop(vm_context_t *context) {
    assert("should not get here" == 0);
    return 0;
}

// ( [params] -- ) ( -- addr )
int instruction_call(vm_context_t *context) {
    const size_t jump_dest = *(const size_t *) NEXT_BYTE(context);
    
    vm_start_scope(context);

    vm_rs_push(context, context->m_counter + 1 + sizeof(size_t));
    return jump_dest - context->m_counter;
}

// ( -- [results] ) ( addr -- )
int instruction_return(vm_context_t *context) {
    size_t jump_dest;
    
    vm_end_scope(context);
    
    vm_rs_pop(context, &jump_dest);
    return jump_dest - context->m_counter;    
}

// ( a -- )
int instruction_drop(vm_context_t *context) {
    vm_ds_pop(context, NULL);
    return 1;
}

// ( a b -- b a )
int instruction_swap(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    vm_ds_push(context, &b);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&b);
    return 1;
}

// ( a -- a a )
int instruction_dup(vm_context_t *context) {
    anon_scalar_t a;
    vm_ds_top(context, &a);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    return 1;
}

// ( -- a ) 
int instruction_intlit(vm_context_t *context) {
    const intptr_t lit = *(const intptr_t *) NEXT_BYTE(context);

    anon_scalar_t a;
    anon_scalar_init(&a);
    anon_scalar_set_int_value(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(intptr_t);
}

// ( -- )
int instruction_branch(vm_context_t *context) {
    const intptr_t offset = *(const intptr_t *) NEXT_BYTE(context);
    return offset;
}

// ( a -- )
int instruction_0branch(vm_context_t *context) {
    const intptr_t branch_offset = *(const intptr_t *) NEXT_BYTE(context);
    int incr = 0;

    anon_scalar_t a;
    vm_ds_pop(context, &a);

    if (anon_scalar_get_int_value(&a) == 0) {
        // branch by offset
        incr = branch_offset;
    }
    else {
        // skip to the next instruction
        incr = 1 + sizeof(intptr_t);
    }
    
    anon_scalar_destroy(&a);
    return incr;
}

// ( a b -- a+b )
int instruction_intadd(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);

    anon_scalar_t c;
    anon_scalar_init(&c);
    anon_scalar_set_int_value(&c, anon_scalar_get_int_value(&a) + anon_scalar_get_int_value(&b));
    vm_ds_push(context, &c);

    debug("%ld + %ld = %ld\n", anon_scalar_get_int_value(&a), anon_scalar_get_int_value(&b), anon_scalar_get_int_value(&c));
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&c);
    return 1;
}

// ( a b -- a-b )
int instruction_intsubt(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    
    anon_scalar_t c;
    anon_scalar_init(&c);
    anon_scalar_set_int_value(&c, anon_scalar_get_int_value(&a) - anon_scalar_get_int_value(&b));
    vm_ds_push(context, &c);

    debug("%ld - %ld = %ld\n", anon_scalar_get_int_value(&a), anon_scalar_get_int_value(&b), anon_scalar_get_int_value(&c));
    anon_scalar_destroy(&c);
    return 1;
}

// ( a b -- a*b )
int instruction_intmult(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);

    anon_scalar_t c;
    anon_scalar_init(&c);
    anon_scalar_set_int_value(&c, anon_scalar_get_int_value(&a) * anon_scalar_get_int_value(&b));
    vm_ds_push(context, &c);
    
    debug("%ld * %ld = %ld\n", anon_scalar_get_int_value(&a), anon_scalar_get_int_value(&b), anon_scalar_get_int_value(&c));
    anon_scalar_destroy(&c);    
    return 1;
}

// ( a b -- a/b )
int instruction_intdiv(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    
    anon_scalar_t c;
    anon_scalar_init(&c);
    anon_scalar_set_int_value(&c, anon_scalar_get_int_value(&a) / anon_scalar_get_int_value(&b));
    vm_ds_push(context, &c);
    
    debug("%ld / %ld = %ld\n", anon_scalar_get_int_value(&a), anon_scalar_get_int_value(&b), anon_scalar_get_int_value(&c));
    anon_scalar_destroy(&c);    
    return 1;
}

// ( a b -- a%b )
int instruction_intmod(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    
    anon_scalar_t c;
    anon_scalar_init(&c);
    anon_scalar_set_int_value(&c, anon_scalar_get_int_value(&a) % anon_scalar_get_int_value(&b));
    vm_ds_push(context, &c);
    
    debug("%ld %% %ld = %ld\n", anon_scalar_get_int_value(&a), anon_scalar_get_int_value(&b), anon_scalar_get_int_value(&c));
    anon_scalar_destroy(&c);
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


