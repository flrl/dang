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

#include "string.h"
#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE 16
#endif
#include "pool.h"

#define STREAM_TYPE_UNDEF   0x00u
#define STREAM_TYPE_FILE    0x01u
#define STREAM_TYPE_FIFO    0x02u
#define STREAM_TYPE_PIPE    0x03u
#define STREAM_TYPE_SOCK    0x04u

#define STREAM_TYPE_MASK    0x07u

#define STREAM_FLAG_READ    0x10u
#define STREAM_FLAG_WRITE   0x20u
#define STREAM_FLAG_TRUNC   0x40u
#define STREAM_FLAG_APPEND  0x80u

typedef struct stream_t {
    /* innards of this struct will most likely change a lot */
    flags32_t m_flags;
    FILE *m_file;
    union {
        string_t *filename;
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

stream_handle_t stream_stdin_handle(void);
stream_handle_t stream_stdout_handle(void);
stream_handle_t stream_stderr_handle(void);

int stream_open(stream_handle_t, flags32_t, const string_t *);
int stream_close(stream_handle_t);

string_t *stream_read_delim(stream_handle_t, int);
string_t *stream_read(stream_handle_t, size_t);
int stream_write(stream_handle_t, const string_t *);

const char *stream_get_filename(stream_handle_t);

#endif
