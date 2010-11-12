/*
 *  io.h
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef IO_H
#define IO_H

#include <stdio.h>

#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE 16
#endif
#include "pool.h"

#define IO_TYPE_UNDEF   0x00000000u
#define IO_TYPE_FILE    0x00000001u
#define IO_TYPE_FIFO    0x00000002u
#define IO_TYPE_PIPE    0x00000004u
#define IO_TYPE_SOCK    0x00000008u

#define IO_TYPE_MASK    0x0000000Fu

#define IO_FLAG_READ    0x00000100u
#define IO_FLAG_WRITE   0x00000200u

typedef struct io_t {
    /* innards of this struct will most likely change a lot */
    flags_t m_flags;
    char *m_filename;
    FILE *m_file;
} io_t;

int _io_init(io_t *);
int _io_destroy(io_t *);

POOL_HEADER_CONTENTS(io_t, io_handle_t, PTHREAD_MUTEX_ERRORCHECK, _io_init, _io_destroy);

int io_pool_init(void);
int io_pool_destroy(void);

io_handle_t io_allocate(void);
io_handle_t io_allocate_many(size_t);
io_handle_t io_reference(io_handle_t);
int io_release(io_handle_t);

int io_open(io_handle_t, const char *, flags_t);
int io_close(io_handle_t);

ssize_t io_read_delim(io_handle_t, char **, int);
ssize_t io_read(io_handle_t, char *, size_t);
int io_write(io_handle_t, const char *, size_t);

const char *io_get_filename(io_handle_t);

#endif
