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
    #if defined(__APPLE__)
        #include <pthread.h> /* for pthread_self() */
        #define THREADID (*(uint32_t*)((void*)pthread_self()+52))
    #elif defined(__linux__)
        #include <pthread.h> /* for pthread_self() */
        #define THREADIT (pthread_self())
    #else
        #define THREADID (0)
    #endif
    #define debug(...)  do { fprintf(stderr, "debug %x %s(%s:%d): ", THREADID, __func__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); } while (0)
    #define FIXME(...)  do { fprintf(stderr, "FIXME %x %s(%s:%d): ", THREADID, __func__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); } while (0)
#endif
//
