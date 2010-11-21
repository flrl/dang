/*
 *  stack.h
 *  dang
 *
 *  Created by Ellie on 16/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef STACK_INITIAL_SIZE
#define STACK_INITIAL_SIZE (16)
#endif

#ifndef STACK_H
#define STACK_H

#include <assert.h>
#include <stdlib.h>

#include "util.h"

/* 
 This macro expands to the struct type for stacks of type "type" 
 */
#define STACK_TYPE(type)    struct type##_STACK

/*
 This macro expands to the struct definition for stacks of type "type"
 */
#define STACK_STRUCT(type)  struct type##_STACK {                                       \
    size_t m_allocated_count;                                                           \
    size_t m_count;                                                                     \
    type *m_items;                                                                      \
}

/*
 These macros wrap calls to inline functions to do the actual work, defined below.
 */
#define STACK_INIT(type, stack)                 type##_STACK_INIT(stack)
#define STACK_DESTROY(type, stack)              type##_STACK_DESTROY(stack)
#define STACK_PUSH(type, stack, value)          type##_STACK_PUSH(stack, 1, value)
#define STACK_POP(type, stack, result)          type##_STACK_POP(stack, 1, result)
#define STACK_NPUSH(type, stack, count, values) type##_STACK_PUSH(stack, count, values)
#define STACK_NPOP(type, stack, count, results) type##_STACK_POP(stack, count, results)
#define STACK_TOP(type, stack, result)          type##_STACK_TOP(stack, result)
#define STACK_SWAP(type, stack)                 type##_STACK_SWAP(stack)
#define STACK_DUP(type, stack)                  type##_STACK_DUP(stack)
#define STACK_OVER(type, stack)                 type##_STACK_OVER(stack)

/*
 These macros provide basic implementations of the init, destroy, and copy functions 
 required by the inline stack functions, suitable for use with primitive
 types.
 */
#define STACK_basic_init(x)         (0)
#define STACK_basic_destroy(x)      (0)
#define STACK_basic_copy(x, y)      ((*(x) = *(y)), 0)

/* 
 This macro defines static inline functions for working with stack structures.  
 The init_func, dest_func and copy_func parameters specify functions for  initialising, 
 destroying, and copying objects of type "type".  They expect to be macros or functions
 of the basic form thus:

 int init_func(type *)
 int dest_func(type *)
 int copy_func(type *, const type *)
 
 You must call this macro exactly once in a source file for each type of stack required, 
 in order to then be able to use these stack functions.
 */
#define STACK_DEFINITIONS(type, init_func, dest_func, copy_func)                        \
static inline int type##_STACK_INIT(struct type##_STACK *stack) {                       \
    assert(stack != NULL);                                                              \
                                                                                        \
    stack->m_items = calloc(STACK_INITIAL_SIZE, sizeof(stack->m_items[0]));             \
    if (stack->m_items != NULL) {                                                       \
        stack->m_allocated_count = STACK_INITIAL_SIZE;                                  \
        stack->m_count = 0;                                                             \
        return 0;                                                                       \
    }                                                                                   \
    else {                                                                              \
        return -1;                                                                      \
    }                                                                                   \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_DESTROY(struct type##_STACK *stack) {                    \
    assert(stack != NULL);                                                              \
                                                                                        \
    while (stack->m_count > 0) {                                                        \
        --stack->m_count;                                                               \
        (void) dest_func(&stack->m_items[(stack->m_count)]);                            \
    }                                                                                   \
    free(stack->m_items);                                                               \
    memset(stack, 0, sizeof(*stack));                                                   \
    return 0;                                                                           \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_RESERVE(struct type##_STACK *stack, size_t new_size) {   \
    assert(stack != NULL);                                                              \
                                                                                        \
    if (stack->m_allocated_count >= new_size)  return 0;                                \
                                                                                        \
    new_size = nextupow2(new_size);                                                     \
    type *tmp = calloc(new_size, sizeof(*tmp));                                         \
    if (tmp == NULL)  return -1;                                                        \
                                                                                        \
    memcpy(tmp, stack->m_items, stack->m_count * sizeof(stack->m_items[0]));            \
    free(stack->m_items);                                                               \
    stack->m_items = tmp;                                                               \
    stack->m_allocated_count = new_size;                                                \
    return 0;                                                                           \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_PUSH(    struct type##_STACK *stack, size_t count,       \
                                        const type *values) {                           \
    assert(stack != NULL);                                                              \
    assert(count > 0);                                                                  \
    assert(values != NULL);                                                             \
                                                                                        \
    if (0 == type##_STACK_RESERVE(stack, stack->m_count + count)) {                     \
        for (size_t i = count; i > 0; i--) {                                            \
            copy_func(&stack->m_items[stack->m_count++], &values[i-1]);                 \
        }                                                                               \
        return 0;                                                                       \
    }                                                                                   \
    else {                                                                              \
        return -1;                                                                      \
    }                                                                                   \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_POP( struct type##_STACK *stack, size_t count,           \
                                    type *results) {                                    \
    assert(stack != NULL);                                                              \
    assert(count > 0);                                                                  \
    assert(stack->m_count >= count);                                                    \
                                                                                        \
    int status = 0;                                                                     \
    if (results != NULL) {                                                              \
        for (size_t i = 0; i < count; i++) {                                            \
            status = copy_func(&results[i], &stack->m_items[--stack->m_count]);         \
            status = dest_func(&stack->m_items[stack->m_count]);                        \
        }                                                                               \
    }                                                                                   \
    else {                                                                              \
        for (size_t i = 0; i < count; i++) {                                            \
            status = dest_func(&stack->m_items[--stack->m_count]);                      \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    return status;                                                                      \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_TOP(struct type##_STACK *stack, type *result) {          \
    assert(stack != NULL);                                                              \
    assert(stack->m_count > 0);                                                         \
    assert(result != NULL);                                                             \
                                                                                        \
    return copy_func(result, &stack->m_items[stack->m_count - 1]);                      \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_SWAP(struct type##_STACK *stack) {                       \
    assert(stack != NULL);                                                              \
    assert(stack->m_count > 1);                                                         \
                                                                                        \
    type tmp;                                                                           \
    memcpy(&tmp, &stack->m_items[stack->m_count - 2], sizeof(type));                    \
    memcpy(&stack->m_items[stack->m_count - 2],                                         \
           &stack->m_items[stack->m_count - 1], sizeof(type));                          \
    memcpy(&stack->m_items[stack->m_count - 1], &tmp, sizeof(type));                    \
                                                                                        \
    return 0;                                                                           \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_DUP(struct type##_STACK *stack) {                        \
    assert(stack != NULL);                                                              \
    assert(stack->m_count > 0);                                                         \
                                                                                        \
    return STACK_PUSH(type, stack, &stack->m_items[stack->m_count - 1]);                \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_OVER(struct type##_STACK *stack) {                       \
    assert(stack != NULL);                                                              \
    assert(stack->m_count > 1);                                                         \
                                                                                        \
    return STACK_PUSH(type, stack, &stack->m_items[stack->m_count - 2]);                \
}

#endif
