/*
 *    webu.c
 *
 *    Webcontrol and Streams for motion.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    Portions of code from Angel Carpintero (motiondevelop@gmail.com)
 *    from webhttpd.c Copyright 2004-2005
 *
 *    Majority of module written by MrDave.
 *
 *    Function naming scheme:
 *      webu*      - All functions in this module have this prefix.
 *      webu_start - Entry point to start the daemon.
 *      webu_stop  - Entry point to stop the daemon
 *      webu_mhd*  - Functions related to libmicrohttd implementation
 *      webu_process_action - Performs most items under the action menu
 *      webu_process_config - Saves the parameter values into Motion.
 *      webu_process_track  - Performs the tracking functions.
 *
 *      Some function names are long and are not expected to contain any
 *      logger message that would display the function name to the user.
 *
 *      Functions are generally kept to under one page in length
 *
 *    Known Issues:
 *      The quit/restart uses signals and this should be reconsidered.
 *      The tracking is "best effort" since developer does not have tracking camera.
 *      The conf_cmdparse assumes that the pointers to the motion context for each
 *        camera are always sequential and enforcement of the pointers being sequential
 *        has not been observed in the other modules. (This is a legacy assumption)
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "motion.h"
#include "webu.h"
#include "webu_html.h"
#include "webu_text.h"
#include "webu_stream.h"
#include "translate.h"

/* Context to pass the parms to functions to start mhd */
struct mhdstart_ctx {
    struct context          **cnt;
    char                    *tls_cert;
    char                    *tls_key;
    int                     ctrl;
    int                     indxthrd;
    struct MHD_OptionItem   *mhd_ops;
    int                     mhd_opt_nbr;
    unsigned int            mhd_flags;
    int                     ipv6;
    struct sockaddr_in      lpbk_ipv4;
    struct sockaddr_in6     lpbk_ipv6;
};


static void webu_context_init(struct context **cntlst, struct context *cnt, struct webui_ctx *webui) {

    int indx;

    webui->url           = mymalloc(WEBUI_LEN_URLI);
    webui->uri_camid     = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd1      = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd2      = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm1     = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value1    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm2     = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value2    = mymalloc(WEBUI_LEN_PARM);
    webui->clientip      = mymalloc(WEBUI_LEN_URLI);
    webui->hostname      = mymalloc(WEBUI_LEN_PARM);
    webui->auth_denied   = mymalloc(WEBUI_LEN_RESP);
    webui->auth_opaque   = mymalloc(WEBUI_LEN_PARM);
    webui->auth_realm    = mymalloc(WEBUI_LEN_PARM);
    webui->text_eol      = mymalloc(WEBUI_LEN_PARM);
    webui->auth_user     = NULL;    /* Buffer to hold the user name*/
    webui->auth_pass     = NULL;    /* Buffer to hold the password */
    webui->authenticated = FALSE;   /* boolean for whether we are authenticated*/
    webui->lang          = mymalloc(3);         /* Two digit lang code plus null terminator */
    webui->lang_full     = mymalloc(6);         /* lang code, e.g US_en */
    webui->resp_size     = WEBUI_LEN_RESP * 10; /* The size of the resp_page buffer.  May get adjusted */
    webui->resp_used     = 0;                   /* How many bytes used so far in resp_page*/
    webui->stream_pos    = 0;                   /* Stream position of image being sent */
    webui->stream_fps    = 1;                   /* Stream rate */
    webui->resp_page     = mymalloc(webui->resp_size);      /* The response being constructed */
    webui->cntlst        = cntlst;  /* The list of context's for all cameras */
    webui->cnt           = cnt;     /* The context pointer for a single camera */
    webui->cnct_type     = WEBUI_CNCT_UNKNOWN;

    /* get the number of cameras and threads */
    indx = 0;
    if (webui->cntlst != NULL){
        while (webui->cntlst[++indx]);
    }
    webui->cam_threads = indx;

    webui->cam_count = indx;
    if (indx > 1)
        webui->cam_count--;

    /* 1 thread, 1 camera = just motion.conf.
     * 2 thread, 1 camera, then using motion.conf plus a separate camera file */
    snprintf(webui->lang_full, 6,"%s", getenv("LANGUAGE"));
    snprintf(webui->lang, 3,"%s",webui->lang_full);

    memset(webui->hostname,'\0',WEBUI_LEN_PARM);
    memset(webui->resp_page,'\0',webui->resp_size);

    return;
}

static void webu_context_null(struct webui_ctx *webui) {
    /* Null out all the pointers in our webui context */
    webui->url           = NULL;
    webui->hostname      = NULL;
    webui->uri_camid     = NULL;
    webui->uri_cmd1      = NULL;
    webui->uri_cmd2      = NULL;
    webui->uri_parm1     = NULL;
    webui->uri_value1    = NULL;
    webui->uri_parm2     = NULL;
    webui->uri_value2    = NULL;
    webui->lang          = NULL;
    webui->lang_full     = NULL;
    webui->resp_page     = NULL;
    webui->connection    = NULL;
    webui->auth_user     = NULL;
    webui->auth_pass     = NULL;
    webui->auth_denied   = NULL;
    webui->auth_opaque   = NULL;
    webui->auth_realm    = NULL;
    webui->clientip      = NULL;
    webui->text_eol      = NULL;

    return;
}

static void webu_context_free(struct webui_ctx *webui) {

    if (webui->hostname      != NULL) free(webui->hostname);
    if (webui->url           != NULL) free(webui->url);
    if (webui->uri_camid     != NULL) free(webui->uri_camid);
    if (webui->uri_cmd1      != NULL) free(webui->uri_cmd1);
    if (webui->uri_cmd2      != NULL) free(webui->uri_cmd2);
    if (webui->uri_parm1     != NULL) free(webui->uri_parm1);
    if (webui->uri_value1    != NULL) free(webui->uri_value1);
    if (webui->uri_parm2     != NULL) free(webui->uri_parm2);
    if (webui->uri_value2    != NULL) free(webui->uri_value2);
    if (webui->lang          != NULL) free(webui->lang);
    if (webui->lang_full     != NULL) free(webui->lang_full);
    if (webui->resp_page     != NULL) free(webui->resp_page);
    if (webui->auth_user     != NULL) free(webui->auth_user);
    if (webui->auth_pass     != NULL) free(webui->auth_pass);
    if (webui->auth_denied   != NULL) free(webui->auth_denied);
    if (webui->auth_opaque   != NULL) free(webui->auth_opaque);
    if (webui->auth_realm    != NULL) free(webui->auth_realm);
    if (webui->clientip      != NULL) free(webui->clientip);
    if (webui->text_eol      != NULL) free(webui->text_eol);

    webu_context_null(webui);

    free(webui);

    return;
}

static void webu_badreq(struct webui_ctx *webui){
    /* This function is used in this webu module as a central function when there is a bad
     * request.  Since sometimes we will be unable to determine what camera context (stream
     * or camera) originated the request and we have NULL for cntlist and cnt, we default the
     * response to be HTML.  Otherwise, we do know the type and we send back to the user the
     * bad request response either with or without the HTML tags.
     */
    if (webui->cnt != NULL) {
        if (webui->cnt->conf.webcontrol_interface == 1){
            webu_text_badreq(webui);
        } else {
            webu_html_badreq(webui);
        }
    } else if (webui->cntlst != NULL) {
        if (webui->cntlst[0]->conf.webcontrol_interface == 1){
            webu_text_badreq(webui);
        } else {
            webu_html_badreq(webui);
        }
    } else {
        webu_html_badreq(webui);
    }
}

void webu_write(struct webui_ctx *webui, const char *buf) {
    /* Copy the buf data to our response buffer.  If the response buffer is not large enough to
     * accept our new data coming in, then expand it in chunks of 10
     */
    int      resp_len;
    char    *temp_resp;
    size_t   temp_size;

    resp_len = strlen(buf);

    temp_size = webui->resp_size;
    while ((resp_len + webui->resp_used) > temp_size){
        temp_size = temp_size + (WEBUI_LEN_RESP * 10);
    }

    if (temp_size > webui->resp_size){
        temp_resp = mymalloc(webui->resp_size);
        memcpy(temp_resp, webui->resp_page, webui->resp_size);
        free(webui->resp_page);
        webui->resp_page = mymalloc(temp_size);
        memset(webui->resp_page,'\0',temp_size);
        memcpy(webui->resp_page, temp_resp, webui->resp_size);
        webui->resp_size = temp_size;
        free(temp_resp);
    }

    memcpy(webui->resp_page + webui->resp_used, buf, resp_len);
    webui->resp_used = webui->resp_used + resp_len;

    return;
}

static int webu_url_decode(char *urlencoded, size_t length) {
    /* We are sent a URI encoded string and this decodes it to characters
     * If the sent URL that isn't valid, then we clear out the URL
     * so it is not processed in further functions.  The "answer" functions
     * look for empty urls and answer with bad request
     */
    char *data = urlencoded;
    char *urldecoded = urlencoded;
    int scan_rslt;
    size_t origlen;

    origlen = length;

    if (urlencoded[0] != '/'){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid url: %s"),urlencoded);
        memset(urlencoded,'\0',origlen);
        return -1;
    }

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
            if (scan_rslt < 1){
                MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error decoding url"));
                memset(urlencoded,'\0',origlen);
                return -1;
            }

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

    return 0;

}

static void webu_parms_edit(struct webui_ctx *webui) {

    /* Determine the thread number provided.
     * If no thread provided, assign it to -1
     * Samples:
     * http://localhost:8081/0/stream (cntlist will be populated and this function will set cnt)
     * http://localhost:8081/stream (cntlist will be null, cnt will be populated)
     * http://localhost:8081/   (cntlist will be null, cnt will be populated)
     */
    int indx, is_nbr;

    if (strlen(webui->uri_camid) > 0){
        is_nbr = TRUE;
        for (indx=0; indx < (int)strlen(webui->uri_camid);indx++){
            if ((webui->uri_camid[indx] > '9') || (webui->uri_camid[indx] < '0')) is_nbr = FALSE;
        }
        if (is_nbr){
            webui->thread_nbr = atoi(webui->uri_camid);
        } else {
            webui->thread_nbr = -1;
        }
    } else {
        webui->thread_nbr = -1;
    }

    /* Set the single context pointer to thread we are answering
     * If the connection is for a single stream (legacy method of a port
     * per stream), then the cntlist will be null and the camera context
     * will already be assigned into webui->cnt.  This is part of the
     * init function which is called for MHD and it has the different
     * variations depending upon how the port and cameras were specified.
     * Also set/convert the camid into the thread number.
    */

    if (webui->cntlst != NULL){
        if (webui->thread_nbr < 0){
            webui->cnt = webui->cntlst[0];
            webui->thread_nbr = 0;
        } else {
            indx = 0;
            while (webui->cntlst[indx] != NULL){
                if (webui->cntlst[indx]->camera_id == webui->thread_nbr){
                    webui->thread_nbr = indx;
                    break;
                }
                indx++;
            }
            /* This may be null, in which case we will not answer the request */
            webui->cnt = webui->cntlst[indx];
        }
    }
}

static void webu_parseurl_parms(struct webui_ctx *webui, char *st_pos) {

    /* Parse the parameters of the URI
     * Earlier functions have assigned the st_pos to the slash after the action and it is
     * pointing at the set/get when this function is invoked.
     * Samples (MHD takes off the IP:port)
     * /{camid}/config/set?{parm}={value1}
     * /{camid}/config/get?query={parm}
     * /{camid}/track/set?x={value1}&y={value2}
     * /{camid}/track/set?pan={value1}&tilt={value2}
     * /{camid}/{cmd1}/{cmd2}?{parm1}={value1}&{parm2}={value2}
     */

    int parm_len, last_parm;
    char *en_pos;


    /* First parse out the "set","get","pan","tilt","x","y"
     * from the uri and put them into the cmd2.
     * st_pos is at the beginning of the command
     * If there is no ? then we are done parsing
     * Note that each section is looking for a different
     * delimitter.  (?, =, &, =, &)
     */
    last_parm = FALSE;
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
            last_parm = TRUE;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return;
        snprintf(webui->uri_parm1, parm_len,"%s", st_pos);

        if (!last_parm){
            /* Get the parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            if (!strcasecmp(webui->uri_parm1,"x") || !strcasecmp(webui->uri_parm1,"pan") ){
                en_pos = strstr(st_pos,"&");
            } else {
                en_pos = NULL;
            }
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value1, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter name */
            st_pos = st_pos + parm_len; /* Move past the previous command */
            en_pos = strstr(st_pos,"=");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_parm2, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter value */
            st_pos = st_pos + parm_len;     /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value2, parm_len,"%s", st_pos);
        }

    }

}

static void webu_parseurl_reset(struct webui_ctx *webui) {

    /* Reset the variables to empty strings*/

    memset(webui->uri_camid,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value2,'\0',WEBUI_LEN_PARM);

}

static int webu_parseurl(struct webui_ctx *webui) {
    /* Parse the sent URI into the commands and parameters
     * so we can check the resulting strings in later functions
     * and determine what actions to take.
     * Samples
     * /
     * /{camid}
     * /{camid}/config/set?log_level=6
     * /{camid}/config/set?{parm}={value1}
     * /{camid}/config/get?query={parm}
     * /{camid}/track/set?x={value1}&y={value2}
     * /{camid}/track/set?pan={value1}&tilt={value2}
     * /{camid}/{cmd1}/{cmd2}?{parm1}={value1}&{parm2}={value2}
     */

    int retcd, parm_len, last_slash;
    char *st_pos, *en_pos;

    retcd = 0;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Sent url: %s"),webui->url);

    webu_parseurl_reset(webui);

    if (strlen(webui->url) == 0) return -1;

    retcd = webu_url_decode(webui->url, strlen(webui->url));
    if (retcd != 0) return retcd;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),webui->url);

    /* Home page */
    if (strlen(webui->url) == 1) return 0;

    last_slash = 0;

    /* Get the camid number and which sometimes this will contain an action if the user
     * is setting the port for a particular camera and requests the
     * stream by using http://localhost:port/stream
     */
    st_pos = webui->url + 1; /* Move past the first "/" */
    if (*st_pos == '-') return -1; /* Never allow a negative number */
    en_pos = strstr(st_pos,"/");
    if (en_pos == NULL){
        parm_len = strlen(webui->url);
        last_slash = 1;
    } else {
        parm_len = en_pos - st_pos + 1;
    }
    if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
    snprintf(webui->uri_camid, parm_len,"%s", st_pos);

    if (!last_slash){
        /* Get cmd1 or action */
        st_pos = st_pos + parm_len; /* Move past the camid */
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
       "camid: >%s< cmd1: >%s< cmd2: >%s< parm1:>%s< val1:>%s< parm2:>%s< val2:>%s<"
               ,webui->uri_camid
               ,webui->uri_cmd1, webui->uri_cmd2
               ,webui->uri_parm1, webui->uri_value1
               ,webui->uri_parm2, webui->uri_value2);


    return retcd;

}

void webu_process_action(struct webui_ctx *webui) {
    /* Process the actions from the webcontrol that the user requested.  This is used
     * for both the html and text interface.  The text interface just adds a additional
     * response whereas the html just performs the action
     */
    int indx;

    indx = 0;
    if ((strcmp(webui->uri_cmd2,"makemovie") == 0) ||
        (strcmp(webui->uri_cmd2,"eventend") == 0)) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx]){
                webui->cntlst[indx]->event_stop = TRUE;
            }
        } else {
            webui->cnt->event_stop = TRUE;
        }

    } else if (strcmp(webui->uri_cmd2,"eventstart") == 0){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx]){
                webui->cntlst[indx]->event_user = TRUE;
            }
        } else {
            webui->cnt->event_user = TRUE;
        }

    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx])
            webui->cntlst[indx]->snapshot = 1;
        } else {
            webui->cnt->snapshot = 1;
        }


    } else if (!strcmp(webui->uri_cmd2,"restart")){
        if (webui->thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all threads"));
            webui->cntlst[0]->webcontrol_finish = TRUE;
            kill(getpid(),SIGHUP);
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Restarting thread %d"),webui->thread_nbr);
            webui->cnt->restart = TRUE;
            if (webui->cnt->running) {
                webui->cnt->event_stop = TRUE;
                webui->cnt->finish = TRUE;
            }

        }

    } else if (!strcmp(webui->uri_cmd2,"quit")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx]){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                    _("Quitting thread %d"),webui->thread_nbr);
                webui->cntlst[indx]->restart = FALSE;
                webui->cntlst[indx]->event_stop = TRUE;
                webui->cntlst[indx]->event_user = TRUE;
                webui->cntlst[indx]->finish = TRUE;
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Quitting thread %d"),webui->thread_nbr);
            webui->cnt->restart = FALSE;
            webui->cnt->event_stop = TRUE;
            webui->cnt->event_user = TRUE;
            webui->cnt->finish = TRUE;
        }

    } else if (!strcmp(webui->uri_cmd2,"end")){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Motion terminating"));
            while (webui->cntlst[indx]){
                webui->cntlst[indx]->webcontrol_finish = TRUE;
                webui->cntlst[indx]->restart = FALSE;
                webui->cntlst[indx]->event_stop = TRUE;
                webui->cntlst[indx]->event_user = TRUE;
                webui->cntlst[indx]->finish = TRUE;
                indx++;
            }


    } else if (!strcmp(webui->uri_cmd2,"start")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cntlst[indx]->pause = 0;
            } while (webui->cntlst[++indx]);
        } else {
            webui->cnt->pause = 0;
        }
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cntlst[indx]->pause = 1;
            } while (webui->cntlst[++indx]);
        } else {
            webui->cnt->pause = 1;
        }

    } else if (!strcmp(webui->uri_cmd2,"connection")){
        webu_text_connection(webui);

    } else if (!strcmp(webui->uri_cmd2,"status")){
        webu_text_status(webui);

    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        conf_print(webui->cntlst);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            , webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        return;
    }
}

static int webu_process_config_set(struct webui_ctx *webui) {
    /* Process the request to change the configuration parameters.  Used
     * both the html and text interfaces.  If the parameter was found, then
     * we return 0 otherwise a -1 to tell the calling function whether it
     * was a valid parm to change.
     */
    int indx, retcd;
    char temp_name[WEBUI_LEN_PARM];

    /* Search through the depreciated parms and if applicable,
     * get the new parameter name so we can check its webcontrol_parms level
     */
    snprintf(temp_name, WEBUI_LEN_PARM, "%s", webui->uri_parm1);
    indx=0;
    while (dep_config_params[indx].name != NULL) {
        if (strcmp(dep_config_params[indx].name,webui->uri_parm1) == 0){
            snprintf(temp_name, WEBUI_LEN_PARM, "%s", dep_config_params[indx].newname);
            break;
        }
        indx++;
    }
    /* Ignore any request to change an option that is designated above the
     * webcontrol_parms level.
     */
    indx=0;
    while (config_params[indx].param_name != NULL) {
        if (((webui->thread_nbr != 0) && (config_params[indx].main_thread)) ||
            (config_params[indx].webui_level > webui->cntlst[0]->conf.webcontrol_parms) ||
            (config_params[indx].webui_level == WEBUI_LEVEL_NEVER) ) {
            indx++;
            continue;
        }
        if (!strcmp(temp_name, config_params[indx].param_name)) break;
        indx++;
    }
    /* If we found the parm, assign it.  If the loop above did not find the parm
     * then we ignore the request
     */
    if (config_params[indx].param_name != NULL){
        if (strlen(webui->uri_parm1) > 0){
            /* This is legacy assumption on the pointers being sequential
             * We send in the original parm name so it will trigger the depreciated warnings
             * and perform any required transformations from old parm to new parm
             */
            conf_cmdparse(webui->cntlst + webui->thread_nbr
                , webui->uri_parm1, webui->uri_value1);

            /*If we are updating vid parms, set the flag to update the device.*/
            if (!strcmp(config_params[indx].param_name, "vid_control_params") &&
                (webui->cntlst[webui->thread_nbr]->vdev != NULL)){
                webui->cntlst[webui->thread_nbr]->vdev->update_parms = TRUE;
            }

            /* If changing language, do it now */
            if (!strcmp(config_params[indx].param_name, "native_language")){
                nls_enabled = webui->cntlst[webui->thread_nbr]->conf.native_language;
                if (nls_enabled){
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : on"));
                } else {
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : off"));
                }
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,_("Set the value to null/zero"));
        }
        retcd = 0;
    } else {
        retcd = -1;
    }

    return retcd;

}

int webu_process_config(struct webui_ctx *webui) {

    int retcd;

    retcd = 0;

    if ((!strcmp(webui->uri_cmd1,"config")) &&
        (!strcmp(webui->uri_cmd2,"set"))) {
        retcd = webu_process_config_set(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"get"))) {
        webu_text_get_query(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"list"))) {
        webu_text_list(webui);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            , webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);

    }

    return retcd;

}

int webu_process_track(struct webui_ctx *webui) {
    /* Call the tracking move functions as requested */
    struct coord cent;
    int retcd;

    if (!strcmp(webui->uri_cmd2, "center")) {
        webui->cntlst[webui->thread_nbr]->moved = track_center(webui->cntlst[webui->thread_nbr], 0, 1, 0, 0);
        retcd = 0;
    } else if (!strcmp(webui->uri_cmd2, "set")) {
        if (!strcmp(webui->uri_parm1, "pan")) {
            cent.width = webui->cntlst[webui->thread_nbr]->imgs.width;
            cent.height = webui->cntlst[webui->thread_nbr]->imgs.height;
            cent.x = atoi(webui->uri_value1);
            cent.y = 0;
            webui->cntlst[webui->thread_nbr]->moved = track_move(webui->cntlst[webui->thread_nbr]
                ,webui->cntlst[webui->thread_nbr]->video_dev
                ,&cent, &webui->cntlst[webui->thread_nbr]->imgs, 1);

            cent.width = webui->cntlst[webui->thread_nbr]->imgs.width;
            cent.height = webui->cntlst[webui->thread_nbr]->imgs.height;
            cent.x = 0;
            cent.y = atoi(webui->uri_value2);
            webui->cntlst[webui->thread_nbr]->moved = track_move(webui->cntlst[webui->thread_nbr]
                ,webui->cntlst[webui->thread_nbr]->video_dev
                ,&cent, &webui->cntlst[webui->thread_nbr]->imgs, 1);
            retcd = 0;
        } else if (!strcasecmp(webui->uri_parm1, "x")) {
            webui->cntlst[webui->thread_nbr]->moved = track_center(webui->cntlst[webui->thread_nbr]
                , webui->cntlst[webui->thread_nbr]->video_dev, 1
                , atoi(webui->uri_value1), atoi(webui->uri_value2));
            retcd = 0;
        } else {
            retcd = -1;
        }
    } else {
        retcd = -1;
    }

    return retcd;

}

static void webu_clientip(struct webui_ctx *webui) {
    /* Extract the IP of the client that is connecting.  When the
     * user specifies Motion to use IPV6 and a IPV4 address comes to us
     * the IPv4 address is prepended with a ::ffff: We then trim that off
     * so we don't confuse our users.
     */
    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ip_dst;
    struct sockaddr_in6 *con_socket6;
    struct sockaddr_in *con_socket4;
    int is_ipv6;

    is_ipv6 = FALSE;
    if (webui->cnt != NULL ){
        if (webui->cnt->conf.webcontrol_ipv6) is_ipv6 = TRUE;
    } else {
        if (webui->cntlst[0]->conf.webcontrol_ipv6) is_ipv6 = TRUE;
    }

    con_info = MHD_get_connection_info(webui->connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (is_ipv6){
        con_socket6 = (struct sockaddr_in6 *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET6, &con_socket6->sin6_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL){
            snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", "Unknown");
        } else {
            if (strncmp(client,"::ffff:",7) == 0){
                snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", client + 7);
            } else {
                snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", client);
            }
        }
    } else {
        con_socket4 = (struct sockaddr_in *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET, &con_socket4->sin_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL){
            snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", "Unknown");
        } else {
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client);
        }
    }
    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),webui->clientip);

}

static void webu_hostname(struct webui_ctx *webui, int ctrl) {

    /* use the hostname the browser used to connect to us when
     * constructing links to the stream ports. If available
     * (which it is in all modern browsers) it is more likely to
     * work than the result of gethostname(), which is reliant on
     * the machine we're running on having it's hostname setup
     * correctly and corresponding DNS in place. */

    const char *hdr;
    char *en_pos;
    int host_len;

    hdr = MHD_lookup_connection_value (webui->connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_HOST);
    if (hdr != NULL){
        snprintf(webui->hostname, WEBUI_LEN_PARM, "%s", hdr);
        /* IPv6 addresses have :'s in them so special case them */
        if (webui->hostname[0] == '['){
            en_pos = strstr(webui->hostname, "]");
            if (en_pos != NULL){
                host_len = en_pos - webui->hostname + 2;
                snprintf(webui->hostname, host_len, "%s", hdr);
            }
        } else {
            en_pos = strstr(webui->hostname, ":");
            if (en_pos != NULL){
                host_len = en_pos - webui->hostname + 1;
                snprintf(webui->hostname, host_len, "%s", hdr);
            }
        }
    } else {
        gethostname(webui->hostname, WEBUI_LEN_PARM - 1);
    }

    /* Assign the type of protocol that is associated with the host
     * so we can use this protocol as we are building the html page or
     * streams.
     */
    if (ctrl){
        if (webui->cnt->conf.webcontrol_tls){
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    } else {
        if (webui->cnt->conf.stream_tls){
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    }

    return;
}

static int webu_mhd_digest_fail(struct webui_ctx *webui,int signal_stale) {
    /* Create a denied response to user*/
    struct MHD_Response *response;
    int retcd;

    webui->authenticated = FALSE;

    response = MHD_create_response_from_buffer(strlen(webui->auth_denied)
        ,(void *)webui->auth_denied, MHD_RESPMEM_PERSISTENT);

    if (response == NULL) return MHD_NO;

    retcd = MHD_queue_auth_fail_response(webui->connection, webui->auth_realm
        ,webui->auth_opaque, response
        ,(signal_stale == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);

    MHD_destroy_response(response);

    return retcd;
}

static int webu_mhd_digest(struct webui_ctx *webui) {
    /* Perform the digest authentication.  This function gets called a couple of
     * times by MHD during the authentication process.
     */
    int retcd;
    char *user;

    /*Get username or prompt for a user/pass */
    user = MHD_digest_auth_get_username(webui->connection);
    if (user == NULL) {
        return webu_mhd_digest_fail(webui, MHD_NO);
    }

    /* Check for valid user name */
    if (strcmp(user, webui->auth_user) != 0){
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), webui->clientip);
        if (user != NULL) free(user);
        return webu_mhd_digest_fail(webui, MHD_NO);
    }
    if (user != NULL) free(user);

    /* Check the password as well*/
    retcd = MHD_digest_auth_check(webui->connection, webui->auth_realm
        , webui->auth_user, webui->auth_pass, 300);

    if (retcd == MHD_NO) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), webui->clientip);
    }

    if ( (retcd == MHD_INVALID_NONCE) || (retcd == MHD_NO) )  {
        return webu_mhd_digest_fail(webui, retcd);
    }

    webui->authenticated = TRUE;
    return MHD_YES;

}

static int webu_mhd_basic_fail(struct webui_ctx *webui) {
    /* Create a denied response to user*/
    struct MHD_Response *response;
    int retcd;

    webui->authenticated = FALSE;

    response = MHD_create_response_from_buffer(strlen(webui->auth_denied)
        ,(void *)webui->auth_denied, MHD_RESPMEM_PERSISTENT);

    if (response == NULL) return MHD_NO;

    retcd = MHD_queue_basic_auth_fail_response (webui->connection, webui->auth_realm, response);

    MHD_destroy_response(response);

    return retcd;

}

static int webu_mhd_basic(struct webui_ctx *webui) {
    /* Perform Basic Authentication.  */
    char *user, *pass;

    pass = NULL;
    user = NULL;

    user = MHD_basic_auth_get_username_password (webui->connection, &pass);
    if ((user == NULL) || (pass == NULL)){
        if (user != NULL) free(user);
        if (pass != NULL) free(pass);
        return webu_mhd_basic_fail(webui);
    }

    if ((strcmp(user, webui->auth_user) != 0) || (strcmp(pass, webui->auth_pass) != 0)) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"),webui->clientip);
        if (user != NULL) free(user);
        if (pass != NULL) free(pass);

        return webu_mhd_basic_fail(webui);
    }

    if (user != NULL) free(user);
    if (pass != NULL) free(pass);

    webui->authenticated = TRUE;
    return MHD_YES;

}

static void webu_mhd_auth_parse(struct webui_ctx *webui, int ctrl){
    int auth_len;
    char *col_pos;

    /* Parse apart the user:pass provided*/
    if (webui->auth_user != NULL) free(webui->auth_user);
    if (webui->auth_pass != NULL) free(webui->auth_pass);
    webui->auth_user = NULL;
    webui->auth_pass = NULL;

    if (ctrl){
        auth_len = strlen(webui->cnt->conf.webcontrol_authentication);
        col_pos = strstr(webui->cnt->conf.webcontrol_authentication,":");
        if (col_pos == NULL){
            webui->auth_user = mymalloc(auth_len+1);
            webui->auth_pass = mymalloc(2);
            snprintf(webui->auth_user, auth_len + 1, "%s"
                ,webui->cnt->conf.webcontrol_authentication);
            snprintf(webui->auth_pass, 2, "%s","");
        } else {
            webui->auth_user = mymalloc(auth_len - strlen(col_pos) + 1);
            webui->auth_pass = mymalloc(strlen(col_pos));
            snprintf(webui->auth_user, auth_len - strlen(col_pos) + 1, "%s"
                ,webui->cnt->conf.webcontrol_authentication);
            snprintf(webui->auth_pass, strlen(col_pos), "%s", col_pos + 1);
        }
    } else {
        auth_len = strlen(webui->cnt->conf.stream_authentication);
        col_pos = strstr(webui->cnt->conf.stream_authentication,":");
        if (col_pos == NULL){
            webui->auth_user = mymalloc(auth_len+1);
            webui->auth_pass = mymalloc(2);
            snprintf(webui->auth_user, auth_len + 1, "%s"
                ,webui->cnt->conf.stream_authentication);
            snprintf(webui->auth_pass, 2, "%s","");
        } else {
            webui->auth_user = mymalloc(auth_len - strlen(col_pos) + 1);
            webui->auth_pass = mymalloc(strlen(col_pos));
            snprintf(webui->auth_user, auth_len - strlen(col_pos) + 1, "%s"
                ,webui->cnt->conf.stream_authentication);
            snprintf(webui->auth_pass, strlen(col_pos), "%s", col_pos + 1);
        }
    }

}

static int webu_mhd_auth(struct webui_ctx *webui, int ctrl){

    /* Set everything up for calling the authentication functions */
    unsigned int rand1,rand2;

    snprintf(webui->auth_denied, WEBUI_LEN_RESP, "%s"
        ,"<html><head><title>Access denied</title>"
        "</head><body>Access denied</body></html>");

    srand(time(NULL));
    rand1 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    rand2 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(webui->auth_opaque, WEBUI_LEN_PARM, "%08x%08x", rand1, rand2);

    snprintf(webui->auth_realm, WEBUI_LEN_PARM, "%s","Motion");

    if (ctrl){
        /* Authentication for the webcontrol*/
        if (webui->cnt->conf.webcontrol_authentication == NULL){
            webui->authenticated = TRUE;
            if (webui->cnt->conf.webcontrol_auth_method != 0){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No webcontrol user:pass provided"));
            }
            return MHD_YES;
        }

        if (webui->auth_user == NULL) webu_mhd_auth_parse(webui, ctrl);

        if (webui->cnt->conf.webcontrol_auth_method == 1){
            return webu_mhd_basic(webui);
        } else if (webui->cnt->conf.webcontrol_auth_method == 2){
            return webu_mhd_digest(webui);
        }

    } else {
        /* Authentication for the streams */
        if (webui->cnt->conf.stream_authentication == NULL){
            webui->authenticated = TRUE;
            if (webui->cnt->conf.stream_auth_method != 0){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No stream user:pass provided"));
            }
            return MHD_YES;
        }

        if (webui->auth_user == NULL) webu_mhd_auth_parse(webui, ctrl);

        if (webui->cnt->conf.stream_auth_method == 1) {
            return webu_mhd_basic(webui);
        } else if (webui->cnt->conf.stream_auth_method == 2){
            return webu_mhd_digest(webui);
        }
    }

    webui->authenticated = TRUE;
    return MHD_YES;

}

static int webu_mhd_send(struct webui_ctx *webui, int ctrl) {
    /* Send the response that we created back to the user.  Now if the user
     * provided a really bad URL, then we couldn't determine which Motion context
     * they were wanting.  In this situation, we have a webui->cnt = NULL and we
     * don't know whether it came from a html or text request.  In this situation
     * we use the MHD defaults and skip adding CORS/Content type.  (There isn't any
     * Motion context so we can't tell where to look)
     * The ctrl parameter is a boolean which just says whether the request is for
     * the webcontrol versus stream
     */
    int retcd;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen(webui->resp_page)
        ,(void *)webui->resp_page, MHD_RESPMEM_PERSISTENT);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->cnt != NULL){
        if (ctrl){
            if (webui->cnt->conf.webcontrol_cors_header != NULL){
                MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
                    , webui->cnt->conf.webcontrol_cors_header);
            }
            if (webui->cnt->conf.webcontrol_interface == 1){
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
            } else {
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
            }
        } else {
            if (webui->cnt->conf.stream_cors_header != NULL){
                MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
                    , webui->cnt->conf.stream_cors_header);
            }
            MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
        }
    }

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

static void webu_answer_strm_type(struct webui_ctx *webui) {
    /* Assign the type of stream that is being answered*/

    if ((strcmp(webui->uri_cmd1,"stream") == 0) ||
        (strcmp(webui->uri_camid,"stream") == 0) ||
        (strlen(webui->uri_camid) == 0)) {
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else if ((strcmp(webui->uri_cmd1,"substream") == 0) ||
        (strcmp(webui->uri_camid,"substream") == 0)){
        webui->cnct_type = WEBUI_CNCT_SUB;

    } else if ((strcmp(webui->uri_cmd1,"motion") == 0) ||
        (strcmp(webui->uri_camid,"motion") == 0)){
        webui->cnct_type = WEBUI_CNCT_MOTION;

    } else if ((strcmp(webui->uri_cmd1,"source") == 0) ||
        (strcmp(webui->uri_camid,"source") == 0)){
        webui->cnct_type = WEBUI_CNCT_SOURCE;

    } else if ((strcmp(webui->uri_cmd1,"current") == 0) ||
        (strcmp(webui->uri_camid,"current") == 0)){
        webui->cnct_type = WEBUI_CNCT_STATIC;

    } else if ((strlen(webui->uri_camid) > 0) &&
        (strlen(webui->uri_cmd1) == 0)){
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else {
        webui->cnct_type = WEBUI_CNCT_UNKNOWN;
    }

}

static int webu_answer_ctrl(void *cls
        , struct MHD_Connection *connection
        , const char *url
        , const char *method
        , const char *version
        , const char *upload_data
        , size_t     *upload_data_size
        , void **ptr) {

    /* This function "answers" the request for a webcontrol.*/
    int retcd;
    struct webui_ctx *webui = *ptr;

    /* Eliminate compiler warnings */
    (void)cls;
    (void)url;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;

    /* Per MHD docs, this is called twice and we should process the second call */
    if (webui->mhd_first) {
        webui->mhd_first = FALSE;
        return MHD_YES;
    }

    if (strcmp (method, "GET") != 0){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Invalid Method requested: %s"),method);
        return MHD_NO;
    }

    webui->cnct_type = WEBUI_CNCT_CONTROL;

    util_threadname_set("wu", 0,NULL);

    webui->connection = connection;

    /* Throw bad URLS back to user*/
    if ((webui->cnt ==  NULL) || (strlen(webui->url) == 0)){
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
        return retcd;
    }

    if (webui->cnt->webcontrol_finish) return MHD_NO;

    if (strlen(webui->clientip) == 0){
        webu_clientip(webui);
    }

    webu_hostname(webui, TRUE);

    if (!webui->authenticated) {
        retcd = webu_mhd_auth(webui, TRUE);
        if (!webui->authenticated) return retcd;
    }

    if ((webui->cntlst[0]->conf.webcontrol_interface == 1) ||
        (webui->cntlst[0]->conf.webcontrol_interface == 2)) {
        webu_text_main(webui);
    } else {
        webu_html_main(webui);
    }

    retcd = webu_mhd_send(webui, TRUE);
    if (retcd == MHD_NO){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed %d"),retcd);
    }
    return retcd;

}

static int webu_answer_strm(void *cls
        , struct MHD_Connection *connection
        , const char *url
        , const char *method
        , const char *version
        , const char *upload_data
        , size_t     *upload_data_size
        , void **ptr) {

    /* Answer the request for all the streams*/
    int retcd;
    struct webui_ctx *webui = *ptr;

    /* Eliminate compiler warnings */
    (void)cls;
    (void)url;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;

    /* Per docs, this is called twice and we should process the second call */
    if (webui->mhd_first) {
        webui->mhd_first = FALSE;
        return MHD_YES;
    }

    if (strcmp (method, "GET") != 0){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Invalid Method requested: %s"),method);
        return MHD_NO;
    }

    util_threadname_set("st", 0,NULL);

    webui->connection = connection;

    /* Throw bad URLS back to user*/
    if ((webui->cnt ==  NULL) || (strlen(webui->url) == 0)){
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
        return retcd;
    }

    /* Do not answer a request until the motion loop has completed at least once */
    if (webui->cnt->passflag == 0) return MHD_NO;

    if (webui->cnt->webcontrol_finish) return MHD_NO;

    if (strlen(webui->clientip) == 0){
        webu_clientip(webui);
    }

    webu_hostname(webui, FALSE);

    if (!webui->authenticated) {
        retcd = webu_mhd_auth(webui, FALSE);
        if (!webui->authenticated) return retcd;
    }

    webu_answer_strm_type(webui);

    retcd = 0;
    if (webui->cnct_type == WEBUI_CNCT_STATIC){
        retcd = webu_stream_static(webui);
        if (retcd == MHD_NO){
            webu_badreq(webui);
            retcd = webu_mhd_send(webui, FALSE);
        }
    } else if (webui->cnct_type != WEBUI_CNCT_UNKNOWN) {
        retcd = webu_stream_mjpeg(webui);
        if (retcd == MHD_NO){
            webu_badreq(webui);
            retcd = webu_mhd_send(webui, FALSE);
        }
    } else {
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
    }

    if (retcd == MHD_NO){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed %d"),retcd);
    }
    return retcd;

}

static void *webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection) {
    /* This is called at the very start of getting a request before the "answer"
     * is processed.  There are two variations of this and the difference is how
     * we call the webu_context_init.  When we are processing for the webcontrol or
     * the stream port specified in the motion.conf file, we pass into the init function
     * the full list of all the cameras.  The other version of the init is used when the
     * user specifies a unique port for each camera.  In this situation, the full list
     * context is passed in as a null and the context of the camera desired is passed
     * instead.
     * When this function is processed, we basically only have the URL that the user requested
     * so we initialize everything and then parse out the URL to determine what the user is
     * asking.
     */

    struct context **cnt = cls;
    struct webui_ctx *webui;
    int retcd;

    (void)connection;

    /* Set the thread name to connection until we know whether control or stream answers*/
    util_threadname_set("cn", 0,NULL);

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(cnt, NULL, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    retcd = webu_parseurl(webui);
    if (retcd != 0){
        webu_parseurl_reset(webui);
        memset(webui->url,'\0',WEBUI_LEN_URLI);
    }

    webu_parms_edit(webui);

    return webui;
}

static void *webu_mhd_init_one(void *cls, const char *uri, struct MHD_Connection *connection) {
    /* This function initializes all the webui variables as we are getting a request.  This
     * variation of the init is the one used when the user has specified a unique port number
     * for each camera.  The variation is in how the webu_context_init is invoked.  This passes
     * in a NULL for the full context list (webui->cntlist) and instead assigns the particular
     * camera context to webui->cnt
     */
    struct context *cnt = cls;
    struct webui_ctx *webui;
    int retcd;

    (void)connection;

    /* Set the thread name to connection until we know whether control or stream answers*/
    util_threadname_set("cn", 0,NULL);

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(NULL, cnt, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    retcd = webu_parseurl(webui);
    if (retcd != 0){
        webu_parseurl_reset(webui);
        memset(webui->url,'\0',WEBUI_LEN_URLI);
    }

    webu_parms_edit(webui);

    return webui;
}

static void webu_mhd_deinit(void *cls
    , struct MHD_Connection *connection
    , void **con_cls
    , enum MHD_RequestTerminationCode toe) {
    /* This is the function called as the connection is closed so we free our webui variables*/
    struct webui_ctx *webui = *con_cls;

    /* Eliminate compiler warnings */
    (void)connection;
    (void)cls;
    (void)toe;

    if (webui->cnct_type == WEBUI_CNCT_FULL ){
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_norm.cnct_count--;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_SUB ){
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_sub.cnct_count--;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION ){
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_motion.cnct_count--;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE ){
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_source.cnct_count--;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    } else if (webui->cnct_type == WEBUI_CNCT_STATIC ){
        pthread_mutex_lock(&webui->cnt->mutex_stream);
            webui->cnt->stream_norm.cnct_count--;
        pthread_mutex_unlock(&webui->cnt->mutex_stream);

    }

    webu_context_free(webui);

    return;
}

static void webu_mhd_features_basic(struct mhdstart_ctx *mhdst){
    /* Use the MHD function to see what features it supports*/
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        int retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
        if (retcd == MHD_YES){
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: available"));
        } else {
            if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method == 1)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method = 0;
            } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method == 1)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
            }
        }
    #endif
}

static void webu_mhd_features_digest(struct mhdstart_ctx *mhdst){
    /* Use the MHD function to see what features it supports*/
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        int retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
        if (retcd == MHD_YES){
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: available"));
        } else {
            if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method == 2)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method = 0;
            } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method == 2)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
            }
        }
    #endif
}

static void webu_mhd_features_ipv6(struct mhdstart_ctx *mhdst){
    /* Use the MHD function to see what features it supports
     * If we have a really old version of MHD, then we will just support
     * IPv4
     */
    #if MHD_VERSION < 0x00094400
        if (mhdst->ipv6){
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old ipv6 disabled"));
            if (mhdst->ipv6) mhdst->ipv6 = 0;
        }
    #else
        int retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
        if (retcd == MHD_YES){
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("IPV6: available"));
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("IPV6: disabled"));
            if (mhdst->ipv6) mhdst->ipv6 = 0;
        }
    #endif
}

static void webu_mhd_features_tls(struct mhdstart_ctx *mhdst){
    /* Use the MHD function to see what features it supports
     * If we have a really old version of MHD, then we will will not
     * support the ssl/tls request.
     */
    #if MHD_VERSION < 0x00094400
        if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls)){
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls = 0;
        } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_tls)) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->cnt[mhdst->indxthrd]->conf.stream_tls = 0;
        }
    #else
        int retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
        if (retcd == MHD_YES){
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: available"));
        } else {
            if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls = 0;
            } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_tls)){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_tls = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
            }
        }
    #endif
}

static void webu_mhd_features(struct mhdstart_ctx *mhdst){
    /* This function goes through at least a few of the MHD features
     * and adjusts the user parameters from the configuration as
     * needed to reflect what MHD can do
     */

    webu_mhd_features_basic(mhdst);

    webu_mhd_features_digest(mhdst);

    webu_mhd_features_ipv6(mhdst);

    webu_mhd_features_tls(mhdst);

}

static char *webu_mhd_loadfile(const char *fname){
    /* This function loads the requested certificate and key files into memory so we
     * can use them as needed if the user wants ssl/tls support.  If the user did not
     * specify a file in the configuration, then we return NULL.
     */
    FILE *infile;
    size_t file_size, read_size;
    char * file_char;

    if (fname == NULL) {
        file_char = NULL;
    } else {
        infile = fopen(fname, "rb");
        if (infile != NULL){
            fseek(infile, 0, SEEK_END);
            file_size = ftell(infile);
            if (file_size > 0 ){
                file_char = mymalloc(file_size +1);
                fseek(infile, 0, SEEK_SET);
                read_size = fread(file_char, file_size, 1, infile);
                if (read_size > 0 ){
                    file_char[file_size] = 0;
                } else {
                    free(file_char);
                    file_char = NULL;
                    MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                        ,_("Error reading file for SSL/TLS support."));
                }
            } else {
                file_char = NULL;
            }
            fclose(infile);
        } else {
            file_char = NULL;
        }
    }
    return file_char;
}

static void webu_mhd_checktls(struct mhdstart_ctx *mhdst){
    /* This function validates that if the user requested a SSL/TLS connection, then
     * they also need to provide a certificate and key file.  If those are not provided
     * then we revise the configuration request for ssl/tls
     */
    if (mhdst->ctrl){
        if (mhdst->cnt[0]->conf.webcontrol_tls){
            if ((mhdst->cnt[0]->conf.webcontrol_cert == NULL) || (mhdst->tls_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
                mhdst->cnt[0]->conf.webcontrol_tls = 0;
            }
            if ((mhdst->cnt[0]->conf.webcontrol_key == NULL) || (mhdst->tls_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
                mhdst->cnt[0]->conf.webcontrol_tls = 0;
            }
        }
    } else {
        if (mhdst->cnt[mhdst->indxthrd]->conf.stream_tls){
            if ((mhdst->cnt[0]->conf.webcontrol_cert == NULL) || (mhdst->tls_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_tls = 0;
            }
            if ((mhdst->cnt[0]->conf.webcontrol_key == NULL) || (mhdst->tls_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_tls = 0;
            }
        }
    }

}

static void webu_mhd_opts_init(struct mhdstart_ctx *mhdst){
    /* This function sets the init function to use for the MHD connection.  If
     * the connection is related to the webcontrol or the stream specified in the
     * motion.conf file, then we pass in the full context list of all cameras.  If
     * the MHD connection is only going to be for a single camera (a unique port for
     * each camera), then we call a different init function which only wants the single
     * motion context for that particular camera.
     */
    if ((!mhdst->ctrl) && (mhdst->indxthrd != 0)){
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init_one;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->cnt[mhdst->indxthrd];
        mhdst->mhd_opt_nbr++;
    } else {
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->cnt;
        mhdst->mhd_opt_nbr++;
    }

}

static void webu_mhd_opts_deinit(struct mhdstart_ctx *mhdst){
    /* Set the MHD option on the function to call when the connection closes */
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

static void webu_mhd_opts_localhost(struct mhdstart_ctx *mhdst){
    /* Set the MHD option on the acceptable connections.  This is used to handle the
     * motion configuation option of localhost only.
     */

    if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_localhost)){
        if (mhdst->ipv6){
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;

        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    } else if((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_localhost)){
        if (mhdst->ipv6){
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.stream_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;
        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.stream_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    }

}

static void webu_mhd_opts_digest(struct mhdstart_ctx *mhdst){
    /* Set the MHD option for the type of authentication that we will be using.  This
     * function is when we are wanting to use digest authentication
     */

    if (((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method == 2)) ||
        ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method == 2))) {

        if (mhdst->ctrl) {
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->cnt[mhdst->indxthrd]->webcontrol_digest_rand);
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->cnt[mhdst->indxthrd]->webcontrol_digest_rand;
            mhdst->mhd_opt_nbr++;
        } else {
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->cnt[mhdst->indxthrd]->webstream_digest_rand);
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->cnt[mhdst->indxthrd]->webstream_digest_rand;
            mhdst->mhd_opt_nbr++;
        }

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NONCE_NC_SIZE;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 300;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_CONNECTION_TIMEOUT;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (unsigned int) 120;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
        mhdst->mhd_opt_nbr++;
    }

}

static void webu_mhd_opts_tls(struct mhdstart_ctx *mhdst){
    /* Set the MHD options needed when we want TLS connections */
    if ((( mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls)) ||
        ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_tls))) {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_CERT;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->tls_cert;
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_KEY;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->tls_key;
        mhdst->mhd_opt_nbr++;
    }

}

static void webu_mhd_opts(struct mhdstart_ctx *mhdst){
    /* Set all the options we need based upon the motion configuration parameters*/

    mhdst->mhd_opt_nbr = 0;

    webu_mhd_checktls(mhdst);

    webu_mhd_opts_deinit(mhdst);

    webu_mhd_opts_init(mhdst);

    webu_mhd_opts_localhost(mhdst);

    webu_mhd_opts_digest(mhdst);

    webu_mhd_opts_tls(mhdst);

    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_END;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

static void webu_mhd_flags(struct mhdstart_ctx *mhdst){

    /* This sets the MHD startup flags based upon what user put into configuration */
    mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION;

    if (mhdst->ipv6) mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_DUAL_STACK;

    if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_tls)){
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_tls)){
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    }

}

static void webu_start_ctrl(struct context **cnt){
    /* This is the function that actually starts the MHD daemon for handling the webcontrol.
     * There are many options for MHD and they will vary depending upon what our Motion user
     * has requested in the configuration.  There are many functions in this module to assign
     * these options and they are passed in a pointer to the mhdst variable so that they can
     * assign the correct values for MHD start up. Since this function is doing the webcontrol
     * we are only using thread 0 values.
     */

    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    mhdst.tls_cert = webu_mhd_loadfile(cnt[0]->conf.webcontrol_cert);
    mhdst.tls_key  = webu_mhd_loadfile(cnt[0]->conf.webcontrol_key);
    mhdst.ctrl = TRUE;
    mhdst.indxthrd = 0;
    mhdst.cnt = cnt;
    mhdst.ipv6 = cnt[0]->conf.webcontrol_ipv6;

    /* Set the rand number for webcontrol digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(cnt[0]->webcontrol_digest_rand
        ,sizeof(cnt[0]->webcontrol_digest_rand),"%d",randnbr);

    cnt[0]->webcontrol_daemon = NULL;
    if (cnt[0]->conf.webcontrol_port != 0 ){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Starting webcontrol on port %d")
            ,cnt[0]->conf.webcontrol_port);

        mhdst.mhd_ops = malloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
        webu_mhd_features(&mhdst);
        webu_mhd_opts(&mhdst);
        webu_mhd_flags(&mhdst);

        cnt[0]->webcontrol_daemon = MHD_start_daemon (mhdst.mhd_flags
            ,cnt[0]->conf.webcontrol_port
            ,NULL, NULL
            ,&webu_answer_ctrl, cnt
            ,MHD_OPTION_ARRAY, mhdst.mhd_ops
            ,MHD_OPTION_END);
        free(mhdst.mhd_ops);
        if (cnt[0]->webcontrol_daemon == NULL){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD"));
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("Started webcontrol on port %d")
                ,cnt[0]->conf.webcontrol_port);
        }
    }

    if (mhdst.tls_cert != NULL) free(mhdst.tls_cert);
    if (mhdst.tls_key  != NULL) free(mhdst.tls_key);

    return;
}

static void webu_strm_ntc(struct context **cnt, int indxthrd){
    int indx;

    if (indxthrd == 0 ){
        if (cnt[1] != NULL) {
            indx = 1;
            while (cnt[indx] != NULL){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Started camera %d stream on port/camera_id %d/%d")
                    ,cnt[indx]->camera_id
                    ,cnt[indxthrd]->conf.stream_port
                    ,cnt[indx]->camera_id);
                indx++;
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("Started camera %d stream on port %d")
                ,cnt[indxthrd]->camera_id,cnt[indxthrd]->conf.stream_port);
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started camera %d stream on port %d")
            ,cnt[indxthrd]->camera_id,cnt[indxthrd]->conf.stream_port);
    }
}

static void webu_start_strm(struct context **cnt){
    /* This function starts up the daemon for the streams. It loops through
     * all of the camera context's provided and starts streams as requested.  If
     * the thread number is zero, then it starts the full list stream context
     */

    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    mhdst.tls_cert = webu_mhd_loadfile(cnt[0]->conf.webcontrol_cert);
    mhdst.tls_key  = webu_mhd_loadfile(cnt[0]->conf.webcontrol_key);
    mhdst.ctrl = FALSE;
    mhdst.indxthrd = 0;
    mhdst.cnt = cnt;
    mhdst.ipv6 = cnt[0]->conf.webcontrol_ipv6;

    /* Set the rand number for webcontrol digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(cnt[0]->webstream_digest_rand
        ,sizeof(cnt[0]->webstream_digest_rand),"%d",randnbr);

    while (cnt[mhdst.indxthrd] != NULL){
        cnt[mhdst.indxthrd]->webstream_daemon = NULL;
        if (cnt[mhdst.indxthrd]->conf.stream_port != 0 ){
            if (mhdst.indxthrd == 0){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Starting all camera streams on port %d")
                    ,cnt[mhdst.indxthrd]->conf.stream_port);
            } else {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Starting camera %d stream on port %d")
                    ,cnt[mhdst.indxthrd]->camera_id
                    ,cnt[mhdst.indxthrd]->conf.stream_port);
            }

            mhdst.mhd_ops= malloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
            webu_mhd_features(&mhdst);
            webu_mhd_opts(&mhdst);
            webu_mhd_flags(&mhdst);
            if (mhdst.indxthrd == 0){
                cnt[mhdst.indxthrd]->webstream_daemon = MHD_start_daemon (mhdst.mhd_flags
                    ,cnt[mhdst.indxthrd]->conf.stream_port
                    ,NULL, NULL
                    ,&webu_answer_strm, cnt
                    ,MHD_OPTION_ARRAY, mhdst.mhd_ops
                    ,MHD_OPTION_END);
            } else {
                cnt[mhdst.indxthrd]->webstream_daemon = MHD_start_daemon (mhdst.mhd_flags
                    ,cnt[mhdst.indxthrd]->conf.stream_port
                    ,NULL, NULL
                    ,&webu_answer_strm, cnt[mhdst.indxthrd]
                    ,MHD_OPTION_ARRAY, mhdst.mhd_ops
                    ,MHD_OPTION_END);
            }
            free(mhdst.mhd_ops);
            if (cnt[mhdst.indxthrd]->webstream_daemon == NULL){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Unable to start stream for camera %d")
                    ,cnt[mhdst.indxthrd]->camera_id);
            } else {
                webu_strm_ntc(cnt,mhdst.indxthrd);
            }
        }
        mhdst.indxthrd++;
    }
    if (mhdst.tls_cert != NULL) free(mhdst.tls_cert);
    if (mhdst.tls_key  != NULL) free(mhdst.tls_key);

    return;
}

static void webu_start_ports(struct context **cnt){
    /* Perform check for duplicate ports being specified.  The config loading will
     * duplicate ports from the motion.conf file to all the cameras so we do not
     * log these duplicates to the user and instead just silently set them to zero
     */
    int indx, indx2;

    if (cnt[0]->conf.webcontrol_port != 0){
        indx = 0;
        while (cnt[indx] != NULL){
            if ((cnt[0]->conf.webcontrol_port == cnt[indx]->conf.webcontrol_port) && (indx > 0)){
                cnt[indx]->conf.webcontrol_port = 0;
            }

            if (cnt[0]->conf.webcontrol_port == cnt[indx]->conf.stream_port){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Duplicate port requested %d")
                    ,cnt[indx]->conf.stream_port);
                cnt[indx]->conf.stream_port = 0;
            }

            indx++;
        }
    }

    /* Now check on the stream ports */
    indx = 0;
    while (cnt[indx] != NULL){
        if (cnt[indx]->conf.stream_port != 0){
            indx2 = indx + 1;
            while (cnt[indx2] != NULL){
                if (cnt[indx]->conf.stream_port == cnt[indx2]->conf.stream_port){
                    if (indx != 0){
                        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                            ,_("Duplicate port requested %d")
                            ,cnt[indx2]->conf.stream_port);
                    }
                    cnt[indx2]->conf.stream_port = 0;
                }
                indx2++;
            }
        }
        indx++;
    }
}

void webu_stop(struct context **cnt) {
    /* This function is called from the main Motion loop to shutdown the
     * various MHD connections
     */
    int indxthrd;

    if (cnt[0]->webcontrol_daemon != NULL){
        cnt[0]->webcontrol_finish = TRUE;
        MHD_stop_daemon (cnt[0]->webcontrol_daemon);
    }


    indxthrd = 0;
    while (cnt[indxthrd] != NULL){
        if (cnt[indxthrd]->webstream_daemon != NULL){
            cnt[indxthrd]->webcontrol_finish = TRUE;
            MHD_stop_daemon (cnt[indxthrd]->webstream_daemon);
        }
        cnt[indxthrd]->webstream_daemon = NULL;
        cnt[indxthrd]->webcontrol_daemon = NULL;
        indxthrd++;
    }
}

void webu_start(struct context **cnt) {
    /* This function is called from the main motion thread to start up the
     * webcontrol and streams.  We need to block some signals otherwise MHD
     * will not function correctly.
     */
    struct sigaction act;
    int indxthrd;

    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);


    indxthrd = 0;
    while (cnt[indxthrd] != NULL){
        cnt[indxthrd]->webstream_daemon = NULL;
        cnt[indxthrd]->webcontrol_daemon = NULL;
        cnt[indxthrd]->webcontrol_finish = FALSE;
        indxthrd++;
    }

    if (cnt[0]->conf.stream_preview_method != 99){
        webu_start_ports(cnt);

        webu_start_strm(cnt);
    }

    webu_start_ctrl(cnt);

    return;

}


