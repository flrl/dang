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
int io_pool_init(void);
int io_pool_destroy(void);

/*
=item io_allocate()

=item io_allocate_many()

=item io_reference()

=item io_release()

Functions for managing io allocations.
=cut
 */
io_handle_t io_allocate(void);
io_handle_t io_allocate_many(size_t);
io_handle_t io_reference(io_handle_t);
int io_release(io_handle_t);

/*
=item io_open()

=item io_close()

Functions for opening and closing ios

=cut
 */
int io_open(io_handle_t, const char *);
int io_close(io_handle_t);

/*
=item io_read_until_byte()

...

=cut
 */
int io_read_until_byte(io_handle_t, char **, size_t *, int);

/*
=item io_read_len()

...

=cut
 */
int io_read_len(io_handle_t, char **, size_t *, size_t);

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
int _io_init(io_t *);
int _io_destroy(io_t *);

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
