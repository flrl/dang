/*
 *  bytecode.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 bytecode

=over

=cut 
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "debug.h"
#include "scalar.h"
#include "vm.h"

#define NEXT_BYTE(x) (const uint8_t*)(&(x)->m_bytecode[(x)->m_counter + 1])

/*
=item END
 
Indicates the end of the bytecode stream.
 
=cut
 */
int inst_END(vm_context_t *context) {
    // dummy function for lookup table -- should not be executed!
    assert("should not get here" == 0);
    return 0;
}

/*
=item NOOP

Does nothing.  Used to pad the bytecode when alignment is important.

=cut
 */
int inst_NOOP(vm_context_t *context) {
    // dummy function for lookup table -- should not be executed!
    assert("should not get here" == 0);
    return 0;
}

/*
=item CALL ( [params] -- ) ( -- addr )

Reads a jump destination from the following bytecode.  Pushes the location of the bytecode following the jump destination
to the return stack, starts a new symbol table scope, then transfers execution control to the jump destination.
 
=cut
 */
int inst_CALL(vm_context_t *context) {
    const size_t jump_dest = *(const size_t *) NEXT_BYTE(context);
    
    vm_start_scope(context);

    vm_rs_push(context, context->m_counter + 1 + sizeof(size_t));
    return jump_dest - context->m_counter;
}

/*
=item RETURN ( -- [results] ) ( addr -- )

Ends the current symbol table scope.  Pops a return destination from the return stack, and transfers execution control to it.

=cut
 */
int inst_RETURN(vm_context_t *context) {
    size_t jump_dest;
    
    vm_end_scope(context);
    
    vm_rs_pop(context, &jump_dest);
    return jump_dest - context->m_counter;    
}

/*
=item DROP ( a -- )
 
Drops an item from the data stack
 
=cut
 */
int inst_DROP(vm_context_t *context) {
    vm_ds_pop(context, NULL);
    return 1;
}

/*
=item SWAP ( a b -- b a )

Swaps the two items at the top of the data stack

=cut
 */
int inst_SWAP(vm_context_t *context) {
    anon_scalar_t a, b;
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    vm_ds_push(context, &b);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&b);
    return 1;
}

/*
=item DUP ( a -- a a )

Duplicates the item on the top of the data stack
 
=cut
 */
int inst_DUP(vm_context_t *context) {
    anon_scalar_t a;
    vm_ds_top(context, &a);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item BRANCH ( -- )
 
Reads a jump destination from the following bytecode and transfers execution control to it.
 
=cut
 */
int inst_BRANCH(vm_context_t *context) {
    const intptr_t offset = *(const intptr_t *) NEXT_BYTE(context);
    return offset;
}

/*
=item 0BRANCH ( a -- )

Reads a jump destination from the following bytecode.  Pops a value from the data stack.  If the value popped is zero, transfers
execution control to the jump destination.  Otherwise, transfers execution control to the next instruction.
 
=cut
 */
int inst_0BRANCH(vm_context_t *context) {
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

/*
=item SYMDEF ( -- )

Reads flags and an identifier from the following bytecode.  Defines a new symbol in the current scope with the requested 
identifier.
 
=cut
 */
int inst_SYMDEF(struct vm_context_t *context) {
    const uint32_t flags = *(const uint32_t *) (&context->m_bytecode[context->m_counter + 1]);
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1 + sizeof(flags)]);
    
    vm_symbol_define(context, identifier, flags);
    
    return 1 + sizeof(flags) + sizeof(identifier);
}

/*
=item SYMFIND ( -- handle )

Reads an identifier from the following bytecode.  Looks up the identifier in the symbol table, and pushes its handle to the
data stack, or 0 if the identifier was not found.
 
=cut
 */
int inst_SYMFIND(struct vm_context_t *context) {
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    const vm_symbol_t *symbol = vm_symbol_lookup(context, identifier);

    anon_scalar_t handle;
    anon_scalar_init(&handle);    
    anon_scalar_set_int_value(&handle, symbol ? symbol->m_referent.as_scalar : 0);
    vm_ds_push(context, &handle);
    anon_scalar_destroy(&handle);
    
    return 1 + sizeof(identifier);
}

/*
=item SYMUNDEF ( -- )
 
Reads an identifier from the following bytecode.  Removes the identifier from the symbol table in the current scope.

=cut
 */
int inst_SYMUNDEF(struct vm_context_t *context) {
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    vm_symbol_undefine(context, identifier);
    
    return 1 + sizeof(identifier);
}

/*
=item SLOAD ( handle -- a )

Pops a scalar handle from the data stack.  Pushes the value of the scalar matching the handle.
 
=cut
 */
int inst_SLOAD(struct vm_context_t *context) {
    anon_scalar_t handle, a;
    anon_scalar_init(&handle);
    anon_scalar_init(&a);

    vm_ds_pop(context, &handle);
    scalar_get_value((scalar_handle_t) anon_scalar_get_int_value(&handle), &a);
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&handle);
    
    return 1;
}

/*
=item SSTORE ( a handle -- )

Pops a scalar handle and a scalar value from the data stack.  Stores the value in the scalar matching the handle.
 
=cut
 */
int inst_SSTORE(struct vm_context_t *context) {
    anon_scalar_t handle, a;
    
    anon_scalar_init(&handle);
    anon_scalar_init(&a);
    
    vm_ds_pop(context, &handle);
    vm_ds_pop(context, &a);
    scalar_set_value((scalar_handle_t) anon_scalar_get_int_value(&handle), &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&handle);
    
    return 1;
}

/*
 =item INTLIT ( -- a ) 
 
 Reads an integer value from the following bytecode and pushes it onto the data stack.  Transfers execution control to the 
 bytecode following the integer value.
 
 =cut
 */
int inst_INTLIT(vm_context_t *context) {
    const intptr_t lit = *(const intptr_t *) NEXT_BYTE(context);
    
    anon_scalar_t a;
    anon_scalar_init(&a);
    anon_scalar_set_int_value(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(intptr_t);
}

/*
=item INTADD ( a b -- a+b )

Pops two values from the data stack, and pushes back their integer sum.
 
=cut
 */
int inst_INTADD(vm_context_t *context) {
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

/*
=item INTSUBT ( a b -- a-b )

Pops two values from the data stack, and pushes back their integer difference.

=cut
 */
int inst_INTSUBT(vm_context_t *context) {
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

/*
=item INTMULT ( a b -- a*b )

Pops two values from the data stack, and pushes back their integer product.

=cut
 */
int inst_INTMULT(vm_context_t *context) {
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

/*
=item INTDIV ( a b -- a/b )

Pops two values from the data stack, and pushes back their integer quotient.

=cut
 */
int inst_INTDIV(vm_context_t *context) {
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

/*
=item INTMOD ( a b -- a%b )

Pops two values from the data stack, and pushes back their integer remainder.
 
=cut
 */
int inst_INTMOD(vm_context_t *context) {
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

/*
=item STRLIT ( -- s )

Reads a length followed by a string of length bytes from the following bytecode, and pushes the string value to the data stack.

If a zero byte occurs within the string, the resulting string will be terminated at this byte.  However, the full length will
always be consumed from the bytecode.

If length is zero, an empty string value will be pushed to the data stack, and no additional bytes will be consumed from the 
bytecode.
 
=cut
 */
int inst_STRLIT(struct vm_context_t *context) {
    const uint16_t len = *(uint16_t *) (&context->m_bytecode[context->m_counter + 1]);
    const char *str = (const char *) (&context->m_bytecode[context->m_counter + 1 + sizeof(len)]);
    
    anon_scalar_t s;
    anon_scalar_init(&s);
    
    if (len > 0) {
        char *buf = calloc(len + 1, sizeof(*buf));
        assert(buf != NULL);
        strncpy(buf, str, len);
        anon_scalar_set_string_value(&s, buf);
        free(buf);
    }
    else {
        anon_scalar_set_string_value(&s, "");
    }

    vm_ds_push(context, &s);
    anon_scalar_destroy(&s);
    
    return 1 + sizeof(len) + len;
}

/*
=item STRCAT ( a b -- ab )

Pops two strings from the stack, and pushes back their concatenation.
 
=cut
 */
int inst_STRCAT(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    char *str_a, *str_b, *str_c;

    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);

    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    
    anon_scalar_get_string_value(&a, &str_a);
    anon_scalar_get_string_value(&b, &str_b);
    
    str_c = calloc(1 + strlen(str_a) + strlen(str_b), sizeof(*str_c));
    assert(str_c != NULL);
    strcpy(str_c, str_a);
    strcat(str_c, str_b);
    free(str_a);
    free(str_b);
    
    anon_scalar_set_string_value(&c, str_c);
    free(str_c);
    
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;
}

/*
=item FLTLIT ( -- a )

Reads a floating point literal from the following bytecode and pushes it onto the data stack.
 
=cut
 */
int inst_FLTLIT(struct vm_context_t *context) {
    const floatptr_t lit = *(const floatptr_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    anon_scalar_t a;
    anon_scalar_init(&a);
    anon_scalar_set_float_value(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(lit);
}

/*
=item FLTADD ( a b -- a+b )

Pops two floating point values from the data stack and pushes back their sum

=cut
 */
int inst_FLTADD(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    
    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, anon_scalar_get_float_value(&a) + anon_scalar_get_float_value(&b));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;
}

/*
=item FLTSUBT ( a b -- a-b )

Pops two floating point values from the data stack and pushes back their difference.

=cut
 */
int inst_FLTSUBT(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    
    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, anon_scalar_get_float_value(&a) - anon_scalar_get_float_value(&b));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;    
}

/*
=item FLTMULT ( a b -- a*b )

Pops two floating point values from the data stack and pushes back their product.

=cut
 */
int inst_FLTMULT(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    
    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, anon_scalar_get_float_value(&a) * anon_scalar_get_float_value(&b));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;    
}

/*
=item FLTDIV ( a b -- a/b )

Pops two floating point values from the data stack and pushes back their quotient.

=cut
 */
int inst_FLTDIV(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    
    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, anon_scalar_get_float_value(&a) / anon_scalar_get_float_value(&b));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;    
}

/*
=item FLTMOD ( a b -- a%b )

Pops two floating point values from the data stack and pushes back their remainder.

=cut
 */
int inst_FLTMOD(struct vm_context_t *context) {
    anon_scalar_t a, b, c;
    
    anon_scalar_init(&a);
    anon_scalar_init(&b);
    anon_scalar_init(&c);
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, fmod(anon_scalar_get_float_value(&a), anon_scalar_get_float_value(&b)));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;    
}
