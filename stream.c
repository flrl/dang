/*
 *  stream.c
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 NAME

stream

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut 
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stream.h"
#include "util.h"

#define STREAM(handle)  POOL_OBJECT(stream_t, handle)

POOL_SOURCE_CONTENTS(stream_t);

/*
=item stream_pool_init()

=item stream_pool_destroy()

Setup and tear down functions for the stream pool

=cut
 */
int stream_pool_init(void) {
    FIXME("set up stdin/stdout/sterr handles here\n");
    return POOL_INIT(stream_t);
}

int stream_pool_destroy(void) {
    return POOL_DESTROY(stream_t);
}

/*
=item stream_allocate()

=item stream_allocate_many()

=item stream_reference()

=item stream_release()

Functions for managing stream allocations.
=cut
 */
stream_handle_t stream_allocate(void) {
    return POOL_ALLOCATE(stream_t, POOL_OBJECT_FLAG_SHARED);
}

stream_handle_t stream_allocate_many(size_t n) {
    return POOL_ALLOCATE_MANY(stream_t, n, POOL_OBJECT_FLAG_SHARED);
}

stream_handle_t stream_reference(stream_handle_t handle) {
    return POOL_REFERENCE(stream_t, handle);
}

int stream_release(stream_handle_t handle) {
    return POOL_RELEASE(stream_t, handle);
}

/*
=item stream_open()

=item stream_close()

Functions for opening and closing streams

=cut
 */
int stream_open(stream_handle_t handle, const char *filename, flags_t flags) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    
    FIXME("finish this\n");
    return -1;
}

int stream_close(stream_handle_t handle) {
    FIXME("finish this\n");
    return -1;
}

/*
=item stream_read_delim()

...

=cut
 */
ssize_t stream_read_delim(stream_handle_t handle, char **result, int delimiter) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    FIXME("make sure it's open for reading\n");
    ssize_t status;

    if (0 == POOL_LOCK(stream_t, handle)) {
        char *buf = NULL;
        size_t bufsize = 0;
        if ((status = getdelim(&buf, &bufsize, delimiter, STREAM(handle).m_file)) > 0) {
            *result = buf;
        }
        else {
            debug("getdelim returned %ld\n", status);
            FIXME("what to do here?\n");
        }
        POOL_UNLOCK(stream_t, handle);
    }
    else {
        debug("couldn't lock stream_t handle %"PRIuPTR"\n", handle);
        status = -1;
    }
    
    return status;
}

/*
=item stream_read()

...

=cut
 */
ssize_t stream_read(stream_handle_t, char *, size_t);

/*
=item stream_write()

...

=cut
 */
int stream_write(stream_handle_t, const char *, size_t);

/*
=item stream_get_filename()

Returns the streamname associated with the stream (if any).

=cut
 */
const char *stream_get_filename(stream_handle_t);


/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
 */

/*
=item _stream_init()

=item _stream_destroy()

Setup and teardown functions for stream_t objects.

=cut
 */
int _stream_init(stream_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    return 0;
}

int _stream_destroy(stream_t *self) {
    assert(self != NULL);
    if (self->m_filename)   free(self->m_filename);
    if (self->m_file)       fclose(self->m_file);
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item _stream_bind()

=item _stream_unbind()

Functions for binding and unbinding an existing FILE object and a stream_t object.

=cut
 */
int _stream_bind(stream_t *, int);
int _stream_unbind(stream_t *, int);

/*
=back

=cut
 */
