/*
 *  symboltable.h
 *  dang
 *
 *  Created by Ellie on 22/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <stdint.h>

#include "scalar.h"
#include "channel.h"

typedef uintptr_t identifier_t;

typedef struct symbol_t {
    struct symbol_t *m_parent;
    struct symbol_t *m_left_child;
    struct symbol_t *m_right_child;
    identifier_t m_identifier;
    uint32_t m_flags;
    union {
        scalar_handle_t as_scalar;
//        array_handle_t as_array;
//        hash_handle_t as_hash;
        channel_handle_t as_channel;
    } m_referent;
} symbol_t;

typedef struct symboltable_t {
    struct symboltable_t *m_parent;
    symbol_t *m_symbols;
    size_t m_references;
} symboltable_t;

int symboltable_init(symboltable_t *restrict, symboltable_t *restrict);
int symboltable_destroy(symboltable_t *);
int symboltable_garbage_collect(void);

int symbol_define(symboltable_t *, identifier_t, uint32_t);
const symbol_t *symbol_lookup(symboltable_t *, identifier_t);
int symbol_undefine(symboltable_t *, identifier_t);

#endif
