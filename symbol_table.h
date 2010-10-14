/*
 *  symbol_table.h
 *  dang
 *
 *  Created by Ellie on 14/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=pod
 
=cut
 */

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdint.h>

#include "scalar.h"

typedef uintptr_t identifier_t;

typedef struct symbol_table_node_t {
    struct symbol_table_node_t *m_parent;
    struct symbol_table_node_t *m_left_child;
    struct symbol_table_node_t *m_right_child;
    uint32_t m_flags;
    identifier_t m_identifier;
    union {
        scalar_handle_t as_scalar;
    } m_referent;
} symbol_table_node_t;

typedef struct symbol_table_t {
    struct symbol_table_t *m_parent;
    struct symbol_table_node_t *m_symbols;
} symbol_table_t;


int symbol_table_init(symbol_table_t *);
int symbol_table_destroy(symbol_table_t *);

int symbol_table_start_scope(symbol_table_t **);
int symbol_table_end_scope(symbol_table_t **);

const symbol_table_node_t *symbol_table_lookup(symbol_table_t *, identifier_t);

int symbol_table_node_create(symbol_table_t *, identifier_t, uint32_t);

#endif
