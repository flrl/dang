/*
 *  util.h
 *  dang
 *
 *  Created by Ellie on 11/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdint.h>

#define MAX(a,b) (((b) > (a)) ? (b) : (a))
#define MIN(a,b) (((b) < (a)) ? (b) : (a))

uintptr_t nextupow2(uintptr_t);

static inline int peekc_unlocked(FILE *stream) {
    int c = getc_unlocked(stream);
    ungetc(c, stream);
    return c;
}

static inline int peekc(FILE *stream) {
    int c;
    flockfile(stream);
    c = peekc_unlocked(stream);
    funlockfile(stream);
    return c;
}

#define DELIMITER_WHITESPACE    (0x100) /* greater than can be stored in a char */
ssize_t getdelim_ext(char **restrict, size_t *restrict, int, FILE *restrict);

#ifdef NEED_GETDELIM
#define getdelim(l,n,d,s)    getdelim_ext(l,n,d,s)
#endif

#ifdef NEED_GETLINE
static inline ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream) {
    return getdelim(lineptr, n, '\n', stream);
}
#endif

#endif
