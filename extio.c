/*
 *  extio.c
 *  dang
 *
 *  Created by Ellie on 11/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "extio.h"


/*
=item freads()

Reads a string from the specified file.  This is meant to present a similar interface to fread(),
but with the text-mode semantics of fgets.  Not sure if I actually need this.

=cut
 */
size_t freads(FILE *stream, char *buf, size_t bufsize) {
    FIXME("do I need this?\n");
    return -1;
}

/*
=item afreadln()

Reads a string from the stream, stopping when the terminator character is encountered.  Returns the number
of characters read, or zero if no characters could be read at all (due to EOF or error).

If EOF or an error occurs after characters have been read, the returned string will end in a value other
than the requested terminator.

The pointer provided needs to be freed once it's no longer required.

Write better docs for this.

=cut
 */
size_t afreadln(FILE *stream, char **result, int terminator) {
    size_t count = 0;
    size_t buflen = 256;
    int status = 0;
    
    char *buf = malloc(buflen);
    
    int c;
    while ((c = getc(stream))) {
        if (buflen - count < 2) {
            char *tmp = malloc(buflen * 2);
            if (tmp == NULL) {
                status = errno;
                goto end;
            }
            memcpy(tmp, buf, count);
            free(buf);
            buf = tmp;
            buflen *= 2;
        }

        switch(c) {
            // any special per-character handling goes here -- line endings?
            default:
                buf[count++] = c;
                break;
        }
        
        if (c == terminator)  break;
    }
    if (c == EOF)  status = errno;

end:
    buf[count+1] = '\0';
    *result = buf;
    if (status != 0)  errno = status;
    return count;
}
