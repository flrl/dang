/*
 *  string.c
 *  dang
 *
 *  Created by Ellie on 20/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "string.h"

int string_reserve(string_t **self, size_t new_size) {
    assert(self != NULL && *self != NULL);
    
    if (new_size > (*self)->m_allocated_size) {
        new_size = nextupow2(new_size + 1);
        string_t *tmp = calloc(1, sizeof(*tmp) + new_size);
        if (tmp) {
            memcpy(tmp, *self, (*self)->m_length);
            tmp->m_allocated_size = new_size;
            tmp->m_length = (*self)->m_length;
            free(*self);
            *self = tmp;
            return 0;
        }
        else {
            return -1;
        }
    }
    else {
        return 0;
    }
}

int string_assign(string_t **restrict self, size_t len, const char *restrict str) {
    assert(self != NULL && *self != NULL);
    
    if (len > (*self)->m_allocated_size)  string_reserve(self, len);
    
    memcpy((*self)->m_bytes, str, len);
    memset(&(*self)->m_bytes[len], 0, (*self)->m_allocated_size - len);
    (*self)->m_length = len;
    
    return 0;
}

int string_append(string_t **restrict self, size_t len, const char *restrict str) {
    assert(self != NULL && *self != NULL);
    
    if (len > (*self)->m_allocated_size)  string_reserve(self, (*self)->m_length + len);
    
    memcpy(&(*self)->m_bytes[(*self)->m_length], str, len);
    (*self)->m_length += len;
    
    return 0;
}

int string_appendc(string_t **self, int c) {
    assert(self != NULL && *self != NULL);
    
    if (0 != string_reserve(self, (*self)->m_length + 1))  return -1;
    
    (*self)->m_bytes[(*self)->m_length++] = (char) c;
    
    return 0;
}
