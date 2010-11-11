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

#ifdef NEED_GETDELIM
ssize_t getdelim(char **restrict, size_t *restrict, int, FILE *restrict);
#endif

static inline int peekc(FILE *stream) {
    int c;
    flockfile(stream);
    c = getc_unlocked(stream);
    ungetc(c, stream);
    funlockfile(stream);
    return c;
}

#ifdef NEED_GETLINE
static inline ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream) {
    return getdelim(lineptr, n, '\n', stream);
}
#endif

#endif
