/**
 *      netcam.c
 *
 *      Module of common routines for handling network cameras.
 *
 */

#include "translate.h"
#include "motion.h"

#include <regex.h>                    /* For parsing of the URL */

#include "netcam_http.h"
#include "netcam_ftp.h"

/*
 * The following three routines (netcam_url_match, netcam_url_parse and
 * netcam_url_free are for 'parsing' (i.e. separating into the relevant
 * components) the URL provided by the user.  They make use of regular
 * expressions (which is outside the scope of this module, so detailed
 * comments are not provided).  netcam_url_parse is called from netcam_start,
 * and puts the "broken-up" components of the URL into the "url" element of
 * the netcam_context structure.
 *
 * Note that the routines are not "very clever", but they work sufficiently
 * well for the limited requirements of this module.  The expression:
 *   (http)://(((.*):(.*))@)?([^/:]|[-.a-z0-9]+)(:([0-9]+))?($|(/[^:]*))
 * requires
 *   1) a string which begins with 'http', followed by '://'
 *   2) optionally a '@' which is preceded by two strings
 *      (with 0 or more characters each) separated by a ':'
 *      [this is for an optional username:password]
 *   3) a string comprising alpha-numerics, '-' and '.' characters
 *      [this is for the hostname]
 *   4) optionally a ':' followed by one or more numeric characters
 *      [this is for an optional port number]
 *   5) finally, either an end of line or a series of segments,
 *      each of which begins with a '/', and contains anything
 *      except a ':'
 */

/**
 * netcam_url_match
 *
 *      Finds the matched part of a regular expression
 *
 * Parameters:
 *
 *      m          A structure containing the regular expression to be used
 *      input      The input string
 *
 * Returns:        The string which was matched
 *
 */
static char *netcam_url_match(regmatch_t m, const char *input)
{
    char *match = NULL;
    int len;

    if (m.rm_so != -1) {
        len = m.rm_eo - m.rm_so;

        if ((match = mymalloc(len + 1)) != NULL) {
            strncpy(match, input + m.rm_so, len);
            match[len] = '\0';
        }
    }

    return match;
}

static void netcam_url_invalid(struct url_t *parse_url){

    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Invalid URL.  Can not parse values."));

    parse_url->host = malloc(5);
    parse_url->service = malloc(5);
    parse_url->path = malloc(10);
    parse_url->userpass = malloc(10);
    parse_url->port = 0;
    sprintf(parse_url->host, "%s","????");
    sprintf(parse_url->service, "%s","????");
    sprintf(parse_url->path, "%s","INVALID");
    sprintf(parse_url->userpass, "%s","INVALID");

}
/**
 * netcam_url_parse
 *
 *      parses a string containing a URL into it's components
 *
 * Parameters:
 *      parse_url          A structure which will receive the results
 *                         of the parsing
 *      text_url           The input string containing the URL
 *
 * Returns:                Nothing
 *
 */
void netcam_url_parse(struct url_t *parse_url, const char *text_url)
{
    char *s;
    int i;

    const char *re = "(http|ftp|mjpg|mjpeg|rtsp|rtmp)://(((.*):(.*))@)?"
                     "([^/:]|[-_.a-z0-9]+)(:([0-9]+))?($|(/[^*]*))";
    regex_t pattbuf;
    regmatch_t matches[10];

    if (!strncmp(text_url, "file", 4))
        re = "(file)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "jpeg", 4))
        re = "(jpeg)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "v4l2", 4))
        re = "(v4l2)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    /*  Note that log messages are commented out to avoid leaking info related
     *  to user/host/pass etc.  Keeing them in the code for easier debugging if
     *  it is needed
     */

    //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Entry netcam_url_parse data %s",text_url);

    memset(parse_url, 0, sizeof(struct url_t));
    /*
     * regcomp compiles regular expressions into a form that is
     * suitable for regexec searches
     * regexec matches the URL string against the regular expression
     * and returns an array of pointers to strings matching each match
     * within (). The results that we need are finally placed in parse_url.
     */
    if (!regcomp(&pattbuf, re, REG_EXTENDED | REG_ICASE)) {
        if (regexec(&pattbuf, text_url, 10, matches, 0) != REG_NOMATCH) {
            for (i = 0; i < 10; i++) {
                if ((s = netcam_url_match(matches[i], text_url)) != NULL) {
                    //MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "Parse case %d data %s", i, s);
                    switch (i) {
                    case 1:
                        parse_url->service = s;
                        break;
                    case 3:
                        parse_url->userpass = s;
                        break;
                    case 6:
                        parse_url->host = s;
                        break;
                    case 8:
                        parse_url->port = atoi(s);
                        free(s);
                        break;
                    case 9:
                        parse_url->path = s;
                        break;
                        /* Other components ignored */
                    default:
                        free(s);
                        break;
                    }
                }
            }
        } else {
            netcam_url_invalid(parse_url);
        }
    } else {
        netcam_url_invalid(parse_url);
    }
    if (((!parse_url->port) && (parse_url->service)) ||
        ((parse_url->port > 65535) && (parse_url->service))) {
        if (!strcmp(parse_url->service, "http"))
            parse_url->port = 80;
        else if (!strcmp(parse_url->service, "ftp"))
            parse_url->port = 21;
        else if (!strcmp(parse_url->service, "rtmp"))
            parse_url->port = 1935;
        else if (!strcmp(parse_url->service, "rtsp"))
            parse_url->port = 554;
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Using port number %d"),parse_url->port);
    }

    regfree(&pattbuf);
}

/**
 * netcam_url_free
 *
 *      General cleanup of the URL structure, called from netcam_cleanup.
 *
 * Parameters:
 *
 *      parse_url       Structure containing the parsed data.
 *
 * Returns:             Nothing
 *
 */
void netcam_url_free(struct url_t *parse_url)
{
    free(parse_url->service);
    parse_url->service = NULL;

    free(parse_url->userpass);
    parse_url->userpass = NULL;

    free(parse_url->host);
    parse_url->host = NULL;

    free(parse_url->path);
    parse_url->path = NULL;
}

/**
 * netcam_handler_loop
 *      This is the "main loop" for the handler thread.  It is created
 *      in netcam_start when a streaming camera is detected.
 *
 * Parameters
 *
 *      arg     Pointer to the motion context for this camera.
 *
 * Returns:     NULL pointer
 *
 */
static void *netcam_handler_loop(void *arg)
{
    int retval;
    int open_error = 0;
    netcam_context_ptr netcam = arg;
    struct context *cnt = netcam->cnt; /* Needed for the SETUP macro :-( */

    netcam->handler_finished = FALSE;

    util_threadname_set("nc",netcam->threadnr,netcam->cnt->conf.camera_name);

    /* Store the corresponding motion thread number in TLS also for this
     * thread (necessary for 'MOTION_LOG' to function properly).
     */
    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cnt->threadnr));

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("Camera handler thread [%d] started"), netcam->threadnr);
    /*
     * The logic of our loop is very simple.  If this is a non-
     * streaming camera, we re-establish connection with the camera
     * and read the header record.  If it's a streaming camera, we
     * position to the next "boundary string" in the input stream.
     * In either case, we then read the following JPEG image into the
     * next available buffer, updating the "next" and "latest" indices
     * in our netcam * structure.  The loop continues until netcam->finish
     * or cnt->finish is set.
     */

    while (!netcam->finish) {
        if (netcam->response) {    /* If html input */
            if (netcam->caps.streaming == NCS_UNSUPPORTED) {
                /* Non-streaming ie. jpeg */
                if (!netcam->connect_keepalive ||
                    (netcam->connect_keepalive && netcam->keepalive_timeup)) {
                    /* If keepalive flag set but time up, time to close this socket. */
                    if (netcam->connect_keepalive && netcam->keepalive_timeup) {
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Closing netcam socket as Keep-Alive time is up "
                            "(camera sent Close field). A reconnect should happen."));
                        netcam_disconnect(netcam);
                        netcam->keepalive_timeup = FALSE;
                    }

                    /* And the netcam_connect call below will open a new one. */
                    if (netcam_connect(netcam, open_error) < 0) {
                        if (!open_error) { /* Log first error. */
                            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                                ,_("re-opening camera (non-streaming)"));
                            open_error = 1;
                        }
                        /* Need to have a dynamic delay here. */
                        SLEEP(5, 0);
                        continue;
                    }

                    if (open_error) {          /* Log re-connection */
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("camera re-connected"));
                        open_error = 0;
                    }
                }
                /* Send our request and look at the response. */
                if ((retval = netcam_read_first_header(netcam)) != 1) {
                    if (retval > 0) {
                        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                            ,_("Unrecognized image header (%d)"), retval);
                    } else if (retval != -1) {
                        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                            ,_("Error in header (%d)"), retval);
                    }
                    /* Need to have a dynamic delay here. */
                    continue;
                }
            } else if (netcam->caps.streaming == NCS_MULTIPART) {    /* Multipart Streaming */
                if (netcam_read_next_header(netcam) < 0) {
                    if (netcam_connect(netcam, open_error) < 0) {
                        if (!open_error) { /* Log first error */
                            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                                ,_("re-opening camera (streaming)"));
                            open_error = 1;
                        }
                        SLEEP(5, 0);
                        continue;
                    }

                    if ((retval = netcam_read_first_header(netcam) != 2)) {
                        if (retval > 0) {
                            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                                ,_("Unrecognized image header (%d)"), retval);
                        } else if (retval != -1) {
                            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                                ,_("Error in header (%d)"), retval);
                        }
                        /* FIXME need some limit. */
                        continue;
                    }
                }
                if (open_error) {          /* Log re-connection */
                    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                        ,_("camera re-connected"));
                    open_error = 0;
                }
            } else if (netcam->caps.streaming == NCS_BLOCK) { /* MJPG-Block streaming */
                /*
                 * Since we cannot move in the stream here, because we will read past the
                 * MJPG-block-header, error handling is done while reading MJPG blocks.
                 */
            }
        }


        if (netcam->get_image(netcam) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Error getting jpeg image"));
            /* If FTP connection, attempt to re-connect to server. */
            if (netcam->ftp) {
                close(netcam->ftp->control_file_desc);
                if (ftp_connect(netcam) < 0)
                    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Trying to re-connect"));
            }
            continue;
        }


        /*
         * FIXME
         * Need to check whether the image was received / decoded
         * satisfactorily.
         */

        /*
         * If non-streaming, want to synchronize our thread with the
         * motion main-loop.
         */
        if (netcam->caps.streaming == NCS_UNSUPPORTED) {
            pthread_mutex_lock(&netcam->mutex);

            /* Before anything else, check for system shutdown. */
            if (netcam->finish) {
                pthread_mutex_unlock(&netcam->mutex);
                break;
            }

            /*
             * If our current loop has finished before the next
             * request from the motion main-loop, we do a
             * conditional wait (wait for signal).  On the other
             * hand, if the motion main-loop has already signalled
             * us, we just continue.  In either event, we clear
             * the start_capture flag set by the main loop.
             */
            if (!netcam->start_capture)
                pthread_cond_wait(&netcam->cap_cond, &netcam->mutex);

            netcam->start_capture = 0;

            pthread_mutex_unlock(&netcam->mutex);
        }
    /* The loop continues forever, or until motion shutdown. */
    }

    /* Our thread is finished - decrement motion's thread count. */
    pthread_mutex_lock(&global_lock);
    threads_running--;
    pthread_mutex_unlock(&global_lock);

    /* Log out a termination message. */
    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("netcam camera handler: finish set, exiting"));

    netcam->handler_finished = TRUE;

    /* Signal netcam_cleanup that we're all done. */
    pthread_mutex_lock(&netcam->mutex);
    pthread_cond_signal(&netcam->exiting);
    pthread_mutex_unlock(&netcam->mutex);

    /* Goodbye..... */
    pthread_exit(NULL);
}

/**
 * netcam_cleanup
 *
 *      This routine releases any allocated data within the netcam context,
 *      then frees the context itself.  Extreme care must be taken to assure
 *      that the multi-threading nature of the program is correctly
 *      handled.
 *      This function is also called from motion_init if first time connection
 *      fails and we start retrying until we get a valid first frame from the
 *      camera.
 *
 * Parameters:
 *
 *      netcam           Pointer to a netcam context
 *      init_retry_flag  1 when the function is called because we are retrying
 *                         making the initial connection with a netcam and we know
 *                         we do not need to kill a netcam handler thread
 *                       0 in any other case.
 *
 * Returns:              Nothing.
 *
 */
void netcam_cleanup(netcam_context_ptr netcam, int init_retry_flag){
    struct timespec waittime;

    if (!netcam) return;

    /*
     * This 'lock' is just a bit of "defensive" programming.  It should
     * only be necessary if the routine is being called from different
     * threads, but in our Motion design, it should only be called from
     * the motion main-loop.
     */
    pthread_mutex_lock(&netcam->mutex);

    if (netcam->cnt->netcam == NULL)
        return;

    /*
     * We set the netcam_context pointer in the motion main-loop context
     * to be NULL, so that this routine won't be called a second time.
     */
    netcam->cnt->netcam = NULL;

    /*
     * Next we set 'finish' in order to get the camera-handler thread
     * to stop.
     */
    netcam->finish = 1;

    /*
     * If the camera is non-streaming, the handler thread could be waiting
     * for a signal, so we send it one.  If it's actually waiting on the
     * condition, it won't actually start yet because we still have
     * netcam->mutex locked.
     */

    if (netcam->caps.streaming == NCS_UNSUPPORTED)
        pthread_cond_signal(&netcam->cap_cond);


    /*
     * Once the camera-handler gets to the end of it's loop (probably as
     * soon as we release netcam->mutex), because netcam->finish has been
     * set it will exit it's loop, do anything it needs to do with the
     * netcam context, and then send *us* as signal (netcam->exiting).
     * Note that when we start our wait on netcam->exiting, our lock on
     * netcam->mutex is automatically released, which will allow the
     * handler to complete it's loop, notice that 'finish' is set and exit.
     * This should always work, but again (defensive programming) we
     * use pthread_cond_timedwait and, if our timeout (8 seconds) expires
     * we just do the cleanup the handler would normally have done.  This
     * assures that (even if there is a bug in our code) motion will still
     * be able to exit.
     * If the init_retry_flag is not set the netcam_cleanup code was
     * called while retrying the initial connection to a netcam and then
     * there is no camera-handler started yet and thread_running must
     * not be decremented.
     */
    waittime.tv_sec = time(NULL) + 8;   /* Seems that 3 is too small */
    waittime.tv_nsec = 0;

    if (!init_retry_flag &&
        pthread_cond_timedwait(&netcam->exiting, &netcam->mutex, &waittime) != 0) {
        /*
         * Although this shouldn't happen, if it *does* happen we will
         * log it (just for the programmer's information).
         */
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("No response from camera handler - it must have already died"));
        pthread_mutex_lock(&global_lock);
        threads_running--;
        pthread_mutex_unlock(&global_lock);
    }

    /* We don't need any lock anymore, so release it. */
    pthread_mutex_unlock(&netcam->mutex);

    /* and cleanup the rest of the netcam_context structure. */
    free(netcam->connect_host);
    free(netcam->connect_request);
    free(netcam->boundary);


    if (netcam->latest != NULL) {
        free(netcam->latest->ptr);
        free(netcam->latest);
    }

    if (netcam->receiving != NULL) {
        free(netcam->receiving->ptr);
        free(netcam->receiving);
    }

    if (netcam->jpegbuf != NULL) {
        free(netcam->jpegbuf->ptr);
        free(netcam->jpegbuf);
    }

    if (netcam->ftp != NULL) {
        ftp_free_context(netcam->ftp);
        netcam->ftp = NULL;
    } else {
        netcam_disconnect(netcam);
    }

    free(netcam->response);

    pthread_mutex_destroy(&netcam->mutex);
    pthread_cond_destroy(&netcam->cap_cond);
    pthread_cond_destroy(&netcam->pic_ready);
    pthread_cond_destroy(&netcam->exiting);
    free(netcam);
}

/**
 * netcam_next
 *
 *      This routine is called when the main 'motion' thread wants a new
 *      frame of video.  It fetches the most recent frame available from
 *      the netcam, converts it to YUV420P, and returns it to motion.
 *
 * Parameters:
 *      cnt             Pointer to the context for this thread
 *      image           Pointer to a buffer for the returned image
 *
 * Returns:             Error code
 */
int netcam_next(struct context *cnt, struct image_data *img_data){

    netcam_context_ptr netcam;

    /*
     * Here we have some more "defensive programming".  This check should
     * never be true, but if it is just return with a "fatal error".
     */
    if ((!cnt) || (!cnt->netcam))
        return NETCAM_FATAL_ERROR;

    netcam = cnt->netcam;

    if (!netcam->latest->used) {
        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO,_("called with no data in buffer"));
        return NETCAM_NOTHING_NEW_ERROR;
    }

    /*
     * If we are controlling a non-streaming camera, we synchronize the
     * motion main-loop with the camera-handling thread through a signal,
     * together with a flag to say "start your next capture".
     */
    if (netcam->caps.streaming == NCS_UNSUPPORTED) {
        pthread_mutex_lock(&netcam->mutex);
        netcam->start_capture = 1;
        pthread_cond_signal(&netcam->cap_cond);
        pthread_mutex_unlock(&netcam->mutex);
    }


    /*
     * If an error occurs in the JPEG decompression which follows this,
     * jpeglib will return to the code within this 'if'.  Basically, our
     * approach is to just return a NULL (failed) to the caller (an
     * error message has already been produced by the libjpeg routines).
     */
    if (setjmp(netcam->setjmp_buffer))
        return NETCAM_GENERAL_ERROR | NETCAM_JPEG_CONV_ERROR;

    /* If there was no error, process the latest image buffer. */
    return netcam_proc_jpeg(netcam, img_data);
}

/**
 * netcam_start
 *
 *      This routine is called from the main motion thread.  It's job is
 *      to open up the requested camera device and do any required
 *      initialization.  If the camera is a streaming type, then this
 *      routine must also start up the camera-handling thread to take
 *      care of it.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure for this device.
 *
 * Returns:     0 on success
 *              -1 on any failure
 *              -2 image dimensions are not modulo 8
 */
int netcam_start(struct context *cnt){

    netcam_context_ptr netcam;        /* Local pointer to our context. */
    pthread_attr_t handler_attribute; /* Attributes of our handler thread. */
    int retval;                       /* Working var. */
    struct url_t url;                 /* For parsing netcam URL. */
    char    err_service[6];

    memset(&url, 0, sizeof(url));

    cnt->netcam = mymalloc(sizeof(struct netcam_context));
    netcam = cnt->netcam;           /* Just for clarity in remaining code. */
    netcam->cnt = cnt;              /* Fill in the "parent" info. */

    /* Our image buffers */
    netcam->receiving = mymalloc(sizeof(netcam_buff));
    netcam->receiving->ptr = mymalloc(NETCAM_BUFFSIZE);

    netcam->latest = mymalloc(sizeof(netcam_buff));
    netcam->latest->ptr = mymalloc(NETCAM_BUFFSIZE);

    netcam->jpegbuf = mymalloc(sizeof(netcam_buff));
    netcam->jpegbuf->ptr = mymalloc(NETCAM_BUFFSIZE);

    /* Thread control structures */
    pthread_mutex_init(&netcam->mutex, NULL);
    pthread_cond_init(&netcam->cap_cond, NULL);
    pthread_cond_init(&netcam->pic_ready, NULL);
    pthread_cond_init(&netcam->exiting, NULL);

    /* Initialize the average frame time to the user's value. */
    netcam->av_frame_time = 1000000.0 / cnt->conf.framerate;

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("Network Camera starting for camera (%s)"), cnt->conf.camera_name);

    /* If a proxy has been specified, parse that URL. */
    if (cnt->conf.netcam_proxy) {
        netcam_url_parse(&url, cnt->conf.netcam_proxy);

        if (!url.host) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Invalid netcam_proxy (%s)"), cnt->conf.netcam_proxy);
            netcam_url_free(&url);
            return -1;
        }

        if (url.userpass) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Username/password not allowed on a proxy URL"));
            netcam_url_free(&url);
            return -1;
        }

        /*
         * A 'proxy' means that our eventual 'connect' to our
         * camera must be sent to the proxy, and that our 'GET' must
         * include the full path to the camera host.
         */
        netcam->connect_host = url.host;
        url.host = NULL;
        netcam->connect_port = url.port;
        netcam_url_free(&url);  /* Finished with proxy */
    }

    /* Parse the URL from the configuration data */
    netcam_url_parse(&url, cnt->conf.netcam_url);

    if (!url.service) {
        snprintf(err_service,5,"%s",cnt->conf.netcam_url);
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Invalid netcam service '%s' "), err_service);
        netcam_url_free(&url);
        return -1;
    }

    if ((!url.host) && (strcmp(url.service, "jpeg"))) {
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Invalid netcam_url for camera (%s)"), cnt->conf.camera_name);
        netcam_url_free(&url);
        return -1;
    }

    if (cnt->conf.netcam_proxy == NULL) {
        netcam->connect_host = url.host;
        url.host = NULL;
        netcam->connect_port = url.port;
    }

    /* Get HTTP Mode (1.0 default, 1.0 Keep-Alive, 1.1) flag from config
     * and report its stata for debug reasons.
     * The flags in the conf structure is read only and cannot be
     * unset if the Keep-Alive needs to be switched off (ie. netcam does
     * not turn out to support it. That is handled by unsetting the flags
     * in the context structures (cnt->...) only.
     */

    if (!strcmp(cnt->conf.netcam_keepalive, "force")) {
            netcam->connect_http_10   = TRUE;
            netcam->connect_http_11   = FALSE;
            netcam->connect_keepalive = TRUE;
    } else if (!strcmp(cnt->conf.netcam_keepalive, "off")) {
            netcam->connect_http_10   = TRUE;
            netcam->connect_http_11   = FALSE;
            netcam->connect_keepalive = FALSE;
    } else if (!strcmp(cnt->conf.netcam_keepalive, "on")) {
            netcam->connect_http_10   = FALSE;
            netcam->connect_http_11   = TRUE;
            netcam->connect_keepalive = TRUE; /* HTTP 1.1 has keepalive by default. */
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Netcam_http parameter '%s' converts to flags: HTTP/1.0: %s HTTP/1.1: %s Keep-Alive %s.")
        ,cnt->conf.netcam_keepalive
        ,netcam->connect_http_10 ? "1":"0", netcam->connect_http_11 ? "1":"0"
        ,netcam->connect_keepalive ? "ON":"OFF");


    /* Initialise the netcam socket to -1 to trigger a connection by the keep-alive logic. */
    netcam->sock = -1;

    if ((url.service) && (!strcmp(url.service, "http"))) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("now calling netcam_setup_html()"));
        retval = netcam_setup_html(netcam, &url);
    } else if ((url.service) && (!strcmp(url.service, "ftp"))) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("now calling netcam_setup_ftp"));
        retval = netcam_setup_ftp(netcam, &url);
    } else if ((url.service) && (!strcmp(url.service, "jpeg"))) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("now calling netcam_setup_file()"));
        retval = netcam_setup_file(netcam, &url);
    } else if ((url.service) && (!strcmp(url.service, "mjpg"))) {
        retval = netcam_setup_mjpg(netcam, &url);
    } else {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Invalid netcam service '%s' - must be http, ftp, mjpg, mjpeg, v4l2 or jpeg.")
            , url.service);
        retval = -1;
    }

    netcam_url_free(&url);
    if (retval < 0) return -1;

    /*
     * We expect that, at this point, we should be positioned to read
     * he first image available from the camera (directly after the
     * applicable header).  We want to decode the image in order to get
     * the dimensions (width and height).  If successful, we will use
     * these to set the required image buffer(s) in our netcam_struct.
     */
    if ((retval = netcam->get_image(netcam)) != 0) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Failed trying to read first image - retval:%d"), retval);
        return -1;
    }

    /*
    * If an error occurs in the JPEG decompression which follows this,
    * jpeglib will return to the code within this 'if'.  If such an error
    * occurs during startup, we will just abandon this attempt.
    */
    if (setjmp(netcam->setjmp_buffer)) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("libjpeg decompression failure on first frame - giving up!"));
        return -1;
    }

    netcam->netcam_tolerant_check = cnt->conf.netcam_tolerant_check;
    netcam->JFIF_marker = 0;
    netcam_get_dimensions(netcam);

    /* Validate image sizes are multiple of 8 */
    if ((netcam->width % 8) || (netcam->height % 8) ) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Width/height(%dx%d) must be multiples of 8")
            ,netcam->width, netcam->height);
        return -2;
    }

    /* Fill in camera details into context structure. */
    cnt->imgs.width = netcam->width;
    cnt->imgs.height = netcam->height;
    cnt->imgs.size_norm = (netcam->width * netcam->height * 3) / 2;
    cnt->imgs.motionsize = netcam->width * netcam->height;

    cnt->imgs.width_high  = 0;
    cnt->imgs.height_high = 0;
    cnt->imgs.size_high   = 0;

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);
    pthread_mutex_lock(&global_lock);
        netcam->threadnr = ++threads_running;
    pthread_mutex_unlock(&global_lock);

    retval = pthread_create(&netcam->thread_id, &handler_attribute,&netcam_handler_loop, netcam);
    if (retval < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("Error starting camera handler thread [%d]"), netcam->threadnr);
        return -1;
    }

    return 0;
}

