/*
 *  return_stack.h
 *  dang
 *
 *  Created by Ellie on 4/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef RETURN_STACK_H
#define RETURN_STACK_H

typedef struct return_stack_t {
    size_t m_allocated_count;
    size_t m_count;
    size_t *m_items;
} return_stack_t;

int return_stack_init(return_stack_t *);
int return_stack_destroy(return_stack_t *);

int return_stack_push(return_stack_t *, size_t);
int return_stack_pop(return_stack_t *, size_t *);


#endif
