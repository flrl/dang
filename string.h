/*
 *  string.h
 *  dang
 *
 *  Created by Ellie on 20/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef STRING_H
#define STRING_H

#include <stdlib.h>
#include <string.h>

#include "util.h"

typedef struct string_t {
    size_t m_allocated_size;
    size_t m_length;
    char m_bytes[];
} string_t;

static inline string_t *string_alloc(size_t initial_length, const char *initial_value) {
    size_t size = nextupow2(initial_length + 1);
    string_t *self = calloc(1, sizeof(*self) + size);
    if (self) {
        self->m_allocated_size = size;
        if (initial_value) {
            memcpy(self->m_bytes, initial_value, initial_length);
            self->m_length = initial_length;
        }
    }
    return self;
}

static inline string_t *string_dup(const string_t *orig) { return string_alloc(orig->m_length, orig->m_bytes); }

static inline void string_free(string_t *self) { free(self); }

int string_reserve(string_t **, size_t);
int string_assign(string_t **restrict, size_t, const char *restrict);

int string_append(string_t **restrict, size_t, const char *restrict);
int string_prepend(string_t **restrict, size_t, const char *restrict);

static inline size_t string_length(const string_t *self) { return self->m_length; }
static inline const char *string_cstr(const string_t *self) { return self->m_bytes; }

#endif
