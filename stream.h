/*
 *  stream.h
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef STREAM_H
#define STREAM_H

#include <stdio.h>

#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE 16
#endif
#include "pool.h"

#define STREAM_TYPE_UNDEF   0x00000000u
#define STREAM_TYPE_FILE    0x00000001u
#define STREAM_TYPE_FIFO    0x00000002u
#define STREAM_TYPE_PIPE    0x00000004u
#define STREAM_TYPE_SOCK    0x00000008u

#define STREAM_TYPE_MASK    0x0000000Fu

#define STREAM_FLAG_READ    0x00000100u
#define STREAM_FLAG_WRITE   0x00000200u
#define STREAM_FLAG_TRUNC   0x00000400u

typedef struct stream_t {
    /* innards of this struct will most likely change a lot */
    flags_t m_flags;
    FILE *m_file;
    union {
        char *filename;
        int child_pid;
    } m_meta;
} stream_t;

int _stream_init(stream_t *);
int _stream_destroy(stream_t *);

POOL_HEADER_CONTENTS(stream_t, stream_handle_t, PTHREAD_MUTEX_ERRORCHECK, _stream_init, _stream_destroy);

int stream_pool_init(void);
int stream_pool_destroy(void);

stream_handle_t stream_allocate(void);
stream_handle_t stream_allocate_many(size_t);
stream_handle_t stream_reference(stream_handle_t);
int stream_release(stream_handle_t);

int stream_open(stream_handle_t, flags_t, const char *);
int stream_close(stream_handle_t);

ssize_t stream_read_delim(stream_handle_t, char **, int);
ssize_t stream_read(stream_handle_t, char *, size_t);
int stream_write(stream_handle_t, const char *, size_t);

const char *stream_get_filename(stream_handle_t);

#endif
