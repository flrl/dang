/*
 *  extio.h
 *  dang
 *
 *  Created by Ellie on 11/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef EXTIO_H
#define EXTIO_H

#include <stdio.h>

static inline int peekc(FILE *stream) {
    int c;
    flockfile(stream);
    c = getc_unlocked(stream);
    ungetc(c, stream);
    funlockfile(stream);
    return c;
}

size_t freads(FILE *, char *, size_t);
size_t afreadln(FILE *, char **, int);

#endif
