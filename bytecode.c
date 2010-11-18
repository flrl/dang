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
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "channel.h"
#include "debug.h"
#include "hash.h"
#include "scalar.h"
#include "stream.h"
#include "util.h"
#include "vm.h"

#include "bytecode.h"

#define NEXT_BYTE(x) (const uint8_t*)(&(x)->m_bytecode[(x)->m_counter + 1])

#define BYTECODE_NUMERIC_OP(type, op) do {                                          \
    scalar_t a = {0}, b = {0}, c = {0};                                             \
    vm_ds_pop(context, &b);                                                         \
    vm_ds_pop(context, &a);                                                         \
    anon_scalar_set_##type##_value(&c,                                              \
        anon_scalar_get_##type##_value(&a) op anon_scalar_get_##type##_value(&b));  \
    vm_ds_push(context, &c);                                                        \
    anon_scalar_destroy(&c);                                                        \
    anon_scalar_destroy(&b);                                                        \
    anon_scalar_destroy(&a);                                                        \
} while (0)


#define BYTECODE_LOGICAL_OP(op) do {                                                \
    scalar_t a = {0}, b = {0}, c = {0};                                             \
    vm_ds_pop(context, &b);                                                         \
    vm_ds_pop(context, &a);                                                         \
    anon_scalar_set_int_value(&c,                                                   \
        (anon_scalar_get_bool_value(&a) op anon_scalar_get_bool_value(&b)));        \
    vm_ds_push(context, &c);                                                        \
    anon_scalar_destroy(&c);                                                        \
    anon_scalar_destroy(&b);                                                        \
    anon_scalar_destroy(&a);                                                        \
} while (0)

/*
=item END ( -- )
 
Terminates execution of the current context (causing vm_execute to return).
 
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
=item CALL ( [params] -- ) ( -- addr )

Reads a jump destination from the following bytecode.  Pushes the location of the bytecode following the jump destination
to the return stack, starts a new symbol table scope, then transfers execution control to the jump destination.
 
=cut
 */
int inst_CALL(vm_context_t *context) {
    const function_handle_t jump_dest = *(const function_handle_t *) NEXT_BYTE(context);
    
    vm_start_scope(context);

    vm_rs_push(context, context->m_counter + 1 + sizeof(function_handle_t));
    return jump_dest - context->m_counter;
}

/*
=item CORO ( [n params] n -- ) ( -- )

Reads a jump destination from the following bytecode.  Spawns a new parallel execution context and starts executing from
the jump destination in the new execution context, meanwhile returning immediately in the calling thread.

Pops n and n parameters from the caller's stack and pushes them in the same order to the new context's stack.

=cut
 */
int inst_CORO(struct vm_context_t *context) {
    const function_handle_t jump_dest = *(const function_handle_t *) NEXT_BYTE(context);
    scalar_t *params = NULL;
    uintptr_t num_params = 0;
    
    scalar_t n = {0};
    vm_ds_pop(context, &n);    
    if (anon_scalar_get_int_value(&n) > 0) {
        num_params = anon_scalar_get_int_value(&n);
        if (NULL != (params = calloc(num_params, sizeof(*params)))) {
            vm_ds_npop(context, num_params, params);
        }
        else {
            debug("couldn't allocate space for %"PRIuPTR" parameters to coro\n", num_params);
            FIXME("what do do here?\n");
        }
    }
    anon_scalar_destroy(&n);
    
    vm_context_t *child_context;
    if (NULL != (child_context = calloc(1, sizeof(*child_context)))) {
        vm_context_init(child_context, context->m_bytecode, context->m_bytecode_length, jump_dest);
        
        if (params != NULL) {
            vm_ds_npush(child_context, num_params, params);
            for (uintptr_t i = 0; i < num_params; i++) {
                anon_scalar_destroy(&params[i]);
            }
            free(params);
            params = NULL;
        }
        
        pthread_t thread;
        int status;
        if (0 == (status = pthread_create(&thread, NULL, vm_execute, child_context))) {
            pthread_detach(thread);
        }
        else {
            debug("pthread_create failed with status %i\n", status);
            FIXME("what to do here?\n");
            vm_context_destroy(child_context);
            free(child_context);
        }
    }
    else {
        debug("couldn't allocate child context\n");
        FIXME("what to do here?\n");
    }
    
    return 1 + sizeof(jump_dest);
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
=item AND ( a b -- c )

Pops two values from the stack, and pushes back the result of a logical and of the two values.

=cut
 */
int inst_AND(struct vm_context_t *context) {
    BYTECODE_LOGICAL_OP(&&);
    return 1;
}

/*
=item OR ( a b -- c )

Pops two values from the stack, and pushes back the result of a logical or of the two values.

=cut
 */
int inst_OR(struct vm_context_t *context) {
    BYTECODE_LOGICAL_OP(||);
    return 1;
}

/*
=item XOR ( a b -- c )

Pops two values from the stack, and pushes back the result of a logical xor of the two values.

=cut
 */
int inst_XOR(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0}, c = {0};
    vm_ds_pop(context, &b);
    vm_ds_pop(context, &a);
    int a_val = anon_scalar_get_bool_value(&a);
    int b_val = anon_scalar_get_bool_value(&b);
    anon_scalar_set_int_value(&c, ((a_val || b_val) && (!(a_val && b_val))));
    vm_ds_push(context, &c);
    anon_scalar_destroy(&c);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item NOT ( a -- b )

Pops a value from the stack, and pushes back the result of a boolean not of the value.

=cut
 */
int inst_NOT(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, !anon_scalar_get_bool_value(&a));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item JMP ( -- )
 
Reads a jump destination from the following bytecode and transfers execution control to it.
 
=cut
 */
int inst_JMP(vm_context_t *context) {
    const intptr_t offset = *(const intptr_t *) NEXT_BYTE(context);
    return offset;
}

/*
=item JMP0 ( a -- )

Reads a jump destination from the following bytecode.  Pops a value from the data stack.  If the value popped is a zero
value (0, 0.0, "0", "", or undef), transfers execution control to the jump destination.  Otherwise, transfers execution 
control to the next instruction.
 
=cut
 */
int inst_JMP0(vm_context_t *context) {
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
        incr = 1 + sizeof(branch_offset);
    }
    
    anon_scalar_destroy(&a);
    return incr;
}

/*
=item JMPU ( a -- )

Reads a jump destination from the following bytecode.  Pops a value from the data stack.  If the value popped is undefined, 
transfers execution control to the jump destination.  Otherwise, transfers execution control to the next instruction.

=cut
*/
int inst_JMPU(struct vm_context_t *context) {
    const intptr_t branch_offset = *(const intptr_t *) NEXT_BYTE(context);
    int incr = 0;
    
    scalar_t a = {0};
    vm_ds_pop(context, &a);
    if (anon_scalar_is_defined(&a) == 0) {
        // branch by offset
        incr = branch_offset;
    }
    else {
        incr = 1 + sizeof(branch_offset);
    }
    
    anon_scalar_destroy(&a);
    return incr;
}


/*
=item SYMDEF ( a -- ref )

Reads flags and an identifier from the following bytecode.  Pops a scalar from the stack.  Defines a new symbol in the
current scope with the requested identifier as follows:

=over

=item 

If the scalar is undefined, the symbol will point to a new object, which is allocated based on the flags provided.

=item 

If the scalar is a reference type, the symbol will point to the referenced object.

=item 

Otherwise, the symbol will point to a new scalar, whose value will be set to the value of the popped scalar.

=back 

Pushes back a reference to the new symbol on success, or undef on failure.

=cut
 */
int inst_SYMDEF(struct vm_context_t *context) {
    const flags_t flags = *(const flags_t *) (&context->m_bytecode[context->m_counter + 1]);
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1 + sizeof(flags)]);
    
    scalar_t a = {0}, ref = {0};
    vm_ds_pop(context, &a);

    const symbol_t *symbol = NULL;
    
    // define an entry in the symbol table
    if ((a.m_flags & SCALAR_TYPE_MASK) == SCALAR_UNDEF) {
        symbol = symbol_define(context->m_symboltable, identifier, flags, 0);
    }
    else if ((a.m_flags & SCALAR_FLAG_REF)) {
        // getting the handle directly rather than requesting a reference to it is safe, because
        // a itself will continue to hold onto the reference and ensure it isn't deallocated
        // before we finish
        handle_t handle = a.m_value.as_scalar_handle;  // handle types are interchangeable
        flags_t flags = 0;
        switch (a.m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_SCAREF:     flags = SYMBOL_SCALAR;      break;
            case SCALAR_ARRREF:     flags = SYMBOL_ARRAY;       break;
            case SCALAR_HASHREF:    flags = SYMBOL_HASH;        break;
            case SCALAR_CHANREF:    flags = SYMBOL_CHANNEL;     break;
            case SCALAR_FUNCREF:    
                FIXME("make symbols able to contain function references\n");
                handle = flags = 0;
                break;
            default:
                debug("unhandled scalar reference type: %"PRIu32"\n", a.m_flags);
                handle = 0;
                break;
        }
        if (handle) {
            symbol = symbol_define(context->m_symboltable, identifier, flags, handle);
        }
        else {
            debug("failed to get a handle from a reference type\n");
        }
        // n.b. don't need to release handle here, cause it wasn't a proper reference in the first place.
    }
    else {
        symbol = symbol_define(context->m_symboltable, identifier, (flags & ~ SYMBOL_TYPE_MASK) | SYMBOL_SCALAR, 0);
        if (symbol)  scalar_set_value(symbol->m_referent, &a);
    }
    
    // set up ref to point to the new symbol (or undef if the symbol failed to be defined)
    if (symbol) {
        switch (symbol->m_flags & SYMBOL_TYPE_MASK) {
            case SYMBOL_SCALAR:
                anon_scalar_set_scalar_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_ARRAY:
                anon_scalar_set_array_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_HASH:
                anon_scalar_set_hash_reference(&ref, symbol->m_referent);
                break;
            //...
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
                break;
            default:
                debug("unhandled symbol type: %"PRIu32"\n", symbol->m_flags);
                break;
        }
    }
    else {
        debug("failed to define symbol\n");
    }
    
    // finish up
    vm_ds_push(context, &ref);
    
    anon_scalar_destroy(&ref);
    anon_scalar_destroy(&a);
    
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
                anon_scalar_set_scalar_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_ARRAY:
                anon_scalar_set_array_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_HASH:
                anon_scalar_set_hash_reference(&ref, symbol->m_referent);
                break;
            //...
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
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
=item SYMCLONE ( -- ref )

Reads an identifier from the following bytecode.  Makes the identifier available directly within the current scope.  Pushes
a reference to the symbol to the data stack.

=cut
 */
int inst_SYMCLONE(struct vm_context_t *context) {
    const identifier_t identifier = *(const identifier_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    const symbol_t *symbol = symbol_clone(context->m_symboltable, identifier);

    scalar_t ref = {0};
    
    if (symbol != NULL) {
        switch(symbol->m_flags & SYMBOL_TYPE_MASK) {
            case SYMBOL_SCALAR:
                anon_scalar_set_scalar_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_ARRAY:
                anon_scalar_set_array_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_HASH:
                anon_scalar_set_hash_reference(&ref, symbol->m_referent);
                break;
            //...
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
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
=item SRUNLOCK ( sr -- )

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
=item ARLEN ( ar -- n )

Pops an array reference from the stack.  Pushes back the number of items it contains.

=cut
*/
int inst_ARLEN(struct vm_context_t *context) {
    scalar_t ar = {0}, n = {0};
    
    vm_ds_pop(context, &ar);
    
    anon_scalar_set_int_value(&n, array_size(anon_scalar_deref_array_reference(&ar)));
    
    vm_ds_push(context, &n);
    
    anon_scalar_destroy(&n);
    anon_scalar_destroy(&ar);
    
    return 1;
}

/*
=item ARINDEX ( i ar -- sr )

Pops an array reference and an index from the data stack.  Pushes back a reference to the item in the array at index.

If the index is out of range, the array automatically grows to accomodate it.  

Negative indices are treated as relative to the end of the array, thus -1 is the last element.

=cut
 */
int inst_ARINDEX(struct vm_context_t *context) {
    scalar_t ar = {0}, i = {0}, sr = {0};
    
    vm_ds_pop(context, &ar);
    vm_ds_pop(context, &i);
    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);
    scalar_handle_t s = array_item_at(anon_scalar_deref_array_reference(&ar), anon_scalar_get_int_value(&i));
    anon_scalar_set_scalar_reference(&sr, s);
    scalar_release(s);
    vm_ds_push(context, &sr);
    
    anon_scalar_destroy(&sr);
    anon_scalar_destroy(&i);
    anon_scalar_destroy(&ar);
    
    return 1;
}

/*
=item ARSLICE ( [indices] count ar -- [references] count )

Pops an array reference, a count, and count indices from the data stack.  Pushes back references to the indexed items
in the array, followed by the count.  Negative indices are treated as relative to the end of the array.

Indexes that are larger than the current size of the array result in the array automatically growing to accomodate the 
index, and a reference to the new undefined scalar being pushed back.

If the indices contain both a negative index and an index greater than the current extent of the array, the results
are undefined.

=cut
*/
int inst_ARSLICE(struct vm_context_t *context) {
    scalar_t ar = {0}, count = {0};
    
    vm_ds_pop(context, &ar);
    vm_ds_pop(context, &count);

    intptr_t c = anon_scalar_get_int_value(&count);
    if (c > 0) {
        scalar_t *indices = calloc(c, sizeof(*indices));
        if (indices != NULL) {
            vm_ds_npop(context, c, indices);

            array_slice(ar.m_value.as_array_handle, indices, c);

            vm_ds_npush(context, c, indices);

            for (int i = 0; i < c; i++) {
                anon_scalar_destroy(&indices[i]);
            }
            
            free(indices);

            vm_ds_push(context, &count);
        }
        else {
            debug("calloc failed\n");
        }
    }
    else {
        anon_scalar_set_int_value(&count, 0);
        vm_ds_push(context, &count);
    }
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&ar);

    return 1;
}

/*
=item ARLIST ( ar -- [values] count )

Pops an array reference from the data stack.  Pushes back the value of each item in the array in order, followed by
the number of items in the array.

=cut
*/
int inst_ARLIST(struct vm_context_t *context) {
    scalar_t ar = {0}, *values = NULL, count = {0};
    size_t n = 0;
    
    vm_ds_pop(context, &ar);
    
    if (0 == array_list(ar.m_value.as_array_handle, &values, &n)) {
        if (n > 0)  vm_ds_npush(context, n, values);
        anon_scalar_set_int_value(&count, n);
    }

    vm_ds_push(context, &count);
    
    if (values != NULL) {
        for (size_t i = 0; i < n; i++) {
            anon_scalar_destroy(&values[i]);
        }
        free(values);
    }
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&ar);
    
    return 1;
}

/*
=item ARFILL ( [items] count ar -- )

Pops an array reference, a count, and count items from the data stack, and replaces the current contents of the 
array (if any) with the items.

To empty an array, provide a count of zero.

=cut
*/
int inst_ARFILL(struct vm_context_t *context) {
    scalar_t ar = {0}, count = {0}, *values = NULL;
    
    vm_ds_pop(context, &ar);
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);

    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        if (NULL != (values = calloc(n, sizeof(*values)))) {
            vm_ds_npop(context, n, values);
            array_fill(anon_scalar_deref_array_reference(&ar), values, n);
            free(values);
        }
        else {
            debug("calloc failed\n");
            return 0;
        }
    }
    else {
        array_fill(anon_scalar_deref_array_reference(&ar), NULL, 0);
    }
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&ar);

    return 1;
}

/*
=item ARPUSH ( [values] count ar -- )

Pops an array reference, a count, and count values from the data stack, and adds the values to the end of the array.

=cut
 */
int inst_ARPUSH(struct vm_context_t *context) {
    scalar_t *values = NULL, count = {0}, ar = {0};
    
    vm_ds_pop(context, &ar);    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);

    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        if (NULL != (values = calloc(n, sizeof(*values)))) {
            vm_ds_npop(context, n, values);

            array_push(anon_scalar_deref_array_reference(&ar), values, n);
            
            for (size_t i = 0; i < n; i++)  anon_scalar_destroy(&values[i]);
            free(values);
        }
        else {
            debug("couldn't allocated values\n");
        }
    }
    
    anon_scalar_destroy(&count);
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
=item HRLEN ( hr -- n )

Pops a hash reference from the stack.  Pushes back the number of items in the hash.

=cut
*/
int inst_HRLEN(struct vm_context_t *context) {
    scalar_t hr = {0}, n = {0};
    
    vm_ds_pop(context, &hr);
    
    anon_scalar_set_int_value(&n, hash_size(anon_scalar_deref_hash_reference(&hr)));
    
    vm_ds_push(context, &n);
    
    anon_scalar_destroy(&n);
    anon_scalar_destroy(&hr);
    
    return 1;
}

/*
=item HRINDEX ( k hr -- sr )

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
=item HRSLICE ( [keys] count hr -- [references] count )

Pops a hash reference, a count, and a list of keys from the data stack.  Pushes back references to the values for
the keys and the number of items returned.

If any keys are not currently defined in the hash, they are automatically created and their values set to undefined.

=cut
*/
int inst_HRSLICE(struct vm_context_t *context) {
    scalar_t *elements = NULL, count = {0}, hr = {0};
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);
    
    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        if (NULL != (elements = calloc(n, sizeof(*elements)))) {
            vm_ds_npop(context, n, elements);
            hash_slice(anon_scalar_deref_hash_reference(&hr), elements, n);
            vm_ds_npush(context, n, elements);
            free(elements);
        }
        else {
            debug("calloc failed: %i\n", errno);
            anon_scalar_set_int_value(&count, 0);
        }
    }
    else {
        anon_scalar_set_int_value(&count, 0);
    }

    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRLISTK ( hr -- [keys] count )

Pops a hash reference from the data stack.  Pushes back a list of the keys defined in it, and their count.

=cut
*/
int inst_HRLISTK(struct vm_context_t *context) {
    scalar_t hr = {0}, *keys = NULL, count = {0};
    size_t n;
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);

    if (0 == hash_list_keys(anon_scalar_deref_hash_reference(&hr), &keys, &n)) {
        if (n > 0) {
            vm_ds_npush(context, n, keys);
            free(keys);
            anon_scalar_set_int_value(&count, n);
        }
        else {
            anon_scalar_set_int_value(&count, 0);
        }
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRLISTV ( hr -- [values] count )

Pops a hash reference from the data stack.  Pushes back a list of the values defined in it, and their count.

=cut
*/
int inst_HRLISTV(struct vm_context_t *context) {
    scalar_t hr = {0}, *values = NULL, count = {0};
    size_t n;
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);

    if (0 == hash_list_values(anon_scalar_deref_hash_reference(&hr), &values, &n)) {
        if (n > 0) {
            vm_ds_npush(context, n, values);
            free(values);
            anon_scalar_set_int_value(&count, n);
        }
        else {
            anon_scalar_set_int_value(&count, 0);
        }
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRLISTP ( hr -- [value key] count )

Pops a hash reference from the data stack.  Pushes back a list of the key and value pairs defined in it, and the 
count of pairs.

=cut
*/
int inst_HRLISTP(struct vm_context_t *context) {
    scalar_t hr = {0}, *pairs = NULL, count = {0};
    size_t n;
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);

    if (0 == hash_list_pairs(anon_scalar_deref_hash_reference(&hr), &pairs, &n)) {
        if (n > 0) {
            vm_ds_npush(context, 2 * n, pairs);
            free(pairs);
            anon_scalar_set_int_value(&count, 2 * n);
        }
        else {
            anon_scalar_set_int_value(&count, 0);
        }
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRFILL ( [value key] count hr -- )

Pops a hash reference, a count, and count key/value pairs from the data stack, and replaces the current contents of 
the hash (if any) with the pairs.

To empty a hash, provide a count of zero.

=cut
*/
int inst_HRFILL(struct vm_context_t *context) {
    FIXME("write this\n");
    return 0;
}


/*
=item HRKEYEX ( k hr -- b )

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
=item HRKEYDEL ( k hr -- )

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
=item CRTRYRD ( ref -- a )

Pops a channel reference from the data stack, and tries to read a value from it without blocking.  If there
is a value available to read, pushes back the value read, or pushes back undef if the read operation would
block.

Note that it is not possible to distinguish between successfully reading an undefined value and failing to
read anything.

=cut
 */
int inst_CRTRYRD(struct vm_context_t *context) {
    scalar_t cr = {0}, a = {0};
    
    vm_ds_pop(context, &cr);
    
    assert((cr.m_flags & SCALAR_TYPE_MASK) == SCALAR_CHANREF);
    channel_tryread(anon_scalar_deref_channel_reference(&cr), &a);
    
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&cr);
    
    return 1;
}

/*
=item CRREAD ( ref -- a )

Pops a channel reference from the data stack, reads a value from it, and pushes back the value read.
If there is no value ready to be read, blocks the calling thread until one becomes available.

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
=item FRCALL ( [params] fr -- [results] )  ( -- addr )

Pops a function reference from the data stack.  Pushes the location of the following instruction to the return stack, 
starts a new symbol table scope, then transfers execution control to the destination reference by the function reference.

=cut
 */
int inst_FRCALL(struct vm_context_t *context) {
    scalar_t fr = {0};
    vm_ds_pop(context, &fr);
    function_handle_t jump_dest = anon_scalar_deref_function_reference(&fr);
    anon_scalar_destroy(&fr);
    
    vm_rs_push(context, context->m_counter + 1);
    
    vm_start_scope(context);
    
    return jump_dest - context->m_counter;
}

/*
=item FRCORO ( [n params] n fr -- ) ( -- )

Pops a function reference from the data stack.  Spawns a new parallel execution context and starts executing from
the function reference in the new execution context, meanwhile returning immediately in the calling thread.

Pops n and n parameters from the caller's stack and pushes them in the same order to the new context's stack.

=cut
 */
int inst_FRCORO(struct vm_context_t *context) {
    function_handle_t jump_dest;
    scalar_t *params = NULL;
    uintptr_t num_params = 0;
    
    scalar_t fr = {0};
    vm_ds_pop(context, &fr);
    assert((fr.m_flags & SCALAR_TYPE_MASK) == SCALAR_FUNCREF);
    jump_dest = anon_scalar_deref_function_reference(&fr);
    anon_scalar_destroy(&fr);
    
    scalar_t n = {0};
    vm_ds_pop(context, &n);    
    if (anon_scalar_get_int_value(&n) > 0) {
        num_params = anon_scalar_get_int_value(&n);
        if (NULL != (params = calloc(num_params, sizeof(*params)))) {
            vm_ds_npop(context, num_params, params);
        }
        else {
            debug("couldn't allocate space for %"PRIuPTR" parameters to coro\n", num_params);
            FIXME("what do do here?\n");
        }
    }
    anon_scalar_destroy(&n);
    
    vm_context_t *child_context;
    if (NULL != (child_context = calloc(1, sizeof(*child_context)))) {
        vm_context_init(child_context, context->m_bytecode, context->m_bytecode_length, jump_dest);

        if (params != NULL) {
            vm_ds_npush(child_context, num_params, params);
            for (uintptr_t i = 0; i < num_params; i++) {
                anon_scalar_destroy(&params[i]);
            }
            free(params);
            params = NULL;
        }
        
        pthread_t thread;
        int status;
        if (0 == (status = pthread_create(&thread, NULL, vm_execute, child_context))) {
            pthread_detach(thread);
        }
        else {
            debug("pthread_create failed with status %i\n", status);
            FIXME("what to do here?\n");
            vm_context_destroy(child_context);
            free(child_context);
        }
    }
    else {
        FIXME("what to do here?\n");
    }
    
    return 1;
}

/*
=item BYTE ( -- a )

Reads the next byte in the bytecode and pushes its integer value onto the data stack.  Transfers execution control to the
bytecode following it.

=cut
*/
int inst_BYTE(vm_context_t *context) {
    const uint8_t byte = *(const uint8_t *) NEXT_BYTE(context);
    
    scalar_t a = {0};
    anon_scalar_set_int_value(&a, byte);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(byte);
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
=item INTLT0 ( a -- b )

Pops a value, and pushes back 1 if the value is less than zero, or 0 otherwise.

=cut
 */
int inst_INTLT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_int_value(&a) < 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item INTGT0 ( a -- b )

Pops a value, and pushes back 1 if the value is greater than zero, or 0 otherwise.

=cut
 */
int inst_INTGT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_int_value(&a) > 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item INTINCR ( a -- b )

Pops a value, and pushes back the same value incremented by 1.

=cut
 */
int inst_INTINCR(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, anon_scalar_get_int_value(&a) + 1);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item INTDECR ( a -- b )

Pops a value, and pushes back the same value decremented by 1.

=cut
 */
int inst_INTDECR(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, anon_scalar_get_int_value(&a) - 1);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
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
=item FLTLT0 ( a -- b )

Pops a floating point value and pushes back 1 if it is less than zero, or 0 otherwise.

=cut
 */
int inst_FLTLT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_float_value(&a) < 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item FLTGT0 ( a -- b )

Pops a floating point value and pushes back 1 if it is greater than zero, or 0 otherwise.

=cut
 */
int inst_FLTGT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_float_value(&a) > 0));
    vm_ds_push(context, &b);
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

/*
=item OUT ( a -- )

Pops a scalar from the stack and prints out its value.

=cut
 */
int inst_OUT(struct vm_context_t *context) {
    scalar_t a = {0};
    char *str;
    
    vm_ds_pop(context, &a);
    anon_scalar_get_string_value(&a, &str);
    printf("%s", str);

    free(str);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item OUTL ( a -- )

Pops a scalar from the stack and prints out its value, followed by a newline.

=cut
 */
int inst_OUTL(struct vm_context_t *context) {
    scalar_t a = {0};
    char *str;
    
    vm_ds_pop(context, &a);
    anon_scalar_get_string_value(&a, &str);
    printf("%s\n", str);

    free(str);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item IN ( -- a )

Reads a scalar from the standard input channel, up until the next newline character, and pushes it to the stack.

=cut
 */
int inst_IN(struct vm_context_t *context) {
    scalar_t a = {0};

    char *buf = NULL;
    size_t bufsize = 0;

    if (getline(&buf, &bufsize, stdin) > 0) {
        anon_scalar_set_string_value(&a, buf);
    }

    if (buf != NULL)  free(buf);

    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item UNDEF ( -- a )

Pushes an undefined value to the data stack

=cut
 */
int inst_UNDEF(struct vm_context_t *context) {
    scalar_t a = {0};
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item STDIN ( -- stream )

Pushes a reference to the stdin stream to the data stack.

=cut
*/
int inst_STDIN(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    stream_handle_t handle = stream_stdin_handle();
    anon_scalar_set_stream_reference(&stream, handle);
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    stream_release(handle);
    return 1;
}

/*
=item STDOUT ( -- stream )

Pushes a reference to the stdout stream to the data stack.

=cut
*/
int inst_STDOUT(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    stream_handle_t handle = stream_stdout_handle();
    anon_scalar_set_stream_reference(&stream, handle);
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    stream_release(handle);
    return 1;
}

/*
=item STDERR ( -- stream )

Pushes a reference to the stderr stream to the data stack.

=cut
*/
int inst_STDERR(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    stream_handle_t handle = stream_stderr_handle();
    anon_scalar_set_stream_reference(&stream, handle);
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    stream_release(handle);
    return 1;
}


/*
=back

=cut
 */
