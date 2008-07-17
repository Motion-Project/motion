/*
 *    webcam.c
 *    Streaming webcam using jpeg images over a multipart/x-mixed-replace stream
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
#include <ctype.h>
#include <sys/fcntl.h>


/* This function sets up a TCP/IP socket for incoming requests. It is called only during
 * initialisation of Motion from the function webcam_init
 * The function sets up a a socket on the port number given by _port_.
 * If the parameter _local_ is not zero the socket is setup to only accept connects from localhost.
 * Otherwise any client IP address is accepted. The function returns an integer representing the socket.
 */
int http_bindsock(int port, int local)
{
    int sl, optval = 1;
    struct sockaddr_in sin;

    if ((sl = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        motion_log(LOG_ERR, 1, "socket()");
        return -1;
    }

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family=AF_INET;
    sin.sin_port=htons(port);
    
    if (local)
        sin.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    else
        sin.sin_addr.s_addr=htonl(INADDR_ANY);

    setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sl, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == -1) {
        motion_log(LOG_ERR, 1, "bind()");
        close(sl);
        return -1;
    }

    if (listen(sl, DEF_MAXWEBQUEUE) == -1) {
        motion_log(LOG_ERR, 1, "listen()");
        close(sl);
        return -1;
    }

    return sl;
}


static int http_acceptsock(int sl)
{
    int sc;
    unsigned long i;
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    if ((sc = accept(sl, (struct sockaddr *)&sin, &addrlen)) >= 0) {
        i = 1;
        ioctl(sc, FIONBIO, &i);
        return sc;
    }
    
    motion_log(LOG_ERR, 1, "accept()");

    return -1;
}


/* Webcam flush sends any outstanding data to all connected clients.
 * It continuously goes through the client list until no data is able
 * to be sent (either because there isn't any, or because the clients
 * are not able to accept it).
 */
static void webcam_flush(struct webcam *list, int *stream_count, int lim)
{
    int written;            /* the number of bytes actually written */
    struct webcam *client;  /* pointer to the client being served */
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
            } else {
                written = 0;
            }
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
             * the client and free the webcam struct
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
static struct webcam_buffer *webcam_tmpbuffer(int size)
{
    struct webcam_buffer *tmpbuffer=mymalloc(sizeof(struct webcam_buffer));
    tmpbuffer->ref = 0;
    tmpbuffer->ptr = mymalloc(size);
        
    return tmpbuffer;
}


static void webcam_add_client(struct webcam *list, int sc)
{
    struct webcam *new = mymalloc(sizeof(struct webcam));
    static const char header[] = "HTTP/1.0 200 OK\r\n"
            "Server: Motion/"VERSION"\r\n"
            "Connection: close\r\n"
            "Max-Age: 0\r\n"
            "Expires: 0\r\n"
            "Cache-Control: no-cache, private\r\n"
            "Pragma: no-cache\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=--BoundaryString\r\n\r\n";

    memset(new, 0, sizeof(struct webcam));
    new->socket = sc;
    
    if ((new->tmpbuffer = webcam_tmpbuffer(sizeof(header))) == NULL) {
        motion_log(LOG_ERR, 1, "Error creating tmpbuffer in webcam_add_client");
    } else {
        memcpy(new->tmpbuffer->ptr, header, sizeof(header)-1);
        new->tmpbuffer->size = sizeof(header)-1;
    }
    
    new->prev = list;
    new->next = list->next;
    
    if (new->next)
        new->next->prev=new;
    
    list->next = new;
}


static void webcam_add_write(struct webcam *list, struct webcam_buffer *tmpbuffer, unsigned int fps)
{
    struct timeval curtimeval;
    unsigned long int curtime;

    gettimeofday(&curtimeval, NULL);
    curtime = curtimeval.tv_usec + 1000000L * curtimeval.tv_sec;
    
    while (list->next) {
        list = list->next;
        
        if (list->tmpbuffer == NULL && ((curtime-list->last) >= 1000000L / fps)) {
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


/* We walk through the chain of webcam structs until we reach the end.
 * Here we check if the tmpbuffer points to NULL
 * We return 1 if it finds a list->tmpbuffer which is a NULL pointer which would
 * be the next client ready to be sent a new image. If not a 0 is returned.
 */
static int webcam_check_write(struct webcam *list)
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
int webcam_init(struct context *cnt)
{
    cnt->webcam.socket = http_bindsock(cnt->conf.webcam_port, cnt->conf.webcam_localhost);
    cnt->webcam.next = NULL;
    cnt->webcam.prev = NULL;
    return cnt->webcam.socket;
}

/* This function is called from the motion_loop when it ends
 * and motion is terminated or restarted
 */
void webcam_stop(struct context *cnt)
{    
    struct webcam *list;
    struct webcam *next = cnt->webcam.next;

    if (cnt->conf.setup_mode)
        motion_log(-1, 0, "Closing webcam listen socket");
    
    close(cnt->webcam.socket);
    cnt->webcam.socket = -1;
    
    if (cnt->conf.setup_mode)
        motion_log(LOG_INFO, 0, "Closing active webcam sockets");

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
}

/* webcam_put is the starting point of the webcam loop. It is called from
 * the motion_loop with the argument 'image' pointing to the latest frame.
 * If config option 'webcam_motion' is 'on' this function is called once
 * per second (frame 0) and when Motion is detected excl pre_capture.
 * If config option 'webcam_motion' is 'off' this function is called once
 * per captured picture frame.
 * It is always run in setup mode for each picture frame captured and with
 * the special setup image.
 * The function does two things:
 * It looks for possible waiting new clients and adds them.
 * It sends latest picture frame to all connected clients.
 * Note: Clients that have disconnected are handled in the webcam_flush()
 * function
 */
void webcam_put(struct context *cnt, unsigned char *image)
{
    struct timeval timeout; 
    struct webcam_buffer *tmpbuffer;
    fd_set fdread;
    int sl=cnt->webcam.socket;
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
    FD_SET(cnt->webcam.socket, &fdread);
    
    /* If we have not reached the max number of allowed clients per
     * thread we will check to see if new clients are waiting to connect.
     * If this is the case we add the client as a new webcam struct and
     * add this to the end of the chain of webcam structs that are linked
     * to each other.
     */
    if ((cnt->stream_count < DEF_MAXSTREAMS) &&
        (select(sl+1, &fdread, NULL, NULL, &timeout) > 0)) {
        sc = http_acceptsock(sl);
        webcam_add_client(&cnt->webcam, sc);
        cnt->stream_count++;
    }
    
    /* call flush to send any previous partial-sends which are waiting */
    webcam_flush(&cnt->webcam, &cnt->stream_count, cnt->conf.webcam_limit);
    
    /* Check if any clients have available buffers */
    if (webcam_check_write(&cnt->webcam)) {
        /* yes - create a new tmpbuffer for current image.
         * Note that this should create a buffer which is *much* larger
         * than necessary, but it is difficult to estimate the
         * minimum size actually required.
         */
        tmpbuffer = webcam_tmpbuffer(cnt->imgs.size);
        
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
                                                 cnt->conf.webcam_quality);

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
            webcam_add_write(&cnt->webcam, tmpbuffer, cnt->conf.webcam_maxrate);
        } else {
            motion_log(LOG_ERR, 1, "Error creating tmpbuffer");
        }
    }
    
    /* Now we call flush again.  This time (assuming some clients were
     * ready for the new frame) the new data will be written out.
     */
    webcam_flush(&cnt->webcam, &cnt->stream_count, cnt->conf.webcam_limit);
    
    return;
}
