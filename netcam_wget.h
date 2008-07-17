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

#ifndef NETCAM_WGET_H
#define NETCAM_WGET_H

#include "netcam.h"

/* Retrieval stream */
struct rbuf
{
    char buffer[4096];      /* the input buffer */
    char *buffer_pos;       /* current position in the buffer */
    size_t buffer_left;     /* number of bytes left in the buffer:
                               buffer_left = buffer_end - buffer_pos */
    int ret;                /* used by RBUF_READCHAR macro */
};

/* Read a character from RBUF.  If there is anything in the buffer,
   the character is returned from the buffer.  Otherwise, refill the
   buffer and return the first character.

   The return value is the same as with read(2).  On buffered read,
   the function returns 1.

   #### That return value is totally screwed up, and is a direct
   result of historical implementation of header code.  The macro
   should return the character or EOF, and in case of error store it
   to rbuf->err or something.  */

#define RBUF_READCHAR(netcam, store)    \
((netcam)->response->buffer_left ? (--(netcam)->response->buffer_left,    \
*((char *) (store)) = *(netcam)->response->buffer_pos++, 1)    \
: ((netcam)->response->buffer_pos = (netcam)->response->buffer,    \
((((netcam)->response->ret = rbuf_read_bufferful (netcam)) <= 0)    \
? (netcam)->response->ret : ((netcam)->response->buffer_left = (netcam->response)->ret - 1,    \
*((char *) (store)) = *(netcam)->response->buffer_pos++,1))))

/* Function declarations */
void rbuf_initialize(netcam_context_ptr);
int rbuf_initialized_p(netcam_context_ptr);
void rbuf_uninitialize(netcam_context_ptr);
int rbuf_readchar(netcam_context_ptr, char *);
int rbuf_peek(netcam_context_ptr, char *);
int rbuf_flush(netcam_context_ptr, char *, int);

/* Internal, but used by the macro. */
int rbuf_read_bufferful(netcam_context_ptr);

/* How many bytes it will take to store LEN bytes in base64.  */
#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

void base64_encode(const char *, char *, int);
char *strdupdelim(const char *, const char *);
int http_process_type(const char *, void *);

enum { 
    HG_OK, 
    HG_ERROR, 
    HG_EOF
};

enum header_get_flags { 
    HG_NONE = 0,
    HG_NO_CONTINUATIONS = 0x2 
};

int header_get (netcam_context_ptr, char **, enum header_get_flags);
int header_process (const char *, const char *,
                    int (*) (const char *, void *), void *);

int header_extract_number(const char *, void *);
int header_strdup(const char *, void *);
int skip_lws(const char *);
int http_result_code(const char *);

#endif /* NETCAM_WGET_H */
