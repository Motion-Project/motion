/***********************************************************
 *  netcam_http.c
 *  Process network camera images using http protocol
 *      This code was inspired by the original netcam.c module
 *      written by Jeroen Vreeken and enhanced by several Motion
 *      project contributors, particularly Angel Carpintero and
 *      Christopher Price.
 *
 *      Copyright 2005, William M. Brack
 *      This software is distributed under the GNU Public license
 *      Version 2.  See also the file 'COPYING'.
 ***********************************************************/

#include "translate.h"
#include "motion.h"  /* Needs to come first, because _GNU_SOURCE_ set there. */
#include "netcam_http.h"

#define CONNECT_TIMEOUT        10     /* Timeout on remote connection attempt */
#define READ_TIMEOUT            5     /* Default timeout on recv requests */
#define POLLING_TIMEOUT  READ_TIMEOUT /* File polling timeout [s] */
#define POLLING_TIME  500*1000*1000   /* File polling time quantum [ns] (500ms) */
#define MAX_HEADER_RETRIES      5     /* Max tries to find a header record */
#define MINVAL(x, y) ((x) < (y) ? (x) : (y))

/* These strings are used for the HTTP connection. */
static const char *connect_req;

#define connect_req_http10 "GET %s HTTP/1.0\r\n" \
                           "Host: %s\r\n" \
                           "User-Agent: Motion-netcam/" VERSION "\r\n"

#define connect_req_http11 "GET %s HTTP/1.1\r\n" \
                           "Host: %s\r\n" \
                           "User-Agent: Motion-netcam/" VERSION "\r\n"

#define connect_req_close "Connection: close\r\n"

#define connect_req_keepalive "Connection: Keep-Alive\r\n"

#define connect_auth_req "Authorization: Basic %s\r\n"

tfile_context *file_new_context(void);
void file_free_context(tfile_context* ctxt);


/**
 * check_quote
 *
 *      Checks a string to see if it's quoted, and if so removes the
 *      quotes.
 *
 * Parameters:
 *
 *      str             Pointer to a string.
 *
 * Returns:             Nothing, but updates the target if necessary.
 *
 */
static void check_quote(char *str)
{
    int len;
    char ch;

    ch = *str;

    if ((ch == '"') || (ch == '\'')) {
        len = strlen(str) - 1;
        if (str[len] == ch) {
            memmove(str, str+1, len-1);
            str[len-1] = 0;
        }
    }
}

/**
 * netcam_check_content_length
 *
 *     Analyse an HTTP-header line to see if it is a Content-length.
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line.
 *
 * Returns:
 *      -1              Not a Content-length line.
 *      >=0             Value of Content-length field.
 *
 */
static long netcam_check_content_length(char *header)
{
    long length = -1;    /* Note this is a long, not an int. */

    if (!header_process(header, "Content-Length", header_extract_number, &length)) {
        /*
         * Some netcams deliver some bad-format data, but if
         * we were able to recognize the header section and the
         * number we might as well try to use it.
         */
        if (length > 0)
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                ,_("malformed token Content-Length but value %ld"), length);
    }

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Content-Length %ld"), length);

    return length;
}

/**
 * netcam_check_keepalive
 *
 *     Analyse an HTTP-header line to see if it is a Keep-Alive.
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line.
 *
 * Returns:
 *      -1              Not a Keep-Alive line.
 *      1               Is a Keep-Alive line.
 *
 */
static int netcam_check_keepalive(char *header)
{
    char *content_type = NULL;

    if (!header_process(header, "Keep-Alive", http_process_type, &content_type))
        return -1;

    /* We do not detect the second field or other case mixes at present. */
    free(content_type);

    return 1;
}

/**
 * netcam_check_close
 *
 *     Analyse an HTTP-header line to see if it is a Connection: close.
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line.
 *
 * Returns:
 *      -1              Not a Connection: close.
 *      1               Is a Connection: close.
 *
 */
static int netcam_check_close(char *header)
{
    char *type = NULL;
    int ret = -1;

    if (!header_process(header, "Connection", http_process_type, &type))
        return -1;

    if (!strcmp(type, "close")) /* strcmp returns 0 for match. */
        ret = 1;

    free(type);

    return ret;
}

/**
 * netcam_check_content_type
 *
 *     Analyse an HTTP-header line to see if it is a Content-type.
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line.
 *
 * Returns:
 *      -1              Not a Content-type line
 *      0               Content-type not recognized
 *      1               image/jpeg
 *      2               multipart/x-mixed-replace or multipart/mixed
 *      3               application/octet-stream (used by WVC200 Linksys IP Camera)
 *
 */
static int netcam_check_content_type(char *header)
{
    char *content_type = NULL;
    int ret;

    if (!header_process(header, "Content-type", http_process_type, &content_type))
        return -1;

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Content-type %s"), content_type);

    if (!strcmp(content_type, "image/jpeg")) {
        ret = 1;
    } else if (!strcmp(content_type, "multipart/x-mixed-replace") ||
               !strcmp(content_type, "multipart/mixed")) {
        ret = 2;
    } else if (!strcmp(content_type, "application/octet-stream")) {
        ret = 3;
    } else {
        ret = 0;
    }

    free(content_type);

    return ret;
}

/**
 * netcam_read_next_header
 *
 *      Read the next header record from the camera.
 *
 * Parameters
 *
 *      netcam          pointer to a netcam_context.
 *
 * Returns:             0 for success, -1 if any error.
 *
 */
int netcam_read_next_header(netcam_context_ptr netcam)
{
    int retval;
    char *header;

    /* Return if not connected */
    if (netcam->sock == -1)
        return -1;
    /*
     * We are expecting a header which *must* contain a mime-type of
     * image/jpeg, and *might* contain a Content-Length.
     *
     * If this is a "streaming" camera, the header *must* be preceded
     * by a "boundary" string.
     *
     */
    netcam->caps.content_length = 0;

    /*
     * If this is a "streaming" camera, the stream header must be
     * preceded by a "boundary" string.
     */
    if (netcam->caps.streaming == NCS_MULTIPART) {
        while (1) {
            retval = header_get(netcam, &header, HG_NONE);

            if (retval != HG_OK) {
                /* Header reported as not-OK, check to see if it's null. */
                if (strlen(header) == 0) {
                    MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                        ,_("Error reading image header, streaming mode (1). Null header."));
                } else {
                    /* Header is not null. Output it in case it's a new camera with unknown headers. */
                    MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                        ,_("Error reading image header, streaming mode (1). Unknown header '%s'")
                        ,header);
                 }

                free(header);
                return -1;
            }

            retval = (strstr(header, netcam->boundary) == NULL);
            free(header);

            if (!retval)
                break;
        }
    }

    while (1) {
        retval = header_get(netcam, &header, HG_NONE);

        if (retval != HG_OK) {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Error reading image header (2)"));
            free(header);
            return -1;
        }

        if (*header == 0)
            break;

        if ((retval = netcam_check_content_type(header)) >= 0) {
            if (retval != 1) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Header not JPEG"));
                free(header);
                return -1;
            }
        }

        if ((retval = (int) netcam_check_content_length(header)) >= 0) {
            if (retval > 0) {
                netcam->caps.content_length = 1;       /* Set flag */
                netcam->receiving->content_length = retval;
            } else {
                netcam->receiving->content_length = 0;
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Content-Length 0"));
                free(header);
                return -1;
            }
        }

        free(header);
    }

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Found image header record"));

    free(header);
    return 0;
}

/**
 * netcam_read_first_header
 *
 * This routine attempts to read a header record from the netcam.  If
 * successful, it analyses the header to determine whether the camera is
 * a "streaming" type.  If it is, the routine looks for the Boundary-string;
 * if found, it positions just past the string so that the image header can
 * be read.  It then reads the image header and continues processing that
 * header as well.
 *
 * If the camera does not appear to be a streaming type, it is assumed that the
 * header just read was the image header.  It is processed to determine whether
 * a Content-length is present.
 *
 * After this processing, the routine returns to the caller.
 *
 * Parameters:
 *      netcam            Pointer to the netcam_context structure.
 *
 * Returns:               Content-type code if successful, -1 if not
 *                                                         -2 if Content-length = 0
 */
int netcam_read_first_header(netcam_context_ptr netcam)
{
    int retval = -3;      /* "Unknown err" */
    int ret;
    int firstflag = 1;
    int aliveflag = 0;    /* If we have seen a Keep-Alive header from cam. */
    int closeflag = 0;    /* If we have seen a Connection: close header from cam. */
    char *header;
    char *boundary;

    /* Send the initial command to the camera. */
    if (send(netcam->sock, netcam->connect_request,
             strlen(netcam->connect_request), 0) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO
            ,_("Error sending 'connect' request"));
        return -1;
    }

    /*
     * We expect to get back an HTTP header from the camera.
     * Successive calls to header_get will return each line
     * of the header received.  We will continue reading until
     * a blank line is received.
     *
     * As we process the header, we are looking for either of
     * header lines Content-type or Content-length.  Content-type
     * is used to determine whether the camera is "streaming" or
     * "non-streaming", and Content-length will be used to determine
     * whether future reads of images will be controlled by the
     * length specified before the image, or by a boundary string.
     *
     * The Content-length will only be present "just before" an
     * image is sent (if it is present at all).  That means that, if
     * this is a "streaming" camera, it will not be present in the
     * "first header", but will occur later (after a boundary-string).
     * For a non-streaming camera, however, there is no boundary-string,
     * and the first header is, in fact, the only header.  In this case,
     * there may be a Content-length.
     *
     */
    while (1) {     /* 'Do forever' */
        ret = header_get(netcam, &header, HG_NONE);

        MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Received first header ('%s')"), header);

        if (ret != HG_OK) {
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                ,_("Error reading first header (%s)"), header);
            free(header);
            return -1;
        }

        if (firstflag) {
            if ((ret = http_result_code(header)) != 200) {
                MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("HTTP Result code %d"), ret);

                free(header);
                if (netcam->connect_keepalive) {
                    /*
                     * Cannot unset netcam->cnt->conf.netcam_keepalive as it is assigned const
                     * But we do unset the netcam keepalive flag which was set in netcam_start
                     * This message is logged as Information as it would be useful to know
                     * if your netcam often returns bad HTTP result codes.
                     */
                    netcam->connect_keepalive = FALSE;
                    free((void *)netcam->cnt->conf.netcam_keepalive);
                    netcam->cnt->conf.netcam_keepalive = strdup("off");
                    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                        ,_("Removed netcam Keep-Alive flag "
                        "due to apparent closed HTTP connection."));
                }
                return ret;
            }
            firstflag = 0;
            free(header);
            continue;
        }

        if (*header == 0)   /* Blank line received */
            break;

        /* Check if this line is the content type. */
        if ((ret = netcam_check_content_type(header)) >= 0) {
            retval = ret;
            /*
             * We are expecting to find one of three types:
             * 'multipart/x-mixed-replace', 'multipart/mixed'
             * or 'image/jpeg'.  The first two will be received
             * from a streaming camera, and the third from a
             * camera which provides a single frame only.
             */
            switch (ret) {
            case 1:         /* Not streaming */
                if (netcam->connect_keepalive)
                    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                        ,_("Non-streaming camera (keep-alive set)"));
                else
                    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                        ,_("Non-streaming camera (keep-alive not set)"));

                netcam->caps.streaming = NCS_UNSUPPORTED;
                break;

            case 2:         /* Streaming */
                MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Streaming camera"));

                netcam->caps.streaming = NCS_MULTIPART;

                if ((boundary = strstr(header, "boundary="))) {
                    /* On error recovery this may already be set. */
                    free(netcam->boundary);

                    netcam->boundary = mystrdup(boundary + 9);
                    /*
                     * HTTP protocol apparently permits the boundary string
                     * to be quoted (the Lumenera does this, which caused
                     * trouble) so we need to get rid of any surrounding
                     * quotes.
                     */
                     check_quote(netcam->boundary);
                     netcam->boundary_length = strlen(netcam->boundary);

                     MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                        ,_("Boundary string [%s]"), netcam->boundary);
                } else {
                    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                        ,_("Boundary string not found in header"));
                    free(header);
                    return -1;
                }
                break;
            case 3:  /* MJPG-Block style streaming. */
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("Streaming camera probably using MJPG-blocks,"
                    " consider using mjpg:// netcam_url."));
                break;

            default:
                /* Error */
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Unrecognized content type"));
                free(header);
                return -1;

            }
        } else if ((ret = (int) netcam_check_content_length(header)) >= 0) {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Content-length present"));

            if (ret > 0) {
                netcam->caps.content_length = 1;     /* Set flag */
                netcam->receiving->content_length = ret;
            } else {
                netcam->receiving->content_length = 0;
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Content-length 0"));
                retval = -2;
            }
        } else if (netcam_check_keepalive(header) == TRUE) {
            /* Note that we have received a Keep-Alive header, and thus the socket can be left open. */
            aliveflag = TRUE;
            netcam->keepalive_thisconn = TRUE;
            /*
             * This flag will not be set when a Streaming cam is in use, but that
             * does not matter as the test below looks at Streaming state also.
             */
        } else if (netcam_check_close(header) == TRUE) {
            /* Note that we have received a Connection: close header. */
            closeflag = TRUE;
            /*
             * This flag is acted upon below.
             * Changed criterion and moved up from below to catch headers that cause returns.
             */
             MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("Found Conn: close header ('%s')"), header);
        }
        free(header);
    }
    free(header);

    if (netcam->caps.streaming == NCS_UNSUPPORTED && netcam->connect_keepalive) {

        /* If we are a non-streaming (ie. Jpeg) netcam and keepalive is configured. */

        if (aliveflag) {
            if (closeflag) {
                netcam->warning_count++;
                if (netcam->warning_count > 3) {
                    netcam->warning_count = 0;
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("Both 'Connection: Keep-Alive' and "
                        "'Connection: close' header received. Motion removes keepalive."));
                    netcam->connect_keepalive = FALSE;
                    free((void *)netcam->cnt->conf.netcam_keepalive);
                    netcam->cnt->conf.netcam_keepalive = strdup("off");
                } else {
                   /*
                    * If not a streaming cam, and keepalive is set, and the flag shows we
                    * did not see a Keep-Alive field returned from netcam and a Close field.
                    * Not quite sure what the correct course of action is here. In for testing.
                    */
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("Both 'Connection: Keep-Alive' and "
                        "'Connection: close' header received. Motion continues unchanged."));
                }
            } else {
               /*
                * aliveflag && !closeflag
                *
                * If not a streaming cam, and keepalive is set, and the flag shows we
                * just got a Keep-Alive field returned from netcam and no Close field.
                * No action, as this is the normal case. In debug we print a notification.
                */

                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("Received a Keep-Alive field in this set of headers."));
            }
        } else { /* !aliveflag */
            if (!closeflag) {
                netcam->warning_count++;

                if (netcam->warning_count > 3) {
                    netcam->warning_count = 0;
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("No 'Connection: Keep-Alive' nor 'Connection: close'"
                        " header received.\n Motion removes keepalive."));
                    netcam->connect_keepalive = FALSE;
                    free((void *)netcam->cnt->conf.netcam_keepalive);
                    netcam->cnt->conf.netcam_keepalive = strdup("off");
                } else {
                   /*
                    * If not a streaming cam, and keepalive is set, and the flag shows we
                    * did not see a Keep-Alive field returned from netcam nor a Close field.
                    * Not quite sure what the correct course of action is here. In for testing.
                    */
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("No 'Connection: Keep-Alive' nor 'Connection: close'"
                        " header received.\n Motion continues unchanged."));
                }
            } else {
                /*
                 * !aliveflag & closeflag
                 * If not a streaming cam, and keepalive is set, and the flag shows we
                 * received a 'Connection: close' field returned from netcam. It is not likely
                 * we will get a Keep-Alive and Close header together - this is picked up by
                 * the test code above.
                 * If we receive a Close header, then we want to cease keep-alive for this cam.
                 * This situation will occur in 2 situations:
                 *    (a) in HTTP 1.1 when the client wants to stop the keep-alive
                 *          (and in this case it would be correct to close connection and then
                 *          make a new one, with keep-alive set again).
                 *    (b) in HTTP 1.0 with keepalive, when the client does not support it.
                 *          In this case we should not attempt to re-start Keep-Alive.
                 * Due to that, we accept a Connection: close header in HTTP 1.0 & 1.1 modes
                 *
                 * To tell between the sitation where a camera has been in Keep-Alive mode and
                 * is now finishing (and will want to be re-started in Keep-Alive) and the other
                 * case when a cam does not support it, we have a flag which says if the netcam
                 * has returned a Keep-Alive flag during this connection. If that's set, we
                 * set ourselves up to re-connect with Keep-Alive after the socket is closed.
                 * If it's not set, then we will not try again to use Keep-Alive.
                 */
                if (!netcam->keepalive_thisconn) {
                    netcam->connect_keepalive = FALSE;    /* No further attempts at keep-alive */
                    free((void *)netcam->cnt->conf.netcam_keepalive);
                    netcam->cnt->conf.netcam_keepalive = strdup("off");
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("Removed netcam Keep-Alive flag because"
                        " 'Connection: close' header received.\n Netcam does not support "
                        "Keep-Alive. Motion continues in non-Keep-Alive."));
                } else {
                    netcam->keepalive_timeup = TRUE;    /* We will close and re-open keep-alive */
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("Keep-Alive has reached end of valid period.\n"
                        "Motion will close netcam, then resume Keep-Alive with a new socket."));
                }
            }
        }
    }

    return retval;
}

/**
 * netcam_disconnect
 *
 *      Disconnect from the network camera.
 *
 * Parameters:
 *
 *      netcam  pointer to netcam context
 *
 * Returns:     Nothing
 *
 */
void netcam_disconnect(netcam_context_ptr netcam)
{
    if (netcam->sock > 0) {
        if (close(netcam->sock) < 0)
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, _("disconnect"));

        netcam->sock = -1;
    }
}

/**
 * netcam_connect
 *
 *      Attempt to open the network camera as a stream device.
 *      Keep-alive is supported, ie. if netcam->connect_keepalive is TRUE, we
 *      re-use netcam->sock unless it has value -1, meaning it is invalid.
 *
 * Parameters:
 *
 *      netcam    pointer to netcam_context structure
 *      err_flag  flag to suppress error printout (1 => suppress)
 *                Note that errors which indicate something other than
 *                a network connection problem are not suppressed.
 *
 * Returns:     0 for success, -1 for error
 *
 */
int netcam_connect(netcam_context_ptr netcam, int err_flag)
{
    struct addrinfo *ai;
    int ret;
    int saveflags;
    int back_err;
    int optval;
    socklen_t optlen = sizeof(optval);
    socklen_t len;
    fd_set fd_w;
    struct timeval selecttime;

    char port[15];
    sprintf(port,"%u",netcam->connect_port);

    /* Lookup the hostname given in the netcam URL. */
    if ((ret = getaddrinfo(netcam->connect_host, port, NULL, &ai)) != 0) {
        if (!err_flag)
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("getaddrinfo() failed (%s): %s")
                ,netcam->connect_host, gai_strerror(ret));

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("disconnecting netcam (1)"));

        netcam_disconnect(netcam);
        return -1;
    }

    /* Assure any previous connection has been closed - IF we are not in keepalive. */
    if (!netcam->connect_keepalive) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("disconnecting netcam since keep-alive not set."));

        netcam_disconnect(netcam);

        /* Create a new socket. */
        if ((netcam->sock = socket(ai->ai_family, SOCK_STREAM, 0)) < 0) {
            MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO
                ,_("with no keepalive, attempt to create socket failed."));
            return -1;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("with no keepalive, new socket created fd %d"), netcam->sock);

    } else if (netcam->sock == -1) {   /* We are in keepalive mode, check for invalid socket. */
        /* Must be first time, or closed, create a new socket. */
        if ((netcam->sock = socket(ai->ai_family, SOCK_STREAM, 0)) < 0) {
            MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO
                ,_("with keepalive set, invalid socket."
                "This could be the first time. Creating a new one failed."));
            return -1;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("with keepalive set, invalid socket."
            "This could be first time, created a new one with fd %d")
            ,netcam->sock);

        /* Record that this connection has not yet received a Keep-Alive header. */
        netcam->keepalive_thisconn = FALSE;

        /* Check the socket status for the keepalive option. */
        if (getsockopt(netcam->sock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "getsockopt()");
            return -1;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("SO_KEEPALIVE is %s")
            ,optval ? _("ON"):_("OFF"));

        /* Set the option active. */
        optval = 1;
        optlen = sizeof(optval);

        if (setsockopt(netcam->sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "setsockopt()");
            return -1;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("SO_KEEPALIVE set on socket."));
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("re-using socket %d since keepalive is set."), netcam->sock);

    /*
     * We set the socket non-blocking and then use a 'select'
     * system call to control the timeout.
     */

    if ((saveflags = fcntl(netcam->sock, F_GETFL, 0)) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, _("fcntl(1) on socket"));
        netcam_disconnect(netcam);
        return -1;
    }

    /* Set the socket non-blocking. */
    if (fcntl(netcam->sock, F_SETFL, saveflags | O_NONBLOCK) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO,_("fcntl(2) on socket"));
        netcam_disconnect(netcam);
        return -1;
    }

    /* Now the connect call will return immediately. */
    ret = connect(netcam->sock, ai->ai_addr, ai->ai_addrlen);
    back_err = errno;           /* Save the errno from connect */

    freeaddrinfo(ai);

    /* If the connect failed with anything except EINPROGRESS, error. */
    if ((ret < 0) && (back_err != EINPROGRESS)) {
        if (!err_flag)
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO
                ,_("connect() failed (%d)"), back_err);

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("disconnecting netcam (4)"));

        netcam_disconnect(netcam);
        return -1;
    }

    /* Now we do a 'select' with timeout to wait for the connect. */
    FD_ZERO(&fd_w);
    FD_SET(netcam->sock, &fd_w);
    selecttime.tv_sec = CONNECT_TIMEOUT;
    selecttime.tv_usec = 0;
    ret = select(FD_SETSIZE, NULL, &fd_w, NULL, &selecttime);

    if (ret == 0) {            /* 0 means timeout. */
        if (!err_flag)
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("timeout on connect()"));

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("disconnecting netcam (2)"));

        netcam_disconnect(netcam);
        return -1;
    }

    /*
     * A +ve value returned from the select (actually, it must be a
     * '1' showing 1 fd's changed) shows the select has completed.
     * Now we must check the return code from the select.
     */
    len = sizeof(ret);

    if (getsockopt(netcam->sock, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("getsockopt after connect"));
        netcam_disconnect(netcam);
        return -1;
    }

    /* If the return code is anything except 0, error on connect. */
    if (ret) {
        if (!err_flag)
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, _("connect returned error"));

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("disconnecting netcam (3)"));

        netcam_disconnect(netcam);
        return -1;
    }

    /* The socket info is stored in the rbuf structure of our context. */
    rbuf_initialize(netcam);

    return 0;   /* Success */
}

void netcam_check_buffsize(netcam_buff_ptr buff, size_t numbytes)
{
    int min_size_to_alloc;
    int real_alloc;
    int new_size;

    if ((buff->size - buff->used) >= numbytes)
        return;

    min_size_to_alloc = numbytes - (buff->size - buff->used);
    real_alloc = ((min_size_to_alloc / NETCAM_BUFFSIZE) * NETCAM_BUFFSIZE);

    if ((min_size_to_alloc - real_alloc) > 0)
        real_alloc += NETCAM_BUFFSIZE;

    new_size = buff->size + real_alloc;

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
        ,_("expanding buffer from [%d/%d] to [%d/%d] bytes.")
        ,(int) buff->used, (int) buff->size
        ,(int) buff->used, new_size);

    buff->ptr = myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

/**
 * Publish new image
 *
 * Moves the image in 'receiving' into 'latest' and updates last frame time
 */
void netcam_image_read_complete(netcam_context_ptr netcam)
{
    struct timeval curtime;
    netcam_buff *xchg;

    if (gettimeofday(&curtime, NULL) < 0)
        MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");

    netcam->receiving->image_time = curtime;
    /*
     * Calculate our "running average" time for this netcam's
     * frame transmissions (except for the first time).
     * Note that the average frame time is held in microseconds.
     */
    if (netcam->last_image.tv_sec) {
        netcam->av_frame_time = ((9.0 * netcam->av_frame_time) + 1000000.0 *
                                 (curtime.tv_sec - netcam->last_image.tv_sec) +
                                 (curtime.tv_usec- netcam->last_image.tv_usec)) / 10.0;

        /* The following floods the log.  Comment out until it is needed. */
        //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Calculated frame time %f", netcam->av_frame_time);
    }

    netcam->last_image = curtime;

    /*
     * read is complete - set the current 'receiving' buffer atomically
     * as 'latest', and make the buffer previously in 'latest' become
     * the new 'receiving'.
     */
    pthread_mutex_lock(&netcam->mutex);

    xchg = netcam->latest;
    netcam->latest = netcam->receiving;
    netcam->receiving = xchg;
    netcam->imgcnt++;

    /*
     * We have a new frame ready.  We send a signal so that
     * any thread (e.g. the motion main loop) waiting for the
     * next frame to become available may proceed.
     */
    pthread_cond_signal(&netcam->pic_ready);
    pthread_mutex_unlock(&netcam->mutex);
}

/**
 * netcam_read_html_jpeg
 *
 * This routine reads a jpeg image from the netcam.  When it is called,
 * the stream is already positioned just after the image header.
 *
 * This routine is called under the four variations of two different
 * conditions:
 *     1) Streaming or non-streaming camera
 *           Note: Keep-Alive is supported for non-streaming cameras,
 *           if enabled in the netcam's config structure.
 *     2) Header does or does not include Content-Length
 * Additionally, if it is a streaming camera, there must always be a
 * boundary-string.
 *
 * The routine will (attempt to) read the JPEG image.  If a Content-Length
 * is present, it will be used (this will result in more efficient code, and
 * also code which should be better at detecting and recovering from possible
 * error conditions).
 *
 * If a boundary-string is present (and, if the camera is streaming, this
 * *must* be the case), the routine will assure that it is recognized and
 * acted upon.
 *
 * Our algorithm for this will be as follows:
 *     1) If a Content-Length is present, set the variable "remaining"
 *        to be equal to that value, else set it to a "very large"
 *        number.
 *        WARNING !!! Content-Length *must* to be greater than 0, even more
 *        a jpeg image cannot be less than 300 bytes or so.
 *     2) While there is more data available from the camera:
 *        a) If there is a "boundary string" specified (from the initial
 *           header):
 *           i)   If the amount of data in the input buffer is less than
 *                the length of the boundary string, get more data into
 *                the input buffer (error if failure).
 *           ii)  If the boundary string is found, check how many
 *                characters remain in the input buffer before the start
 *                of the boundary string.  If that is less than the
 *                variable "remaining", reset "remaining" to be equal
 *                to that number.
 *        b) Try to copy up to "remaining" characters from the input
 *           buffer into our destination buffer.
 *        c) If there are no more characters available from the camera,
 *           exit this loop, else subtract the number of characters
 *           actually copied from the variable "remaining".
 *     3) If Content-Length was present, and "remaining" is not equal
 *        to zero, generate a warning message for logging.
 *
 *
 * Parameters:
 *      netcam          Pointer to netcam context
 *
 *
 * Returns:             0 for success, -1 for error
 *
 */
static int netcam_read_html_jpeg(netcam_context_ptr netcam)
{
    netcam_buff_ptr buffer;
    size_t remaining;       /* # characters to read */
    size_t maxflush;        /* # chars before boundary */
    size_t rem, rlen, ix;   /* Working vars */
    int retval;
    char *ptr, *bptr, *rptr;
    /*
     * Initialisation - set our local pointers to the context
     * information.
     */
    buffer = netcam->receiving;
    /* Assure the target buffer is empty. */
    buffer->used = 0;
    /* Prepare for read loop. */
    if (buffer->content_length != 0)
        remaining = buffer->content_length;
    else
        remaining = 9999999;

    /* Now read in the data. */
    while (remaining) {
        /* Assure data in input buffer. */
        if (netcam->response->buffer_left <= 0) {
            retval = rbuf_read_bufferful(netcam);

            if (retval <= 0)
                break;

            netcam->response->buffer_left = retval;
            netcam->response->buffer_pos = netcam->response->buffer;
        }

        /* If a boundary string is present, take it into account. */
        bptr = netcam->boundary;

        if (bptr) {
            rptr = netcam->response->buffer_pos;
            rlen = netcam->response->buffer_left;

            /* Loop through buffer looking for start of boundary. */
            while (1) {
                /*
                 * Logic gets a little complicated here.  The
                 * problem is that we are reading in input
                 * data in packets, and there is a (small)
                 * chance that the boundary string could be
                 * split across successive packets.
                 * First a quick check if the string *might*
                 * be in the current buffer.
                 */
                if (rlen > remaining)
                    rlen = remaining;

                if (remaining < netcam->boundary_length)
                    break;

                if ((ptr = memchr(rptr, *bptr, rlen)) == NULL)
                    /* Boundary not here (normal path) */
                    break;
                /*
                 * At least the first char was found in the
                 * buffer - check for the rest.
                 */
                rem = rlen - (ptr - rptr);
                for (ix = 1; (ix < rem) && (ix < netcam->boundary_length); ix++) {
                    if (ptr[ix] != bptr[ix])
                        break;
                }

                if ((ix != netcam->boundary_length) && (ix != rem)) {
                    /*
                     * Not pointing at a boundary string -
                     * step along input.
                     */
                    ix = ptr - rptr + 1;
                    rptr += ix;
                    rlen -= ix;

                    if (rlen <= 0)
                        /* boundary not in buffer - go copy out */
                        break;
                    /*
                     * Not yet decided - continue
                     * through input.
                     */
                    continue;
                }
                /*
                 * If we finish the 'for' with
                 * ix == boundary_length, that means we found
                 * the string, and should copy any data which
                 * precedes it into the target buffer, then
                 * exit the main loop.
                 */
                if (ix == netcam->boundary_length) {
                    if ((ptr - netcam->response->buffer) < (int) remaining)
                        remaining = ptr - netcam->response->buffer;

                    /* Go copy everything up to boundary. */
                    break;
                }

                /*
                 * If not, and ix == rem, that means we reached
                 * the end of the input buffer in the middle of
                 * our check, which is the (somewhat messy)
                 * problem mentioned above.
                 *
                 * Assure there is data before potential
                 * boundary string.
                 */
                if (ptr != netcam->response->buffer) {
                    /*
                     * We have a boundary string crossing
                     * packets :-(. We will copy all the
                     * data up to the beginning of the
                     * potential boundary, then re-position
                     * the (partial) string to the
                     * beginning and get some more input
                     * data.  First we flush the input
                     * buffer up to the beginning of the
                     * (potential) boundary string.
                     */
                    ix = ptr - netcam->response->buffer_pos;
                    netcam_check_buffsize(buffer, ix);
                    retval = rbuf_flush(netcam, buffer->ptr + buffer->used, ix);
                    buffer->used += retval;
                    remaining -= retval;

                    /*
                     * Now move the boundary fragment to
                     * the head of the input buffer.
                     * This is really a "hack" - ideally,
                     * we should have a function within the
                     * module netcam_wget.c to do this job!
                     */

                    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                        ,_("Potential split boundary - "
                        "%d chars flushed, %d re-positioned")
                        , ix, (int) netcam->response->buffer_left);

                    memmove(netcam->response->buffer, ptr,
                            netcam->response->buffer_left);
                }   /* End of boundary split over buffer. */

                retval = netcam_recv(netcam, netcam->response->buffer +
                                     netcam->response->buffer_left,
                                     sizeof(netcam->response->buffer) -
                                     netcam->response->buffer_left);

                if (retval <= 0) { /* This is a fatal error. */
                    MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO
                        ,_("recv() fail after boundary string"));
                    return -1;
                }

                /* Reset the input buffer pointers. */
                netcam->response->buffer_left = retval + netcam->response->buffer_left;
                netcam->response->buffer_pos = netcam->response->buffer;

                /* This will cause a 'continue' of the main loop. */
                bptr = NULL;

                /* Return to do the boundary compare from the start. */
                break;
            }             /* End of while(1) input buffer search. */

            /* !bptr shows we're processing split boundary. */
            if (!bptr)
                continue;
        }   /* end of if (bptr) */

        /* boundary string not present, so just write out as much data as possible. */
        if (remaining) {
            maxflush = MINVAL(netcam->response->buffer_left, remaining);
            netcam_check_buffsize(buffer, maxflush);
            retval = rbuf_flush(netcam, buffer->ptr + buffer->used, maxflush);
            buffer->used += retval;
            remaining -= retval;
        }
    }

    /* Fix starting of JPEG if needed , some cameras introduce thrash before
     * SOI  0xFFD8  Start Of Image
     */
    netcam_fix_jpeg_header(netcam);

    netcam_image_read_complete(netcam);

    if (netcam->caps.streaming == NCS_UNSUPPORTED) {
        if (!netcam->connect_keepalive) {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("disconnecting netcam since keep-alive not set."));
            netcam_disconnect(netcam);
            return 0;
        }
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("leaving netcam connected."));
    }

    return 0;
}

/**
 * netcam_http_request
 *
 * This routine initiates a connection on the specified netcam,
 * for which every parameter has already been set (url, etc).
 * It uses the HTTP protocol, which is what many IP cameras use.
 * If this function succeeds, the HTTP response along with the
 * headers are already processed, and you can start reading contents
 * from here.
 *
 * Parameters:
 *      netcam          Pointer to a netcam_context structure
 *
 * Returns:             0 on success, -1 if an error occurs.
 */
static int netcam_http_request(netcam_context_ptr netcam)
{
    int ix;

    /*
     * Our basic initialisation has been completed.  Now we will attempt
     * to connect with the camera so that we can then get a "header"
     * in order to find out what kind of camera we are dealing with,
     * as well as what are the picture dimensions.  Note that for
     * this initial connection, any failure will cause an error
     * return from netcam_start (unlike later possible attempts at
     * re-connecting, if the network connection is later interrupted).
     */
    for (ix = 0; ix < MAX_HEADER_RETRIES; ix++) {
        /*
         * netcam_connect does an automatic netcam_close, so it's
         * safe to include it as part of this loop
         * (Not always true now Keep-Alive is implemented).
         */
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("about to try to connect, time #%d"), ix);

        if (netcam_connect(netcam, 0) != 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Failed to open camera - check your config and that netcamera is online"));

            /* Fatal error on startup */
            ix = MAX_HEADER_RETRIES;
            break;;
        }

        if (netcam_read_first_header(netcam) >= 0)
            break;

        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Error reading first header - re-trying"));
    }

    if (ix == MAX_HEADER_RETRIES) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Failed to read first camera header - giving up for now"));
        return -1;
    }

    return 0;
}

/**
 * netcam_http_build_url
 *
 * This routing takes care of the url-processing part of the http protocol.
 * This includes url scheme and parsing, proxy handling, http-authentication
 * preparation, response buffer allocation and so on. At the end of this
 * routine, we are ready to call netcam_http_request().
 *
 * Parameters:
 *      netcam          Pointer to a netcam_context structure
 *      url             Pointer to a netcam url structure
 *
 * Returns:             0 on success,
 *                      or -1 if an fatal error occurs.
 */
static int netcam_http_build_url(netcam_context_ptr netcam, struct url_t *url)
{
    struct context *cnt = netcam->cnt;
    const char *ptr;                  /* Working var */
    char *userpass;                   /* Temp pointer to config value */
    char *encuserpass;                /* Temp storage for encoded ver */
    char *request_pass = NULL;        /* Temp storage for base64 conv */
    int ix;

    /* First the http context structure. */
    netcam->response = mymalloc(sizeof(struct rbuf));

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Netcam has flags:"
        " HTTP/1.0: %s HTTP/1.1: %s Keep-Alive %s.")
        ,netcam->connect_http_10 ? "1":"0", netcam->connect_http_11 ? "1":"0"
        ,netcam->connect_keepalive ? "ON":"OFF");

    /*
     * The network camera may require a username and password.  If
     * so, the information can come from two different places in the
     * motion configuration file.  Either it can be present in
     * the netcam_userpass, or it can be present as a part of the URL
     * for the camera.  We assume the first of these has a higher
     * relevance.
     */
    if (cnt->conf.netcam_userpass)
        ptr = cnt->conf.netcam_userpass;
    else
        ptr = url->userpass;

    /* motion_base64_encode needs up to 3 additional chars. */
    if (ptr) {
        userpass = mymalloc(strlen(ptr) + 3);
        strcpy(userpass, ptr);
    } else {
        userpass = NULL;
    }

    /*
     * Now we want to create the actual string which will be used to
     * connect to the camera.  It may or may not contain a username /
     * password.  We first compose a basic connect message, then check
     * if a Keep-Alive header is to be included (or just 'close'), then
     * whether a username / password is required and, if so, just
     * concatenate it with the request.
     *
     */

    /* Space for final \r\n plus string terminator. */
    ix = 3;

    /* See if username / password is required. */
    if (userpass) { /* If either of the above are non-NULL. */
        /* Allocate space for the base64-encoded string. */
        encuserpass = mymalloc(BASE64_LENGTH(strlen(userpass)) + 1);
        /* Fill in the value. */
        motion_base64_encode(userpass, encuserpass, strlen(userpass));
        /* Now create the last part (authorization) of the request. */
        request_pass = mymalloc(strlen(connect_auth_req) +
                                strlen(encuserpass) + 1);
        ix += sprintf(request_pass, connect_auth_req, encuserpass);
        /* Free the working variables. */
        free(encuserpass);
    }

    /*
     * We are now ready to set up the netcam's "connect request".  Most of
     * this comes from the (preset) string 'connect_req', but additional
     * characters are required if there is a proxy server, or if there is
     * a Keep-Alive connection rather than a close connection, or
     * a username / password for the camera.  The variable 'ix' currently
     * has the number of characters required for username/password (which
     * could be zero) and for the \r\n and string terminator.  We will also
     * always need space for the netcam path, and if a proxy is being used
     * we also need space for a preceding  'http://{hostname}' for the
     * netcam path.
     * Note: Keep-Alive (but not HTTP 1.1) is disabled if a proxy URL
     * is set, since HTTP 1.0 Keep-alive cannot be transferred through.
     */
    if (cnt->conf.netcam_proxy) {
        /*
         * Allocate space for a working string to contain the path.
         * The extra 4 is for "://" and string terminator.
         */
        if (url->port != 0) {
            ptr = mymalloc(strlen(url->service) + strlen(url->host) + 7 + strlen(url->path) + 4);
            sprintf((char *)ptr, "http://%s:%d%s", url->host, url->port , url->path);
        }else {
            ptr = mymalloc(strlen(url->service) + strlen(url->host) + strlen(url->path) + 4);
            sprintf((char *)ptr, "http://%s%s", url->host, url->path);
        }

        netcam->connect_keepalive = FALSE; /* Disable Keepalive if proxy */
        free((void *)netcam->cnt->conf.netcam_keepalive);
        netcam->cnt->conf.netcam_keepalive = strdup("off");

        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Removed netcam_keepalive flag due to proxy set."
            "Proxy is incompatible with Keep-Alive."));
    } else {
        /* If no proxy, set as netcam_url path. */
        ptr = url->path;
        /*
         * After generating the connect message the string
         * will be freed, so we don't want netcam_url_free
         * to free it as well.
         */
        url->path = NULL;
    }

    ix += strlen(ptr);

    /*
     * Now add the required number of characters for the close header
     * or Keep-Alive header.  We test the flag which can be unset if
     * there is a problem (rather than the flag in the conf structure
     * which is read-only.
     */

    if (netcam->connect_keepalive)
        ix += strlen(connect_req_keepalive);
    else
        ix += strlen(connect_req_close);


    /*
     * Point to either the HTTP 1.0 or 1.1 request header set
     * If the configuration is anything other than 1.1, use 1.0
     * as a default. This avoids a chance of being left with none.
     */
    if (netcam->connect_http_11 == TRUE)
        connect_req = connect_req_http11;
    else
        connect_req = connect_req_http10;

    /*
     * Now that we know how much space we need, we can allocate space
     * for the connect-request string.
     */
    netcam->connect_request = mymalloc(strlen(connect_req) + ix +
                              strlen(netcam->connect_host));

    /* Now create the request string with an sprintf. */
    sprintf(netcam->connect_request, connect_req, ptr,
            netcam->connect_host);

    if (netcam->connect_keepalive)
        strcat(netcam->connect_request, connect_req_keepalive);
    else
        strcat(netcam->connect_request, connect_req_close);


    if (userpass) {
        strcat(netcam->connect_request, request_pass);
        free(request_pass);
        free(userpass);
    }

    /* Put on the final CRLF onto the request. */
    strcat(netcam->connect_request, "\r\n");
    free((void *)ptr);
    netcam_url_free(url);  /* Cleanup the url data. */

    /*  Note that log messages are commented out to avoid leaking info related
     *  to user/host/pass etc.  Keeing them in the code for easier debugging if
     *  it is needed
     */
    //MOTION_LOG(INF , TYPE_NETCAM, NO_ERRNO, "Camera connect"
    //           " string is ''%s'' End of camera connect string.",
    //           netcam->connect_request);

    return 0;
}

/**
 * netcam_setup_html
 *      This function will parse the netcam url, connect to the camera,
 *      set its type to jpeg-based, detect multipart and keep-alive,
 *      and the get_image method accordingly. The cam can be non-streaming
 *      or multipart-streaming.
 *
 * Parameters
 *
 *      netcam  Pointer to the netcam_context for the camera
 *      url     Pointer to the url of the camera
 *
 * Returns:     0 on success (camera link ok) or -1 if an error occurred.
 *
 */
int netcam_setup_html(netcam_context_ptr netcam, struct url_t *url)
{
    netcam->timeout.tv_sec = READ_TIMEOUT;

    /*
     * This netcam is http-based, so build the required URL and
     * structures, like the connection-string and so on.
     */
    if (netcam_http_build_url(netcam, url) < 0)
        return -1;

    /*
     * Then we will send our http request and get headers.
     */
    if (netcam_http_request(netcam) < 0)
         return -1;

    /*
     * If this is a streaming camera, we need to position just
     * past the boundary string and read the image header.
     */
    if (netcam->caps.streaming == NCS_MULTIPART) {
        if (netcam_read_next_header(netcam) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Failed to read first stream header - giving up for now"));
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("connected, going on to read image."));

    netcam->get_image = netcam_read_html_jpeg;
    return 0;
}

/**
 * netcam_mjpg_buffer_refill
 *
 * This routing reads content from the MJPG-camera until the response
 * buffer of the specified netcam_context is full. If the connection is
 * lost during this operation, it tries to re-connect.
 *
 * Parameters:
 *      netcam          Pointer to a netcam_context structure
 *
 * Returns:             The number of read bytes,
 *                      or -1 if an fatal connection error occurs.
 */
static int netcam_mjpg_buffer_refill(netcam_context_ptr netcam)
{
    int retval;

    if (netcam->response->buffer_left > 0)
        return netcam->response->buffer_left;

    while (1) {
        retval = rbuf_read_bufferful(netcam);
        if (retval <= 0) { /* If we got 0, we timeoutted. */
            MOTION_LOG(ALR, TYPE_NETCAM, NO_ERRNO
                ,_("Read error, trying to reconnect.."));
            /* We may have lost the connexion */
            if (netcam_http_request(netcam) < 0) {
                MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                    ,_("lost the cam."));
                return -1; /* We REALLY lost the cam... bail out for now. */
            }
        }

        if (retval > 0)
            break;
    }

    netcam->response->buffer_left = retval;
    netcam->response->buffer_pos = netcam->response->buffer;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Refilled buffer with [%d] bytes from the network."), retval);

    return retval;
}


/**
 * netcam_read_mjpg_jpeg
 *
 *     This routine reads from a netcam using a MJPG-chunk based
 *     protocol, used by Linksys WVC200 for example.
 *     This implementation has been made by reverse-engineering
 *     the protocol, so it may contain bugs and should be considered as
 *     experimental.
 *
 * Protocol explanation:
 *
 *     The stream consists of JPG pictures, spanned across multiple
 *     MJPG chunks (in general 3 chunks, altough that's not guaranteed).
 *
 *     Each data chunk can range from 1 to 65535 bytes + a header, altough
 *     i have not seen anything bigger than 20000 bytes + a header.
 *
 *     One MJPG chunk is constituted by a header plus the chunk data.
 *     The chunk header is of fixed size, and the following data size
 *     and position in the frame is specified in the chunk header.
 *
 *     From what i have seen on WVC200 cameras, the stream always begins
 *     on JPG frame boundary, so you don't have to worry about beginning
 *     in the middle of a frame.
 *
 *     See netcam.h for the mjpg_header structure and more details.
 *
 * Parameters:
 *      netcam          Pointer to a netcam_context structure
 *
 * Returns:             0 if an image was obtained from the camera,
 *                      or -1 if an error occurred.
 */
static int netcam_read_mjpg_jpeg(netcam_context_ptr netcam)
{
    netcam_buff_ptr buffer;
    mjpg_header mh;
    size_t read_bytes;
    int retval;

    /*
     * Initialisation - set our local pointers to the context
     * information.
     */
    buffer = netcam->receiving;
    /* Assure the target buffer is empty. */
    buffer->used = 0;

    if (netcam_mjpg_buffer_refill(netcam) < 0)
        return -1;

    /* Loop until we have a complete JPG. */
    while (1) {
        read_bytes = 0;
        while (read_bytes < sizeof(mh)) {

            /* Transfer what we have in buffer in the header structure. */
            retval = rbuf_flush(netcam, ((char *)&mh) + read_bytes, sizeof(mh) - read_bytes);

            read_bytes += retval;

            MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                ,_("Read [%d/%d] header bytes."), read_bytes, sizeof(mh));

            /* If we don't have received a full header, refill our buffer. */
            if (read_bytes < sizeof(mh)) {
                if (netcam_mjpg_buffer_refill(netcam) < 0)
                    return -1;
            }
        }

        /* Now check the validity of our header. */
        if (strncmp(mh.mh_magic, MJPG_MH_MAGIC, MJPG_MH_MAGIC_SIZE)) {
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                ,_("Invalid header received, reconnecting"));
            /*
             * We shall reconnect to restart the stream, and get a chance
             * to resync.
             */
            if (netcam_http_request(netcam) < 0)
                return -1; /* We lost the cam... bail out. */
            /* Even there, we need to resync. */
            buffer->used = 0;
            continue ;
        }

        /* Make room for the chunk. */
        netcam_check_buffsize(buffer, (int) mh.mh_chunksize);

        read_bytes = 0;
        while (read_bytes < mh.mh_chunksize) {
            retval = rbuf_flush(netcam, buffer->ptr + buffer->used + read_bytes,
                                mh.mh_chunksize - read_bytes);
            read_bytes += retval;
            MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                ,_("Read [%d/%d] chunk bytes, [%d/%d] total")
                ,read_bytes, mh.mh_chunksize
                ,buffer->used + read_bytes, mh.mh_framesize);

            if (retval < (int) (mh.mh_chunksize - read_bytes)) {
                /* MOTION_LOG(EMG, TYPE_NETCAM, NO_ERRNO, "Chunk incomplete, going to refill."); */
                if (netcam_mjpg_buffer_refill(netcam) < 0)
                    return -1;

            }
        }
        buffer->used += read_bytes;

        MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
            ,_("Chunk complete, buffer used [%d] bytes."), buffer->used);

        /* Is our JPG image complete ? */
        if (mh.mh_framesize == buffer->used) {
            MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                ,_("Image complete, buffer used [%d] bytes."), buffer->used);
            break;
        }
        /* MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Rlen now at [%d] bytes", rlen); */
    }

    netcam_image_read_complete(netcam);

    return 0;
}

/**
 * netcam_setup_mjpg
 *      This function will parse the netcam url, connect to the camera,
 *      set its type to MJPG-Streaming, and the get_image method accordingly.
 *
 * Parameters
 *
 *      netcam  Pointer to the netcam_context for the camera
 *      url     Pointer to the url of the camera
 *
 * Returns:     0 on success (camera link ok) or -1 if an error occurred.
 *
 */
int netcam_setup_mjpg(netcam_context_ptr netcam, struct url_t *url)
{
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("now calling netcam_setup_mjpg()"));

    netcam->timeout.tv_sec = READ_TIMEOUT;

    strcpy(url->service, "http"); /* Put back a real URL service. */

    /*
     * This netcam is http-based, so build the required URL and
     * structures, like the connection-string and so on.
     */
    if (netcam_http_build_url(netcam, url) != 0)
        return -1;

    /* Then we will send our http request and get headers. */
    if (netcam_http_request(netcam) < 0)
        return -1;

    /* We have a special type of streaming camera. */
    netcam->caps.streaming = NCS_BLOCK;

    /*
     * We are positionned right just at the start of the first MJPG
     * header, so don't move anymore, initialization complete.
     */
    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("connected, going on to read and decode MJPG chunks."));

    netcam->get_image = netcam_read_mjpg_jpeg;

    return 0;
}

/**
 * netcam_read_file_jpeg
 *
 *      This routine reads local image jpeg. ( netcam_url jpeg:///path/image.jpg )
 *      The current implementation is still a little experimental,
 *      and needs some additional code for error detection and
 *      recovery.
 */
static int netcam_read_file_jpeg(netcam_context_ptr netcam)
{
    int loop_counter = 0;

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("Begin"));

    netcam_buff_ptr buffer;
    int len;
    struct stat statbuf;

    /* Point to our working buffer. */
    buffer = netcam->receiving;
    buffer->used = 0;

    /*int fstat(int filedes, struct stat *buf);*/
    do {
        if (stat(netcam->file->path, &statbuf)) {
            MOTION_LOG(CRT, TYPE_NETCAM, SHOW_ERRNO
                ,_("stat(%s) error"), netcam->file->path);
            return -1;
        }

        MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
            ,_("statbuf.st_mtime[%d] != last_st_mtime[%d]")
            , statbuf.st_mtime, netcam->file->last_st_mtime);

        /* its waits POLLING_TIMEOUT */
        if (loop_counter>((POLLING_TIMEOUT*1000*1000)/(POLLING_TIME/1000))) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("waiting new file image timeout"));
            return -1;
        }

        MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
            ,_("delay waiting new file image "));

        //its waits 5seconds - READ_TIMEOUT
        //SLEEP(netcam->timeout.tv_sec, netcam->timeout.tv_usec*1000);
        SLEEP(0, POLLING_TIME); // its waits 500ms
        /*return -1;*/
        loop_counter++;

    } while (statbuf.st_mtime == netcam->file->last_st_mtime);

    netcam->file->last_st_mtime = statbuf.st_mtime;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("processing new file image - st_mtime %d"), netcam->file->last_st_mtime);

    /* Assure there's enough room in the buffer. */
    while (buffer->size < (size_t)statbuf.st_size)
        netcam_check_buffsize(buffer, statbuf.st_size);


    /* Do the read */
    netcam->file->control_file_desc = open(netcam->file->path, O_RDONLY|O_CLOEXEC);
    if (netcam->file->control_file_desc < 0) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("open(%s) error: %d")
            ,netcam->file->path, netcam->file->control_file_desc);
        return -1;
    }
    if ((len = read(netcam->file->control_file_desc,
                    buffer->ptr + buffer->used, statbuf.st_size)) < 0) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("read(%s) error: %d"), netcam->file->control_file_desc, len);
        return -1;
    }

    buffer->used += len;
    close(netcam->file->control_file_desc);

    netcam_image_read_complete(netcam);

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO,_("End"));

    return 0;
}

tfile_context *file_new_context(void)
{
    /* Note that mymalloc will exit on any problem. */
    return mymalloc(sizeof(tfile_context));
}

void file_free_context(tfile_context* ctxt)
{
    if (ctxt == NULL)
        return;

    free(ctxt->path);
    free(ctxt);
}

int netcam_setup_file(netcam_context_ptr netcam, struct url_t *url)
{

    if ((netcam->file = file_new_context()) == NULL)
        return -1;

    /*
     * We copy the strings out of the url structure into the ftp_context
     * structure.  By setting url->{string} to NULL we effectively "take
     * ownership" of the string away from the URL (i.e. it won't be freed
     * when we cleanup the url structure later).
     */
    netcam->file->path = url->path;
    url->path = NULL;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("netcam->file->path %s"), netcam->file->path);

    netcam->get_image = netcam_read_file_jpeg;

    return 0;
}

/**
 * netcam_recv
 *
 *      This routine receives the next block from the netcam.  It takes care
 *      of the potential timeouts and interrupt which may occur because of
 *      the settings from setsockopt.
 *
 * Parameters:
 *
 *      netcam          Pointer to a netcam context
 *      buffptr         Pointer to the receive buffer
 *      buffsize        Length of the buffer
 *
 * Returns:
 *      If successful, the length of the message received, otherwise the
 *      error reply from the system call.
 *
 */
ssize_t netcam_recv(netcam_context_ptr netcam, void *buffptr, size_t buffsize)
{
    ssize_t retval;
    fd_set fd_r;
    struct timeval selecttime;

    if (netcam->sock < 0)
        return -1; /* We are not connected, it's impossible to receive data. */

    FD_ZERO(&fd_r);
    FD_SET(netcam->sock, &fd_r);
    selecttime = netcam->timeout;

    retval = select(FD_SETSIZE, &fd_r, NULL, NULL, &selecttime);
    if (retval == 0)              /* 0 means timeout */
        return -1;

    return recv(netcam->sock, buffptr, buffsize, 0);
}


