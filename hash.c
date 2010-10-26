/*
 *  hash.c
 *  dang
 *
 *  Created by Ellie on 24/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 NAME

hash

=head1 INTRODUCTION

Hash keys are scalars, which are converted to their string representation for lookup.  

This means, for example, that when used as keys, the integer 1 and the string "1" would both refer to the same item.  

This also means that it's generally not useful to use a reference type as a hash key.

=head1 PUBLIC INTERFACE

=over

=cut 
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scalar.h"

#include "hash.h"

#ifdef POOL_INITIAL_SIZE
#undef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE (64)
#endif
#include "pool.h"

#define HASH(handle)    POOL_OBJECT(hash_t, handle)

static int _hash_bucket_init(hash_bucket_t *);
static int _hash_bucket_destroy(hash_bucket_t *);
static int _hash_item_init(hash_item_t *, const char *);
static int _hash_item_destroy(hash_item_t *);

static scalar_handle_t _hash_key_item_unlocked(hash_t *, const char *);
static int _hash_key_delete_unlocked(hash_t *, const char *);
static int _hash_key_exists_unlocked(hash_t *, const char *);
static inline uint32_t _hash_key(const char *);

POOL_SOURCE_CONTENTS(hash_t);

/*
=item hash_pool_init()

=item hash_pool_destroy()

Setup and teardown functions for the hash pool

=cut
 */
int hash_pool_init(void) {
    return POOL_INIT(hash_t);
}

int hash_pool_destroy(void) {
    return POOL_DESTROY(hash_t);
}

/*
=item hash_allocate()

=item hash_allocate_many()

=item hash_reference()

=item hash_release()

Functions for managing allocation of hashes.

=cut
 */
hash_handle_t hash_allocate(void) {
    return POOL_ALLOCATE(hash_t, 0);
}

hash_handle_t hash_allocate_many(size_t many) {
    return POOL_ALLOCATE_MANY(hash_t, many, 0);
}

hash_handle_t hash_reference(hash_handle_t handle) {
    return POOL_REFERENCE(hash_t, handle);
}

int hash_release(hash_handle_t handle) {
    return POOL_RELEASE(hash_t, handle);
}

/*
=item hash_key_item()

Returns a handle to the item in the hash with the given scalar key.  If the key is not currently defined, 
it is first automatically created.

The caller must release the returned handle with C<scalar_release()> when they are done with it.
=cut
 */
scalar_handle_t hash_key_item(hash_handle_t handle, const struct scalar_t *key) {
    assert(POOL_HANDLE_VALID(hash_t, handle));
    assert(key != NULL);
    
    if (0 == POOL_LOCK(hash_t, handle)) {
        char *skey;
        anon_scalar_get_string_value(key, &skey);
        scalar_handle_t item = _hash_key_item_unlocked(&POOL_OBJECT(hash_t, handle), skey);
        free(skey);
        POOL_UNLOCK(hash_t, handle);
        return item;
    }
    else {
        return 0;
    }
}

/*
=item hash_key_delete()

Deletes the item in the hash with the given scalar key.  If the key is not currently defined, this does nothing.

=cut 
 */
int hash_key_delete(hash_handle_t handle, const struct scalar_t *key) {
    assert(POOL_HANDLE_VALID(hash_t, handle));
    assert(key != NULL);
    
    int status;
    if (0 == POOL_LOCK(hash_t, handle)) {
        char *skey;
        anon_scalar_get_string_value(key, &skey);
        status = _hash_key_delete_unlocked(&POOL_OBJECT(hash_t, handle), skey);
        free(skey);
        POOL_UNLOCK(hash_t, handle);
    }
    else {
        status = -1;
    }
    return status;
}

/*
=item hash_key_exists()

Checks whether a given key exists in the hash or not.  Returns 1 if it exists, 0 if it does not.

=cut
 */
int hash_key_exists(hash_handle_t handle, const struct scalar_t *key) {
    assert(POOL_HANDLE_VALID(hash_t, handle));
    assert(key != NULL);
    
    int status = 0;
    if (0 == POOL_LOCK(hash_t, handle)) {
        char *skey;
        anon_scalar_get_string_value(key, &skey);
        status = _hash_key_exists_unlocked(&POOL_OBJECT(hash_t, handle), skey);
        free(skey);
        POOL_UNLOCK(hash_t, handle);
    }
    return status;
}


/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
 */


/*
=item _hash_init()

=item _hash_destroy()

Setup and teardown functions for hash_t objects

=cut
 */
int _hash_init(hash_t *self) {
    assert(self != NULL);
    
    for (size_t i = 0; i < HASH_BUCKETS; i++) {
        _hash_bucket_init(&self->m_buckets[i]);
    }

    return 0;
}

int _hash_destroy(hash_t *self) {
    assert(self != NULL);
    
    for (size_t i = 0; i < HASH_BUCKETS; i++) {
        _hash_bucket_destroy(&self->m_buckets[i]);
    }
    
    return 0;
}

/*
=item _hash_bucket_init()

=item _hash_bucked_destroy()

Setup and teardown for hash_bucket_t objects

=cut
 */
static int _hash_bucket_init(hash_bucket_t *self) {
    memset(self, 0, sizeof(*self));
    return 0;
}

static int _hash_bucket_destroy(hash_bucket_t *self) {
    if (self->m_count > 0) {
        hash_item_t *item = self->m_first_item;
        while (item != NULL) {
            hash_item_t *tmp = item->m_next_item;
            _hash_item_destroy(item);
            free(item);
            item = tmp;
        }
    }
    return 0;
}

/*
=item _hash_item_init()

=item _hash_item_destroy()

Setup and teardown functions for hash_item_t objects

=cut
 */
static int _hash_item_init(hash_item_t *self, const char *key) {
    assert(self != NULL);
    assert(key != NULL);
    
    self->m_key = strdup(key);
    self->m_value = scalar_allocate(0); // FIXME flags
    self->m_next_item = NULL;
    return 0;
}

static int _hash_item_destroy(hash_item_t *self) {
    assert(self != NULL);
    
    free(self->m_key);
    scalar_release(self->m_value);
    memset(self, 0, sizeof(*self));
    return 0;
}
    
/*
=item _hash_key_item_unlocked()

Looks up an item with the given key, and returns a reference to its value.  If the key does not currently exist, it
is automatically created.

The caller must release the handle returned using C<scalar_release()> when they are done with it.
=cut
 */
static scalar_handle_t _hash_key_item_unlocked(hash_t *self, const char *key) {
    assert(self != NULL);
    assert(key != NULL);
    
    hash_bucket_t *bucket = &self->m_buckets[_hash_key(key) % HASH_BUCKETS];
    hash_item_t *item = bucket->m_first_item;
    hash_item_t *prev = NULL;
    scalar_handle_t handle = 0;    

    while (item != NULL) {
        int cmp = strcmp(item->m_key, key);
        if (cmp == 0) {         // found it
            handle = scalar_reference(item->m_value);
            break;
        }
        else if (cmp > 0) {     // have gone past where it should have been: auto-vivify it
            hash_item_t *new_item = calloc(1, sizeof(*new_item));
            if (new_item != NULL) {
                _hash_item_init(new_item, key);
                new_item->m_next_item = item;
                if (prev != NULL) {
                    prev->m_next_item = new_item;
                }
                else {
                    bucket->m_first_item = new_item;
                }
                handle = scalar_reference(new_item->m_value);
            }
            break;
        }
        else {                  // keep looking
            prev = item;
            item = item->m_next_item;
        }
    }
    
    return handle;
}

/*
=item _hash_key_delete_unlocked()

Finds the key in the hash, and if it exists, deletes it.  If it does not exist, does nothing, and considers this to be a success.

Returns 0 on success, non-zero on failure.

=cut
 */
static int _hash_key_delete_unlocked(hash_t *self, const char *key) {
    assert(self != NULL);
    assert(key != NULL);
    
    hash_bucket_t *bucket = &self->m_buckets[_hash_key(key) % HASH_BUCKETS];
    if (bucket->m_count == 0)  return 0;
    
    hash_item_t *item = bucket->m_first_item;
    hash_item_t *prev = NULL;
    
    int status = 0;
    while (item != NULL) {
        int cmp = strcmp(item->m_key, key);
        if (cmp == 0) {     // found it
            if (prev != NULL) {
                prev->m_next_item = item->m_next_item;
            }
            else {
                bucket->m_first_item = item->m_next_item;
            }
            _hash_item_destroy(item);
            break;
        }
        else if (cmp > 0) { // gone past where it should be -- doesn't exist
            break;
        }
        else {              // keep looking
            prev = item;
            item = item->m_next_item;
        }        
    }

    return status;
}

/*
=item _hash_key_exists_unlocked()

Check for existence of a key in a hash.  

Returns 1 if the key exists, or 0 if it does not.

=cut
 */
static int _hash_key_exists_unlocked(hash_t *self, const char *key) {
    assert(self != NULL);
    assert(key != NULL);
    
    hash_bucket_t *bucket = &self->m_buckets[_hash_key(key) % HASH_BUCKETS];
    if (bucket->m_count == 0)  return 0;
    
    hash_item_t *item = bucket->m_first_item;
    
    int found = 0;
    while (item != NULL) {
        int cmp = strcmp(item->m_key, key);
        if (cmp == 0) {
            found = 1;
            break;
        }
        else if (cmp > 0) {
            break;
        }
        else {
            item = item->m_next_item;
        }
    }
    return found;
}

/*
=item _hash_key()

Hashes a string key into a uint32_t.

=cut
 * http://www.burtleburtle.net/bob/hash/doobs.html
 * one at a time hash: it's good enough for perl
 */
static inline uint32_t _hash_key(const char *key) {
    const uint8_t *p = (const uint8_t *) key;
    const size_t len = strlen(key);

    uint32_t hash = 0;    
    for (size_t i = 0; i < len; ++i) {
        hash += p[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

/*
=back

=cut
 */
