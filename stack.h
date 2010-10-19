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
#define STACK_INIT(type, stack)             type##_STACK_INIT(stack)
#define STACK_DESTROY(type, stack)          type##_STACK_DESTROY(stack)
#define STACK_PUSH(type, stack, value)      type##_STACK_PUSH(stack, value)
#define STACK_POP(type, stack, result)      type##_STACK_POP(stack, result)
#define STACK_TOP(type, stack, result)      type##_STACK_TOP(stack, result)

/*
 These macros provide basic implementations of the init, destroy, clone and assign
 functions required by the inline stack functions, suitable for use with primitive
 types.
 */
#define STACK_basic_init(x)         (0)
#define STACK_basic_destroy(x)      (0)
#define STACK_basic_clone(x, y)     ((*(x) = *(y)), 0)
#define STACK_basic_assign(x, y)    ((*(x) = *(y)), 0)

/* 
 This macro defines static inline functions for working with stack structures.  
 The init_func, dest_func, clone_func and ass_func parameters specify functions for 
 initialising, destroying, clonining, and assigning objects of type "type".  They 
 expect to be macros or functions of the basic form thus:

 int init_func(type *)
 int dest_func(type *)
 int clone_func(type *, const type *)
 int ass_func(type *, const type *)
 
 You must call this macro exactly once in a source file for each type of stack required, 
 in order to then be able to use these stack functions.
 */
#define STACK_DEFINITIONS(type, init_func, dest_func, clone_func, ass_func)             \
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
    type *tmp = calloc(new_size, sizeof(*tmp));                                         \
    if (tmp == NULL)  return -1;                                                        \
                                                                                        \
    memcpy(tmp, stack->m_items, stack->m_count * sizeof(stack->m_items[0]));            \
    free(stack->m_items);                                                               \
    stack->m_items = tmp;                                                               \
    return 0;                                                                           \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_PUSH(struct type##_STACK *stack, const type *value) {    \
    assert(stack != NULL);                                                              \
    assert(value != NULL);                                                              \
                                                                                        \
    int status = 0;                                                                     \
    if (stack->m_count == stack->m_allocated_count) {                                   \
        status = type##_STACK_RESERVE(stack, stack->m_allocated_count * 2);             \
    }                                                                                   \
                                                                                        \
    if (status == 0) {                                                                  \
        status = clone_func(&stack->m_items[stack->m_count++], value);                  \
    }                                                                                   \
                                                                                        \
    return status;                                                                      \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_POP(struct type##_STACK *stack, type *result) {          \
    assert(stack != NULL);                                                              \
                                                                                        \
    if (result != NULL) {                                                               \
        return ass_func(result, &stack->m_items[--stack->m_count]);                     \
    }                                                                                   \
    else {                                                                              \
        return dest_func(&stack->m_items[--stack->m_count]);                            \
    }                                                                                   \
}                                                                                       \
                                                                                        \
static inline int type##_STACK_TOP(struct type##_STACK *stack, type *result) {          \
    assert(stack != NULL);                                                              \
    assert(result != NULL);                                                             \
                                                                                        \
    return clone_func(result, &stack->m_items[stack->m_count - 1]);                     \
}


#endif
