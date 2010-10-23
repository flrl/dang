/*
 *  symboltable.c
 *  dang
 *
 *  Created by Ellie on 22/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

#include "symboltable.h"

typedef struct symboltable_registry_node_t {
    struct symboltable_registry_node_t *m_next;
    symboltable_t *m_table;
} symboltable_registry_node_t;

static symboltable_registry_node_t *_symboltable_registry = NULL;
static pthread_mutex_t _symboltable_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

int _symbol_init(symbol_t *);
int _symbol_destroy(symbol_t *);
int _symbol_reap(symbol_t *);

/*
=head1 NAME

symboltable

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut
*/

/*
=item symboltable_init() 

Initialises a symbol table and associates it with its parent.  The symbol table is also registered globally for later
garbage collection.

=cut
 */
int symboltable_init(symboltable_t *restrict self, symboltable_t *restrict parent) {
    assert(self != NULL);
    assert(self != parent);
    
    self->m_references = 1;
    self->m_symbols = NULL;
    self->m_parent = parent;
    if (self->m_parent != NULL)  ++self->m_parent->m_references;
    
    symboltable_registry_node_t *reg_entry;
    if (NULL != (reg_entry = calloc(1, sizeof(*reg_entry)))) {
        if (0 == pthread_mutex_lock(&_symboltable_registry_mutex)) {
            reg_entry->m_table = self;
            reg_entry->m_next = _symboltable_registry;
            _symboltable_registry = reg_entry;
            pthread_mutex_unlock(&_symboltable_registry_mutex);
        }
    }
    
    return 0;
}

/*
=item symboltable_destroy()

Decrements a symbol table's reference count.  If the reference count reaches zero, destroys the symbol table, removes it from
the global registry, and decrements its parent's reference count.  If the reference count does not reach zero, the symbol table
is left in the global registry for later garbage collection.

=cut
 */
int symboltable_destroy(symboltable_t *self) {
    assert(self != NULL);
    assert(self->m_references > 0);
    
    if (--self->m_references == 0) {
        if (self->m_parent != NULL)  --self->m_parent->m_references;
        if (self->m_symbols != NULL) {
            _symbol_reap(self->m_symbols);
            free(self->m_symbols);
        } 
        
        if (0 == pthread_mutex_lock(&_symboltable_registry_mutex)) {
            for (symboltable_registry_node_t *i = _symboltable_registry; i != NULL && i->m_next != NULL; i = i->m_next) {
                if (i->m_next->m_table == self) {
                    symboltable_registry_node_t *reg = i->m_next;
                    i->m_next = i->m_next->m_next;
                    free(reg);
                }
            }
            
            pthread_mutex_unlock(&_symboltable_registry_mutex);
        }
        
        memset(self, 0, sizeof(*self));
        return 0;
    }
    else {
        return -1;
    }
}

/*
=item symboltable_garbage_collect()

Checks the global symbol table registry for entries with a zero reference count, and destroys them.

This situation arises when a scope ends while it is still referenced elsewhere (e.g. by a child scope running in a different
thread of execution).  When the child scope ends it decrements the parent's reference count, but by this time the parent scope 
has already ended.  Periodically calling symboltable_garbage_collect allows the resources held by these scopes to be reclaimed
by the system.

=cut
 */
int symboltable_garbage_collect(void) {
    if (0 == pthread_mutex_lock(&_symboltable_registry_mutex)) {
        while (_symboltable_registry != NULL) {
            assert(_symboltable_registry->m_table != NULL);
            if (_symboltable_registry->m_table->m_references > 0) {
                break;  
            } 
            else {
                symboltable_registry_node_t *old_reg = _symboltable_registry;
                _symboltable_registry = _symboltable_registry->m_next;
                
                symboltable_t *old_table = old_reg->m_table;
                memset(old_reg, 0, sizeof(*old_reg));
                free(old_reg);
                
                if (old_table->m_parent != NULL)  --old_table->m_parent->m_references;
                if (old_table->m_symbols != NULL) {
                    _symbol_reap(old_table->m_symbols);
                    free(old_table->m_symbols);
                }
                
                memset(old_table, 0, sizeof(*old_table));
                free(old_table);
            }
        }
        pthread_mutex_unlock(&_symboltable_registry_mutex);
        return 0;
    }
    else {
        return -1;
    }
}

/*
=item symbol_define()

Define a symbol within the current scope.

=cut
 */
int symbol_define(symboltable_t *table, identifier_t identifier, uint32_t flags) {
    assert(table != NULL);
    symbol_t *symbol = calloc(1, sizeof(*symbol));
    _symbol_init(symbol);
    symbol->m_identifier = identifier;
    switch (flags & SYMBOL_TYPE_MASK) {
        case SYMBOL_SCALAR:
            symbol->m_flags = SYMBOL_SCALAR;
            symbol->m_referent.as_scalar = scalar_allocate(flags & ~SYMBOL_TYPE_MASK);
            break;
        //...
        case SYMBOL_CHANNEL:
            symbol->m_flags = SYMBOL_SCALAR;
            symbol->m_referent.as_channel = channel_allocate();
            break;
        default:
            debug("unhandled symbol type: %"PRIu32"\n", flags);
            break;
    }
    
    if (table->m_symbols == NULL) {
        table->m_symbols = symbol;
        return 0;
    }
    else {
        symbol_t *parent = table->m_symbols;
        do {
            if (identifier < parent->m_identifier) {
                if (parent->m_left_child == NULL) {
                    parent->m_left_child = symbol;
                    symbol->m_parent = parent;
                    return 0;
                }
                else {
                    parent = parent->m_left_child;
                }
            }
            else if (identifier > parent->m_identifier) {
                if (parent->m_right_child == NULL) {
                    parent->m_right_child = symbol;
                    symbol->m_parent = parent;
                    return 0;
                }
                else {
                    parent = parent->m_right_child;
                }
            }
            else {
                debug("identifier %"PRIuPTR" is already defined in current scope\n", identifier);
                _symbol_destroy(symbol);
                free(symbol);
                return -1;
            }
        } while (parent != NULL);
        
        debug("not supposed to get here\n");
        _symbol_destroy(symbol);
        return -1;
    }
}

/*
=item symbol_lookup()

Look for a symbol in the current scope.  If no such symbol is found, search parent scopes, then grandparent scopes, etc,
until either a matching symbol is found, or the top level ("global") scope has been unsuccessfully searched.

Returns a pointer to the symbol_t object found, or NULL if the symbol is not defined.

=cut
 */
const symbol_t *symbol_lookup(symboltable_t *table, identifier_t identifier) {
    assert(table != NULL);
    symboltable_t *scope = table;
    while (scope != NULL) {
        symbol_t *symbol = scope->m_symbols;
        while (symbol != NULL) {
            if (identifier < symbol->m_identifier) {
                symbol = symbol->m_left_child;
            }
            else if (identifier >  symbol->m_identifier) {
                symbol = symbol->m_right_child;
            }
            else {
                return symbol;
            }
        }
        scope = scope->m_parent;
    }
    return NULL;
}

/*
=item symbol_undefine()

Removes a symbol from the current scope.  This does I<not> search upwards through ancestor scopes.

Returns 0 when the requested symbol no longer exists (or never did exist) at the current scope.  Other
return values indicate an error undefining the symbol, and that it may still exist at the current scope.

=cut
 */
int symbol_undefine(symboltable_t *table, identifier_t identifier) {
    assert(table != NULL);
    
    symbol_t *symbol = (symbol_t *) symbol_lookup(table, identifier);
    if (symbol == NULL)  return 0;
    
    if (symbol->m_left_child != NULL) {
        if (symbol->m_right_child != NULL) {
            symbol_t *replacement = symbol;
            symbol_t *join;
            if (rand() & 0x4000) {
                // reconnect from left
                replacement = symbol->m_left_child;
                while (replacement != NULL && replacement->m_right_child != NULL)  replacement = replacement->m_right_child;
                join = replacement->m_parent;
                join->m_right_child = replacement->m_left_child;
                join->m_right_child->m_parent = join;
                
            }
            else {
                // reconnect from right
                replacement = symbol->m_right_child;
                while (replacement != NULL && replacement->m_left_child != NULL)  replacement = replacement->m_left_child;
                join = replacement->m_parent;
                join->m_left_child = replacement->m_right_child;
                join->m_left_child->m_parent = join;
            }
            
            replacement->m_left_child = symbol->m_left_child;
            if (replacement->m_left_child != NULL)  replacement->m_left_child->m_parent = replacement;
            replacement->m_right_child = symbol->m_right_child;
            if (replacement->m_right_child != NULL)  replacement->m_right_child->m_parent = replacement;
            
            replacement->m_parent = symbol->m_parent;
            if (replacement->m_parent == NULL) {
                table->m_symbols = replacement;   
            }
            else if (replacement->m_parent->m_left_child == symbol) {
                replacement->m_parent->m_left_child = replacement;
            }
            else if (replacement->m_parent->m_right_child == symbol) {
                replacement->m_parent->m_right_child = replacement;
            }
            else {
                debug("symbol's parent doesn't claim it as a child\n");
            }
            
            _symbol_destroy(symbol);
            free(symbol);
            return 0;                
        }
        else {
            if (symbol->m_parent->m_left_child == symbol) {
                symbol->m_parent->m_left_child = symbol->m_left_child;
            }
            else if (symbol->m_parent->m_right_child == symbol) {
                symbol->m_parent->m_right_child = symbol->m_left_child;
            }
            else {
                debug("symbol's parent doesn't claim it as a child\n");
            }
            _symbol_destroy(symbol);
            free(symbol);
            return 0;
        }
    }
    else if (symbol->m_right_child != NULL) {
        if (symbol->m_parent->m_left_child == symbol) {
            symbol->m_parent->m_left_child = symbol->m_right_child;
        }
        else if (symbol->m_parent->m_right_child == symbol) {
            symbol->m_parent->m_right_child = symbol->m_right_child;
        }
        else {
            debug("symbol's parent doesn't claim it as a child\n");
        }
        _symbol_destroy(symbol);
        free(symbol);
        return 0;
    }
    else {
        if (symbol->m_parent->m_left_child == symbol) {
            symbol->m_parent->m_left_child = NULL;
        }
        else if (symbol->m_parent->m_right_child == symbol) {
            symbol->m_parent->m_right_child = NULL;
        }
        else {
            debug("symbol's parent doesn't claim it as a child\n");
        }
        _symbol_destroy(symbol);
        free(symbol);
        return 0;
    }
}

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
 */

/*
=item _symbol_init()

=item _symbol_destroy()

Setup and teardown functions for individual symbol_t objects

=cut
 */
int _symbol_init(symbol_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    return 0;
}

int _symbol_destroy(symbol_t *self) {
    assert(self != NULL);
    switch (self->m_flags & SYMBOL_TYPE_MASK) {
        case SYMBOL_SCALAR:
            scalar_release(self->m_referent.as_scalar);
            break;
        //...
        case SYMBOL_CHANNEL:
            channel_release(self->m_referent.as_channel);
            break;
        default:
            debug("unhandled symbol type: %"PRIu32"\n", self->m_flags);
            break;
    }
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item _symbol_reap()

Recursively destroys a symbol_t object and all of its children

=cut
 */
int _symbol_reap(symbol_t *self) {
    assert(self != NULL);
    if (self->m_left_child != NULL) {
        _symbol_reap(self->m_left_child);
        free(self->m_left_child);
    }
    if (self->m_right_child != NULL) {
        _symbol_reap(self->m_right_child);
        free(self->m_right_child);
    }
    return _symbol_destroy(self);
}

/*
=back

=cut
*/
