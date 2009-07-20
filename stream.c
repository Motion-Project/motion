/*
 *    stream.c (based in webcam.c)
 *    Streaming using jpeg images over a multipart/x-mixed-replace stream
 *    Copyright (C) 2002 Jeroen Vreeken (pe1rxq@amsat.org)
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "picture.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/fcntl.h>


/* This function sets up a TCP/IP socket for incoming requests. It is called only during
 * initialisation of Motion from the function stream_init
 * The function sets up a a socket on the port number given by _port_.
 * If the parameter _local_ is not zero the socket is setup to only accept connects from localhost.
 * Otherwise any client IP address is accepted. The function returns an integer representing the socket.
 */
int http_bindsock(int port, int local, int ipv6_enabled)
{
    int sl = -1, optval;
    struct addrinfo hints, *res = NULL, *ressave = NULL;
    char portnumber[10], hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];


    snprintf(portnumber, sizeof(portnumber), "%u", port);

    memset(&hints, 0, sizeof(struct addrinfo));
    /* Use the AI_PASSIVE flag, which indicates we are using this address for a listen() */
    hints.ai_flags = AI_PASSIVE;
#if defined(BSD)
    hints.ai_family = AF_INET;
#else
    if (!ipv6_enabled)
        hints.ai_family = AF_INET;
    else
        hints.ai_family = AF_UNSPEC;
#endif
    hints.ai_socktype = SOCK_STREAM;

    optval = getaddrinfo(local ? "localhost" : NULL, portnumber, &hints, &res);

    if (optval != 0) {
        motion_log(LOG_ERR, 1, "%s: getaddrinfo() for motion-stream socket failed: %s", 
                   __FUNCTION__, gai_strerror(optval));
        
        if (res != NULL)
            freeaddrinfo(res);
        return -1;
    }

    ressave = res;

    while (res) {
        /* create socket */
        sl = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
                    sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

        if (sl >= 0) {
            optval = 1;
            /* Reuse Address */ 
            setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

            motion_log(LOG_INFO, 0, "%s: motion-stream testing : %s addr: %s port: %s",
                       __FUNCTION__, res->ai_family == AF_INET ? "IPV4":"IPV6", hbuf, sbuf);

            if (bind(sl, res->ai_addr, res->ai_addrlen) == 0) {
                motion_log(LOG_INFO, 0, "%s: motion-stream Bound : %s addr: %s port: %s",
                           __FUNCTION__, res->ai_family == AF_INET ? "IPV4":"IPV6", hbuf, sbuf);    
                break;
            }

            motion_log(LOG_ERR, 1, "%s: motion-stream bind() failed, retrying", __FUNCTION__);
            close(sl);
            sl = -1;
        }
        motion_log(LOG_ERR, 1, "%s: motion-stream socket failed, retrying", __FUNCTION__);
        res = res->ai_next;
    }

    freeaddrinfo(ressave);

    if (sl < 0) {
        motion_log(LOG_ERR, 1, "%s: motion-stream creating socket/bind ERROR", __FUNCTION__);
        return -1;
    }
    

    if (listen(sl, DEF_MAXWEBQUEUE) == -1) {
        motion_log(LOG_ERR, 1, "%s: motion-stream listen() ERROR", __FUNCTION__);
        close(sl);
        sl = -1;
    }

    return sl;
}


static int http_acceptsock(int sl)
{
    int sc;
    unsigned long i;
    struct sockaddr_storage sin;
    socklen_t addrlen = sizeof(sin);

    if ((sc = accept(sl, (struct sockaddr *)&sin, &addrlen)) >= 0) {
        i = 1;
        ioctl(sc, FIONBIO, &i);
        return sc;
    }
    
    motion_log(LOG_ERR, 1, "%s: motion-stream accept()", __FUNCTION__);

    return -1;
}


/* stream flush sends any outstanding data to all connected clients.
 * It continuously goes through the client list until no data is able
 * to be sent (either because there isn't any, or because the clients
 * are not able to accept it).
 */
static void stream_flush(struct stream *list, int *stream_count, int lim)
{
    int written;            /* the number of bytes actually written */
    struct stream *client;  /* pointer to the client being served */
    int workdone = 0;       /* flag set any time data is successfully
                               written */

    client = list->next;

    while (client) {
    
        /* If data waiting for client, try to send it */
        if (client->tmpbuffer) {
        
            /* We expect that list->filepos < list->tmpbuffer->size
             * should always be true.  The check is more for safety,
             * in case of trouble is some other part of the code.
             * Note that if it is false, the following section will
             * clean up.
             */
            if (client->filepos < client->tmpbuffer->size) {
                
                /* Here we are finally ready to write out the
                 * data.  Remember that (because the socket
                 * has been set non-blocking) we may only
                 * write out part of the buffer.  The var
                 * 'filepos' contains how much of the buffer
                 * has already been written.
                 */
                written = write(client->socket, 
                          client->tmpbuffer->ptr + client->filepos,
                          client->tmpbuffer->size - client->filepos);
        
                /* If any data has been written, update the
                 * data pointer and set the workdone flag
                 */
                if (written > 0) {
                    client->filepos += written;
                    workdone = 1;
                }
            } else
                written = 0;
            
            /* If we have written the entire buffer to the socket,
             * or if there was some error (other than EAGAIN, which
             * means the system couldn't take it), this request is
             * finished.
             */
            if ((client->filepos >= client->tmpbuffer->size) ||
                (written < 0 && errno != EAGAIN)) {
                /* If no other clients need this buffer, free it */
                if (--client->tmpbuffer->ref <= 0) {
                    free(client->tmpbuffer->ptr);
                    free(client->tmpbuffer);
                }
            
                /* Mark this client's buffer as empty */
                client->tmpbuffer = NULL;
                client->nr++;
            }
            
            /* If the client is no longer connected, or the total
             * number of frames already sent to this client is
             * greater than our configuration limit, disconnect
             * the client and free the stream struct
             */
            if ((written < 0 && errno != EAGAIN) ||
                (lim && !client->tmpbuffer && client->nr > lim)) {
                void *tmp;

                close(client->socket);
                
                if (client->next)
                    client->next->prev = client->prev;
                
                client->prev->next = client->next;
                tmp = client;
                client = client->prev;
                free(tmp);
                (*stream_count)--;
            }
        }        /* end if (client->tmpbuffer) */
        
        /* Step the the next client in the list.  If we get to the
         * end of the list, check if anything was written during
         * that loop; (if so) reset the 'workdone' flag and go back
         * to the beginning
         */
        client = client->next;

        if (!client && workdone) {
            client = list->next;
            workdone = 0;
        }
    }            /* end while (client) */
}

/* Routine to create a new "tmpbuffer", which is a common
 * object used by all clients connected to a single camera
 */
static struct stream_buffer *stream_tmpbuffer(int size)
{
    struct stream_buffer *tmpbuffer = mymalloc(sizeof(struct stream_buffer));
    tmpbuffer->ref = 0;
    tmpbuffer->ptr = mymalloc(size);
        
    return tmpbuffer;
}


static void stream_add_client(struct stream *list, int sc)
{
    struct stream *new = mymalloc(sizeof(struct stream));
    static const char header[] = "HTTP/1.0 200 OK\r\n"
                                 "Server: Motion/"VERSION"\r\n"
                                 "Connection: close\r\n"
                                 "Max-Age: 0\r\n"
                                 "Expires: 0\r\n"
                                 "Cache-Control: no-cache, private\r\n"
                                 "Pragma: no-cache\r\n"
                                 "Content-Type: multipart/x-mixed-replace; "
                                 "boundary=--BoundaryString\r\n\r\n";

    memset(new, 0, sizeof(struct stream));
    new->socket = sc;
    
    if ((new->tmpbuffer = stream_tmpbuffer(sizeof(header))) == NULL) {
        motion_log(LOG_ERR, 1, "%s: Error creating tmpbuffer in stream_add_client", __FUNCTION__);
    } else {
        memcpy(new->tmpbuffer->ptr, header, sizeof(header)-1);
        new->tmpbuffer->size = sizeof(header)-1;
    }
    
    new->prev = list;
    new->next = list->next;
    
    if (new->next)
        new->next->prev = new;
    
    list->next = new;
}


static void stream_add_write(struct stream *list, struct stream_buffer *tmpbuffer, unsigned int fps)
{
    struct timeval curtimeval;
    unsigned long int curtime;

    gettimeofday(&curtimeval, NULL);
    curtime = curtimeval.tv_usec + 1000000L * curtimeval.tv_sec;
    
    while (list->next) {
        list = list->next;
        
        if (list->tmpbuffer == NULL && ((curtime - list->last) >= 1000000L / fps)) {
            list->last = curtime;
            list->tmpbuffer = tmpbuffer;
            tmpbuffer->ref++;
            list->filepos = 0;
        }
    }
    
    if (tmpbuffer->ref <= 0) {
        free(tmpbuffer->ptr);
        free(tmpbuffer);
    }
}


/* We walk through the chain of stream structs until we reach the end.
 * Here we check if the tmpbuffer points to NULL
 * We return 1 if it finds a list->tmpbuffer which is a NULL pointer which would
 * be the next client ready to be sent a new image. If not a 0 is returned.
 */
static int stream_check_write(struct stream *list)
{
    while (list->next) {
        list = list->next;
        
        if (list->tmpbuffer == NULL)
            return 1;
    }
    return 0;
}


/* This function is called from motion.c for each motion thread starting up.
 * The function setup the incoming tcp socket that the clients connect to
 * The function returns an integer representing the socket.
 */
int stream_init(struct context *cnt)
{
    cnt->stream.socket = http_bindsock(cnt->conf.stream_port, cnt->conf.stream_localhost, cnt->conf.ipv6_enabled);
    cnt->stream.next = NULL;
    cnt->stream.prev = NULL;
    return cnt->stream.socket;
}

/* This function is called from the motion_loop when it ends
 * and motion is terminated or restarted
 */
void stream_stop(struct context *cnt)
{    
    struct stream *list;
    struct stream *next = cnt->stream.next;

    if (debug_level >= CAMERA_VERBOSE)
        motion_log(0, 0, "%s: Closing motion-stream listen socket"
                   " & active motion-stream sockets", __FUNCTION__);
    else
        motion_log(LOG_INFO, 0, "%s: Closing motion-stream listen socket" 
                   " & active motion-stream sockets", __FUNCTION__);
    
    close(cnt->stream.socket);
    cnt->stream.socket = -1;

    while (next) {
        list = next;
        next = list->next;
        
        if (list->tmpbuffer) {
            free(list->tmpbuffer->ptr);
            free(list->tmpbuffer);
        }
        
        close(list->socket);
        free(list);
    }

    if (debug_level >= CAMERA_VERBOSE)
        motion_log(0, 0, "%s: Closed motion-stream listen socket"
                   " & active motion-stream sockets", __FUNCTION__);
    else
        motion_log(LOG_INFO, 0, "%s: Closed motion-stream listen socket" 
                   " & active motion-stream sockets", __FUNCTION__);
}

/* stream_put is the starting point of the stream loop. It is called from
 * the motion_loop with the argument 'image' pointing to the latest frame.
 * If config option 'stream_motion' is 'on' this function is called once
 * per second (frame 0) and when Motion is detected excl pre_capture.
 * If config option 'stream_motion' is 'off' this function is called once
 * per captured picture frame.
 * It is always run in setup mode for each picture frame captured and with
 * the special setup image.
 * The function does two things:
 * It looks for possible waiting new clients and adds them.
 * It sends latest picture frame to all connected clients.
 * Note: Clients that have disconnected are handled in the stream_flush()
 * function
 */
void stream_put(struct context *cnt, unsigned char *image)
{
    struct timeval timeout; 
    struct stream_buffer *tmpbuffer;
    fd_set fdread;
    int sl = cnt->stream.socket;
    int sc;
    /* the following string has an extra 16 chars at end for length */
    const char jpeghead[] = "--BoundaryString\r\n"
                            "Content-type: image/jpeg\r\n"
                            "Content-Length:                ";
    int headlength = sizeof(jpeghead) - 1;    /* don't include terminator */
    char len[20];    /* will be used for sprintf, must be >= 16 */
    
    /* timeout struct used to timeout the time we wait for a client
     * and we do not wait at all
     */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&fdread);
    FD_SET(cnt->stream.socket, &fdread);
    
    /* If we have not reached the max number of allowed clients per
     * thread we will check to see if new clients are waiting to connect.
     * If this is the case we add the client as a new stream struct and
     * add this to the end of the chain of stream structs that are linked
     * to each other.
     */
    if ((cnt->stream_count < DEF_MAXSTREAMS) &&
        (select(sl+1, &fdread, NULL, NULL, &timeout)>0)) {
        sc = http_acceptsock(sl);
        stream_add_client(&cnt->stream, sc);
        cnt->stream_count++;
    }
    
    /* call flush to send any previous partial-sends which are waiting */
    stream_flush(&cnt->stream, &cnt->stream_count, cnt->conf.stream_limit);
    
    /* Check if any clients have available buffers */
    if (stream_check_write(&cnt->stream)) {
        /* yes - create a new tmpbuffer for current image.
         * Note that this should create a buffer which is *much* larger
         * than necessary, but it is difficult to estimate the
         * minimum size actually required.
         */
        tmpbuffer = stream_tmpbuffer(cnt->imgs.size);
        
        /* check if allocation went ok */
        if (tmpbuffer) {
            int imgsize;

            /* We need a pointer that points to the picture buffer
             * just after the mjpeg header. We create a working pointer wptr
             * to be used in the call to put_picture_memory which we can change
             * and leave tmpbuffer->ptr intact.
             */
            unsigned char *wptr = tmpbuffer->ptr;
            
            /* For web protocol, our image needs to be preceded
             * with a little HTTP, so we put that into the buffer
             * first.
             */
            memcpy(wptr, jpeghead, headlength);

            /* update our working pointer to point past header */
            wptr += headlength;

            /* create a jpeg image and place into tmpbuffer */
            tmpbuffer->size = put_picture_memory(cnt, wptr, cnt->imgs.size, image,
                                                 cnt->conf.stream_quality);

            /* fill in the image length into the header */
            imgsize = sprintf(len, "%9ld\r\n\r\n", tmpbuffer->size);
            memcpy(wptr - imgsize, len, imgsize);
            
            /* append a CRLF for good measure */
            memcpy(wptr + tmpbuffer->size, "\r\n", 2);
            
            /* now adjust tmpbuffer->size to reflect the
             * header at the beginning and the extra CRLF
             * at the end.
             */
            tmpbuffer->size += headlength + 2;
            
            /* and finally put this buffer to all clients with
             * no outstanding data from previous frames.
             */
            stream_add_write(&cnt->stream, tmpbuffer, cnt->conf.stream_maxrate);
        } else {
            motion_log(LOG_ERR, 1, "%s: Error creating tmpbuffer", __FUNCTION__);
        }
    }
    
    /* Now we call flush again.  This time (assuming some clients were
     * ready for the new frame) the new data will be written out.
     */
    stream_flush(&cnt->stream, &cnt->stream_count, cnt->conf.stream_limit);
    
    return;
}
