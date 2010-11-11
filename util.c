/*
 *  util.c
 *  dang
 *
 *  Created by Ellie on 11/11/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "util.h"

uintptr_t nextupow2(uintptr_t x) {
    if (x == 0)  return 1;
    
    --x;
    for (unsigned i = 1; i < sizeof(uintptr_t) * CHAR_BIT; i = i << 1) {
        x = x | (x >> i);
    }
         
    return x + 1;
}


/*
=item getdelim()

The getdelim() function shall read from stream until it encounters a character matching the delimiter character. The 
delimiter argument is an int, the value of which the application shall ensure is a character representable as an unsigned
char of equal value that terminates the read process. If the delimiter argument has any other value, the behavior is 
undefined.

The application shall ensure that *lineptr is a valid argument that could be passed to the free() function. If *n is 
non-zero, the application shall ensure that *lineptr either points to an object of size at least *n bytes, or is a null 
pointer.

The size of the object pointed to by *lineptr shall be increased to fit the incoming line, if it isn't already large 
enough, including room for the delimiter and a terminating NUL. The characters read, including any delimiter, shall be 
stored in the string pointed to by the lineptr argument, and a terminating NUL added when the delimiter or end of file 
is encountered.

The getline() function shall be equivalent to the getdelim() function with the delimiter character equal to the 
<newline> character.

The getdelim() and getline() functions may mark the last data access timestamp of the file associated with stream for 
update. The last data access timestamp shall be marked for update by the first successful execution of fgetc(), fgets(), 
fread(), fscanf(), getc(), getchar(), getdelim(), getline(), gets(), or scanf() using stream that returns data not 
supplied by a prior call to ungetc().

Upon successful completion, the getline() and getdelim() functions shall return the number of characters written into the
buffer, including the delimiter character if one was encountered before EOF, but excluding the terminating NUL character. 
If no characters were read, and the end-of-file indicator for the stream is set, or if the stream is at end-of-file, the 
end-of-file indicator for the stream shall be set and the function shall return -1. If an error occurs, the error 
indicator for the stream shall be set, and the function shall return -1 and set errno to indicate the error.

For the conditions under which the getdelim() and getline() functions shall fail and may fail, refer to fgetc .

In addition, these functions shall fail if:

[EINVAL]    lineptr or n is a null pointer.
[ENOMEM]    Insufficient memory is available.

These functions may fail if:

[EOVERFLOW] More than {SSIZE_MAX} characters were read without encountering the delimiter character.

=cut
 */
#ifdef NEED_GETDELIM
ssize_t getdelim(char **restrict lineptr, size_t *restrict n, int delimiter, FILE *restrict stream) {
    assert(lineptr != NULL);
    assert(n != NULL);
    assert(stream != NULL);
    
    if (lineptr == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    size_t count = 0, buflen = (*n != 0 ? *n : 256);
    char *buf = *lineptr;
    int status = 0;
        
    if (buf == NULL && (buf = malloc(buflen)) == NULL) {
        return -1;
    }
    
    {  // n.b. no early returns within this block
        flockfile(stream);

        int c;
        while ((c = getc_unlocked(stream)) != EOF) {
            if (buflen - count < 2) {
                char *tmp;
                if (NULL != (tmp = realloc(buf, buflen * 2))) {
                    buf = tmp;
                    buflen *= 2;
                }
                else {
                    status = errno;
                    break;
                }
            }

            switch(c) {
                // any special per-character handling goes here -- line endings?
                default:
                    buf[count++] = c;
                    break;
            }
            
            if (c == delimiter)  break;
        }
        if (c == EOF && ferror(stream))  status = errno;

        funlockfile(stream);
    }
    
    buf[count + 1] = '\0';
    *lineptr = buf;
    *n = buflen;
    if (status != 0)  errno = status;
    return count;
}
#endif
