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
    int c = getc(stream);
    ungetc(c, stream);
    return c;
}



#endif
