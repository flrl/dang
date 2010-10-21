/*
 *  channel.h
 *  dang
 *
 *  Created by Ellie on 1/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <pthread.h>

#include "scalar.h"
#include "pool.h"

typedef POOL_HANDLE(channel_t) channel_handle_t;

int channel_pool_init(void);
int channel_pool_destroy(void);

channel_handle_t channel_allocate(void);
int channel_release(channel_handle_t);
int channel_increase_refcount(channel_handle_t);

int channel_read(channel_handle_t, scalar_t *);
int channel_tryread(channel_handle_t, scalar_t *);
int channel_write(channel_handle_t, const scalar_t *);

#if 0
/*
=head1 channel.h

=over

=item channel_init()

=item channel_destroy()

Setup and teardown functions for channel_t objects
 
=cut
 */
int channel_init(channel_t *);
int channel_destroy(channel_t *);

/*
=item channel_read()
 
=item channel_tryread()

Functions for reading from a channel

=cut
 */
int channel_read(channel_t *, scalar_t *);
int channel_tryread(channel_t *, scalar_t *);

/*
=item channel_write()

Write to a channel

=cut
 */
int channel_write(channel_t *, const scalar_t *);

#endif

#endif
/*
=back

=cut
*/
