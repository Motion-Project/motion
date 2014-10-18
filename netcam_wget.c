/*
Copyright (C) 1995, 1996, 1997, 1998, 2000, 2001, 2002
    Free Software Foundation, Inc.

Additional Copyright (C) 2004-2005 Christopher Price,
Angel Carpintero, and other contributing authors.

Major part of this file is reused code from GNU Wget. It has been
merged and modified for use in the program Motion which is also
released under the terms of the GNU General Public License.

GNU Wget and Motion is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "motion.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MINVAL(x, y) ((x) < (y) ? (x) : (y))

/* This file contains the generic routines for work with headers.
   Currently they are used only by HTTP in http.c, but they can be
   used by anything that cares about RFC822-style headers.

   Header is defined in RFC2068, as quoted below.  Note that this
   definition is not HTTP-specific -- it is virtually
   indistinguishable from the one given in RFC822 or RFC1036.

    message-header = field-name ":" [ field-value ] CRLF

    field-name     = token
    field-value    = *( field-content | LWS )

    field-content  = <the OCTETs making up the field-value
             and consisting of either *TEXT or combinations
             of token, tspecials, and quoted-string>

   The public functions are header_get() and header_process(), which
   see.  */


/* Get a header from read-buffer RBUF and return it in *HDR.

   As defined in RFC2068 and elsewhere, a header can be folded into
   multiple lines if the continuation line begins with a space or
   horizontal TAB.  Also, this function will accept a header ending
   with just LF instead of CRLF.

   The header may be of arbitrary length; the function will allocate
   as much memory as necessary for it to fit.  It need not contain a
   `:', thus you can use it to retrieve, say, HTTP status line.

   All trailing whitespace is stripped from the header, and it is
   zero-terminated.  
 */
int header_get(netcam_context_ptr netcam, char **hdr, enum header_get_flags flags)
{
    int i;
    int bufsize = 80;

    *hdr = mymalloc(bufsize);

    for (i = 0; 1; i++) {
        int res;
        /* #### Use DO_REALLOC?  */
        if (i > bufsize - 1)
            *hdr = (char *)myrealloc(*hdr, (bufsize <<= 1), "");

        res = RBUF_READCHAR (netcam, *hdr + i);

        if (res == 1) {
            if ((*hdr)[i] == '\n') {
                if (!((flags & HG_NO_CONTINUATIONS) || i == 0
                    || (i == 1 && (*hdr)[0] == '\r'))) {
                    char next;
                    /* 
                     * If the header is non-empty, we need to check if
                     * it continues on to the other line.  We do that by
                     * peeking at the next character.  
                     */
                    res = rbuf_peek(netcam, &next);

                    if (res == 0) {
                        (*hdr)[i] = '\0';
                        return HG_EOF;
                    } else if (res == -1) {
                        (*hdr)[i] = '\0';
                        return HG_ERROR;
                    }
                    /* If the next character is HT or SP, just continue. */
                    if (next == '\t' || next == ' ')
                        continue;
                }

                /*
                 * Strip trailing whitespace.  (*hdr)[i] is the newline;
                 * decrement I until it points to the last available
                 * whitespace.  
                 */
                while (i > 0 && isspace((*hdr)[i - 1]))
                    --i;
                    
                (*hdr)[i] = '\0';
                break;
            }
        } else if (res == 0) {
            (*hdr)[i] = '\0';
            return HG_EOF;
        } else {
            (*hdr)[i] = '\0';
            return HG_ERROR;
        }
    }

    return HG_OK;
}

/**
 * header_process
 * 
 *  Check whether HEADER begins with NAME and, if yes, skip the `:' and
 *  the whitespace, and call PROCFUN with the arguments of HEADER's
 *  contents (after the `:' and space) and ARG.  Otherwise, return 0. 
 */
int header_process(const char *header, const char *name,
                    int (*procfun)(const char *, void *), void *arg)
{
    /* Check whether HEADER matches NAME. */
    while (*name && (tolower (*name) == tolower (*header)))
        ++name, ++header;

    if (*name || *header++ != ':')
        return 0;
    
    header += skip_lws (header);
    return ((*procfun) (header, arg));
}

/* Helper functions for use with header_process(). */

/**
 * header_extract_number
 * 
 *  Extract a long integer from HEADER and store it to CLOSURE.  If an
 *  error is encountered, return 0, else 1.  
 */
int header_extract_number(const char *header, void *closure)
{
    const char *p = header;
    long result;

    for (result = 0; isdigit (*p); p++)
        result = 10 * result + (*p - '0');

    /* Failure if no number present. */
    if (p == header)
        return 0;

    /* Skip trailing whitespace. */
    p += skip_lws (p);

    /* We return the value, even if a format error follows. */
    *(long *)closure = result;

    /* Indicate failure if trailing garbage is present. */
    if (*p)
        return 0;

    return 1;
}

/**
 * header_strdup
 * 
 *  Strdup HEADER, and place the pointer to CLOSURE.  
 */
int header_strdup(const char *header, void *closure)
{
    *(char **)closure = mystrdup(header);
    return 1;
}


/**
 * skip_lws
 *  Skip LWS (linear white space), if present.  Returns number of
 *  characters to skip.  
 */
int skip_lws(const char *string)
{
    const char *p = string;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    return p - string;
}


/**
 * base64_encode
 *
 *   Encode the string S of length LENGTH to base64 format and place it
 *   to STORE.  STORE will be 0-terminated, and must point to a writable
 *   buffer of at least 1+BASE64_LENGTH(length) bytes.  
 */
void base64_encode(const char *s, char *store, int length)
{
    /* Conversion table.  */
    static const char tbl[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
    };
    
    int i;
    unsigned char *p = (unsigned char *)store;

    /* Transform the 3x8 bits to 4x6 bits, as required by base64. */
    for (i = 0; i < length; i += 3) {
        *p++ = tbl[s[0] >> 2];
        *p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
        *p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
        *p++ = tbl[s[2] & 0x3f];
        s += 3;
    }
    
    /* Pad the result if necessary... */
    if (i == length + 1)
        *(p - 1) = '=';
    else if (i == length + 2)
        *(p - 1) = *(p - 2) = '=';

    /* ...and zero-terminate it.  */
    *p = '\0';
}

/**
 * strdupdelim
 */ 
char *strdupdelim(const char *beg, const char *end)
{
    char *res = mymalloc(end - beg + 1);
    memcpy (res, beg, end - beg);

    res[end - beg] = '\0';
    return res;
}

/**
 * http_process_type
 */ 
int http_process_type(const char *hdr, void *arg)
{
    char **result = (char **)arg;
    /* Locate P on `;' or the terminating zero, whichever comes first. */
    const char *p = strchr (hdr, ';');

    if (!p)
        p = hdr + strlen (hdr);

    while (p > hdr && isspace (*(p - 1)))
        --p;

    *result = strdupdelim (hdr, p);
    return 1;
}

/**
 * rbuf_initialize
 * 
 *  This is a simple implementation of buffering IO-read functions.  
 */
void rbuf_initialize(netcam_context_ptr netcam)
{
    netcam->response->buffer_pos = netcam->response->buffer;
    netcam->response->buffer_left = 0;
}

int rbuf_read_bufferful(netcam_context_ptr netcam)
{
    return netcam_recv(netcam, netcam->response->buffer,
                       sizeof (netcam->response->buffer));
}

/**
 * rbuf_peek
 * 
 *  Like rbuf_readchar(), only don't move the buffer position.  
 */
int rbuf_peek(netcam_context_ptr netcam, char *store)
{
    if (!netcam->response->buffer_left) {
        int res;
        rbuf_initialize(netcam);
        res = netcam_recv (netcam, netcam->response->buffer,
                           sizeof (netcam->response->buffer));

        if (res <= 0) {
            *store = '\0';
            return res;
        }

        netcam->response->buffer_left = res;
    }
    
    *store = *netcam->response->buffer_pos;
    return 1;
}

/**
 * rbuf_flush
 * 
 *   Flush RBUF's buffer to WHERE.  Flush MAXSIZE bytes at most.
 *   Returns the number of bytes actually copied.  If the buffer is
 *   empty, 0 is returned.  
 */
int rbuf_flush(netcam_context_ptr netcam, char *where, int maxsize)
{
    if (!netcam->response->buffer_left) {
        return 0;
    } else {
        int howmuch = MINVAL ((int)netcam->response->buffer_left, maxsize);

        if (where)
            memcpy(where, netcam->response->buffer_pos, howmuch);

        netcam->response->buffer_left -= howmuch;
        netcam->response->buffer_pos += howmuch;
        return howmuch;
    }
}

/**
 * http_result_code
 *
 *  Get the HTTP result code 
 */
int http_result_code(const char *header)
{
    char *cptr;

    /* Assure the header starts out right. */
    if (strncmp(header, "HTTP", 4))
        return -1;

    /* Find the space following the HTTP/1.x */
    if ((cptr = strchr(header+4, ' ')) == NULL)
        return -1;

    return atoi(cptr + 1);
}

