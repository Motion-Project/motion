/*
 *      Much of the FTP code was inspired by the nanoftp.c module from
 *      libxml2 (Copyright Daniel Veillard, 2003).  The routines have been
 *      modified to fit the needs of the Motion project.
 *
 *      Copyright 2005, William M. Brack
 *      This software is distributed under the GNU Public license Version 2.
 *      See also the file 'COPYING'.
 *
 */
#include "motion.h"  /* Needs to come first, because _GNU_SOURCE_ set there. */

#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include "netcam_ftp.h"

/**
* ftp_new_context
*
*      Create a new FTP context structure.
*
* Parameters
*
*       None
*
* Returns:     Pointer to the newly-created structure, NULL if error.
*
*/
ftp_context_pointer ftp_new_context(void)
{
    ftp_context_pointer ret;

    /* Note that mymalloc will exit on any problem. */
    ret = mymalloc(sizeof(ftp_context));

    memset(ret, 0, sizeof(ftp_context));
    ret->control_file_desc = -1;                /* No control connection yet. */
    ret->data_file_desc = -1;                   /* No data connection yet. */
    return ret;
}

/**
* ftp_free_context
*
*      Free the resources allocated for this context.
*
* Parameters
*
*      ctxt    Pointer to the ftp_context structure.
*
* Returns:     Nothing
*
*/
void ftp_free_context(ftp_context_pointer ctxt)
{
    if (ctxt == NULL)
        return;

    free(ctxt->path);
    free(ctxt->user);
    free(ctxt->passwd);
    if (ctxt->control_file_desc >= 0)
        close(ctxt->control_file_desc);

    free(ctxt);
}

/**
* ftp_parse_response
*
*      Parses the answer from the server, extracting the numeric code.
*
* Parameters:
*
*      buf     the buffer containing the response.
*      len     the buffer length.
*
* Returns:
*     0 for errors
*     +XXX for last line of response
*     -XXX for response to be continued
*/
static int ftp_parse_response(char *buf, int len)
{
    int val = 0;

    if (len < 3)
        return -1;

    if ((*buf >= '0') && (*buf <= '9'))
        val = val * 10 + (*buf - '0');
    else
        return 0;

    buf++;

    if ((*buf >= '0') && (*buf <= '9'))
        val = val * 10 + (*buf - '0');
    else
        return 0;

    buf++;

    if ((*buf >= '0') && (*buf <= '9'))
        val = val * 10 + (*buf - '0');
    else
        return 0;

    buf++;

    if (*buf == '-')
        return -val;

    return val;
}

/**
* ftp_get_more
*
*      Read more information from the FTP control connection.
*
* Parameters:
*
*      ctxt    pointer to an FTP context.
*
* Returns the number of bytes read, < 0 indicates an error
*/
static int ftp_get_more(ftp_context_pointer ctxt)
{
    int len;
    int size;

    /* Validate that our context structure is valid. */
    if ((ctxt == NULL) || (ctxt->control_file_desc < 0))
        return -1;

    if ((ctxt->control_buffer_index < 0) || (ctxt->control_buffer_index > FTP_BUF_SIZE))
        return -1;

    if ((ctxt->control_buffer_used < 0) || (ctxt->control_buffer_used > FTP_BUF_SIZE))
        return -1;

    if (ctxt->control_buffer_index > ctxt->control_buffer_used)
        return -1;

    /* First pack the control buffer. */
    if (ctxt->control_buffer_index > 0) {
        memmove(&ctxt->control_buffer[0],
        &ctxt->control_buffer[ctxt->control_buffer_index],
        ctxt->control_buffer_used - ctxt->control_buffer_index);
        ctxt->control_buffer_used -= ctxt->control_buffer_index;
        ctxt->control_buffer_index = 0;
    }

    size = FTP_BUF_SIZE - ctxt->control_buffer_used;

    if (size == 0)
        return 0;

    /* Read the amount left on the control connection. */
    if ((len = recv(ctxt->control_file_desc,
                    &ctxt->control_buffer[ctxt->control_buffer_index], size, 0)) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: recv failed in ftp_get_more");
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    ctxt->control_buffer_used += len;
    ctxt->control_buffer[ctxt->control_buffer_used] = 0;

    return len;
}

/**
* ftp_get_response
*
*      Read the response from the FTP server after a command.
*
* Parameters
*
*      ctxt    pointer to an FTP context
*
* Returns the code number
*/
static int ftp_get_response(ftp_context_pointer ctxt)
{
    char *ptr, *end;
    int len;
    int res = -1, cur = -1;

    if ((ctxt == NULL) || (ctxt->control_file_desc < 0))
        return -1;

    get_more:
    /*
     * Assumes everything up to control_buffer[control_buffer_index]
     * has been read and analyzed.
     */
    len = ftp_get_more(ctxt);

    if (len < 0)
        return -1;


    if ((ctxt->control_buffer_used == 0) && (len == 0))
        return -1;


    ptr = &ctxt->control_buffer[ctxt->control_buffer_index];
    end = &ctxt->control_buffer[ctxt->control_buffer_used];

    while (ptr < end) {
        cur = ftp_parse_response(ptr, end - ptr);
        if (cur > 0) {
            /*
             * Successfully scanned the control code, skip
             * till the end of the line, but keep the index to be
             * able to analyze the result if needed.
             */
            res = cur;
            ptr += 3;
            ctxt->control_buffer_answer = ptr - ctxt->control_buffer;
            while ((ptr < end) && (*ptr != '\n'))
                ptr++;

            if (*ptr == '\n')
                ptr++;

            if (*ptr == '\r')
                ptr++;

            break;
        }

        while ((ptr < end) && (*ptr != '\n'))
            ptr++;

        if (ptr >= end) {
            ctxt->control_buffer_index = ctxt->control_buffer_used;
            goto get_more;
        }

        if (*ptr != '\r')
            ptr++;
    }

    if (res < 0)
        goto get_more;

    ctxt->control_buffer_index = ptr - ctxt->control_buffer;

    return (res / 100);
}

/**
* ftp_send_user
*       Sends the user authentication.
*/
static int ftp_send_user(ftp_context_pointer ctxt)
{
    char buf[200];
    int len;
    int res;

    if (ctxt->user == NULL)
        snprintf(buf, sizeof(buf), "USER anonymous\r\n");
    else
        snprintf(buf, sizeof(buf), "USER %s\r\n", ctxt->user);

    buf[sizeof(buf) - 1] = 0;
    len = strlen(buf);
    res = send(ctxt->control_file_desc, buf, len, 0);

    if (res < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_send_user");
        return res;
    }
    return 0;
}

/**
* ftp_send_passwd
*       Sends the password authentication.
*/
static int ftp_send_passwd(ftp_context_pointer ctxt)
{
    char buf[200];
    int len;
    int res;

    if (ctxt->passwd == NULL)
        snprintf(buf, sizeof(buf), "PASS anonymous@\r\n");
    else
        snprintf(buf, sizeof(buf), "PASS %s\r\n", ctxt->passwd);

    buf[sizeof(buf) - 1] = 0;
    len = strlen(buf);
    res = send(ctxt->control_file_desc, buf, len, 0);

    if (res < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_send_passwd");
        return res;
    }

    return 0;
}

/**
* ftp_quit
*
*      Send a QUIT command to the server
*
* Parameters:
*
*      ctxt    pointer to an FTP context
*
* Returns -1 in case of error, 0 otherwise
*/
static int ftp_quit(ftp_context_pointer ctxt)
{
    char buf[200];
    int len, res;

    if ((ctxt == NULL) || (ctxt->control_file_desc < 0))
        return -1;

    snprintf(buf, sizeof(buf), "QUIT\r\n");
    len = strlen(buf);
    res = send(ctxt->control_file_desc, buf, len, 0);

    if (res < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_quit");
        return res;
    }

    return 0;
}

/**
* ftp_connect
*
*      Tries to open a control connection.
*
* Parameters:
*
*      ctxt    an FTP context
*
* Returns -1 in case of error, 0 otherwise.
*/
int ftp_connect(netcam_context_ptr netcam)
{
    ftp_context_pointer ctxt;
    struct hostent *hp;
    int port;
    int res;
    int addrlen = sizeof (struct sockaddr_in);

    if (netcam == NULL)
        return -1;

    ctxt = netcam->ftp;

    if (ctxt == NULL)
        return -1;

    if (netcam->connect_host == NULL)
        return -1;

    /* Do the blocking DNS query. */
    port = netcam->connect_port;

    if (port == 0)
        port = 21;

    memset (&ctxt->ftp_address, 0, sizeof(ctxt->ftp_address));

    hp = gethostbyname (netcam->connect_host);

    if (hp == NULL) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: gethostbyname failed in ftp_connect");
        return -1;
    }

    if ((unsigned int) hp->h_length >
         sizeof(((struct sockaddr_in *)&ctxt->ftp_address)->sin_addr)) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: gethostbyname address mismatch "
                   "in ftp_connect");
        return -1;
    }

    /* Prepare the socket */
    ((struct sockaddr_in *)&ctxt->ftp_address)->sin_family = AF_INET;
    memcpy (&((struct sockaddr_in *)&ctxt->ftp_address)->sin_addr, hp->h_addr_list[0], hp->h_length);
    ((struct sockaddr_in *)&ctxt->ftp_address)->sin_port = (u_short)htons ((unsigned short)port);
    ctxt->control_file_desc = socket (AF_INET, SOCK_STREAM, 0);
    addrlen = sizeof (struct sockaddr_in);

    if (ctxt->control_file_desc < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: socket failed");
        return -1;
    }

    /* Do the connect. */
    if (connect(ctxt->control_file_desc, (struct sockaddr *) &ctxt->ftp_address,
        addrlen) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: Failed to create a connection");
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    /* Wait for the HELLO from the server. */
    res = ftp_get_response(ctxt);

    if (res != 2) {
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    /* Do the authentication */
    res = ftp_send_user(ctxt);

    if (res < 0) {
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    res = ftp_get_response(ctxt);

    switch (res) {
    case 2:
        return 0;
    case 3:
        break;
    case 1:
    case 4:
    case 5:
    case -1:
    default:
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    res = ftp_send_passwd(ctxt);

    if (res < 0) {
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    res = ftp_get_response(ctxt);
    switch (res) {
    case 2:
        break;
    case 3:
        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: FTP server asking for ACCT on anonymous");
    case 1:
    case 4:
    case 5:
    case -1:
    default:
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        ctxt->control_file_desc = -1;
        return-1;
    }

    return 0;
}

/**
* ftp_get_connection
*
*      Try to open a data connection to the server.
*
* Parameters:
*
*      ctxt    pointer to an FTP context.
*
* Returns -1 in case of error, 0 otherwise
*/
static int ftp_get_connection(ftp_context_pointer ctxt)
{
    char buf[200], *cur;
    int len, i;
    int res;
    int on;
    unsigned char ad[6], *adp, *portp;
    unsigned int temp[6];
    struct sockaddr_in data_address;
    unsigned int data_address_length;

    if (ctxt == NULL)
    return -1;

    /* Set up a socket for our data address. */
    if (ctxt->data_file_desc != -1)
        close(ctxt->data_file_desc);

    memset (&data_address, 0, sizeof(data_address));
    ctxt->data_file_desc = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (ctxt->data_file_desc < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: socket failed");
        return -1;
    }

    on = 1;

    if (setsockopt(ctxt->data_file_desc, SOL_SOCKET, SO_REUSEADDR,
        (char *)&on, sizeof(on)) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: setting socket option SO_REUSEADDR");
        return -1;
    }

    ((struct sockaddr_in *)&data_address)->sin_family = AF_INET;
    data_address_length = sizeof (struct sockaddr_in);

    if (ctxt->passive) {
        /* Send PASV command over control channel. */
        snprintf (buf, sizeof(buf), "PASV\r\n");
        len = strlen (buf);
        res = send(ctxt->control_file_desc, buf, len, 0);

        if (res < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_get_connection");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return res;
        }
        /* Check server's answer */
        res = ftp_get_response(ctxt);

        if (res != 2) {
            if (res == 5) {
                close(ctxt->data_file_desc);
                ctxt->data_file_desc = -1;
                return -1;
            } else {
                /* Retry with an active connection. */
                close(ctxt->data_file_desc);
                ctxt->data_file_desc = -1;
                ctxt->passive = 0;
            }
        }
        /* Parse the IP address and port supplied by the server. */
        cur = &ctxt->control_buffer[ctxt->control_buffer_answer];

        while (((*cur < '0') || (*cur > '9')) && *cur != '\0')
            cur++;

        if (sscanf(cur, "%u,%u,%u,%u,%u,%u", &temp[0], &temp[1], &temp[2],
            &temp[3], &temp[4], &temp[5]) != 6) {
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: Invalid answer to PASV");
            if (ctxt->data_file_desc != -1) {
                close (ctxt->data_file_desc);
                ctxt->data_file_desc = -1;
            }
            return -1;
        }

        for (i = 0; i < 6; i++)
            ad[i] = (unsigned char) (temp[i] & 0xff) ;

        memcpy (&((struct sockaddr_in *)&data_address)->sin_addr, &ad[0], 4);
        memcpy (&((struct sockaddr_in *)&data_address)->sin_port, &ad[4], 2);

        /* Now try to connect to the data port. */
        if (connect(ctxt->data_file_desc, (struct sockaddr *) &data_address,
            data_address_length) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: Failed to create a data connection");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return -1;
        }
    } else {
        /*
         * We want to bind to a port to receive the data.  To do this,
         * we need the address of our host.  One easy way to get it is
         * to get the info from the control connection that we have
         * with the remote server.
         */
        getsockname(ctxt->control_file_desc, (struct sockaddr *)&data_address,
                    &data_address_length);
        ((struct sockaddr_in *)&data_address)->sin_port = 0;

        /* Bind to the socket - should give us a unique port. */
        if (bind(ctxt->data_file_desc, (struct sockaddr *) &data_address,
            data_address_length) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: bind failed");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return -1;
        }

        /* We get the port number by reading back in the sockaddr. */
        getsockname(ctxt->data_file_desc, (struct sockaddr *)&data_address,
                    &data_address_length);

        /* Set up a 'listen' on the port to get the server's connection. */
        if (listen(ctxt->data_file_desc, 1) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: listen failed");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return -1;
        }

        /* Now generate the PORT command. */
        adp = (unsigned char *) &((struct sockaddr_in *)&data_address)->sin_addr;
        portp = (unsigned char *) &((struct sockaddr_in *)&data_address)->sin_port;
        snprintf(buf, sizeof(buf), "PORT %d,%d,%d,%d,%d,%d\r\n",
                 adp[0] & 0xff, adp[1] & 0xff, adp[2] & 0xff, adp[3] & 0xff,
                 portp[0] & 0xff, portp[1] & 0xff);

        buf[sizeof(buf) - 1] = 0;
        len = strlen(buf);

        /* Send the PORT command to the server. */
        res = send(ctxt->control_file_desc, buf, len, 0);

        if (res < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_get_connection");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return res;
        }

        res = ftp_get_response(ctxt);

        if (res != 2) {
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return -1;
        }
    }

    return ctxt->data_file_desc;
}

/**
* ftp_close_connection
*
*      Close the data connection from the server.
*
* Parameters:
*
*      ctxt    Pointer to an FTP context.
*
* Returns -1 in case of error, 0 otherwise
*/
static int ftp_close_connection(ftp_context_pointer ctxt)
{
    int res;
    fd_set rfd, efd;
    struct timeval tv;

    if ((ctxt == NULL) || (ctxt->control_file_desc < 0))
        return -1;

    close(ctxt->data_file_desc);
    ctxt->data_file_desc = -1;

    /* Check for data on the control channel. */
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    FD_ZERO(&rfd);
    FD_SET(ctxt->control_file_desc, &rfd);
    FD_ZERO(&efd);
    FD_SET(ctxt->control_file_desc, &efd);
    res = select(ctxt->control_file_desc + 1, &rfd, NULL, &efd, &tv);

    if (res < 0) {
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
        return -1;
    }

    if (res == 0) {             /* Timeout */
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
    } else {                    /* Read the response */
        res = ftp_get_response(ctxt);

        if (res != 2) {         /* Should be positive completion (2) */
            close(ctxt->control_file_desc);
            ctxt->control_file_desc = -1;
            return -1;
        }
    }

    return 0;
}

/**
* ftp_get_socket
*
*      Initiate fetch of the given file from the server.
*
* Parameters:
*
*      ctxt            an FTP context
*
* Returns the socket for the data connection, or <0 in case of error
*/
int ftp_get_socket(ftp_context_pointer ctxt)
{
    char buf[300];
    int res, len;
    int acfd;

    if ((ctxt == NULL) || (ctxt->path == NULL))
        return -1;

    /* Set up the data connection. */
    ctxt->data_file_desc = ftp_get_connection(ctxt);

    if (ctxt->data_file_desc == -1)
        return -1;

    /* Generate a "retrieve" command for the file. */
    snprintf(buf, sizeof(buf), "RETR %s\r\n", ctxt->path);
    buf[sizeof(buf) - 1] = 0;
    len = strlen(buf);

    /* Send it to the server. */
    res = send(ctxt->control_file_desc, buf, len, 0);

    if (res < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_get_socket");
        close(ctxt->data_file_desc);
        ctxt->data_file_desc = -1;
        return res;
    }

    /* Check the answer */
    res = ftp_get_response(ctxt);

    if (res != 1) {
        close(ctxt->data_file_desc);
        ctxt->data_file_desc = -1;
        return -res;
    }

    /*
     * If not a passive connection, need to do an accept to get the
     * connection from the server.
     */
    if (!ctxt->passive) {
        struct sockaddr_in data_address;
        unsigned int data_address_length = sizeof(struct sockaddr_in);

        if ((acfd = accept(ctxt->data_file_desc, (struct sockaddr *)&data_address,
            &data_address_length)) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: accept in ftp_get_socket");
            close(ctxt->data_file_desc);
            ctxt->data_file_desc = -1;
            return -1;
        }

        close(ctxt->data_file_desc);
        ctxt->data_file_desc = acfd;
    }

    return ctxt->data_file_desc;
}
/**
* ftp_send_type
*
*     Send a TYPE (either 'I' or 'A') command to the server.
*
* Parameters
*
*      ctxt    pointer to the ftp_context
*      type    ascii character ('I' or 'A')
*
* Returns      0 for success, negative error code for failure.
*
*/
int ftp_send_type(ftp_context_pointer ctxt, char type)
{
    char buf[100];
    int len, res;

    toupper(type);
    /* Assure transfer will be in "image" mode. */
    snprintf(buf, sizeof(buf), "TYPE I\r\n");
    len = strlen(buf);
    res = send(ctxt->control_file_desc, buf, len, 0);

    if (res < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: send failed in ftp_get_socket");
        close(ctxt->data_file_desc);
        ctxt->data_file_desc = -1;
        return res;
    }

    res = ftp_get_response(ctxt);

    if (res != 2) {
        close(ctxt->data_file_desc);
        ctxt->data_file_desc = -1;
        return -res;
    }

    return 0;
}

/**
* ftp_read
*
*      This function tries to read len bytes from the existing FTP
*      connection and saves them in dest. This is a blocking call.
*
* Parameters:
*      ctxt    the FTP context
*      dest    a buffer
*      len     the buffer length
*
* Returns:     the number of bytes read.
*              0 is an indication of an end of connection.
*              -1 indicates a parameter error.
*/
int ftp_read(ftp_context_pointer ctxt, void *dest, int len)
{
    if (ctxt == NULL)
        return -1;

    if (ctxt->data_file_desc < 0)
        return 0;

    if (dest == NULL)
        return -1;

    if (len <= 0)
        return 0;

    len = recv(ctxt->data_file_desc, dest, len, 0);

    if (len <= 0) {
        if (len < 0)
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: recv failed in ftp_read");
        ftp_close_connection(ctxt);
    }

    return len;
}


/**
* ftp_close
*
*      Close the connection and both control and transport.
*
* Parameters:
*
*      ctxt    Pointer to an FTP context.
*
* Returns -1 in case of error, 0 otherwise.
*/
int ftp_close(ftp_context_pointer ctxt)
{
    if (ctxt == NULL)
        return -1;

    if (ctxt->data_file_desc >= 0) {
        close(ctxt->data_file_desc);
        ctxt->data_file_desc = -1;
    }

    if (ctxt->control_file_desc >= 0) {
        ftp_quit(ctxt);
        close(ctxt->control_file_desc);
        ctxt->control_file_desc = -1;
    }

    ftp_free_context(ctxt);
    return 0;
}
