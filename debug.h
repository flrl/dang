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
#include <inttypes.h>
#include <stdint.h>
#endif

// these are redefined each time this file is included, to allow each source file to define NDEBUG independently
#ifdef NDEBUG
    #define debug(...) ((void)0)
    #define FIXME(...) ((void)0)
#else
    #if defined(__APPLE__)
        #include <pthread.h> /* for pthread_self() */
        #define THREADID (*(uint32_t*)((void*)pthread_self()+52))
        #define PRITID  "x"
    #elif defined(__linux__)
        #include <pthread.h> /* for pthread_self() */
        #define THREADID (pthread_self())
        #define PRITID "lx"
    #else
        #define THREADID (0)
        #define PRITID "x"
    #endif
    #define debug(...)  do {                                                                            \
        char s1[512], s2[512];                                                                          \
        snprintf(s1, sizeof(s1), "debug %"PRITID" %s(%s:%d)", THREADID, __func__, __FILE__, __LINE__);  \
        snprintf(s2, sizeof(s2), __VA_ARGS__);                                                          \
        fprintf(stderr, "%s: %s", s1, s2);                                                              \
    } while (0)
    
    #define FIXME(...)  do {                                                                            \
        char s1[512], s2[512];                                                                          \
        snprintf(s1, sizeof(s1), "FIXME %"PRITID" %s(%s:%d)", THREADID, __func__, __FILE__, __LINE__);  \
        snprintf(s2, sizeof(s2), __VA_ARGS__);                                                          \
        fprintf(stderr, "%s: %s", s1, s2);                                                              \
    } while (0)
#endif
//
