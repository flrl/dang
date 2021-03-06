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
    
    vm_state_t ret_state = {0};
    vm_state_init(&ret_state, context->m_counter + 1 + sizeof(function_handle_t), context->m_flags, context->m_symboltable);
    vm_rs_push(context, &ret_state);
    vm_state_destroy(&ret_state);

    vm_start_scope(context);
    
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
    vm_state_t ret_state = {0};
    function_handle_t jump_dest;
    symboltable_t *symboltable_top;

    vm_rs_pop(context, &ret_state);
    jump_dest = ret_state.m_position;
    context->m_flags = ret_state.m_flags;
    symboltable_top = ret_state.m_symboltable_top;
    vm_state_destroy(&ret_state);

    while (context->m_symboltable != NULL && context->m_symboltable != symboltable_top) {
        vm_end_scope(context);
    }
    
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
=item ROT ( [xn .. x2 x1] n -- [x1 xn .. x2] )

Rotates n items on the stack such that the top item is moved to the bottom.

=cut
*/
int inst_ROT(struct vm_context_t *context) {
    scalar_t count = {0}, top = {0}, *items = NULL;
    
    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 1) {
        if (NULL != (items = calloc(n - 1, sizeof(*items)))) {
            vm_ds_pop(context, &top);
            vm_ds_npop(context, n - 1, items);

            vm_ds_push(context, &top);
            vm_ds_npush(context, n - 1, items);

            anon_scalar_destroy(&top);
        
            for (size_t i = 0; i < n - 1; i++)  anon_scalar_destroy(&items[i]);
            free(items);
        }
        else {
            debug("calloc failed\n");
        }
    }
    
    anon_scalar_destroy(&count);

    return 1;
}

/*
=item TOR ( [xn .. x2 x1] n -- [.. x2 x1 xn] )

Rotates n items on the stack such that the bottom is moved to the top.

=cut
*/
int inst_TOR(struct vm_context_t *context) {
    scalar_t count = {0}, bottom = {0}, *items = NULL;
    
    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 1) {
        if (NULL != (items = calloc(n - 1, sizeof(*items)))) {
            vm_ds_npop(context, n - 1, items);
            vm_ds_pop(context, &bottom);
            
            vm_ds_npush(context, n - 1, items);
            vm_ds_push(context, &bottom);
            
            anon_scalar_destroy(&bottom);
        
            for (size_t i = 0; i < n - 1; i++)  anon_scalar_destroy(&items[i]);
            free(items);
        }
        else {
            debug("calloc failed\n");
        }
    }
    
    anon_scalar_destroy(&count);
    
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
=item SCALAR ( -- ref )

Defines a new empty scalar and places a reference to it on the stack.

=cut
*/
int inst_SCALAR(struct vm_context_t *context) {
    scalar_t ref = {0};
    
    scalar_handle_t handle = scalar_allocate(0);  FIXME("handle flags\n");
    anon_scalar_set_scalar_reference(&ref, handle);
    
    vm_ds_push(context, &ref);
    
    scalar_release(handle);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item ARRAY ( -- ref )

Defines a new empty array and places a reference to it on the stack.

=cut
*/
int inst_ARRAY(struct vm_context_t *context) {
    scalar_t ref = {0};
    
    array_handle_t handle = array_allocate(0);  FIXME("handle flags\n");
    anon_scalar_set_array_reference(&ref, handle);
    
    vm_ds_push(context, &ref);
    
    array_release(handle);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item HASH ( -- ref )

Defines a new empty hash and places a reference to it on the stack.

=cut
*/
int inst_HASH(struct vm_context_t *context) {
    scalar_t ref = {0};
    
    hash_handle_t handle = hash_allocate();
    anon_scalar_set_hash_reference(&ref, handle);
    
    vm_ds_push(context, &ref);
    
    hash_release(handle);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item CHANNEL ( -- ref )

Defines a new channel and places a reference to it on the stack.

=cut
*/
int inst_CHANNEL(struct vm_context_t *context) {
    scalar_t ref = {0};
    
    channel_handle_t handle = channel_allocate();
    anon_scalar_set_channel_reference(&ref, handle);
    
    vm_ds_push(context, &ref);
    
    channel_release(handle);
    anon_scalar_destroy(&ref);
    
    return 1;
}

/*
=item STREAM ( -- ref )

Defines a new stream and places a reference to it on the stack.

=cut
*/
int inst_STREAM(struct vm_context_t *context) {
    scalar_t ref = {0};
    
    stream_handle_t handle = stream_allocate();
    anon_scalar_set_stream_reference(&ref, handle);
    
    vm_ds_push(context, &ref);
    
    stream_release(handle);
    anon_scalar_destroy(&ref);
    
    return 1;
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
    const flags32_t flags = *(const flags32_t *) (&context->m_bytecode[context->m_counter + 1]);
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
        flags32_t flags = 0;
        switch (a.m_flags & SCALAR_TYPE_MASK) {
            case SCALAR_SCAREF:     flags = SYMBOL_SCALAR;      break;
            case SCALAR_ARRREF:     flags = SYMBOL_ARRAY;       break;
            case SCALAR_HASHREF:    flags = SYMBOL_HASH;        break;
            case SCALAR_CHANREF:    flags = SYMBOL_CHANNEL;     break;
            case SCALAR_FUNCREF:    flags = SYMBOL_FUNCTION;    break;
            case SCALAR_STRMREF:    flags = SYMBOL_STREAM;      break;
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
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_FUNCTION:
                anon_scalar_set_function_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_STREAM:
                anon_scalar_set_stream_reference(&ref, symbol->m_referent);
                break;
            //...
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
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_FUNCTION:
                anon_scalar_set_function_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_STREAM:
                anon_scalar_set_stream_reference(&ref, symbol->m_referent);
                break;
            //...
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
            case SYMBOL_CHANNEL:
                anon_scalar_set_channel_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_FUNCTION:
                anon_scalar_set_function_reference(&ref, symbol->m_referent);
                break;
            case SYMBOL_STREAM:
                anon_scalar_set_stream_reference(&ref, symbol->m_referent);
                break;
            //...
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
            for (size_t i = 0; i < n; i++)  anon_scalar_destroy(&values[i]);
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
=item ARUNSHFT ( [values] count ar -- )

Pops an array reference, a count, and count values from the data stack, and adds the values to the start of the array.

=cut
 */
int inst_ARUNSHFT(struct vm_context_t *context) {
    scalar_t *values = NULL, count = {0}, ar = {0};
    
    vm_ds_pop(context, &ar);    
    assert((ar.m_flags & SCALAR_TYPE_MASK) == SCALAR_ARRREF);

    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        if (NULL != (values = calloc(n, sizeof(*values)))) {
            vm_ds_npop(context, n, values);

            array_unshift(anon_scalar_deref_array_reference(&ar), values, n);
            
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
        }
        anon_scalar_set_int_value(&count, n);
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
        }
        anon_scalar_set_int_value(&count, n);
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRLISTP ( hr -- [value key] count )

Pops a hash reference from the data stack.  Pushes back a list of the key and value pairs defined in it, and the 
count of items (i.e. an even number).

=cut
*/
int inst_HRLISTP(struct vm_context_t *context) {
    scalar_t hr = {0}, *pairs = NULL, count = {0};
    size_t n;
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);

    if (0 == hash_list_pairs(anon_scalar_deref_hash_reference(&hr), &pairs, &n)) {
        if (n > 0) {
            vm_ds_npush(context, n, pairs);
            for (size_t i = 0; i < n; i++)  anon_scalar_destroy(&pairs[i]);
            free(pairs);
        }
        anon_scalar_set_int_value(&count, n);
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
}

/*
=item HRFILL ( [valuen keyn ... value2 key2 value1 key1] count hr -- )

Pops a hash reference, a count, and count items from the data stack, and replaces the current contents of 
the hash (if any) with the items.  The count must be an even number.

To empty a hash, provide a count of zero.

=cut
*/
int inst_HRFILL(struct vm_context_t *context) {
    scalar_t hr = {0}, count = {0}, *pairs = NULL;
    
    vm_ds_pop(context, &hr);
    assert((hr.m_flags & SCALAR_TYPE_MASK) == SCALAR_HASHREF);

    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        if (n & 1) {
            debug("odd count\n");
            --n;
        }
        
        if (NULL != (pairs = calloc(n, sizeof(*pairs)))) {
            vm_ds_npop(context, n, pairs);
            hash_fill(anon_scalar_deref_hash_reference(&hr), pairs, n);
            for (size_t i = 0; i < n; i++)  anon_scalar_destroy(&pairs[i]);
            free(pairs);
        }
        else {
            debug("calloc failed\n");
            return 0;
        }
    }
    else {
        hash_fill(anon_scalar_deref_hash_reference(&hr), NULL, 0);
    }
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&hr);

    return 1;
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

    vm_state_t ret_state = {0};
    vm_state_init(&ret_state, context->m_counter + 1, context->m_flags, context->m_symboltable);
    vm_rs_push(context, &ret_state);
    vm_state_destroy(&ret_state);
    
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
=item INT ( -- a ) 

Reads an integer value from the following bytecode and pushes it onto the data stack.  Transfers execution control to the 
bytecode following the integer value.

=cut
*/
int inst_INT(vm_context_t *context) {
    const intptr_t lit = *(const intptr_t *) NEXT_BYTE(context);
    
    scalar_t a = {0};
    anon_scalar_set_int_value(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(intptr_t);
}

/*
=item ADD ( a b -- a+b )

Pops two values from the data stack, and pushes back their integer sum.
 
=cut
 */
int inst_ADD(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, +);
    return 1;
}

/*
=item SUBT ( a b -- a-b )

Pops two values from the data stack, and pushes back their integer difference.

=cut
 */
int inst_SUBT(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, -);
    return 1;
}

/*
=item MULT ( a b -- a*b )

Pops two values from the data stack, and pushes back their integer product.

=cut
 */
int inst_MULT(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, *);
    return 1;
}

/*
=item DIV ( a b -- a/b )

Pops two values from the data stack, and pushes back their integer quotient.

=cut
 */
int inst_DIV(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, /);
    return 1;
}

/*
=item MOD ( a b -- a%b )

Pops two values from the data stack, and pushes back their integer remainder.
 
=cut
 */
int inst_MOD(vm_context_t *context) {
    BYTECODE_NUMERIC_OP(int, %);
    return 1;
}

/*
=item LT0 ( a -- b )

Pops a value, and pushes back 1 if the value is less than zero, or 0 otherwise.

=cut
 */
int inst_LT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_int_value(&a) < 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item GT0 ( a -- b )

Pops a value, and pushes back 1 if the value is greater than zero, or 0 otherwise.

=cut
 */
int inst_GT0(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_int_value(&a) > 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item INCR ( a -- b )

Pops a value, and pushes back the same value incremented by 1.

=cut
 */
int inst_INCR(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, anon_scalar_get_int_value(&a) + 1);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item DECR ( a -- b )

Pops a value, and pushes back the same value decremented by 1.

=cut
 */
int inst_DECR(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, anon_scalar_get_int_value(&a) - 1);
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item STR ( -- s )

Reads a length followed by a string of length bytes from the following bytecode, and pushes the string value to the data stack.

If a zero byte occurs within the string, the resulting string will be terminated at this byte.  However, the full length will
always be consumed from the bytecode.

If length is zero, an empty string value will be pushed to the data stack, and no additional bytes will be consumed from the 
bytecode.

The length is encoded as a single unsigned byte, so the maximum length of the string is 255 characters.

=cut
*/
int inst_STR(struct vm_context_t *context) {
    const uint8_t len = *(uint8_t *) (&context->m_bytecode[context->m_counter + 1]);
    const char *str = (const char *) (&context->m_bytecode[context->m_counter + 1 + sizeof(len)]);
    
    scalar_t s = {0};
    
    if (len > 0) {
        string_t *buf = string_alloc(len, str);
        anon_scalar_set_string_value(&s, buf);
        string_free(buf);
    }
    else {
        string_t *buf = string_alloc(0, NULL);
        anon_scalar_set_string_value(&s, buf);
        string_free(buf);
    }

    vm_ds_push(context, &s);
    anon_scalar_destroy(&s);
    
    return 1 + sizeof(len) + len;
}


/*
=item STRING ( -- s )

Reads a length followed by a string of length bytes from the following bytecode, and pushes the string value to the data stack.

If a zero byte occurs within the string, the resulting string will be terminated at this byte.  However, the full length will
always be consumed from the bytecode.

If length is zero, an empty string value will be pushed to the data stack, and no additional bytes will be consumed from the 
bytecode.

The length is encoded as an unsigned four byte value, so the maximum length of the string is a big as your mom.
=cut
 */
int inst_STRING(struct vm_context_t *context) {
    const uint32_t len = *(uint32_t *) (&context->m_bytecode[context->m_counter + 1]);
    const char *str = (const char *) (&context->m_bytecode[context->m_counter + 1 + sizeof(len)]);
    
    scalar_t s = {0};
    
    if (len > 0) {
        string_t *buf = string_alloc(len, str);
        anon_scalar_set_string_value(&s, buf);
        string_free(buf);
    }
    else {
        string_t *buf = string_alloc(0, NULL);
        anon_scalar_set_string_value(&s, buf);
        string_free(buf);
    }

    vm_ds_push(context, &s);
    anon_scalar_destroy(&s);
    
    return 1 + sizeof(len) + len;
}

/*
=item LEN ( string -- length )

Pops a string from the stack, and pushes back its length.

=cut
*/
int inst_LEN(struct vm_context_t *context) {
    scalar_t s = {0};
    string_t *str;
    
    vm_ds_pop(context, &s);
    anon_scalar_get_string_value(&s, &str);
    anon_scalar_set_int_value(&s, string_length(str));
    vm_ds_push(context, &s);
    
    anon_scalar_destroy(&s);
    
    return 1;
}

/*
=item XPLOD ( s -- [characters] count )

Pops a string from the stack, and pushes back a scalar for each character within it, and the count.

=cut
*/
int inst_XPLOD(struct vm_context_t *context) {
    scalar_t s = {0}, *characters = NULL, count = {0};
    string_t *str = NULL;
    
    vm_ds_pop(context, &s);
    
    anon_scalar_get_string_value(&s, &str);
    size_t n = string_length(str);
    
    if (n > 0) {
        if (NULL != (characters = calloc(n, sizeof(*characters)))) {
            string_t *buf = string_alloc(1, NULL);
            for (size_t i = 0; i < n; i++) {
                string_assign(&buf, 1, &str->m_bytes[i]);
                anon_scalar_set_string_value(&characters[i], buf);

            }
            string_free(buf);
            vm_ds_npush(context, n, characters);

            for (size_t i = 0; i < n; i++)  anon_scalar_destroy(&characters[i]);
            free(characters);
        }
        else {
            debug("calloc failed\n");
            n = 0;
        }
    }
    
    string_free(str);
    
    anon_scalar_set_int_value(&count, n);
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    anon_scalar_destroy(&s);
    
    return 1;
}

/*
=item CAT ( [strings] count -- s )

Pops a count and count strings from the data stack, and pushes back their concatenation.

=cut
*/
int inst_CAT(struct vm_context_t *context) {
    scalar_t count = {0}, s = {0};
    
    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);
    
    if (n > 0) {
        string_t **strings;
        if (NULL != (strings = calloc(n, sizeof(*strings)))) {
            size_t len = 0;
            
            scalar_t tmp = {0};
            for (size_t i = 0; i < n; i++) {
                vm_ds_pop(context, &tmp);
                anon_scalar_get_string_value(&tmp, &strings[i]);
                len += string_length(strings[i]);
            }
            anon_scalar_destroy(&tmp);
            
            string_t *result = string_alloc(len, NULL);
            
            for (size_t i = 0; i < n; i++) {
                string_append(&result, string_length(strings[i]), string_cstr(strings[i]));
                string_free(strings[i]);
            }
            free(strings);
            
            anon_scalar_set_string_value(&s, result);
            string_free(result);
        }
        else {
            debug("calloc failed\n");
        }
    }
    
    vm_ds_push(context, &s);
    
    anon_scalar_destroy(&s);
    anon_scalar_destroy(&count);
    
    return 1;
}

/*
=item FLOAT ( -- a )

Reads a floating point literal from the following bytecode and pushes it onto the data stack.
 
=cut
 */
int inst_FLOAT(struct vm_context_t *context) {
    const floatptr_t lit = *(const floatptr_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    scalar_t a = {0};
    anon_scalar_set_float_value(&a, lit);
    vm_ds_push(context, &a);
    anon_scalar_destroy(&a);
    
    return 1 + sizeof(lit);
}

/*
=item ADDF ( a b -- a+b )

Pops two floating point values from the data stack and pushes back their sum

=cut
 */
int inst_ADDF(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, +);
    return 1;
}

/*
=item SUBTF ( a b -- a-b )

Pops two floating point values from the data stack and pushes back their difference.

=cut
 */
int inst_SUBTF(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, -);
    return 1;    
}

/*
=item MULTF ( a b -- a*b )

Pops two floating point values from the data stack and pushes back their product.

=cut
 */
int inst_MULTF(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, *);
    return 1;    
}

/*
=item DIVF ( a b -- a/b )

Pops two floating point values from the data stack and pushes back their quotient.

=cut
 */
int inst_DIVF(struct vm_context_t *context) {
    BYTECODE_NUMERIC_OP(float, /);
    return 1;    
}

/*
=item MODF ( a b -- a%b )

Pops two floating point values from the data stack and pushes back their remainder.

=cut
 */
int inst_MODF(struct vm_context_t *context) {
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
=item LT0F ( a -- b )

Pops a floating point value and pushes back 1 if it is less than zero, or 0 otherwise.

=cut
 */
int inst_LT0F(struct vm_context_t *context) {
    scalar_t a = {0}, b = {0};
    vm_ds_pop(context, &a);
    anon_scalar_set_int_value(&b, (anon_scalar_get_float_value(&a) < 0));
    vm_ds_push(context, &b);
    anon_scalar_destroy(&b);
    anon_scalar_destroy(&a);
    return 1;
}

/*
=item GT0F ( a -- b )

Pops a floating point value and pushes back 1 if it is greater than zero, or 0 otherwise.

=cut
 */
int inst_GT0F(struct vm_context_t *context) {
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
=item OPEN ( path stream -- stream )

Reads flags from the following byte.  Pops a stream reference and a path, and opens the path according to the flags.
Pushes back a reference to the opened stream.

FIXME write something about flags and path format here.

=cut
*/
int inst_OPEN(struct vm_context_t *context) {
    const flags8_t flags = *(const flags8_t *) (&context->m_bytecode[context->m_counter + 1]);
    
    scalar_t stream = {0}, path = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    vm_ds_pop(context, &path);
    string_t *s; 
    anon_scalar_get_string_value(&path, &s);
    
    if (0 == stream_open(anon_scalar_deref_stream_reference(&stream), flags, s)) {
        debug("opened stream\n");
    }
    else {
        debug("failed to open stream\n");
    }

    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    vm_ds_push(context, &stream);
    
    anon_scalar_destroy(&stream);
    anon_scalar_destroy(&path);
    string_free(s);
    
    return 1 + sizeof(flags);
}

/*
=item CLOSE ( stream -- )

Pops a stream reference from the stack and closes the underlying stream.

=cut
*/
int inst_CLOSE(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    stream_close(anon_scalar_deref_stream_reference(&stream));
    
    anon_scalar_destroy(&stream);

    return 1;
}

/*
=item OUT ( value stream -- )

Pops a stream reference and a scalar value from from the stack, and prints the value to the stream.

=cut
 */
int inst_OUT(struct vm_context_t *context) {
    scalar_t value = {0}, stream = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    vm_ds_pop(context, &value);
    string_t *str;
    anon_scalar_get_string_value(&value, &str);
    stream_write(anon_scalar_deref_stream_reference(&stream), str);
    string_free(str);
    
    anon_scalar_destroy(&value);
    anon_scalar_destroy(&stream);
    
    return 1;
}

/*
=item OUTL ( string delimiter stream -- )

Pops a stream reference, a delimiter byte value, and a string value from the data stack.  Outputs the string followed by
the delimiter to the stream.

=cut
 */
int inst_OUTL(struct vm_context_t *context) {
    scalar_t string = {0}, delimiter = {0}, stream = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    vm_ds_pop(context, &delimiter);
    vm_ds_pop(context, &string);
    
    string_t *str;
    anon_scalar_get_string_value(&string, &str);
    string_appendc(&str, anon_scalar_get_int_value(&delimiter));
    stream_write(anon_scalar_deref_stream_reference(&stream), str);
    string_free(str);
    
    anon_scalar_destroy(&string);
    anon_scalar_destroy(&delimiter);
    anon_scalar_destroy(&stream);

    return 1;
}

/*
=item IN ( len stream -- value )

Pops a stream reference and an integer length from the stack.  Reads up to len bytes from the stream, and pushes back
the value read.

=cut
 */
int inst_IN(struct vm_context_t *context) {
    scalar_t len = {0}, stream = {0}, value = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    vm_ds_pop(context, &len);
    size_t n = anon_scalar_get_int_value(&len);
    
    if (n > 0) {
        string_t *str = stream_read(anon_scalar_deref_stream_reference(&stream), n);
        anon_scalar_set_string_value(&value, str);
        string_free(str);
    }
    
    vm_ds_push(context, &value);

    anon_scalar_destroy(&value);
    anon_scalar_destroy(&len);
    anon_scalar_destroy(&stream);

    return 1;
}

/*
=item INL ( delimiter stream -- value )

Pops a stream reference and a delimiter byte from the stack.  Reads a string from the stream up to and including
the delimiter, and pushes back the value read.

=cut
*/
int inst_INL(struct vm_context_t *context) {
    scalar_t delimiter = {0}, stream = {0}, value = {0};
    
    vm_ds_pop(context, &stream);
    assert((stream.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRMREF);
    
    vm_ds_pop(context, &delimiter);

    string_t *str = stream_read_delim(anon_scalar_deref_stream_reference(&stream), anon_scalar_get_int_value(&delimiter));
    
    if (str != NULL) {
        anon_scalar_set_string_value(&value, str);
        string_free(str);
    }
    
    vm_ds_push(context, &value);
    
    anon_scalar_destroy(&value);
    anon_scalar_destroy(&delimiter);
    anon_scalar_destroy(&stream);
    
    return 1;
}

/*
=item CHOMP ( s delimiter -- s )

Pops a delimiter byte and a scalar from the stack, and pushes back the scalar minus the trailing delimiter (if any).

=cut
*/

int inst_CHOMP(struct vm_context_t *context) {
    scalar_t s = {0}, delimiter = {0};
    
    vm_ds_pop(context, &delimiter);
    vm_ds_pop(context, &s);
    
    if ((s.m_flags & SCALAR_TYPE_MASK) == SCALAR_STRING) {
        string_chomp(s.m_value.as_string, anon_scalar_get_int_value(&delimiter));
    }
    else {
        string_t *str;
        anon_scalar_get_string_value(&s, &str);
        string_chomp(str, anon_scalar_get_int_value(&delimiter));
        anon_scalar_set_string_value(&s, str);
        string_free(str);
    }
    
    vm_ds_push(context, &s);
    
    anon_scalar_destroy(&s);
    anon_scalar_destroy(&delimiter);
    
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
    
    anon_scalar_set_stream_reference(&stream, stream_stdin_handle());
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    return 1;
}

/*
=item STDOUT ( -- stream )

Pushes a reference to the stdout stream to the data stack.

=cut
*/
int inst_STDOUT(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    anon_scalar_set_stream_reference(&stream, stream_stdout_handle());
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    return 1;
}

/*
=item STDERR ( -- stream )

Pushes a reference to the stderr stream to the data stack.

=cut
*/
int inst_STDERR(struct vm_context_t *context) {
    scalar_t stream = {0};
    
    anon_scalar_set_stream_reference(&stream, stream_stderr_handle());
    vm_ds_push(context, &stream);
    anon_scalar_destroy(&stream);
    return 1;
}

/*
=item CHR ( i -- a )

Pops an integer value from the data stack.  Pushes back a string containing the character represented by that integer.

=cut
*/
int inst_CHR(struct vm_context_t *context) {
    scalar_t i = {0}, a = {0};
    string_t *str;
    
    vm_ds_pop(context, &i);

    str = string_alloc(1, NULL);
    str->m_bytes[0] = (char) anon_scalar_get_int_value(&i);
    anon_scalar_set_string_value(&a, str);
    string_free(str);
    
    vm_ds_push(context, &a);
    
    anon_scalar_destroy(&a);
    anon_scalar_destroy(&i);
    
    return 1;
}

/*
=item ORD ( a -- i )

Pops a string value from the data stack.  Pushes back the integer value of the first character in the string, 
or undefined if the string is empty.

=cut
*/
int inst_ORD(struct vm_context_t *context) {
    scalar_t i = {0}, a = {0};
    string_t *str = NULL;
    
    vm_ds_pop(context, &a);
    
    anon_scalar_get_string_value(&a, &str);
    if (string_length(str) > 0)  anon_scalar_set_int_value(&i, str->m_bytes[0]);
    string_free(str);
    
    vm_ds_push(context, &i);
    
    anon_scalar_destroy(&i);
    anon_scalar_destroy(&a);
    
    return 1;
}

/*
=item REV ( [an .. a2 a1] count -- [a1 a2 .. an] count )

Reverses a list of count items on the stack.

=cut
*/
int inst_REV(vm_context_t *context) {
    scalar_t count = {0}, *items = NULL;
    
    vm_ds_pop(context, &count);
    size_t n = anon_scalar_get_int_value(&count);

    if (n > 0) {
        if (NULL != (items = calloc(n, sizeof(*items)))) {
            vm_ds_npop(context, n, items);
            
            for (size_t i = 0; i < n; i++) {
                vm_ds_push(context, &items[i]);
                anon_scalar_destroy(&items[i]);
            }
            
            free(items);
        }
        else {
            debug("calloc failed\n");
            anon_scalar_set_int_value(&count, 0);
        }
    }
    
    vm_ds_push(context, &count);
    
    anon_scalar_destroy(&count);
    return 1;
}

/*
=item SIG ( sig fr -- )

Pops a function reference and a signal number from the stack, and installs the function reference as the handler
for the signal.

The referenced function should take the basic form ( sig -- ).

FIXME The special values VM_SIGNAL_IGNORE and VM_SIGNAL_DEFAULT can be used to ignore or restore the default signal
handler for the signal, respectively.

=cut
*/
int inst_SIG(vm_context_t *context) {
    scalar_t sig = {0}, fr = {0};
    
    vm_ds_pop(context, &fr);
    assert((fr.m_flags & SCALAR_TYPE_MASK) == SCALAR_FUNCREF);
    
    vm_ds_pop(context, &sig);
    
    vm_set_signal_handler(anon_scalar_get_int_value(&sig), anon_scalar_deref_function_reference(&fr));
    
    anon_scalar_destroy(&sig);
    anon_scalar_destroy(&fr);

    return 1;
}

/*
=back

=cut
 */
