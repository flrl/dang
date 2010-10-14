/*
 *  symbol_table.c
 *  dang
 *
 *  Created by Ellie on 14/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>

#include "symbol_table.h"

//typedef struct symbol_table_node_t {
//    struct symbol_table_node_t *m_parent;
//    struct symbol_table_node_t *m_left_child;
//    struct symbol_table_node_t *m_right_child;
//    uint32_t m_flags;
//    identifier_t m_identifier;
//    union {
//        scalar_handle_t as_scalar;
//    } m_referent;
//} symbol_table_node_t;
//
//typedef struct symbol_table_t {
//    struct symbol_table_t *m_parent;
//    struct symbol_table_node_t *m_symbols;
//} symbol_table_t;


int symbol_table_init(symbol_table_t *self) {
    assert(self != NULL);
    
    self->m_parent = NULL;
    self->m_symbols = NULL;
    return 0;
}

int symbol_table_destroy(symbol_table_t *self) {    // FIXME write this
    assert(self != NULL);

    return -1;
}

int symbol_table_start_scope(symbol_table_t **symbol_stack_top) {
    // FIXME does this need registry stuff like data_stack?  does data_stack still need it?
    // FIXME can this whole bunch of functionality just be bundled into data_stack, since every vm is going to need both anyway?
    return -1;
}

int symbol_table_end_scope(symbol_table_t **symbol_stack_top) {
    return -1;
}

const symbol_table_node_t *symbol_table_lookup(symbol_table_t *, identifier_t);

int symbol_table_node_create(symbol_table_t *, identifier_t, uint32_t);

