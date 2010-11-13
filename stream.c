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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stream.h"
#include "util.h"

#define STREAM(handle)  POOL_OBJECT(stream_t, handle)

POOL_SOURCE_CONTENTS(stream_t);

int _stream_open_file(stream_t *restrict, flags_t, const char *restrict);
int _stream_open_pipe(stream_t *restrict, flags_t, const char *restrict);
void _stream_close(stream_t *);

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
int stream_open(stream_handle_t handle, flags_t flags, const char *arg) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    
    if (0 == POOL_LOCK(stream_t, handle)) {
        int status = 0;
        
        if ((STREAM(handle).m_flags & STREAM_TYPE_MASK) != STREAM_TYPE_UNDEF)  _stream_close(&STREAM(handle));

        switch (flags & STREAM_TYPE_MASK) {
            case STREAM_TYPE_FILE:
                status = _stream_open_file(&STREAM(handle), flags, arg);
                break;
            default:
                debug("unhandle stream type: %"PRIu32"\n", flags);
                return -1;
                break;
        }

        return status;
    }
    else {
        debug("failed to lock stream %"PRIuPTR"\n", handle);
        return -1;
    }
}

int stream_close(stream_handle_t handle) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    
    if (0 == POOL_LOCK(stream_t, handle)) {
        _stream_close(&STREAM(handle));
        POOL_UNLOCK(stream_t, handle);
        return 0;
    }
    else {
        debug("failed to lock stream %"PRIuPTR"\n", handle);
        return -1;
    }
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
size_t stream_read(stream_handle_t handle, char *buf, size_t bufsize) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    FIXME("do this properly");
    
    size_t status;
    if (0 == POOL_LOCK(stream_t, handle)) {
        assert(STREAM(handle).m_flags & STREAM_FLAG_READ);
    
        status = fread(buf, bufsize, 1, STREAM(handle).m_file);
        POOL_UNLOCK(stream_t, handle);
    }
    else {
        debug("couldn't lock stream_t handle %"PRIuPTR"\n", handle);
        status = 0;
    }
    
    return status;
}

/*
=item stream_write()

...

=cut
 */
int stream_write(stream_handle_t handle, const char *buf, size_t bufsize) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    assert(STREAM(handle).m_flags & STREAM_FLAG_WRITE);
    
    FIXME("do this properly");
    
    int status;
    if (0 == POOL_LOCK(stream_t, handle)) {
        status = fwrite(buf, bufsize, 1, STREAM(handle).m_file);
        POOL_UNLOCK(stream_t, handle);
    }
    else {
        status = -1;
    }

    return status;
}

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
    if ((self->m_flags & STREAM_TYPE_MASK) != STREAM_TYPE_UNDEF) _stream_close(self);
    memset(self, 0, sizeof(*self));
    return 0;
}

/*
=item _stream_open_file()

Opens an ordinary file according to flags provided.  Valid flags are STREAM_FLAG_READ, 
STREAM_FLAG_WRITE, STREAM_FLAG_TRUNC; other flags are ignored.

At least one of STREAM_FLAG_READ and STREAM_FLAG_WRITE must be provided.  If only one
of these are provided, the file is opened only reading or only writing respectively.

If both STREAM_FLAG_READ and STREAM_FLAG_WRITE are provided, then the file is opened
for both reading and writing at the beginning of the file.  If the STREAM_FLAG_TRUNC
flag is provided, the existing contents of the file will be truncated, and the file
position set to the start of the file; otherwise the existing contents will be left
alone and the file position set to the start of the file.

FIXME: what about appending to existing files?

=cut
*/
int _stream_open_file(stream_t *restrict self, flags_t flags, const char *restrict filename) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    
    flags_t truncate = flags & STREAM_FLAG_TRUNC;
    char *mode;
    switch (flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE)) {
        case STREAM_FLAG_READ:
            self->m_flags |= STREAM_FLAG_READ;
            mode = "r";
            break;
        case STREAM_FLAG_WRITE:
            self->m_flags |= STREAM_FLAG_WRITE;
            mode = "w";
            break;
        case STREAM_FLAG_READ | STREAM_FLAG_WRITE:
            self->m_flags |= STREAM_FLAG_READ | STREAM_FLAG_WRITE | truncate;
            mode = (truncate ? "w+" : "r+");
            break;
        default:
            debug("no read or write mode specified: %"PRIu32"\n", flags);
            return -1;
    }
    
    if (NULL != (self->m_file = fopen(filename, mode))) {
        fcntl(fileno(self->m_file), F_SETFD, 1);
        self->m_meta.filename = strdup(filename);
        self->m_flags |= STREAM_TYPE_FILE;
        return 0;
    }
    else {
        self->m_flags = STREAM_TYPE_UNDEF;
        return -1;
    }
}

/*
=item _stream_open_pipe()

Opens a pipe to or from the the specified command, according to the specified flags.

Flags must have exactly one of STREAM_FLAG_READ or STREAM_FLAG_WRITE set.  If STREAM_FLAG_READ
is set, the pipe is attached to the standard output of the command.  If STREAM_FLAG_WRITE is
set, the pipe is attached to the standard input of the command.

The command is interpreted by /bin/sh.

=cut
*/
int _stream_open_pipe(stream_t *restrict self, flags_t flags, const char *restrict command) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    
    flags_t flag_mode = flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE);
    int fildes[2], pid, child_end, parent_end;
    char *stream_mode;
    
    if (flag_mode == STREAM_FLAG_READ) {
        child_end = 1;
        parent_end = 0;
        stream_mode = "r";
    }
    else if (flag_mode == STREAM_FLAG_WRITE) {
        child_end = 0;
        parent_end = 1;
        stream_mode = "w";
    }
    else {
        debug("invalid flags: %"PRIu32"\n", flags);
        return -1;
    }

    if (0 == pipe(fildes)) {
        if (-1 == (pid = fork())) {
            debug("fork() failed with error %i\n", errno);
            return -1;
        }
        else if (pid == 0) {    /* child */
            close(fildes[parent_end]);
            dup2(fildes[child_end], child_end);
            close(fildes[child_end]);
            execl("/bin/sh", "sh", "-c", command, NULL);
            debug("execl failed with error %i\n", errno);
            _exit(1);
        }
        else {                  /* parent */
            close(fildes[child_end]);
            if (NULL != (self->m_file = fdopen(fildes[parent_end], stream_mode))) {
                fcntl(fileno(self->m_file), F_SETFD, 1);
                self->m_meta.child_pid = pid;
                self->m_flags = STREAM_TYPE_PIPE | flag_mode;
                return 0;
            }
            else {
                debug("fdopen failed with error %i\n", errno);
                close(fildes[parent_end]);
                debug("waiting for child pid %i to terminate...\n", pid);
                while (waitpid(self->m_meta.child_pid, NULL, 0) == -1 && errno == EINTR) {
                    ;
                }
                debug("child pid %i terminated\n", pid);
                return -1;
            }
        }
    }
    else {
        debug("pipe failed with error %i\n", errno);
        return -1;
    }
}

/*
=item _stream_close()

Closes a stream and makes it safe for possible re-use or destruction.

=cut
*/
void _stream_close(stream_t *self) {
    assert(self != NULL);
    
    switch (self->m_flags & STREAM_TYPE_MASK) {
        case STREAM_TYPE_FILE:
            fclose(self->m_file);
            free(self->m_meta.filename);
            break;
        case STREAM_TYPE_PIPE:
            fclose(self->m_file);
            debug("waiting for child pid %i to terminate...\n", self->m_meta.child_pid);
            while (waitpid(self->m_meta.child_pid, NULL, 0) == -1 && errno == EINTR) {
                ;
            }
            debug("child pid %i terminated\n", self->m_meta.child_pid);
            break;
        default:
            debug("unhandled stream type: %"PRIu32"\n", self->m_flags);
            assert("shouldn't get here" == 0);
    }
    
    self->m_flags = STREAM_TYPE_UNDEF;
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
