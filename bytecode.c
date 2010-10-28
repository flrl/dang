/*
 *  bytecode.c
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 NAME

bytecode

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut 
 */

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "debug.h"
#include "scalar.h"
#include "vm.h"

#define NEXT_BYTE(x) (const uint8_t*)(&(x)->m_bytecode[(x)->m_counter + 1])

#define BYTECODE_NUMERIC_OP(type, op) do {                                          \
    scalar_t a = {0}, b = {0}, c = {0};                                             \
    vm_ds_pop(context, &b);                                                         \
    vm_ds_pop(context, &b);                                                         \
    anon_scalar_set_##type##_value(&c,                                              \
        anon_scalar_get_##type##_value(&a) op anon_scalar_get_##type##_value(&b));  \
    vm_ds_push(context, &c);                                                        \
    anon_scalar_destroy(&c);                                                        \
    anon_scalar_destroy(&b);                                                        \
    anon_scalar_destroy(&a);                                                        \
} while (0)


/*
=item END ( -- )
 
Indicates the end of the bytecode stream.
 
=cut
 */
int inst_END(vm_context_t *context) {
    // dummy function for lookup table -- should not be executed!
    assert("should not get here" == 0);
    return 0;
}

/*
=item NOOP ( -- )

Does nothing.  Used to pad the bytecode when alignment is important.

=cut
 */
int inst_NOOP(vm_context_t *context) {
    // dummy function for lookup table -- should not be executed!
    assert("should not get here" == 0);
    return 0;
}

/*
=item CALL ( [params] fr -- ) ( -- addr )

Pops a function reference from the data stack.  Pushes the instruction to be jumped to upon return to the return stack, 
starts a new symbol table scope, then transfers execution control to the instruction referenced by the function reference.
 
=cut
 */
int inst_CALL(vm_context_t *context) {
    scalar_t fr = {0};
    vm_ds_pop(context, &fr);
    function_handle_t jump_dest = anon_scalar_deref_function_reference(&fr);
    anon_scalar_destroy(&fr);
    
    vm_rs_push(context, context->m_counter + 1);
    
    vm_start_scope(context);
    
    return jump_dest - context->m_counter;
}

/*
=item RETURN ( -- [results] ) ( addr -- )

Ends the current symbol table scope.  Pops a return destination from the return stack, and transfers execution control to it.

=cut
 */
int inst_RETURN(vm_context_t *context) {
    function_handle_t jump_dest;
    
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
    vm_ds_swap(context);
    return 1;
}

/*
=item DUP ( a -- a a )

Duplicates the item on the top of the data stack
 
=cut
 */
int inst_DUP(vm_context_t *context) {
    vm_ds_dup(context);
    return 1;
}

/*
=item OVER ( a b -- a b a )

Duplicates the next from top item on the data stack

=cut
 */
int inst_OVER(vm_context_t *context) {
    vm_ds_over(context);
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

Reads a jump destination from the following bytecode.  Pops a value from the data stack.  If the value popped is false, transfers
execution control to the jump destination.  Otherwise, transfers execution control to the next instruction.
 
=cut
 */
int inst_0BRANCH(vm_context_t *context) {
    const intptr_t branch_offset = *(const intptr_t *) NEXT_BYTE(context);
    int incr = 0;

    scalar_t a = {0};
    vm_ds_pop(context, &a);

    if (anon_scalar_get_bool_value(&a) == 0) {
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
    
    symbol_define(context->m_symboltable, identifier, flags);
    
    return 1 + sizeof(flags) + sizeof(identifier);
}

/*
=item SYMFIND ( -- ref )

Reads an identifier from the following bytecode and looks it up in the symbol table.

Pushes a reference to the symbol to the data stack if found, or undef if not found.
 
=cut
 */
int inst_SYMFIND(struct vm_context_t *context) {
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    const symbol_t *symbol = symbol_lookup(context->m_symboltable, identifier);

    scalar_t ref = {0};
    
    if (symbol != NULL) {
        switch(symbol->m_flags & SYMBOL_TYPE_MASK) {
            case SYMBOL_SCALAR:
                anon_scalar_set_scalar_reference(&ref, symbol->m_referent.as_scalar);
                break;
            case SYMBOL_ARRAY:
                anon_scalar_set_array_reference(&ref, symbol->m_referent.as_array);
                break;
            case SYMBOL_HASH:
                anon_scalar_set_hash_reference(&ref, symbol->m_referent.as_hash);
                break;
            //...
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent.as_channel);
                break;
            default:
                debug("unhandled symbol type: %"PRIu32"\n", symbol->m_flags);
                break;
        }
    }
    
    vm_ds_push(context, &ref);
    anon_scalar_destroy(&ref);
    
    return 1 + sizeof(identifier);
}

/*
=item SYMUNDEF ( -- )
 
Reads an identifier from the following bytecode.  Removes the identifier from the symbol table in the current scope.

=cut
 */
int inst_SYMUNDEF(struct vm_context_t *context) {
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    symbol_undefine(context->m_symboltable, identifier);
    
    return 1 + sizeof(identifier);
}

/*
=item SRLOCK ( sr -- sr )

Pops a scalar reference from the data stack and arranges for the referenced scalar to be locked if it is shared.

=cut
 */
int inst_SRLOCK(struct vm_context_t *context) {
    scalar_t sr = {0};
    
    vm_ds_top(context, &sr);
    scalar_lock(anon_scalar_deref_scalar_reference(&sr));
    
    anon_scalar_destroy(&sr);
    return 1;
}

/*
item SRUNLOCK ( sr -- )

Pops a scalar reference from the data stack and arranges for the referenced scalar to be unlocked if it is shared.

=cut
 */
int inst_SRUNLOCK(struct vm_context_t *context) {
    scalar_t sr = {0};
    
    vm_ds_pop(context, &sr);
    scalar_unlock(anon_scalar_deref_scalar_reference(&sr));
    
    anon_scalar_destroy(&sr);
    return 1;
}

/*
=item SRREAD ( ref -- a )

Pops a scalar reference from the data stack.  Pushes the value of the referenced scalar.

=cut
 */
int inst_SRREAD(struct vm_context_t *context) {
    scalar_t ref = {0}, a = {0};

    vm_ds_pop(context, &ref);
    scalar_get_value(anon_scalar_deref_scalar_reference(&ref), &a);
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item SRWRITE ( a ref -- )

Pops a scalar reference and a scalar value from the data stack.  Stores the value in the scalar referenced by the reference.
 
=cut
 */
int inst_SRWRITE(struct vm_context_t *context) {
    scalar_t ref = {0}, a = {0};
    
    vm_ds_pop(context, &ref);
    vm_ds_pop(context, &a);

    scalar_set_value(anon_scalar_deref_scalar_reference(&ref), &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item ARINDEX ( i ar -- sr )

Pops an array reference and an index from the data stack.  Pushes back a reference to the item in the array at index.

If the index is out of range, the array automatically grows to accomodate it.

=cut
 */
int inst_ARINDEX(struct vm_context_t *context) {
    scalar_t ar = {0}, i = {0}, sr = {0};
    
    vm_ds_pop(context, &ar);
    vm_ds_pop(context, &i);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    scalar_handle_t s = array_item_at(anon_scalar_deref_array_reference(&ar), (size_t) anon_scalar_get_int_value(&i));
    anon_scalar_set_scalar_reference(&sr, s);
    scalar_release(s);
    vm_ds_push(context, &sr);
    
    anon_scalar_destroy(&sr);
    anon_scalar_destroy(&i);
    anon_scalar_destroy(&ar);
    
    return 1;
}

/*
=item ARPUSH ( a ar -- )

Pops an array reference and a scalar value from the data stack, and adds the scalar value to the end of the array.

=cut
 */
int inst_ARPUSH(struct vm_context_t *context) {
    scalar_t a = {0}, ar = {0};
    
    vm_ds_pop(context, &ar);
    vm_ds_pop(context, &a);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    array_push(ar.m_value.as_array_handle, &a);
    
    anon_scalar_destroy(&ar);
    anon_scalar_destroy(&a);
    
    return 1;
}

/*
=item ARPOP ( ar -- a )

Pops an array reference from the data stack.  Pops the last item off the referenced array, and pushes its value back to the
data stack.

=cut
 */
int inst_ARPOP(struct vm_context_t *context) {
    scalar_t ar = {0}, a = {0};
    
    vm_ds_pop(context, &ar);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    array_pop(ar.m_value.as_array_handle, &a);
    
    vm_ds_push(context, &a);

    anon_scalar_destroy(&a);
    anon_scalar_destroy(&ar);
    
    return 1;
}

/*
=item ARSHFT ( ar -- a )

Pops an array reference from the data stack.  Shifts the first item off the referenced array, and pushes its value back to the
data stack.

=cut
 */
int inst_ARSHFT(struct vm_context_t *context) {
    scalar_t ar = {0}, a = {0};
    
    vm_ds_pop(context, &ar);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    array_shift(ar.m_value.as_array_handle, &a);
    
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&ar);
    
    return 1;    
}

/*
=item ARUNSHFT ( a ar -- )

Pops an array reference and a scalar value from the data stack, and adds the scalar value to the start of the array.

=cut
 */
int inst_ARUNSHFT(struct vm_context_t *context) {
    scalar_t a = {0}, ar = {0};
    
    vm_ds_pop(context, &ar);
    vm_ds_pop(context, &a);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    array_unshift(ar.m_value.as_array_handle, &a);
    
    anon_scalar_destroy(&ar);
    anon_scalar_destroy(&a);
    
    return 1;    
}

/*
=item HRINDEX ( k hr - sr )

Pops a hash reference and a key from the data stack, and pushes a reference to the value for that key.

If the key does not exist, it is automatically created and its value set to undefined.

=cut
 */
int inst_HRINDEX(struct vm_context_t *context) {
    scalar_t hr = {0}, k = {0}, sr = {0};
    
    vm_ds_pop(context, &hr);
    vm_ds_pop(context, &k);
    
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);
    scalar_handle_t s = hash_key_item(anon_scalar_deref_hash_reference(&hr), &k);
    anon_scalar_set_scalar_reference(&sr, s);
    scalar_release(s);
    
    vm_ds_push(context, &sr);
    
    anon_scalar_destroy(&sr);
    anon_scalar_destroy(&k);
    anon_scalar_destroy(&hr);
    
    return 1;
}

/*
=item HRKEYEX ( k hr - b )

Pops a hash reference and a key from the data stack.  If the key exists in the hash, pushes back the value 1.
If it does not, pushes back the value 0.

=cut
 */
int inst_HRKEYEX(struct vm_context_t *context) {
    scalar_t hr = {0}, k = {0}, b = {0};
    
    vm_ds_pop(context, &hr);
    vm_ds_pop(context, &k);
    
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);
    anon_scalar_set_int_value(&b, hash_key_exists(anon_scalar_deref_hash_reference(&hr), &k));
    
    vm_ds_push(context, &b);
    
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&k);
    anon_scalar_destroy(&hr);
    
    return 1;
}

/*
=item HRKEYDEL ( k hr - )

Pops a hash reference and a key from the data stack.  Deletes the key from the hash.

=cut
 */
int inst_HRKEYDEL(struct vm_context_t *context) {
    scalar_t hr = {0}, k = {0};
    
    vm_ds_pop(context, &hr);
    vm_ds_pop(context, &k);
    
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);
    hash_key_delete(anon_scalar_deref_hash_reference(&hr), &k);
    
    anon_scalar_destroy(&hr);
    anon_scalar_destroy(&k);
    
    return 1;
}


/*
=item CRREAD ( ref -- a )

Pops a channel reference from the data stack, reads a value from it, and pushes back the value read.

=cut
 */
int inst_CRREAD(struct vm_context_t *context) {
    scalar_t ref = {0}, a = {0};
    
    vm_ds_pop(context, &ref);
    
    assert((ref.m_flags & SCALAR_TYPE_MASK) == SCALAR_CHANREF);
    channel_read(anon_scalar_deref_channel_reference(&ref), &a);
    
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&ref);
    return 1;
}

/*
=item CRWRITE ( a ref -- )

Pops a channel reference and a value from the data stack, and writes the value to the channel.

=cut
 */
int inst_CRWRITE(struct vm_context_t *context) {
    scalar_t a = {0}, ref = {0};
    
    vm_ds_pop(context, &ref);
    vm_ds_pop(context, &a);
    
    assert((ref.m_flags & SCALAR_TYPE_MASK) == SCALAR_CHANREF);
    channel_write(anon_scalar_deref_channel_reference(&ref), &a);
    
    anon_scalar_destroy(&ref);
    anon_scalar_destroy(&a);
    
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
    
    scalar_t a = {0};
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
    BYTECODE_NUMERIC_OP(int, +);
    return 1;
}

/*
=item INTSUBT ( a b -- a-b )

Pops two values from the data stack, and pushes back their integer difference.

=cut
 */
int inst_INTSUBT(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, -);
    return 1;
}

/*
=item INTMULT ( a b -- a*b )

Pops two values from the data stack, and pushes back their integer product.

=cut
 */
int inst_INTMULT(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, *);
    return 1;
}

/*
=item INTDIV ( a b -- a/b )

Pops two values from the data stack, and pushes back their integer quotient.

=cut
 */
int inst_INTDIV(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, /);
    return 1;
}

/*
=item INTMOD ( a b -- a%b )

Pops two values from the data stack, and pushes back their integer remainder.
 
=cut
 */
int inst_INTMOD(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, %);
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
    
    scalar_t s = {0};
    
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
    scalar_t a = {0}, b = {0}, c = {0};
    char *str_a, *str_b, *str_c;

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
    
    scalar_t a = {0};
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
    BYTECODE_NUMERIC_OP(float, +);
    return 1;
}

/*
=item FLTSUBT ( a b -- a-b )

Pops two floating point values from the data stack and pushes back their difference.

=cut
 */
int inst_FLTSUBT(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, -);
    return 1;    
}

/*
=item FLTMULT ( a b -- a*b )

Pops two floating point values from the data stack and pushes back their product.

=cut
 */
int inst_FLTMULT(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, *);
    return 1;    
}

/*
=item FLTDIV ( a b -- a/b )

Pops two floating point values from the data stack and pushes back their quotient.

=cut
 */
int inst_FLTDIV(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, /);
    return 1;    
}

/*
=item FLTMOD ( a b -- a%b )

Pops two floating point values from the data stack and pushes back their remainder.

=cut
 */
int inst_FLTMOD(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0}, c = {0};
    
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    anon_scalar_set_float_value(&c, fmod(anon_scalar_get_float_value(&a), anon_scalar_get_float_value(&b)));
    vm_ds_push(context, &c);
    
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    
    return 1;    
}

/*
=item FUNLIT ( -- fr )

Reads a function reference literal from the following bytecode and pushes it onto the data stack.

=cut
 */
int inst_FUNLIT(struct vm_context_t *context) {
    const function_handle_t lit = *(const function_handle_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    scalar_t a = {0};
    anon_scalar_set_function_reference(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(lit);
}
