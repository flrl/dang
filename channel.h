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

typedef struct channel_t {
    size_t m_allocated_count;
    size_t m_count;
    anon_scalar_t *m_items;
    size_t m_start;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_has_items;
    pthread_cond_t m_has_space;
} channel_t;

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
int channel_read(channel_t *, anon_scalar_t *);
int channel_tryread(channel_t *, anon_scalar_t *);

/*
=item channel_write()

Write to a channel

=cut
 */
int channel_write(channel_t *, const anon_scalar_t *);

#endif
/*
=back

=cut
*/
