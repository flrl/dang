/*
 *  channel.c
 *  dang
 *
 *  Created by Ellie on 1/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <sys/errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "scalar.h"

#include "channel.h"

#define CHANNEL(handle)         POOL_OBJECT(channel_t, (handle))
#define CHANNEL_MUTEX(handle)   POOL_WRAPPER(channel_t, (handle)).m_mutex

POOL_SOURCE_CONTENTS(channel_t);

/*
=head1 NAME

channel

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut
 */

static const size_t _channel_initial_size = 16;

static int _channel_reserve_unlocked(channel_t *, size_t);

static inline int _channel_lock(channel_handle_t);
static inline int _channel_unlock(channel_handle_t);

/*
=item channel_pool_init()

=item channel_pool_destroy()

Setup and teardown functions for the channel pool

=cut
 */
int channel_pool_init(void) {
    return POOL_INIT(channel_t);
}

int channel_pool_destroy(void) {
    return POOL_DESTROY(channel_t);
}

/*
=item channel_allocate()

Allocate a channel object and return a handle to it

=cut
 */
channel_handle_t channel_allocate(void) {
    return POOL_ALLOCATE(channel_t, POOL_OBJECT_FLAG_SHARED);
}

/*
=item channel_allocate_many()

Allocate a number of channels with sequential handles, and return the first handle.

The caller must explicitly release each handle in the sequence.

=cut
 */
channel_handle_t channel_allocate_many(size_t count) {
    return POOL_ALLOCATE_MANY(channel_t, count, POOL_OBJECT_FLAG_SHARED);
}

/*
=item channel_reference()

Get a handle to an existing channel (increasing its reference count).  The handle must be released with C<channel_release()>.

=cut
 */
channel_handle_t channel_reference(channel_handle_t handle) {
    return POOL_REFERENCE(channel_t, handle);
}

/*
=item channel_release()

Release a channel handle, decrementing the objects refcount and causing its destruction if it reaches zero.

=cut
 */
int channel_release(channel_handle_t handle) {
    return POOL_RELEASE(channel_t, handle);
}

/*
=item channel_read()

Read a scalar from a channel.  If the channel is currently empty, blocks until an item arrives or the channel is destroyed.

=cut
*/
int channel_read(channel_handle_t handle, scalar_t *result) {
    assert(POOL_HANDLE_VALID(channel_t, handle));
    assert(POOL_HANDLE_IN_USE(channel_t, handle));
    assert(result != NULL);
    
    if (0 == _channel_lock(handle)) {
        while (CHANNEL(handle).m_count == 0) {
            pthread_cond_wait(&CHANNEL(handle).m_has_items, CHANNEL_MUTEX(handle));
            FIXME("what if the condition object is destroyed while we're waiting?\n");
        }
        anon_scalar_assign(result, &CHANNEL(handle).m_items[CHANNEL(handle).m_start]);
        CHANNEL(handle).m_start = (CHANNEL(handle).m_start + 1) % CHANNEL(handle).m_allocated_count;
        CHANNEL(handle).m_count--;
        _channel_unlock(handle);
        
        pthread_cond_signal(&CHANNEL(handle).m_has_space);
        return 0;
    }
    else {
        return -1;
    }
}

/*
=item channel_tryread()

Read a scalar from a channel if there is one queued waiting.

Returns 0 if a scalar was read, EWOULDBLOCK if the read operation would block, or -1 if something goes wrong.  Does not modify
result unless a scalar is successfully read.

=cut
 */
int channel_tryread(channel_handle_t handle, scalar_t *result) {
    assert(POOL_HANDLE_VALID(channel_t, handle));
    assert(POOL_HANDLE_IN_USE(channel_t, handle));
    assert(result != NULL);
    
    if (0 == _channel_lock(handle)) {
        int status;
        if (CHANNEL(handle).m_count > 0) {
            anon_scalar_assign(result, &CHANNEL(handle).m_items[CHANNEL(handle).m_start]);
            CHANNEL(handle).m_start = (CHANNEL(handle).m_start + 1) % CHANNEL(handle).m_allocated_count;
            CHANNEL(handle).m_count--;
            status = 0;
        }
        else {
            status = EWOULDBLOCK;
        }
        _channel_unlock(handle);
        return status;
    }
    else {
        return -1;
    }
}

/*
=item channel_write()

Writes a scalar to a channel.  If the channel is currently at capacity, blocks until either space is available, or wait_timeout
expires.  If the timeout expires, allocates additional space in the channel, writes the scalar and returns.

=cut
 */
int channel_write(channel_handle_t handle, const scalar_t *value) {
    assert(POOL_HANDLE_VALID(channel_t, handle));
    assert(POOL_HANDLE_IN_USE(channel_t, handle));
    assert(value != NULL);
    
    struct timespec wait_timeout = {0};
    wait_timeout.tv_sec = time(NULL) + 10;
    
    if (0 == _channel_lock(handle)) {
        while (CHANNEL(handle).m_count >= CHANNEL(handle).m_allocated_count) {
            debug("channel %"PRIuPTR" is full, waiting for space to become available...\n", handle);
            if (ETIMEDOUT == pthread_cond_timedwait(&CHANNEL(handle).m_has_space, CHANNEL_MUTEX(handle), &wait_timeout)) {
                _channel_reserve_unlocked(&CHANNEL(handle), CHANNEL(handle).m_allocated_count * 2);
            }
        }
        size_t index = (CHANNEL(handle).m_start + CHANNEL(handle).m_count) % CHANNEL(handle).m_allocated_count;
        anon_scalar_clone(&CHANNEL(handle).m_items[index], value);
        CHANNEL(handle).m_count++;
        _channel_unlock(handle);
        
        pthread_cond_signal(&CHANNEL(handle).m_has_items);
        return 0;        
    }
    else {
        return -1;
    }
}

/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
*/

/*
=item _channel_init()

=item _channel_destroy()

Setup and teardown functions for channel_t objects

=cut
 */
int _channel_init(channel_t *self) {
    assert(self != NULL);
    
    if (0 == pthread_cond_init(&self->m_has_items, NULL)) {
        if (0 == pthread_cond_init(&self->m_has_space, NULL)) {
            if (NULL != (self->m_items = calloc(_channel_initial_size, sizeof(scalar_t)))) {
                self->m_allocated_count = _channel_initial_size;
                self->m_start = 0;
                self->m_count = 0;
                return 0;
            }
        }
        else {
            pthread_cond_destroy(&self->m_has_items);
        }
    }
    
    debug("couldn't initialise channel");
    return -1;
}

int _channel_destroy(channel_t *self) {
    assert(self != NULL);
    
    for (size_t i = 0; i < self->m_count; i++) {
        anon_scalar_destroy(&self->m_items[(self->m_start + self->m_count) % self->m_allocated_count]);
    }
    free(self->m_items);

    self->m_items = NULL;
    self->m_allocated_count = self->m_count = self->m_start = 0;    
    pthread_cond_destroy(&self->m_has_items);
    pthread_cond_destroy(&self->m_has_space);
    
    return 0;
}

/*
=item _channel_lock()

=item _channel_unlock()

Lock and unlock a channel_t object

=cut
*/
static inline int _channel_lock(channel_handle_t handle) {
    assert(POOL_HANDLE_VALID(channel_t, handle));

    return POOL_LOCK(channel_t, handle);
}

static inline int _channel_unlock(channel_handle_t handle) {
    assert(POOL_HANDLE_VALID(channel_t, handle));
    
    return POOL_UNLOCK(channel_t, handle);
}

/*
=item _channel_reserve_unlocked()

Allocate additional space in a channel object such that the channel has space for at least new_size items.

=cut
*/
static int _channel_reserve_unlocked(channel_t *self, size_t new_size) {
    assert(self != NULL);
    assert(new_size != 0);

    debug("reserving space for %zd items\n", new_size);
    
    if (self->m_allocated_count >= new_size)  return 0;
    
    scalar_t *new_ringbuf = calloc(new_size, sizeof(*new_ringbuf));
    if (new_ringbuf == NULL)  return -1;
    
    size_t straight_count = self->m_allocated_count - self->m_start > self->m_count ? self->m_count : self->m_allocated_count - self->m_start;
    size_t rotated_count = self->m_count - straight_count;
    memcpy(&new_ringbuf[0], &self->m_items[self->m_start], straight_count * sizeof(scalar_t));
    memcpy(&new_ringbuf[straight_count], &self->m_items[0], rotated_count * sizeof(scalar_t));
    free(self->m_items);
    self->m_allocated_count = new_size;
    self->m_items = new_ringbuf;
    self->m_start = 0;
    return 0;
}


/*
=back

=cut
*/
