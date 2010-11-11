/*
 *  io.c
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 NAME

io

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut 
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"

POOL_SOURCE_CONTENTS(io_t);

/*
=item io_pool_init()

=item io_pool_destroy()

Setup and tear down functions for the io pool

=cut
 */
int io_pool_init(void) {
    FIXME("set up stdin/stdout/sterr handles here\n");
    return POOL_INIT(io_t);
}

int io_pool_destroy(void) {
    return POOL_DESTROY(io_t);
}

/*
=item io_allocate()

=item io_allocate_many()

=item io_reference()

=item io_release()

Functions for managing io allocations.
=cut
 */
io_handle_t io_allocate(void) {
    return POOL_ALLOCATE(io_t, POOL_OBJECT_FLAG_SHARED);
}

io_handle_t io_allocate_many(size_t n) {
    return POOL_ALLOCATE_MANY(io_t, n, POOL_OBJECT_FLAG_SHARED);
}

io_handle_t io_reference(io_handle_t handle) {
    return POOL_REFERENCE(io_t, handle);
}

int io_release(io_handle_t handle) {
    return POOL_RELEASE(io_t, handle);
}

/*
=item io_open()

=item io_close()

Functions for opening and closing ios

=cut
 */
int io_open(io_handle_t handle, const char *filename, flags_t flags) {
    assert(POOL_HANDLE_VALID(io_t, handle));
    assert(POOL_HANDLE_IN_USE(io_t, handle));
    
    FIXME("finish this\n");
    return -1;
}

int io_close(io_handle_t handle) {
    FIXME("finish this\n");
    return -1;
}

/*
=item io_read_until()

...

=cut
 */
ssize_t io_read_until(io_handle_t, char **, int);

/*
=item io_read()

...

=cut
 */
ssize_t io_read(io_handle_t, char *, size_t);

/*
=item io_write()

...

=cut
 */
int io_write(io_handle_t, const char *, size_t);

/*
=item io_get_filename()

Returns the ioname associated with the io (if any).

=cut
 */
const char *io_get_filename(io_handle_t);


/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
 */

/*
=item _io_init()

=item _io_destroy()

Setup and teardown functions for io_t objects.

=cut
 */
int _io_init(io_t *self) {
    assert(self != NULL);
    memset(self, 0, sizeof(*self));
    return 0;
}

int _io_destroy(io_t *self) {
    assert(self != NULL);
    if (self->m_filename)   free(self->m_filename);
    if (self->m_file)       fclose(self->m_file);
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item _io_bind()

=item _io_unbind()

Functions for binding and unbinding an existing FILE object and a io_t object.

=cut
 */
int _io_bind(io_t *, int);
int _io_unbind(io_t *, int);

/*
=back

=cut
 */
