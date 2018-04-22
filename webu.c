/*
 *    webu.c
 *
 *    Web User control interface control for motion.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    Portions of code from Angel Carpintero (motiondevelop@gmail.com)
 *    from webhttpd.c Copyright 2004-2005
 *
 *    Majority of module written by MrDave.
 *
 *    Function scheme:
 *      webu*      - All functions in this module have this prefix.
 *      webu_main  - Main entry point from the motion thread and only function exposed.
 *      webu_html* - Functions that create the display web page.
 *        webu_html_style*  - The style section of the web page
 *        webu_html_script* - The javascripts of the web page
 *        webu_html_navbar* - The navbar section of the web page
 *      webu_text* - Functions that create the text interface.
 *      webu_process_action - Performs most items under the action menu
 *      webu_process_config - Saves the parameter values into Motion.
 *
 *      Some function names are long and are not expected to contain any
 *      logger message that would display the function name to the user.
 *
 *      Functions are generally kept to under one page in length
 *
 *    The "written" variable is used extensively with the writes but even
 *    if it fails, we choose to continue.  The user would get a bad page in
 *    this situation and would be expected to hit "refresh"
 *
 *    To debug, run code, open page, view source and make copy of html
 *    into a local file to revise changes then determine applicable section(s)
 *    in this code to modify to match modified version.
 *
 *    Known Issues:
 *      The quit/restart uses signals and this should be reconsidered.
 *      The tracking is "best effort" since developer does not have tracking camera.
 *      The conf_cmdparse assumes that the pointers to the motion context for each
 *        camera are always sequential and enforcement of the pointers being sequential
 *        has not been observed in the other modules. (This is a legacy assumption)
 *      Need to investigate use of 'if (webui->uri_thread == NULL){'  The pointer should
 *        never be null until we exit.  We memset the memory pointed to with '\0'
 *      Should store the thread number as a number in the context
 *    Known HTML Issues:
 *      Single and double quotes are not consistently used.
 *      HTML ids do not follow any naming convention.
 *      After clicking restart/quit, do something..close page? Try to connect again?
 *
 *    Additional functionality considerations:
 *      Match stream authentication methods
 *      Add cors (Cross origin requests)
 *      Notification to user of items that require restart when changed.
 *      Notification to user that item successfully implemented (config change/tracking)
 *      Implement post method to handle larger string parameters.
 *      List motion parms somewhere so they can be found by xgettext
 *
 *
 *
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <ctype.h>
#include "motion.h"
#include "webu.h"
#include "translate.h"

/* Some defines of lengths for our buffers */
#define WEBUI_LEN_PARM 512          /* Parameters specified */
#define WEBUI_LEN_BUFF 1024         /* Buffer from the header */
#define WEBUI_LEN_RESP 1024         /* Our responses.  (Break up response if more space needed) */
#define WEBUI_LEN_SPRM 10           /* Shorter parameter buffer (method/protocol) */
#define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
#define WEBUI_LEN_THRD 6            /* Maximum length for thread number e.g. 99999 */

struct webui_ctx {
    pthread_mutex_t webu_mutex;  /* The mutex to lock activity on the pipe*/

    int   client_socket;         /* Client socket to the pipe */
    char *auth;                  /* Authorization provided by user*/
    char *auth_parms;            /* Authorization parms from config file*/

    char *url;                   /* The URL sent from the client */
    char *protocol;              /* Protocol provided from the body of request*/
    char *method;                /* Method provided from the body of the request*/

    char *uri_thread;            /* Parsed thread number from the url*/
    char *uri_cmd1;              /* Parsed command(action) from the url*/
    char *uri_cmd2;              /* Parsed command (set) from the url*/
    char *uri_parm1;             /* Parameter 1 for the command */
    char *uri_value1;            /* The value for parameter 1*/
    char *uri_parm2;             /* Parameter 2 for the command */
    char *uri_value2;            /* The value for parameter 2*/

    char *uri_buffer;            /* Buffer of the header request content */

    char *hostname;              /* Host name provided from header content*/
    int   cam_count;             /* Count of the number of cameras*/
    int   cam_threads;           /* Count of the number of camera threads running*/
    char *lang;                  /* Two character abbreviation for locale language*/
    char *lang_full;             /* Five character abbreviation for language-country*/

};


static void webu_context_init(struct context **cnt, struct webui_ctx *webui) {

    int indx;

    webui->auth_parms    = NULL;
    webui->method        = NULL;
    webui->url           = NULL;
    webui->protocol      = NULL;
    webui->uri_buffer    = NULL;
    webui->client_socket = -1;
    webui->hostname      = NULL;

    /* These will be re-used for multiple calls
     * so we reserve WEBUI_LEN_PARM bytes which should be
     * plenty (too much?) space and we
     * don't have into overrun problems
     */

    webui->method      = mymalloc(WEBUI_LEN_SPRM);
    webui->url         = mymalloc(WEBUI_LEN_URLI);
    webui->protocol    = mymalloc(WEBUI_LEN_SPRM);

    webui->uri_thread  = mymalloc(WEBUI_LEN_THRD);
    webui->uri_cmd1    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd2    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm1   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value1  = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm2   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value2  = mymalloc(WEBUI_LEN_PARM);

    webui->uri_buffer = mymalloc(WEBUI_LEN_BUFF);
    webui->lang       = mymalloc(3);    /* Two digit lang code plus null terminator */
    webui->lang_full  = mymalloc(6);    /* lang code, underscore, country plus null terminator */

    /* get the number of cameras and threads */
    indx = 0;
    while (cnt[++indx]);

    webui->cam_threads = indx;

    webui->cam_count = indx;
    if (indx > 1)
        webui->cam_count--;

    /* 1 thread, 1 camera = just motion.conf.
     * 2 thread, 1 camera, then using motion.conf plus a separate camera file */

    snprintf(webui->lang_full, 6,"%s", getenv("LANGUAGE"));
    snprintf(webui->lang, 3,"%s",webui->lang_full);

    return;
}

static void webu_context_null(struct webui_ctx *webui) {

    webui->auth_parms = NULL;
    webui->method     = NULL;
    webui->url        = NULL;
    webui->protocol   = NULL;
    webui->uri_buffer = NULL;
    webui->hostname   = NULL;

    webui->uri_thread   = NULL;
    webui->uri_cmd1     = NULL;
    webui->uri_cmd2     = NULL;
    webui->uri_parm1    = NULL;
    webui->uri_value1   = NULL;
    webui->uri_parm2    = NULL;
    webui->uri_value2   = NULL;
    webui->lang         = NULL;
    webui->lang_full    = NULL;

    return;
}

static void webu_context_free(struct webui_ctx *webui) {

    if (webui->auth_parms != NULL) free(webui->auth_parms);
    if (webui->method     != NULL) free(webui->method);
    if (webui->url        != NULL) free(webui->url);
    if (webui->protocol   != NULL) free(webui->protocol);
    if (webui->uri_buffer != NULL) free(webui->uri_buffer);
    if (webui->hostname   != NULL) free(webui->hostname);

    if (webui->uri_thread   != NULL) free(webui->uri_thread);
    if (webui->uri_cmd1     != NULL) free(webui->uri_cmd1);
    if (webui->uri_cmd2     != NULL) free(webui->uri_cmd2);
    if (webui->uri_parm1    != NULL) free(webui->uri_parm1);
    if (webui->uri_value1   != NULL) free(webui->uri_value1);
    if (webui->uri_parm2    != NULL) free(webui->uri_parm2);
    if (webui->uri_value2   != NULL) free(webui->uri_value2);
    if (webui->lang         != NULL) free(webui->lang);
    if (webui->lang_full    != NULL) free(webui->lang_full);

    webu_context_null(webui);

    free(webui);

    return;
}

static void webu_clientip(char *buf, int fd) {
    /* Return the IP of the connecting client*/
    struct sockaddr_in6 client;
    socklen_t client_len;
    char host[NI_MAXHOST];
    int retcd;

    strncpy(buf, "Unknown", NI_MAXHOST - 1);

    client_len = sizeof(client);

    retcd = getpeername(fd, (struct sockaddr *)&client, &client_len);
    if (retcd != 0) return;

    retcd = getnameinfo((struct sockaddr *)&client, client_len, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    if (retcd != 0) return;

    strncpy(buf, host, NI_MAXHOST - 1);
}

static ssize_t webu_read(int fd ,void *buf, ssize_t size) {
    /* Reads device and returns number of bytes read*/
    ssize_t nread = -1;
    struct timeval tm;
    fd_set fds;

    tm.tv_sec = 1; /* Timeout in seconds */
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (select(fd + 1, &fds, NULL, NULL, &tm) > 0) {
        if (FD_ISSET(fd, &fds)) {
            if ((nread = read(fd , buf, size)) < 0) {
                if (errno != EWOULDBLOCK)
                    return -1;
            }
        }
    }

    return nread;
}

static ssize_t webu_write(int fd, const void *buf, size_t buffer_size) {

    ssize_t nwrite = -1;
    struct timeval tm;
    fd_set fds;

    tm.tv_sec = 1;
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (select(fd + 1, NULL, &fds, NULL, &tm) > 0) {
        if (FD_ISSET(fd, &fds)) {
            if ((nwrite = write(fd , buf, buffer_size)) < 0) {
                if (errno != EWOULDBLOCK)
                    return -1;
            }
        }
    }

    return nwrite;
}

static void webu_html_ok(struct webui_ctx *webui){
    /* Send message that everything is OK */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "HTTP/1.1 200 OK\r\n"
        "Server: Motion /"VERSION"\r\n"
        "Connection: close\r\n"
        "Max-Age: 0\r\n"
        "Expires: 0\r\n"
        "Cache-Control: no-cache\r\n"
        "Cache-Control: private\r\n"
        "Pragma: no-cache\r\n"
        "Content-type: text/html\r\n\r\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_badreq(struct webui_ctx *webui) {

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "HTTP/1.0 400 Bad Request\r\n"
        "Content-type: text/html\r\n\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "<h1>Bad Request</h1>\n"
        "<p>The server did not understand your request.</p>\n"
        "</body>\n"
        "</html>\n");
    written = webu_write(webui->client_socket, response, strlen(response));
    if (written <= 0){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
    }

    return;

}

static void webu_text_ok(struct webui_ctx *webui){
    /* Send message that everything is OK */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "HTTP/1.1 200 OK\r\n"
        "Server: Motion-httpd/"VERSION"\r\n"
        "Connection: close\r\n"
        "Max-Age: 0\r\n"
        "Expires: 0\r\n"
        "Cache-Control: no-cache\r\n"
        "Cache-Control: private\r\n"
        "Pragma: no-cache\r\n"
        "Content-type: text/plain\r\n\r\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_badreq(struct webui_ctx *webui) {

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "HTTP/1.0 400 Bad Request\r\n"
        "Content-type: text/plain\r\n\r\n"
        "Bad Request\n");
    written = webu_write(webui->client_socket, response, strlen(response));
    if (written <= 0){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
    }

    return;
}

static void webu_resp_auth(struct webui_ctx *webui) {

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response), "%s",
        "HTTP/1.0 401 Authorization Required\r\n"
        "WWW-Authenticate: Basic realm=\"Motion Security Access\"\r\n\r\n");
    written = webu_write(webui->client_socket, response, strlen(response));
    if (written <= 0){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
    }

    return;
}

static int webu_nonblock(int serverfd) {

    int curfd;
    int timeout = 1;
    struct sockaddr_in6 client;
    socklen_t client_len = sizeof(client);

    struct timeval tm;
    fd_set fds;

    tm.tv_sec = timeout; /* Timeout in seconds */
    tm.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(serverfd, &fds);

    if (select(serverfd + 1, &fds, NULL, NULL, &tm) > 0) {
        if (FD_ISSET(serverfd, &fds)) {
            if ((curfd = accept(serverfd, (struct sockaddr *)&client, &client_len)) > 0)
                return curfd;
        }
    }

    return -1;
}

static void webu_url_decode(char *urlencoded, size_t length) {
    char *data = urlencoded;
    char *urldecoded = urlencoded;
    int scan_rslt;

    while (length > 0) {
        if (*data == '%') {
            char c[3];
            int i;
            data++;
            length--;
            c[0] = *data++;
            length--;
            c[1] = *data;
            c[2] = 0;

            scan_rslt = sscanf(c, "%x", &i);
            if (scan_rslt < 1) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error decoding");

            if (i < 128) {
                *urldecoded++ = (char)i;
            } else {
                *urldecoded++ = '%';
                *urldecoded++ = c[0];
                *urldecoded++ = c[1];
            }

        } else if (*data == '<' || *data == '+' || *data == '>') {
            *urldecoded++ = ' ';
        } else {
            *urldecoded++ = *data;
        }

        data++;
        length--;
    }
    *urldecoded = '\0';


}

static int webu_header_read(struct webui_ctx *webui) {

    ssize_t bytes_read = 0, readb = -1;
    int uri_length=WEBUI_LEN_BUFF - 1;
    int parm_len;
    char *st_pos, *en_pos;

    bytes_read = webu_read(webui->client_socket, webui->uri_buffer, uri_length);
    if (bytes_read <= 0) {
        MOTION_LOG(DBG, TYPE_STREAM, SHOW_ERRNO, "First Read Error %d",bytes_read);
        return -1;
    }

    webui->uri_buffer[bytes_read] = '\0';

    parm_len = 0;
    /* Get the method from the header */
    st_pos = webui->uri_buffer;
    en_pos = strstr(st_pos," ");
    if (en_pos != NULL){
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_SPRM){
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid method.  Buffer:>%s<",webui->uri_buffer);
            return -1;
        }
        snprintf(webui->method, parm_len,"%s", st_pos);
    }

    /* Get the url name */
    st_pos = st_pos + parm_len; /* Move past the method */
    en_pos = strstr(st_pos," ");
    if (en_pos != NULL){
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_URLI){
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid url.  Buffer:>%s<",webui->uri_buffer);
            return -1;
        }
        snprintf(webui->url, parm_len,"%s", st_pos);
    }

    /* Get the protocol name */
    st_pos = st_pos + parm_len; /* Move past the url */
    en_pos = strstr(st_pos,"\r");
    if (en_pos != NULL){
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_SPRM){
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid protocol.  Buffer:>%s< %d"
                ,webui->uri_buffer,parm_len);
            return -1;
        }
        snprintf(webui->protocol, parm_len,"%s", st_pos);
    }

    /* Read remaining header requests until crlf crlf */
    while ((strstr(webui->uri_buffer, "\r\n\r\n") == NULL) &&
           (bytes_read != 0) &&
           (bytes_read < uri_length)) {

        readb = webu_read(webui->client_socket
                              , webui->uri_buffer + bytes_read
                              , sizeof (webui->uri_buffer) - bytes_read);
        if (readb == -1) {
            bytes_read = -1;
            break;
        }

        bytes_read += readb;
        if (bytes_read > uri_length) {
            MOTION_LOG(WRN, TYPE_STREAM, SHOW_ERRNO, "End of buffer"
                       " reached waiting for buffer ending");
            break;
        }
        webui->uri_buffer[bytes_read] = '\0';
    }

    /* Check that last read was OK */
    if (bytes_read == -1) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "Invalid last read!");
        return -1;
    }

    if (((strcmp(webui->protocol, "HTTP/1.0")) &&
         (strcmp(webui->protocol, "HTTP/1.1"))) ||
        (strcmp(webui->method, "GET"))) {
        MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO, "Invalid protocol/method");
        return -1;
    }

    return 0;
}

static int webu_header_auth(struct webui_ctx *webui) {

    char *authentication=NULL;
    char *end_auth = NULL;
    char clientip[NI_MAXHOST];

    if (webui->auth_parms != NULL) {
        if ((authentication = strstr(webui->uri_buffer,"Basic"))) {
            authentication = authentication + 6;

            if ((end_auth  = strstr(authentication,"\r\n"))) {
                authentication[end_auth - authentication] = '\0';
            } else {
                webu_resp_auth(webui);
                return -1;
            }

            if (strcmp(webui->auth_parms, authentication)) {
                webu_resp_auth(webui);
                webu_clientip(clientip, webui->client_socket);
                MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO, "Failed auth attempt from %s", clientip);
                return -1;
            }
        } else {
            webu_resp_auth(webui);
            return -1;
        }
    }
    return 0;
}

static int webu_header_hostname(struct webui_ctx *webui) {

    /* use the hostname the browser used to connect to us when
     * constructing links to the stream ports. If available
     * (which it is in all modern browsers) it is more likely to
     * work than the result of gethostname(), which is reliant on
     * the machine we're running on having it's hostname setup
     * correctly and corresponding DNS in place. */

    char *end_host;
    char *st_host;
    char *colon = NULL;
    char *end_bracket;

    if ((st_host = strstr(webui->uri_buffer,"Host:"))) {
        st_host += strlen("Host:");
        end_host = strstr(st_host,"\r\n");
        if (end_host) {
            while (st_host < end_host && isspace(st_host[0]))
                st_host++;
            while (st_host < end_host && isspace(end_host[-1]))
                end_host--;
            /* Strip off any port number and colon */
            /* hostname is a IPv6 address like "[::1]" */
            if (st_host[0] == '[') {
                end_bracket = memchr(st_host, ']', end_host - st_host);
                // look for the colon after the "]"
                colon = memchr(end_bracket, ':', end_host-end_bracket);
            } else {
                colon = memchr(st_host, ':', end_host - st_host);
            }
            if (colon) end_host = colon;

            if (webui->hostname != NULL) free(webui->hostname);
            webui->hostname = mymalloc(end_host-st_host+2);
            snprintf(webui->hostname, end_host-st_host+1, "%s", st_host);
        }
    }

    if (webui->hostname == NULL){
        /* Set the host that is running Motion */
        webui->hostname = mymalloc(WEBUI_LEN_PARM);
        memset(webui->hostname,'\0',WEBUI_LEN_PARM);
        gethostname(webui->hostname, WEBUI_LEN_PARM - 1);
    }
    return 0;
}

static void webu_parseurl_parms(struct webui_ctx *webui, char *st_pos) {

    int parm_len, last_parm;
    char *en_pos;

    /* First parse out the "set","get","pan","tilt","x","y"
     * from the uri and put them into the cmd2.
     * st_pos is at the beginning of the command
     * If there is no ? then we are done parsing
     */
    last_parm = 0;
    en_pos = strstr(st_pos,"?");
    if (en_pos != NULL){
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_PARM) return;
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);

        /* Get the parameter name */
        st_pos = st_pos + parm_len; /* Move past the command */
        en_pos = strstr(st_pos,"=");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len;
            last_parm = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return;
        snprintf(webui->uri_parm1, parm_len,"%s", st_pos);

        if (!last_parm){
            /* Get the parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value1, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter name */
            st_pos = st_pos + parm_len; /* Move past the command */
            en_pos = strstr(st_pos,"=");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_parm2, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value2, parm_len,"%s", st_pos);
        }

    }

}

static void webu_parseurl_reset(struct webui_ctx *webui) {

    /* Reset the variable to empty strings since they
     * are re-used across calls.  These are allocated
     * larger sizes to allow for multiple variable types.
     */
    memset(webui->uri_thread,'\0',WEBUI_LEN_THRD);
    memset(webui->uri_cmd1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value2,'\0',WEBUI_LEN_PARM);

}

static int webu_parseurl(struct webui_ctx *webui) {

    int retcd, parm_len, last_slash;
    char *st_pos, *en_pos;

    retcd = 0;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, "Sent url: %s",webui->url);

    webu_parseurl_reset(webui);

    if (webui->url == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid url: %s",webui->url);
        return -1;
    }

    if (webui->url[0] != '/'){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid url: %s",webui->url);
        return -1;
    }

    webu_url_decode(webui->url, strlen(webui->url));

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, "Decoded url: %s",webui->url);

    /* Home page, nothing else */
    if (strlen(webui->url) == 1) return 0;

    last_slash = 0;

    /* Get thread number */
    st_pos = webui->url + 1; /* Move past the first "/" */
    if (*st_pos == '-') return -1; /* Never allow a negative thread number */
    en_pos = strstr(st_pos,"/");
    if (en_pos == NULL){
        parm_len = strlen(webui->url);
        last_slash = 1;
    } else {
        parm_len = en_pos - st_pos + 1;
    }
    if (parm_len >= WEBUI_LEN_THRD) return -1; /* var was malloc'd to WEBUI_LEN_THRD */
    snprintf(webui->uri_thread, parm_len,"%s", st_pos);

    if (!last_slash){
        /* Get cmd1 or action */
        st_pos = st_pos + parm_len; /* Move past the thread number */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len ;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
        snprintf(webui->uri_cmd1, parm_len,"%s", st_pos);
    }

    if (!last_slash){
        /* Get cmd2 or action */
        st_pos = st_pos + parm_len; /* Move past the first command */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);
    }

    if ((!strcmp(webui->uri_cmd1,"config") ||
         !strcmp(webui->uri_cmd1,"track") ) &&
        (strlen(webui->uri_cmd2) > 0)) {
        webu_parseurl_parms(webui, st_pos);
    }

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s< parm1:>%s< val1:>%s< parm2:>%s< val2:>%s<"
               ,webui->uri_thread
               ,webui->uri_cmd1, webui->uri_cmd2
               ,webui->uri_parm1, webui->uri_value1
               ,webui->uri_parm2, webui->uri_value2);


    return retcd;

}

static void webu_html_style_navbar(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    .navbar {\n"
        "      overflow: hidden;\n"
        "      background-color: #333;\n"
        "      font-family: Arial;\n"
        "    }\n"
        "    .navbar a {\n"
        "      float: left;\n"
        "      font-size: 16px;\n"
        "      color: white;\n"
        "      text-align: center;\n"
        "      padding: 14px 16px;\n"
        "      text-decoration: none;\n"
        "    }\n"
        "    .navbar a:hover, {\n"
        "      background-color: darkgray;\n"
        "    }\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_style_dropdown(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    .dropdown {\n"
        "      float: left;\n"
        "      overflow: hidden;\n"
        "    }\n"
        "    .dropdown .dropbtn {\n"
        "      font-size: 16px;\n"
        "      border: none;\n"
        "      outline: none;\n"
        "      color: white;\n"
        "      padding: 14px 16px;\n"
        "      background-color: inherit;\n"
        "      font-family: inherit;\n"
        "      margin: 0;\n"
        "    }\n"
        "    .dropdown-content {\n"
        "      display: none;\n"
        "      position: absolute;\n"
        "      background-color: #f9f9f9;\n"
        "      min-width: 160px;\n"
        "      box-shadow: 0px 8px 16px 0px rgba(0,0,0,0.2);\n"
        "      z-index: 1;\n"
        "    }\n"
        "    .dropdown-content a {\n"
        "      float: none;\n"
        "      color: black;\n"
        "      padding: 12px 16px;\n"
        "      text-decoration: none;\n"
        "      display: block;\n"
        "      text-align: left;\n"
        "    }\n"
        "    .dropdown-content a:hover {\n"
        "      background-color: lightgray;\n"
        "    }\n"
        "    .dropdown:hover .dropbtn {\n"
        "      background-color: darkgray;\n"
        "    }\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_style_input(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    input , select  {\n"
        "      width: 25%;\n"
        "      padding: 5px;\n"
        "      margin: 0;\n"
        "      display: inline-block;\n"
        "      border: 1px solid #ccc;\n"
        "      border-radius: 4px;\n"
        "      box-sizing: border-box;\n"
        "      height: 50%;\n"
        "      font-size: 75%;\n"
        "      margin-bottom: 5px;\n"
        "    }\n"
        "    .frm-input{\n"
        "      text-align:center;\n"
        "    }\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_style_base(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    * {margin: 0; padding: 0; }\n"
        "    body {\n"
        "      padding: 0;\n"
        "      margin: 0;\n"
        "      font-family: Arial, Helvetica, sans-serif;\n"
        "      font-size: 16px;\n"
        "      line-height: 1;\n"
        "      color: #606c71;\n"
        "      background-color: #159957;\n"
        "      background-image: linear-gradient(120deg, #155799, #159957);\n"
        "      margin-left:0.5% ;\n"
        "      margin-right:0.5% ;\n"
        "      width: device-width ;\n"
        "    }\n"
        "    img {\n"
        "      max-width: 100%;\n"
        "      max-height: 100%;\n"
        "      height: auto;\n"
        "    }\n"
        "    .page-header {\n"
        "      color: #fff;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "    }\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "    .page-header h4 {\n"
        "      height: 2px;\n"
        "      padding: 0;\n"
        "      margin: 1rem 0;\n"
        "      border: 0;\n"
        "    }\n"
        "    .main-content {\n"
        "      background-color: #000000;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "      font-size: 0.90em;\n"
        "    }\n"
        "    .header-right{\n"
        "      float: right;\n"
        "      color: white;\n"
        "    }\n"
        "    .header-center {\n"
        "      text-align: center;\n"
        "      color: white;\n"
        "      margin-top: 10px;\n"
        "      margin-bottom: 10px;\n"
        "    }\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_style(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s", "  <style>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_style_base(webui);

    webu_html_style_navbar(webui);

    webu_html_style_input(webui);

    webu_html_style_dropdown(webui);

    snprintf(response, sizeof (response),"%s", "  </style>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_head(struct webui_ctx *webui) {
    /* Write out the header section of the web page */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s","<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <title>Motion</title>\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_style(webui);

    snprintf(response, sizeof (response),"%s", "</head>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_navbar_camera(struct context **cnt, struct webui_ctx *webui) {
    /*Write out the options included in the camera dropdown */
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx;

    if (webui->cam_threads == 1){
        /* Only Motion.conf file */
        if (cnt[0]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_all');\">%s 1</a>\n"
                ,_("Cameras")
                ,_("Camera"));
            written = webu_write(webui->client_socket, response, strlen(response));
        } else {
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_all');\">%s</a>\n"
                ,_("Cameras")
                ,cnt[0]->conf.camera_name);
            written = webu_write(webui->client_socket, response, strlen(response));
        }
    } else if (webui->cam_threads > 1){
        /* Motion.conf + separate camera.conf file */
        snprintf(response, sizeof (response),
            "    <div class=\"dropdown\">\n"
            "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
            "      <div id='cam_btn' class=\"dropdown-content\">\n"
            "        <a onclick=\"camera_click('cam_all');\">%s</a>\n"
            ,_("Cameras")
            ,_("All"));
        written = webu_write(webui->client_socket, response, strlen(response));

        for (indx=1;indx <= webui->cam_count;indx++){
            if (cnt[indx]->conf.camera_name == NULL){
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%03d');\">%s %d</a>\n"
                    , indx, _("Camera"), indx);
            } else {
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%03d');\">%s</a>\n",
                    indx, cnt[indx]->conf.camera_name
                );
            }
            written = webu_write(webui->client_socket, response, strlen(response));
        }
    }

    snprintf(response, sizeof (response),"%s",
        "      </div>\n"
        "    </div>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_navbar_action(struct webui_ctx *webui) {
    /* Write out the options included in the actions dropdown*/
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "    <div class=\"dropdown\">\n"
        "      <button onclick='display_actions()' id=\"act_drop\" class=\"dropbtn\">%s</button>\n"
        "      <div id='act_btn' class=\"dropdown-content\">\n"
        "        <a onclick=\"action_click('/action/makemovie');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/snapshot');\">%s</a>\n"
        "        <a onclick=\"action_click('config');\">%s</a>\n"
        "        <a onclick=\"action_click('/config/write');\">%s</a>\n"
        "        <a onclick=\"action_click('track');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/pause');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/start');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/restart');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/quit');\">%s</a>\n"
        "      </div>\n"
        "    </div>\n"
        ,_("Action")
        ,_("Make Movie")
        ,_("Snapshot")
        ,_("Change Configuration")
        ,_("Write Configuration")
        ,_("Tracking")
        ,_("Pause")
        ,_("Start")
        ,_("Restart")
        ,_("Quit"));
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_navbar(struct context **cnt, struct webui_ctx *webui) {
    /* Write the navbar section*/
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "  <div class=\"navbar\">\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_navbar_camera(cnt, webui);

    webu_html_navbar_action(webui);

    snprintf(response, sizeof (response),
        "    <a href=\"https://motion-project.github.io/motion_guide.html\" "
        " target=\"_blank\">%s</a>\n"
        "    <p class=\"header-right\">Motion "VERSION"</p>\n"
        "  </div>\n"
        ,_("Help"));
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_config_notice(struct context **cnt, struct webui_ctx *webui) {

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    if (cnt[0]->conf.webcontrol_parms == 0){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 0 (%s)</h4>\n"
            ,_("No Configuration Options"));
    } else if (cnt[0]->conf.webcontrol_parms == 1){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 1 (%s)</h4>\n"
            ,_("Limited Configuration Options"));
    } else if (cnt[0]->conf.webcontrol_parms == 2){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 2 (%s)</h4>\n"
            ,_("Advanced Configuration Options"));
    } else{
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 3 (%s)</h4>\n"
            ,_("Restricted Configuration Options"));
    }
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_html_config(struct context **cnt, struct webui_ctx *webui) {

    /* Write out the options to put into the config dropdown
     * We use html data attributes to store the values for the options
     * We always set a cam_all attribute and if the value if different for
     * any of our cameras, then we also add a cam_xxx which has the config
     * value for camera xxx  The javascript then decodes these to display
     */

    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx_parm, indx, diff_vals;
    const char *val_main, *val_thread;
    char *val_temp;


    snprintf(response, sizeof (response),"%s",
        "  <div id='cfg_form' style=\"display:none\">\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_config_notice(cnt, webui);

    snprintf(response, sizeof (response),
        "    <form class=\"frm-input\">\n"
        "      <select id='cfg_parms' name='onames' "
        " autocomplete='off' onchange='config_change();'>\n"
        "        <option value='default' data-cam_all=\"\" >%s</option>\n"
        ,_("Select option"));
    written = webu_write(webui->client_socket, response, strlen(response));

    /* The config_params[indx_parm].print reuses the buffer so create a
     * temporary variable for storing our parameter from main to compare
     * to the thread specific value
     */
    val_temp=malloc(PATH_MAX);
    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER)){
            indx_parm++;
            continue;
        }

        val_main = config_params[indx_parm].print(cnt, NULL, indx_parm, 0);

        snprintf(response, sizeof (response),
            "        <option value='%s' data-cam_all=\""
            , config_params[indx_parm].param_name);
        written = webu_write(webui->client_socket, response, strlen(response));

        memset(val_temp,'\0',PATH_MAX);
        if (val_main != NULL){
            snprintf(response, sizeof (response),"%s", val_main);
            written = webu_write(webui->client_socket, response, strlen(response));
            snprintf(val_temp, PATH_MAX,"%s", val_main);
        }

        if (webui->cam_threads > 1){
            for (indx=1;indx <= webui->cam_count;indx++){
                val_thread=config_params[indx_parm].print(cnt, NULL, indx_parm, indx);
                diff_vals = 0;
                if (((strlen(val_temp) == 0) && (val_thread == NULL)) ||
                    ((strlen(val_temp) != 0) && (val_thread == NULL))) {
                    diff_vals = 0;
                } else if (((strlen(val_temp) == 0) && (val_thread != NULL)) ) {
                    diff_vals = 1;
                } else {
                    if (strcasecmp(val_temp, val_thread)) diff_vals = 1;
                }
                if (diff_vals){
                    snprintf(response, sizeof (response),"%s","\" \\ \n");
                    written = webu_write(webui->client_socket, response, strlen(response));

                    snprintf(response, sizeof (response),
                        "           data-cam_%03d=\"",indx);
                    written = webu_write(webui->client_socket, response, strlen(response));
                    if (val_thread != NULL){
                        snprintf(response, sizeof (response),"%s%s", response, val_thread);
                        written = webu_write(webui->client_socket, response, strlen(response));
                    }
                }
            }
        }
        /* Terminate the open quote and option.  For foreign language put hint in ()  */
        if (!strcasecmp(webui->lang,"en") ||
            !strcasecmp(config_params[indx_parm].param_name
                ,_(config_params[indx_parm].param_name))){
            snprintf(response, sizeof (response),"\" >%s</option>\n",
                config_params[indx_parm].param_name);
            written = webu_write(webui->client_socket, response, strlen(response));
        } else {
            snprintf(response, sizeof (response),"\" >%s (%s)</option>\n",
                config_params[indx_parm].param_name
                ,_(config_params[indx_parm].param_name));
            written = webu_write(webui->client_socket, response, strlen(response));
        }

        indx_parm++;
    }

    free(val_temp);

    snprintf(response, sizeof (response),
        "      </select>\n"
        "      <input type=\"text\"   id=\"cfg_value\" >\n"
        "      <input type='button' id='cfg_button' value='%s' onclick='config_click()'>\n"
        "    </form>\n"
        "  </div>\n"
        ,_("Save"));
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_track(struct webui_ctx *webui) {

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "  <div id='trk_form' style='display:none'>\n"
        "    <form class='frm-input'>\n"
        "      <select id='trk_option' name='trkopt'  autocomplete='off' "
        " style='width:20%%' onchange='track_change();'>\n"
        "        <option value='pan/tilt' data-trk='pan' >%s</option>\n"
        "        <option value='absolute' data-trk='abs' >%s</option>\n"
        "        <option value='center' data-trk='ctr' >%s</option>\n"
        "      </select>\n"
        "      <label id='trk_lblpan' style='color:white; display:inline' >%s</label>\n"
        "      <label id='trk_lblx'   style='color:white; display:none' >X</label>\n"
        "      <input type='text'   id='trk_panx' style='width:10%%' >\n"
        "      <label id='trk_lbltilt' style='color:white; display:inline' >%s</label>\n"
        "      <label id='trk_lbly'   style='color:white; display:none' >Y</label>\n"
        "      <input type='text'   id='trk_tilty' style='width:10%%' >\n"
        "      <input type='button' id='trk_button' value='%s' "
        " style='width:10%%' onclick='track_click()'>\n"
        "    </form>\n"
        "  </div>\n"
        ,_("Pan/Tilt")
        ,_("Absolute Change")
        ,_("Center")
        ,_("Pan")
        ,_("Tilt")
        ,_("Save"));
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_preview(struct context **cnt, struct webui_ctx *webui) {

    /* Write the initial version of the preview section.  The javascript
     * will change this section when user selects a different camera */
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, strm_port;

    snprintf(response, sizeof (response),"%s",
        "  <div id=\"liveview\">\n"
        "    <section class=\"main-content\">\n"
        "      <br>\n"
        "      <p id=\"id_preview\">\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (cnt[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","<br>");
            written = webu_write(webui->client_socket, response, strlen(response));
        }
        strm_port = cnt[indx]->conf.stream_port;
        if (cnt[indx]->conf.substream_port) strm_port = cnt[indx]->conf.substream_port;
        snprintf(response, sizeof (response),
            "      <a href=http://%s:%d> <img src=http://%s:%d/ border=0 width=%d%%></a>\n",
            webui->hostname, cnt[indx]->conf.stream_port,webui->hostname,
            strm_port, cnt[indx]->conf.stream_preview_scale);
        written = webu_write(webui->client_socket, response, strlen(response));
    }

    snprintf(response, sizeof (response),"%s",
        "      </p>\n"
        "      <br>\n"
        "    </section>\n"
        "  </div>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script_action(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascript action_click() function.
     * We do not have a good notification section on the page so the successful
     * submission and response is currently a empty if block for the future
     * enhancement to somehow notify the user everything worked */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function event_reloadpage() {\n"
        "      window.location.reload();\n"
        "    }\n\n"
    );
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "    function action_click(actval) {\n"
        "      if (actval == \"config\"){\n"
        "        document.getElementById('trk_form').style.display=\"none\";\n"
        "        document.getElementById('cfg_form').style.display=\"inline\";\n"
        "      } else if (actval == \"track\"){\n"
        "        document.getElementById('cfg_form').style.display=\"none\";\n"
        "        document.getElementById('trk_form').style.display=\"inline\";\n"
        "      } else {\n"
        "        document.getElementById('cfg_form').style.display=\"none\";\n"
        "        document.getElementById('trk_form').style.display=\"none\";\n"
        "        var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "        var camnbr = camstr.substring(4,7);\n"
        "        var http = new XMLHttpRequest();\n"
        "        if ((actval == \"/detection/pause\") || (actval == \"/detection/start\")) {\n"
        "          http.addEventListener('load', event_reloadpage); \n"
        "        }\n"
    );
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),
        "        var url = \"http://%s:%d/\"; \n",
        webui->hostname,cnt[0]->conf.webcontrol_port);
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "        if (camnbr == \"all\"){\n"
        "          url = url + \"0\";\n"
        "        } else {\n"
        "          url = url + camnbr;\n"
        "        }\n"
        "        url = url + actval;\n"
        "        http.open(\"GET\", url, true);\n"
        "        http.onreadystatechange = function() {\n"
        "          if(http.readyState == 4 && http.status == 200) {\n"

        "          }\n"
        "        }\n"
        "        http.send(null);\n"
        "      }\n"
        "      document.getElementById('act_btn').style.display=\"none\"; \n"
        "      document.getElementById('cfg_value').value = '';\n"
        "      document.getElementById('cfg_parms').value = 'default';\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script_camera_thread(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascript thread IF conditions of camera_click() function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int  strm_port;
    int indx, indx_st;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    written = 1;
    for (indx = indx_st; indx<webui->cam_threads; indx++){
        snprintf(response, sizeof (response),
            "      if (camid == \"cam_%03d\"){\n",indx);
        written = webu_write(webui->client_socket, response, strlen(response));

        strm_port = cnt[indx]->conf.stream_port;
        if (cnt[indx]->conf.substream_port) strm_port = cnt[indx]->conf.substream_port;

        snprintf(response, sizeof (response),
            "        preview=\"<a href=http://%s:%d> "
            " <img src=http://%s:%d/ border=0></a>\"  \n",
            webui->hostname, cnt[indx]->conf.stream_port, webui->hostname, strm_port);
        written = webu_write(webui->client_socket, response, strlen(response));

        if (cnt[indx]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s %d (%s)</h3>\"\n"
                ,_("Camera")
                , indx
                ,(!cnt[indx]->running)? _("Not running") :
                 (cnt[indx]->lost_connection)? _("Lost connection"):
                 (cnt[indx]->pause)? _("Paused"):_("Active")
             );
        } else {
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s (%s)</h3>\"\n"
                , cnt[indx]->conf.camera_name
                ,(!cnt[indx]->running)? _("Not running") :
                 (cnt[indx]->lost_connection)? _("Lost connection"):
                 (cnt[indx]->pause)? _("Paused"):_("Active")
                );
        }
        written = webu_write(webui->client_socket, response, strlen(response));

        snprintf(response, sizeof (response),"%s","      }\n");
        written = webu_write(webui->client_socket, response, strlen(response));

    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

    return;
}

static void webu_html_script_camera_all(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascript "All" IF condition of camera_click() function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int  strm_port;
    int indx, indx_st;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    snprintf(response, sizeof (response), "      if (camid == \"cam_all\"){\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (indx == indx_st){
            snprintf(response, sizeof (response),"%s","        preview = \"\";\n");
            written = webu_write(webui->client_socket, response, strlen(response));
        }

        strm_port = cnt[indx]->conf.stream_port;
        if (cnt[indx]->conf.substream_port) strm_port = cnt[indx]->conf.substream_port;

        if (cnt[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","    preview = preview + \"<br>\" ");
            written = webu_write(webui->client_socket, response, strlen(response));
        }

        snprintf(response, sizeof (response),
            "        preview = preview + \"<a href=http://%s:%d> "
            " <img src=http://%s:%d/ border=0 width=%d%%></a>\"; \n",
            webui->hostname, cnt[indx]->conf.stream_port,
            webui->hostname, strm_port,cnt[indx]->conf.stream_preview_scale);
        written = webu_write(webui->client_socket, response, strlen(response));

    }

    snprintf(response, sizeof (response),
        "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
        " class='header-center' >%s</h3>\"\n"
        "      }\n"
        ,_("All Cameras"));
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

    return;
}

static void webu_html_script_camera(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascript camera_click() function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function camera_click(camid) {\n"
        "      var preview = \"\";\n"
        "      var header = \"\";\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_script_camera_thread(cnt, webui);

    webu_html_script_camera_all(cnt, webui);

    snprintf(response, sizeof (response),"%s",
        "      document.getElementById(\"id_preview\").innerHTML = preview; \n"
        "      document.getElementById(\"id_header\").innerHTML = header; \n"
        "      document.getElementById('cfg_form').style.display=\"none\"; \n"
        "      document.getElementById('trk_form').style.display=\"none\"; \n"
        "      document.getElementById('cam_btn').style.display=\"none\"; \n"
        "      document.getElementById('cfg_value').value = '';\n"
        "      document.getElementById('cfg_parms').value = 'default';\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script_menucam(struct webui_ctx *webui) {
    /* Write the javascript display_cameras() function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function display_cameras() {\n"
        "      document.getElementById('act_btn').style.display = 'none';\n"
        "      if (document.getElementById('cam_btn').style.display == 'block'){\n"
        "        document.getElementById('cam_btn').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('cam_btn').style.display = 'block';\n"
        "      }\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script_menuact(struct webui_ctx *webui) {
    /* Write the javascript display_actions() function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function display_actions() {\n"
        "      document.getElementById('cam_btn').style.display = 'none';\n"
        "      if (document.getElementById('act_btn').style.display == 'block'){\n"
        "        document.getElementById('act_btn').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('act_btn').style.display = 'block';\n"
        "      }\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script_evtclk(struct webui_ctx *webui) {
    /* Write the javascript 'click' EventListener */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    document.addEventListener('click', function(event) {\n"
        "      const dropCam = document.getElementById('cam_drop');\n"
        "      const dropAct = document.getElementById('act_drop');\n"
        "      if (!dropCam.contains(event.target) && !dropAct.contains(event.target)) {\n"
        "        document.getElementById('cam_btn').style.display = 'none';\n"
        "        document.getElementById('act_btn').style.display = 'none';\n"
        "      }\n"
        "    });\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Error writing");
}

static void webu_html_script_cfgclk(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascript config_click function
     * We do not have a good notification section on the page so the successful
     * submission and response is currently a empty if block for the future
     * enhancement to somehow notify the user everything worked */

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function config_click() {\n"
        "      var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var camnbr = camstr.substring(4,7);\n"
        "      var opts = document.getElementById('cfg_parms');\n"
        "      var optsel = opts.options[opts.selectedIndex].value;\n"
        "      var baseval = document.getElementById('cfg_value').value;\n"
        "      var http = new XMLHttpRequest();\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),
        "      var url = \"http://%s:%d/\"; \n",
        webui->hostname,cnt[0]->conf.webcontrol_port);
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "      var optval=encodeURI(baseval);\n"
        "      if (camnbr == \"all\"){\n"
        "        url = url + \"0\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        "      url = url + \"/config/set?\" + optsel + \"=\" + optval;\n"
        "      http.open(\"GET\", url, true);\n"
        "      http.onreadystatechange = function() {\n"
        "        if(http.readyState == 4 && http.status == 200) {\n"

        "        }\n"
        "      }\n"
        "      http.send(null);\n"
        "      document.getElementById('cfg_value').value = \"\";\n"
        "      opts.options[opts.selectedIndex].setAttribute('data-'+camstr,baseval);\n"
        "      opts.value = 'default';\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));


    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_html_script_cfgchg(struct webui_ctx *webui) {
    /* Write the javascript option_change function */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function config_change() {\n"
        "      var camSel = 'data-'+ document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var opts = document.getElementById('cfg_parms');\n"
        "      var optval = opts.options[opts.selectedIndex].getAttribute(camSel);\n"
        "      if (optval == null){\n"
        "        optval = opts.options[opts.selectedIndex].getAttribute('data-cam_all');\n"
        "      }\n"
        "      document.getElementById('cfg_value').value = optval;\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_html_script_trkchg(struct webui_ctx *webui) {
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function track_change() {\n"
        "      var opts = document.getElementById('trk_option');\n"
        "      var optval = opts.options[opts.selectedIndex].getAttribute('data-trk');\n"
        "      if (optval == 'pan'){\n"
        "        document.getElementById('trk_panx').disabled=false;\n"
        "        document.getElementById('trk_tilty').disabled = false;\n"
        "        document.getElementById('trk_lblx').style.display='none';\n"
        "        document.getElementById('trk_lbly').style.display='none';\n"
        "        document.getElementById('trk_lblpan').style.display='inline';\n"
        "        document.getElementById('trk_lbltilt').style.display='inline';\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "      } else if (optval =='abs'){\n"
        "        document.getElementById('trk_panx').disabled=false;\n"
        "        document.getElementById('trk_tilty').disabled = false;\n"
        "        document.getElementById('trk_lblx').value = 'X';\n"
        "        document.getElementById('trk_lbly').value = 'Y';\n"
        "        document.getElementById('trk_lblpan').style.display='none';\n"
        "        document.getElementById('trk_lbltilt').style.display='none';\n"
        "        document.getElementById('trk_lblx').style.display='inline';\n"
        "        document.getElementById('trk_lbly').style.display='inline';\n");
   written = webu_write(webui->client_socket, response, strlen(response));

   snprintf(response, sizeof (response),"%s",
        "      } else {\n"
        "        document.getElementById('cfg_form').style.display='none';\n"
        "        document.getElementById('trk_panx').disabled=true;\n"
        "        document.getElementById('trk_tilty').disabled = true;\n"
        "      }\n"
        "    }\n\n");
   written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_html_script_trkclk(struct context **cnt, struct webui_ctx *webui) {
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    snprintf(response, sizeof (response),"%s",
        "    function track_click() {\n"
        "      var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var camnbr = camstr.substring(4,7);\n"
        "      var opts = document.getElementById('trk_option');\n"
        "      var optsel = opts.options[opts.selectedIndex].getAttribute('data-trk');\n"
        "      var optval1 = document.getElementById('trk_panx').value;\n"
        "      var optval2 = document.getElementById('trk_tilty').value;\n"
        "      var http = new XMLHttpRequest();\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),
        "      var url = \"http://%s:%d/\"; \n",
        webui->hostname,cnt[0]->conf.webcontrol_port);
    written = webu_write(webui->client_socket, response, strlen(response));

    snprintf(response, sizeof (response),"%s",
        "      if (camnbr == \"all\"){\n"
        "        url = url + \"0\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        "      if (optsel == 'pan'){\n"
        "        url = url + '/track/set?pan=' + optval1 + '&tilt=' + optval2;\n"
        "      } else if (optsel == 'abs') {\n"
        "        url = url + '/track/set?x=' + optval1 + '&y=' + optval2;\n"
        "      } else {\n"
        "        url = url + '/track/center'\n"
        "      }\n"
        "      http.open(\"GET\", url, true);\n"
        "      http.onreadystatechange = function() {\n"
        "        if(http.readyState == 4 && http.status == 200) {\n"
        "         }\n"
        "      }\n"
        "      http.send(null);\n"
        "    }\n\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_script(struct context **cnt, struct webui_ctx *webui) {
    /* Write the javascripts */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s", "  <script>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_script_action(cnt, webui);

    webu_html_script_camera(cnt, webui);

    webu_html_script_cfgclk(cnt, webui);

    webu_html_script_cfgchg(webui);

    webu_html_script_trkclk(cnt, webui);

    webu_html_script_trkchg(webui);

    webu_html_script_menucam(webui);

    webu_html_script_menuact(webui);

    webu_html_script_evtclk(webui);

    snprintf(response, sizeof (response),"%s", "  </script>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_html_body(struct context **cnt, struct webui_ctx *webui) {
    /* Write the body section of the form */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s","<body class=\"body\">\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_navbar(cnt, webui);

    snprintf(response, sizeof (response),
        "  <div id=\"id_header\">\n"
        "    <h3 id='h3_cam' data-cam=\"cam_all\" class='header-center'>%s</h3>\n"
        "  </div>\n"
        ,_("All Cameras"));
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_config(cnt, webui);

    webu_html_track(webui);

    webu_html_preview(cnt, webui);

    webu_html_script(cnt, webui);

    snprintf(response, sizeof (response),"%s", "</body>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_html_page(struct context **cnt, struct webui_ctx *webui) {
    /* Write the main page html */
    ssize_t written;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "<!DOCTYPE html>\n"
        "<html lang=\"%s\">\n",webui->lang);
    written = webu_write(webui->client_socket, response, strlen(response));

    webu_html_head(webui);

    webu_html_body(cnt, webui);

    snprintf(response, sizeof (response),"%s", "</html>\n");
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_process_action(struct context **cnt, struct webui_ctx *webui) {

    int thread_nbr, indx;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx = 0;
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        if (thread_nbr == 0) {
            while (cnt[++indx])
            cnt[indx]->makemovie = 1;
        } else {
            cnt[thread_nbr]->makemovie = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        if (thread_nbr == 0) {
            while (cnt[++indx])
            cnt[indx]->snapshot = 1;
        } else {
            cnt[thread_nbr]->snapshot = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"restart")){
        /* This is the legacy method...(we can do better than signals..).*/
        if (thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "httpd is going to restart");
            kill(getpid(),SIGHUP);
            cnt[0]->webcontrol_finish = TRUE;
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                "httpd is going to restart thread %d",thread_nbr);
            if (cnt[thread_nbr]->running) {
                cnt[thread_nbr]->makemovie = 1;
                cnt[thread_nbr]->finish = 1;
            }
            cnt[thread_nbr]->restart = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"quit")){
        /* This is the legacy method...(we can do better than signals..).*/
        if (thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "httpd quits");
            kill(getpid(),SIGQUIT);
           cnt[0]->webcontrol_finish = TRUE;
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                "httpd quits thread %d",thread_nbr);
            cnt[thread_nbr]->restart = 0;
            cnt[thread_nbr]->makemovie = 1;
            cnt[thread_nbr]->finish = 1;
            cnt[thread_nbr]->watchdog = WATCHDOG_OFF;
        }
    } else if (!strcmp(webui->uri_cmd2,"start")){
        if (thread_nbr == 0) {
            do {
                cnt[indx]->pause = 0;
            } while (cnt[++indx]);
        } else {
            cnt[thread_nbr]->pause = 0;
        }
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        if (thread_nbr == 0) {
            do {
                cnt[indx]->pause = 1;
            } while (cnt[++indx]);
        } else {
            cnt[thread_nbr]->pause = 1;
        }
    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        conf_print(cnt);
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            "Invalid action requested: %s",webui->uri_cmd2);
        return;
    }
}

static void webu_process_config(struct context **cnt, struct webui_ctx *webui) {

    int thread_nbr, indx;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (strcasecmp(webui->uri_cmd2, "set")) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid command request: %s",webui->uri_cmd2);
        return;
    }

    indx=0;
    while (config_params[indx].param_name != NULL) {
        if (((thread_nbr != 0) && (config_params[indx].main_thread)) ||
            (config_params[indx].webui_level > cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx].webui_level == WEBUI_LEVEL_NEVER) ) {
            indx++;
            continue;
        }
        if (!strcasecmp(webui->uri_parm1, config_params[indx].param_name)) break;
        indx++;
    }
    if (config_params[indx].param_name != NULL){
        if (strlen(webui->uri_parm1) > 0){
            /* This is legacy assumption on the pointers being sequential*/
            conf_cmdparse(cnt + thread_nbr, config_params[indx].param_name, webui->uri_value1);

            /*If we are updating vid parms, set the flag to update the device.*/
            if (!strcasecmp(config_params[indx].param_name, "vid_control_params") &&
                (cnt[thread_nbr]->vdev != NULL)) cnt[thread_nbr]->vdev->update_parms = TRUE;

            /* If changing language, do it now */
            if (!strcasecmp(config_params[indx].param_name, "motion_lang")){
                translate_locale_chg(webui->uri_value1);
            }

        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,"set the value to null/zero");
        }
    }

    return;

}

static void webu_process_track(struct context **cnt, struct webui_ctx *webui) {

    int thread_nbr;
    struct coord cent;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (!strcasecmp(webui->uri_cmd2, "center")) {
        cnt[thread_nbr]->moved = track_center(cnt[thread_nbr], 0, 1, 0, 0);
    } else if (!strcasecmp(webui->uri_cmd2, "set")) {
        if (!strcasecmp(webui->uri_parm1, "pan")) {
            cent.width = cnt[thread_nbr]->imgs.width;
            cent.height = cnt[thread_nbr]->imgs.height;
            cent.x = atoi(webui->uri_value1);
            cent.y = 0;
            cnt[thread_nbr]->moved = track_move(cnt[thread_nbr],
                                            cnt[thread_nbr]->video_dev,
                                            &cent, &cnt[thread_nbr]->imgs, 1);

            cent.width = cnt[thread_nbr]->imgs.width;
            cent.height = cnt[thread_nbr]->imgs.height;
            cent.x = 0;
            cent.y = atoi(webui->uri_value2);
            cnt[thread_nbr]->moved = track_move(cnt[thread_nbr],
                                            cnt[thread_nbr]->video_dev,
                                            &cent, &cnt[thread_nbr]->imgs, 1);

        } else if (!strcasecmp(webui->uri_parm1, "x")) {
            cnt[thread_nbr]->moved = track_center(cnt[thread_nbr]
                                                  , cnt[thread_nbr]->video_dev, 1
                                                  , atoi(webui->uri_value1)
                                                  , atoi(webui->uri_value2));
        }
    }

    return;

}

static int webu_html_main(struct context **cnt, struct webui_ctx *webui) {

    /* Note some detection and config requested actions call the
     * action function.  This is because the legacy interface
     * put these into those pages.  We put them together here
     * based upon the structure of the new interface
     */
    int retcd;

    /*
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s<"
       " parm1:>%s< val1:>%s<"
       " parm2:>%s< val2:>%s<"
       ,webui->uri_thread
       ,webui->uri_cmd1, webui->uri_cmd2
       ,webui->uri_parm1, webui->uri_value1
       ,webui->uri_parm2, webui->uri_value2);
    */

    retcd = 0;
    if (strlen(webui->uri_thread) == 0){
        webu_html_ok(webui);
        webu_html_page(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"set"))) {
        webu_html_ok(webui);
        webu_process_config(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_html_ok(webui);
        webu_process_action(cnt, webui);

    } else if (!strcmp(webui->uri_cmd1,"action")){
        webu_html_ok(webui);
        webu_process_action(cnt, webui);

    } else if (!strcmp(webui->uri_cmd1,"detection")){
        webu_html_ok(webui);
        webu_process_action(cnt, webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        webu_html_ok(webui);
        webu_process_track(cnt, webui);

    } else{
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid action requested");
        retcd = -1;
    }

    return retcd;
}

static void webu_text_page(struct webui_ctx *webui) {
    /* Write the main page text */
    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx, indx_st;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    snprintf(response, sizeof (response),
        "Motion "VERSION" Running [%d] Camera%s\n0\n",
        webui->cam_count, (webui->cam_count > 1 ? "s" : ""));
    written = webu_write(webui->client_socket, response, strlen(response));

    for (indx = indx_st; indx < webui->cam_threads; indx++) {
        snprintf(response, sizeof (response), "%d\n", indx);
        written = webu_write(webui->client_socket, response, strlen(response));
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");
}

static void webu_text_list(struct context **cnt, struct webui_ctx *webui) {
    /* Write out the options and values */

    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int thread_nbr, indx_parm;
    const char *val_parm;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx_parm = 0;
    written = 1;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((thread_nbr != 0) && (config_params[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(cnt, NULL, indx_parm, thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(cnt, NULL, indx_parm, 0);
        }
        snprintf(response, sizeof (response),"  %s = %s \n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        written = webu_write(webui->client_socket, response, strlen(response));

        indx_parm++;
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_get(struct context **cnt, struct webui_ctx *webui) {
    /* Write out the option value for one parm */

    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx_parm, thread_nbr;
    const char *val_parm;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx_parm = 0;
    written = 1;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            strcmp(webui->uri_parm1,"query") ||
            strcmp(webui->uri_value1, config_params[indx_parm].param_name)){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(cnt, NULL, indx_parm, thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(cnt, NULL, indx_parm, 0);
        }

        snprintf(response, sizeof (response),"%s = %s \nDone\n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        written = webu_write(webui->client_socket, response, strlen(response));

        break;
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_status(struct context **cnt, struct webui_ctx *webui) {
    /* Write out the pause/active status */

    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, thread_nbr;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    written = 0;
    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }
    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (thread_nbr == 0){
        for (indx = indx_st; indx < webui->cam_threads; indx++) {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s\n"
            ,cnt[indx]->conf.camera_id
            ,(!cnt[indx]->running)? "NOT RUNNING":
            (cnt[indx]->pause)? "PAUSE":"ACTIVE");
            written = webu_write(webui->client_socket, response, strlen(response));
        }
    } else {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s\n"
            ,cnt[thread_nbr]->conf.camera_id
            ,(!cnt[thread_nbr]->running)? "NOT RUNNING":
            (cnt[thread_nbr]->pause)? "PAUSE":"ACTIVE");
        written = webu_write(webui->client_socket, response, strlen(response));
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_connection(struct context **cnt, struct webui_ctx *webui) {
    /* Write out the connection status */

    ssize_t written;
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, thread_nbr;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    written = 0;
    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }
    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (thread_nbr == 0){
        for (indx = indx_st; indx < webui->cam_threads; indx++) {
        snprintf(response,sizeof(response)
            , "Camera %d%s%s %s\n"
            ,cnt[indx]->conf.camera_id
            ,cnt[indx]->conf.camera_name ? " -- " : ""
            ,cnt[indx]->conf.camera_name ? cnt[indx]->conf.camera_name : ""
            ,(!cnt[indx]->running)? "NOT RUNNING" :
             (cnt[indx]->lost_connection)? "Lost connection": "Connection OK");
        written = webu_write(webui->client_socket, response, strlen(response));
        }
    } else {
        snprintf(response,sizeof(response)
            , "Camera %d%s%s %s\n"
            ,cnt[thread_nbr]->conf.camera_id
            ,cnt[thread_nbr]->conf.camera_name ? " -- " : ""
            ,cnt[thread_nbr]->conf.camera_name ? cnt[thread_nbr]->conf.camera_name : ""
            ,(!cnt[thread_nbr]->running)? "NOT RUNNING" :
             (cnt[thread_nbr]->lost_connection)? "Lost connection": "Connection OK");
        written = webu_write(webui->client_socket, response, strlen(response));
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_set(struct context **cnt, struct webui_ctx *webui) {
    /* Write out the connection status */

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    webu_process_config(cnt, webui);

    snprintf(response,sizeof(response)
        , "%s = %s\nDone \n"
        ,webui->uri_parm1
        ,webui->uri_value1
    );
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_action(struct context **cnt, struct webui_ctx *webui) {
    /* Call the start */

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    webu_process_action(cnt, webui);

    written = 0;
    /* Send response message for action */
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        snprintf(response,sizeof(response)
            ,"makemovie for thread %s \nDone\n"
            ,webui->uri_thread
        );
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        snprintf(response,sizeof(response)
            ,"Snapshot for thread %s \nDone\n"
            ,webui->uri_thread
        );
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if (!strcmp(webui->uri_cmd2,"restart")){
        snprintf(response,sizeof(response)
            ,"Restart in progress ...\nDone\n");
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if (!strcmp(webui->uri_cmd2,"quit")){
        snprintf(response,sizeof(response)
            ,"quit in progress ... bye \nDone\n");
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if (!strcmp(webui->uri_cmd2,"start")){
        snprintf(response,sizeof(response)
            ,"Camera %s Detection resumed\nDone \n"
            ,webui->uri_thread
        );
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        snprintf(response,sizeof(response)
            ,"Camera %s Detection paused\nDone \n"
            ,webui->uri_thread
        );
        written = webu_write(webui->client_socket, response, strlen(response));
    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        snprintf(response,sizeof(response)
            ,"Camera %s write\nDone \n"
            ,webui->uri_thread
        );
        written = webu_write(webui->client_socket, response, strlen(response));
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            "Invalid action requested: %s",webui->uri_cmd2);
        return;
    }

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}

static void webu_text_track(struct context **cnt, struct webui_ctx *webui) {
    /* Call the start */

    ssize_t written;
    char response[WEBUI_LEN_RESP];

    webu_process_track(cnt, webui);
    snprintf(response,sizeof(response)
        ,"Camera %s \nTrack set %s\nDone \n"
        ,webui->uri_thread
        ,webui->uri_cmd2
    );
    written = webu_write(webui->client_socket, response, strlen(response));

    if (written <= 0) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error writing");

}


static int webu_text_main(struct context **cnt, struct webui_ctx *webui) {

    int retcd;

    /*
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s<"
       " parm1:>%s< val1:>%s<"
       " parm2:>%s< val2:>%s<"
       ,webui->uri_thread
       ,webui->uri_cmd1, webui->uri_cmd2
       ,webui->uri_parm1, webui->uri_value1
       ,webui->uri_parm2, webui->uri_value2);
    */

    retcd = 0;
    if (strlen(webui->uri_thread) == 0){
        webu_text_ok(webui);
        webu_text_page(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"set"))) {
        webu_text_ok(webui);
        webu_text_set(cnt,webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_text_ok(webui);
        webu_text_action(cnt,webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"list"))) {
        webu_text_ok(webui);
        webu_text_list(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"get"))) {
        webu_text_ok(webui);
        webu_text_get(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"status"))) {
        webu_text_ok(webui);
        webu_text_status(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"connection"))) {
        webu_text_ok(webui);
        webu_text_connection(cnt, webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"start"))) {
        webu_text_ok(webui);
        webu_text_action(cnt,webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"pause"))) {
        webu_text_ok(webui);
        webu_text_action(cnt,webui);

    } else if (!strcmp(webui->uri_cmd1,"action")) {
        webu_text_ok(webui);
        webu_text_action(cnt, webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        webu_text_ok(webui);
        webu_text_track(cnt, webui);

    } else{
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid action requested");
        retcd = -1;
    }

    return retcd;
}

static int webu_read_client(struct context **cnt, struct webui_ctx *webui) {

    int retcd;

    pthread_mutex_lock(&webui->webu_mutex);

        retcd = webu_header_read(webui);
        if (retcd == 0) retcd = webu_parseurl(webui);
        if (retcd == 0) retcd = webu_header_hostname(webui);
        if (retcd == 0) retcd = webu_header_auth(webui);
        if (cnt[0]->conf.webcontrol_interface == 1){
            if (retcd == 0) retcd = webu_text_main(cnt, webui);
            if (retcd <  0) webu_text_badreq(webui);
        } else {
            if (retcd == 0) retcd = webu_html_main(cnt, webui);
            if (retcd <  0) webu_html_badreq(webui);
        }

    pthread_mutex_unlock(&webui->webu_mutex);

    return 0;
}

static void webu_auth_parms(struct context **cnt, struct webui_ctx *webui) {

    char *userpass = NULL;
    size_t auth_size;

    if (cnt[0]->conf.webcontrol_authentication != NULL) {
        auth_size = strlen(cnt[0]->conf.webcontrol_authentication);

        webui->auth_parms = mymalloc(BASE64_LENGTH(auth_size) + 1);
        userpass = mymalloc(auth_size + 4);
        /* motion_base64_encode can read 3 bytes after the end of the string, initialize it */
        memset(userpass, 0, auth_size + 4);
        strcpy(userpass, cnt[0]->conf.webcontrol_authentication);
        motion_base64_encode(userpass, webui->auth_parms, auth_size);
        free(userpass);
    }

    return;
}

static void webu_httpd_run(struct context **cnt, struct webui_ctx *webui) {

    int closehttpd, socket_desc, retcd;
    char clientip[NI_MAXHOST];

    socket_desc = http_bindsock(
                          cnt[0]->conf.webcontrol_port
                         ,cnt[0]->conf.webcontrol_localhost
                         ,cnt[0]->conf.ipv6_enabled);
    if (socket_desc < 0) return;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
               ,"Started motion-httpd server on port %d (auth %s)"
               ,cnt[0]->conf.webcontrol_port
               ,cnt[0]->conf.webcontrol_authentication ? "Enabled":"Disabled");

    webu_auth_parms(cnt, webui);

    closehttpd = FALSE;
    retcd = 0;
    while ((retcd == 0) && (!closehttpd)) {
        webui->client_socket = webu_nonblock(socket_desc);
        if (webui->client_socket < 0) {
            if ((!cnt[0]) || (cnt[0]->webcontrol_finish)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "Finishing");
                closehttpd = TRUE;
            }
        } else {
            /* Get the Client request */
            retcd = webu_read_client(cnt, webui);
            if (retcd < 0){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "Error processing web interface");
            }
            webu_clientip(clientip, webui->client_socket);
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "Read from client (%s) %d", clientip, retcd);
            if (webui->client_socket) close(webui->client_socket);
        }
    }

    if (webui->client_socket!= -1) close(webui->client_socket);
    close(socket_desc);

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "Closing");
}

void *webu_main(void *arg) {
    /* This is the entry point for the web control thread*/
    struct context **cnt = arg;
    struct sigaction act;
    struct webui_ctx *webui;

    util_threadname_set("wu", 0,NULL);

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(cnt, webui);

    pthread_mutex_init(&webui->webu_mutex, NULL);

    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    webu_httpd_run(cnt, webui);

    pthread_mutex_destroy(&webui->webu_mutex);

    webu_context_free(webui);

    /* Update how many threads we have running. */
    pthread_mutex_lock(&global_lock);
        threads_running--;
        cnt[0]->webcontrol_running = 0;
    pthread_mutex_unlock(&global_lock);

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "Thread exit");
    pthread_exit(NULL);

}
