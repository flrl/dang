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

typedef struct io_t {
    char *m_filename;
    int m_fd;
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

int io_open(io_handle_t, const char *);
int io_close(io_handle_t);

int io_read_until_byte(io_handle_t, char **, size_t *, int);
int io_read_len(io_handle_t, char **, size_t *, size_t);
int io_write(io_handle_t, const char *, size_t);

const char *io_get_filename(io_handle_t);

#endif
