/*
 *      stream.h
 *      
 *      Include file for stream.c
 *      Copyright (C) 2002 Jeroen Vreeken (pe1rxq@amsat.org)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _INCLUDE_STREAM_H_
#define _INCLUDE_STREAM_H_

struct stream_buffer {
    unsigned char *ptr;
    int ref;
    long size;
};

struct stream {
    int socket;
    FILE *fwrite;
    struct stream_buffer *tmpbuffer;
    long filepos;
    int nr;
    unsigned long int last;
    struct stream *prev;
    struct stream *next;
};

int stream_init(struct context *);
void stream_put(struct context *, unsigned char *);
void stream_stop(struct context *);

#endif /* _INCLUDE_STREAM_H_ */
