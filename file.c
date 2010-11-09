/*
 *  file.c
 *  dang
 *
 *  Created by Ellie on 9/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
=head1 NAME

file

=head1 INTRODUCTION

...

=head1 PUBLIC INTERFACE

=over

=cut 
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"

POOL_SOURCE_CONTENTS(file_t);

/*
=item file_pool_init()

=item file_pool_destroy()

Setup and tear down functions for the file pool

=cut
 */
int file_pool_init(void);
int file_pool_destroy(void);

/*
=item file_allocate()

=item file_allocate_many()

=item file_reference()

=item file_release()

Functions for managing file allocations.
=cut
 */
file_handle_t file_allocate(void);
file_handle_t file_allocate_many(size_t);
file_handle_t file_reference(file_handle_t);
int file_release(file_handle_t);

/*
=item file_open()

=item file_close()

Functions for opening and closing files

=cut
 */
int file_open(file_handle_t, const char *);
int file_close(file_handle_t);

/*
=item file_read_until_byte()

...

=cut
 */
int file_read_until_byte(file_handle_t, char **, size_t *, int);

/*
=item file_read_len()

...

=cut
 */
int file_read_len(file_handle_t, char **, size_t *, size_t);

/*
=item file_write()

...

=cut
 */
int file_write(file_handle_t, const char *, size_t);

/*
=item file_get_filename()

Returns the filename associated with the file (if any).

=cut
 */
const char *file_get_filename(file_handle_t);


/*
=back

=head1 PRIVATE INTERFACE

=over

=cut
 */

/*
=item _file_init()

=item _file_destroy()

Setup and teardown functions for file_t objects.

=cut
 */
int _file_init(file_t *);
int _file_destroy(file_t *);

/*
=back

=cut
 */
