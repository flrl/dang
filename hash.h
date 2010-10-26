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

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#endif
#define POOL_INITIAL_SIZE 64
#include "pool.h"

#ifndef HAVE_HASH_HANDLE_T
#define HAVE_HASH_HANDLE_T
typedef POOL_HANDLE(hash_t) hash_handle_t;
#endif

#ifndef HAVE_SCALAR_HANDLE_T
#define HAVE_SCALAR_HANDLE_T
typedef POOL_HANDLE(scalar_t) scalar_handle_t;
#endif

#define HASH_BUCKETS    (256)

typedef struct hash_item_t {
    char *m_key;
    scalar_handle_t m_value;
    struct hash_item_t *m_next_item;
} hash_item_t;

typedef struct hash_bucket_t {
    size_t m_count;
    hash_item_t *m_first_item;
} hash_bucket_t;

typedef struct hash_t {
    hash_bucket_t m_buckets[HASH_BUCKETS];
} hash_t;

int _hash_init(hash_t *);
int _hash_destroy(hash_t *);

POOL_HEADER_CONTENTS(hash_t, _hash_init, _hash_destroy);

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
