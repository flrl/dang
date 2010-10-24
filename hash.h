/*
 *  hash.h
 *  dang
 *
 *  Created by Ellie on 24/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef HASH_H
#define HASH_H

#include "pool.h"

#ifndef HAVE_HASH_HANDLE_T
#define HAVE_HASH_HANDLE_T
typedef POOL_HANDLE(hash_t) hash_handle_t;
#endif

#ifndef HAVE_SCALAR_HANDLE_T
#define HAVE_SCALAR_HANDLE_T
typedef POOL_HANDLE(scalar_t) scalar_handle_t;
#endif

typedef struct hash_t hash_t;
struct scalar_t;

int hash_pool_init(void);
int hash_pool_destroy(void);

hash_handle_t hash_allocate(void);
hash_handle_t hash_allocate_many(size_t);
hash_handle_t hash_reference(hash_handle_t);
int hash_release(hash_handle_t);

scalar_handle_t hash_key_item(hash_handle_t, const struct scalar_t *);
int hash_key_delete(hash_handle_t, const struct scalar_t *);
int hash_key_exists(hash_handle_t, const struct scalar_t *);

#endif
