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

// these are redefined each time this file is included, to allow each source file to define NDEBUG independently
#ifdef NDEBUG
    #define debug(...) ((void)0)
    #define FIXME(...) ((void)0)
#else
    #define debug(...)  do { fprintf(stderr, "debug %s(%s:%d): ", __func__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); } while (0)
    #define FIXME(...)  do { fprintf(stderr, "FIXME %s(%s:%d): ", __func__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); } while (0)
#endif
//
