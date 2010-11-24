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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stream.h"
#include "util.h"

#define STREAM(handle)  POOL_OBJECT(stream_t, handle)

POOL_SOURCE_CONTENTS(stream_t);

int _stream_open_file(stream_t *restrict, flags32_t, const string_t *restrict);
int _stream_open_pipe(stream_t *restrict, flags32_t, const string_t *restrict);
int _stream_open_socket(stream_t *restrict, flags32_t, const string_t *restrict);
void _stream_close(stream_t *);
int _stream_bind(stream_t *, flags32_t, int);
int _stream_unbind(stream_t *, int);
int _stream_parse_socket_dest(const string_t *restrict, string_t **restrict, string_t **restrict);


static stream_handle_t _stream_stdin_handle = 0;
static stream_handle_t _stream_stdout_handle = 0;
static stream_handle_t _stream_stderr_handle = 0;

/*
=item stream_pool_init()

=item stream_pool_destroy()

Setup and tear down functions for the stream pool

=cut
 */
int stream_pool_init(void) {
    int status = POOL_INIT(stream_t);
    
    _stream_stdin_handle = POOL_ALLOCATE(stream_t, POOL_OBJECT_FLAG_SHARED);
    _stream_bind(&STREAM(_stream_stdin_handle), STREAM_FLAG_READ, STDIN_FILENO);
    
    _stream_stdout_handle = POOL_ALLOCATE(stream_t, POOL_OBJECT_FLAG_SHARED);
    _stream_bind(&STREAM(_stream_stdout_handle), STREAM_FLAG_WRITE, STDOUT_FILENO);
    
    _stream_stderr_handle = POOL_ALLOCATE(stream_t, POOL_OBJECT_FLAG_SHARED);
    _stream_bind(&STREAM(_stream_stderr_handle), STREAM_FLAG_WRITE, STDERR_FILENO);
    
    return status;
}

int stream_pool_destroy(void) {
    POOL_RELEASE(stream_t, _stream_stdin_handle);
    POOL_RELEASE(stream_t, _stream_stderr_handle);
    POOL_RELEASE(stream_t, _stream_stdout_handle);

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
=item stream_stdin_handle()

=item stream_stdout_handle()

=item stream_stderr_handle()

Return handles to stdin, stdout or stderr.

=cut
*/
stream_handle_t stream_stdin_handle(void) {
    return POOL_REFERENCE(stream_t, _stream_stdin_handle);
}

stream_handle_t stream_stdout_handle(void) {
    return POOL_REFERENCE(stream_t, _stream_stdout_handle);
}

stream_handle_t stream_stderr_handle(void) {
    return POOL_REFERENCE(stream_t, _stream_stderr_handle);
}

/*
=item stream_open()

=item stream_close()

Functions for opening and closing streams

=cut
 */
int stream_open(stream_handle_t handle, flags32_t flags, const string_t *arg) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    
    if (0 == POOL_LOCK(stream_t, handle)) {
        int status = 0;
        
        if ((STREAM(handle).m_flags & STREAM_TYPE_MASK) != STREAM_TYPE_UNDEF)  _stream_close(&STREAM(handle));

        switch (flags & STREAM_TYPE_MASK) {
            case STREAM_TYPE_FILE:
                status = _stream_open_file(&STREAM(handle), flags, arg);
                break;
            case STREAM_TYPE_PIPE:
                status = _stream_open_pipe(&STREAM(handle), flags, arg);
                break;
            case STREAM_TYPE_SOCK:
                status = _stream_open_socket(&STREAM(handle), flags, arg);
                break;
            default:
                debug("unhandle stream type: %"PRIu32"\n", flags);
                status = -1;
                break;
        }

        POOL_UNLOCK(stream_t, handle);
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

Reads from the stream until the delimiter is encountered.  Returns the string read including the delimiter character.

=cut
 */
string_t *stream_read_delim(stream_handle_t handle, int delimiter) {
    assert(POOL_HANDLE_VALID(stream_t, handle));

    string_t *string = NULL;
    
    if (0 == POOL_LOCK(stream_t, handle)) {
        assert(POOL_HANDLE_IN_USE(stream_t, handle));
        assert(STREAM(handle).m_flags & STREAM_FLAG_READ);

        char *buf = NULL;
        size_t bufsize = 0;
        ssize_t len;
        if ((len = getdelim_ext(&buf, &bufsize, delimiter, STREAM(handle).m_file)) > 0) {
            string = string_alloc(len, buf);
            free(buf);
            buf = NULL;
        }

        if (buf != NULL)  free(buf);

        POOL_UNLOCK(stream_t, handle);
    }
    else {
        debug("couldn't lock stream_t handle %"PRIuPTR"\n", handle);
    }
    
    return string;
}

/*
=item stream_read()

Reads up to nbytes byte from a stream.  Returns the string read.

=cut
 */
string_t *stream_read(stream_handle_t handle, size_t bytes) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    assert(POOL_HANDLE_IN_USE(stream_t, handle));
    FIXME("do this properly\n");
    
    string_t *string = NULL;

    if (0 == POOL_LOCK(stream_t, handle)) {
        assert(STREAM(handle).m_flags & STREAM_FLAG_READ);
    
        char *buf = calloc(1, bytes + 1);
        if (buf) {
            size_t rem = bytes, count;
            char *p = buf;

            do {
                count = fread(p, rem, 1, STREAM(handle).m_file);
                rem -= count;
                p += count;
            } while (count > 0 && rem > 0 && !ferror(STREAM(handle).m_file));
            
            if (rem != bytes)  string = string_alloc(bytes - rem, buf);
            
            free(buf);
        }
        POOL_UNLOCK(stream_t, handle);
    }
    else {
        debug("couldn't lock stream_t handle %"PRIuPTR"\n", handle);
    }
    
    return string;
}

/*
=item stream_write()

Writes a string to the stream.

=cut
 */
int stream_write(stream_handle_t handle, const string_t *string) {
    assert(POOL_HANDLE_VALID(stream_t, handle));
    
    int status;
    if (0 == POOL_LOCK(stream_t, handle)) {
        assert(POOL_HANDLE_IN_USE(stream_t, handle));
        assert(STREAM(handle).m_flags & STREAM_FLAG_WRITE);
        status = fwrite(string_cstr(string), string_length(string), 1, STREAM(handle).m_file);
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
of these are provided, the file is opened as read-only or write-only respectively.

If both STREAM_FLAG_READ and STREAM_FLAG_WRITE are provided, then the file is opened
for both reading and writing at the beginning of the file.  If the STREAM_FLAG_TRUNC
flag is provided, the existing contents of the file will be truncated, and the file
position set to the start of the file; otherwise the existing contents will be left
alone and the file position set to the start of the file.

FIXME: what about appending to existing files?

=cut
*/
int _stream_open_file(stream_t *restrict self, flags32_t flags, const string_t *restrict filename) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    
    flags32_t truncate = flags & STREAM_FLAG_TRUNC;
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
    
    if (NULL != (self->m_file = fopen(string_cstr(filename), mode))) {
        fcntl(fileno(self->m_file), F_SETFD, FD_CLOEXEC);
        self->m_meta.filename = string_dup(filename);
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
int _stream_open_pipe(stream_t *restrict self, flags32_t flags, const string_t *restrict command) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    
    flags32_t flag_mode = flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE);
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
            execl("/bin/sh", "sh", "-c", string_cstr(command), NULL);
            debug("execl failed with error %i\n", errno);
            _exit(1);
        }
        else {                  /* parent */
            close(fildes[child_end]);
            if (NULL != (self->m_file = fdopen(fildes[parent_end], stream_mode))) {
                fcntl(fileno(self->m_file), F_SETFD, FD_CLOEXEC);
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
=item _stream_open_socket()

Opens and connects a client TCP socket to dest, where dest is a server specified in the format "address:port".

The address component can be an IPv4 or IPv6 address, or a hostname.  The post component can be a numeric port
number or a service name as listed in /etc/services.

Returns 0 on success or non-zero on failure.

=cut
*/
int _stream_open_socket(stream_t *restrict self, flags32_t flags, const string_t *restrict dest) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    assert((flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE)) != 0);
    
    string_t *nodename = NULL, *servname = NULL;
    if (0 == _stream_parse_socket_dest(dest, &nodename, &servname)) {
        struct addrinfo hints = {0}, *results = NULL;
        int status = 0;
    
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_CANONNAME;
        
        if (0 == (status = getaddrinfo(string_cstr(nodename), string_cstr(servname), &hints, &results))) {
            int s = -1;
            
            for (struct addrinfo *iter = results; iter != NULL; iter = iter->ai_next) {
                s = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
                if (s >= 0) {
                    debug("s is %i\n", s);
                    if (0 == connect(s, iter->ai_addr, iter->ai_addrlen)) {
                        /* got a connection */
                        break;
                    }
                    else {
                        debug("connect failed: %i\n", errno);
                        close(s);
                    }
                }
                else {
                    debug("socket failed: %i\n", errno);
                }
            }
            
            if (s >= 0) {
                if (NULL != (self->m_file = fdopen(s, "r+"))) {
                    fcntl(fileno(self->m_file), F_SETFD, FD_CLOEXEC);
                    self->m_meta.addr_info = results;
                    self->m_flags = STREAM_TYPE_SOCK | STREAM_FLAG_READ | STREAM_FLAG_WRITE;
                    status = 0;
                }
                else {
                    debug("fdopen failed with error %i\n", errno);
                    close(s);
                    freeaddrinfo(results);
                    status = -1;
                }
            }
            else {
                freeaddrinfo(results);
                status = -1;
            }
        }
        else {
            debug("getaddrinfo failed: %i\n", status);
        }
    
        string_free(nodename);
        string_free(servname);

        return status;
    }
    else {
        debug("couldn't parse socket dest: %s\n", string_cstr(dest));
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
            string_free(self->m_meta.filename);
            break;
        case STREAM_TYPE_PIPE:
            fclose(self->m_file);
            debug("waiting for child pid %i to terminate...\n", self->m_meta.child_pid);
            while (waitpid(self->m_meta.child_pid, NULL, 0) == -1 && errno == EINTR) {
                ;
            }
            debug("child pid %i terminated\n", self->m_meta.child_pid);
            break;
        case STREAM_TYPE_SOCK:
            fclose(self->m_file);
            freeaddrinfo(self->m_meta.addr_info);
            break;
        default:
            debug("unhandled stream type: %"PRIu32"\n", self->m_flags);
            assert("shouldn't get here" == 0);
    }
    
    self->m_flags = STREAM_TYPE_UNDEF;
}

/*
=item _stream_bind()

Binds an existing file descriptor to a stream_t object.  Flags must contain at least one of STREAM_FLAG_READ 
or STREAM_FLAG_WRITE, with the caller being responsible for ensuring that the chosen combination is 
suitable for the file descriptor being bound.

If you wish to later unbind the file descriptor, you must keep a copy of fd to use as a "key" to unbind it.
Alternatively, the stream_t object can be safely closed with C<_stream_close()>.

=cut
*/
int _stream_bind(stream_t *self, flags32_t flags, int fd) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_UNDEF);
    
    const char *mode;
    switch (flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE)) {
        case STREAM_FLAG_READ:
            mode = "r";
            break;
        case STREAM_FLAG_WRITE:
            mode = "w";
            break;
        case STREAM_FLAG_READ | STREAM_FLAG_WRITE:
            mode = "r+";
            break;
        default:
            debug("invalid mode: %"PRIu32"\n", flags);
            return -1;
    }

    if (NULL != (self->m_file = fdopen(fd, mode))) {
        self->m_flags = STREAM_TYPE_FILE | (flags & (STREAM_FLAG_READ | STREAM_FLAG_WRITE));
        self->m_meta.filename = string_alloc(4, "<fd>");
        return 0;
    }
    else {
        debug("fdopen returned %i\n", errno);
        self->m_flags = STREAM_TYPE_UNDEF;
        return -1;
    }
}

/*
=item _stream_unbind()

Unbinds an existing file descriptor from a stream.  The caller must know and supply the original file descriptor;
the file descriptor will only be unbound if it matches.

To close the stream without knowing the existing file descriptor, simply use C<_stream_close()>.

=cut
*/
int _stream_unbind(stream_t *self, int fd) {
    assert(self != NULL);
    assert(self->m_flags == STREAM_TYPE_FILE);
    
    if (fileno(self->m_file) == fd) {
        fclose(self->m_file);
        string_free(self->m_meta.filename);
        self->m_flags = STREAM_TYPE_UNDEF;
        return 0;
    }
    else {
        debug("attempt to unbind from the wrong file descriptor\n");
        return -1;
    }
}

/*
=item _stream_parse_socket_dest()

Takes a string of format "nodename:servname" and parses it into its separate components.

The nodename part of the string may contain ':' characters (e.g. IPv6 addresses), but the servname
part may not.  (The delimiting ':' is discovered by searching backwards from the end of the string.)

=cut
*/
int _stream_parse_socket_dest(const string_t *restrict dest, string_t **restrict nodename, string_t **restrict servname) {
    assert(dest != NULL);
    assert(dest->m_length >= 3);
    assert(nodename != NULL);
    assert(servname != NULL);
    
    const char *node = dest->m_bytes;
    const char *serv = NULL;
    size_t node_len = dest->m_length;
    size_t serv_len = 0;
    
    for (; node_len > 0; node_len--) {
        if (dest->m_bytes[node_len - 1] == ':') {
            serv = &dest->m_bytes[node_len];
            serv_len = dest->m_length - node_len;
            --node_len;
            break;
        }
    }

    if (node_len == 0 || serv_len == 0)  return -1;

    *nodename = string_alloc(node_len, node);
    *servname = string_alloc(serv_len, serv);

    return 0;
}

/*
=back

=cut
 */
