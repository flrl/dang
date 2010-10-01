/*
 *  string.c
 *  dang
 *
 *  Created by Ellie on 26/09/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string.h"

struct string {
    size_t m_allocated_length;
    size_t m_length;
    char *m_str;
};

//String string_create(const char *);
//String string_destroy(String self);
//
//String string_clone(const String self);
//size_t string_length(const String self);
//const char *string_ccstr(const String);
//
//int string_assign(String self, const String other);
//int string_append(String self, const String other);
//

String string_create(const char *orig) {
    if (orig == NULL)  return NULL;
    
    struct string *self = calloc(1, sizeof(struct string));
    if (self == NULL)  return NULL;
    
    self->m_length = strlen(orig);
    self->m_allocated_length = 1 + self->m_length;
    self->m_str = calloc(self->m_allocated_length, sizeof(char));
    if (self->m_str == NULL) {
        free(self);
        return NULL;
    }
    strncpy(self->m_str, orig, 1 + self->m_length);
    return self;
}

String string_destroy(String self) {
    assert(self != NULL);
    if (self->m_str != NULL)  free(self->m_str);
    free(self);
    return NULL;
}

String string_clone(const String self) {
    assert(self != NULL);
    return string_create(self->m_str);
}

size_t string_length(const String self) {
    assert(self != NULL);
    return self->m_length;
}

const char *string_ccstr(const String self) {
    assert(self != NULL);
    return self->m_str;
}

int string_assign(String restrict self, const String restrict other) {
    assert(self != NULL);
    if (self == other)  return 0;
    if (self->m_allocated_length > other->m_length) {
        strncpy(self->m_str, other->m_str, 1 + other->m_length);
        self->m_length = other->m_length;
        return 0;
    }
    else {
        char *tmp = calloc(1 + other->m_length, sizeof(char));
        if (tmp) {
            free(self->m_str);
            self->m_str = tmp;
            self->m_allocated_length = 1 + other->m_length;
            self->m_length = other->m_length;
            strncpy(self->m_str, other->m_str, 1 + self->m_length);
            return 0;
        }
        else {
            return -1;
        }
    }
}

int string_append(String self, const String other) {
    assert(self != NULL);
    assert(other != NULL);
    
    if (self->m_allocated_length > 1 + self->m_length + other->m_length) {
        strncpy(&self->m_str[self->m_length], other->m_str, 1 + other->m_length);
        self->m_length += other->m_length;
        return 0;
    }
    else {
        size_t new_length = self->m_length + other->m_length;
        char *tmp = calloc(1 + new_length, sizeof(char));
        if (tmp) {
            strncpy(tmp, self->m_str, 1 + self->m_length);
            strncat(tmp, other->m_str, other->m_length);
            free(self->m_str);
            self->m_length = new_length;
            self->m_allocated_length = 1 + self->m_length;
            self->m_str = tmp;
            return 0;
        }
        else {
            return -1;
        }
    }
}











