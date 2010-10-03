/*
 *  debug.h
 *  dang
 *
 *  Created by Ellie on 3/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>
#endif

// these are redefined each time, to allow each source file to define NDEBUG independently
#ifdef NDEBUG
    #define debug(...) ((void)0)
#else
    #define debug(...)  do { fprintf(stderr, "debug(%s:%d) %s: ", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); } while (0)
#endif
//