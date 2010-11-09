/*
 *  file.h
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef FILE_H
#define FILE_H

#include <stdio.h>

#include "vmtypes.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE 16
#endif
#include "pool.h"

typedef struct file_t {
    char *m_filename;
    FILE *m_file;
} file_t;

int _file_init(file_t *);
int _file_destroy(file_t *);

POOL_HEADER_CONTENTS(file_t, file_handle_t, PTHREAD_MUTEX_ERRORCHECK, _file_init, _file_destroy);

int file_pool_init(void);
int file_pool_destroy(void);

file_handle_t file_allocate(void);
file_handle_t file_allocate_many(size_t);
file_handle_t file_reference(file_handle_t);
int file_release(file_handle_t);

int file_open(file_handle_t, const char *);
int file_close(file_handle_t);

int file_read_until_byte(file_handle_t, char **, size_t *, int);
int file_read_len(file_handle_t, char **, size_t *, size_t);
int file_write(file_handle_t, const char *, size_t);

const char *file_get_filename(file_handle_t);

#endif
