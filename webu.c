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
 *    Known HTML Issues:
 *      Single and double quotes are not consistently used.
 *      HTML ids do not follow any naming convention.
 *      After clicking restart/quit, do something..close page? Try to connect again?
 *
 *    Additional functionality considerations:
 *      Notification to user of items that require restart when changed.
 *      Notification to user that item successfully implemented (config change/tracking)
 *      List motion parms somewhere so they can be found by xgettext
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
    char                    *ssl_cert;
    char                    *ssl_key;
    int                     ctrl;
    int                     indxthrd;
    struct MHD_OptionItem   *mhd_ops;
    int                     mhd_opt_nbr;
    unsigned int            mhd_flags;
};


static void webu_context_init(struct context **cntlst, struct context *cnt, struct webui_ctx *webui) {

    int indx;

    webui->url         = mymalloc(WEBUI_LEN_URLI);
    webui->uri_thread  = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd1    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd2    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm1   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value1  = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm2   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value2  = mymalloc(WEBUI_LEN_PARM);
    webui->clientip    = mymalloc(WEBUI_LEN_URLI);
    webui->hostname    = mymalloc(WEBUI_LEN_PARM);

    webui->lang          = mymalloc(3);    /* Two digit lang code plus null terminator */
    webui->lang_full     = mymalloc(6);    /* lang code, underscore, country plus null terminator */
    webui->resp_size     = WEBUI_LEN_RESP * 10;
    webui->resp_used     = 0;
    webui->resp_page     = mymalloc(webui->resp_size);
    webui->userpass_size = WEBUI_LEN_PARM;
    webui->userpass      = mymalloc(webui->userpass_size);
    webui->auth_config   = NULL;    /*We allocate when we assign it */
    webui->stream_img    = NULL;    /*We allocate once we get an image */
    webui->stream_imgsub = NULL;    /*We allocate once we get an image */
    webui->stream_img_size  = 0;
    webui->valid_subsize = FALSE;
    webui->cntlst = cntlst;
    webui->cnt    = cnt;

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

    return;
}

static void webu_context_null(struct webui_ctx *webui) {

    webui->url           = NULL;
    webui->hostname      = NULL;
    webui->uri_thread    = NULL;
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
    webui->userpass      = NULL;
    webui->auth_config   = NULL;
    webui->clientip      = NULL;
    webui->stream_img    = NULL;
    webui->stream_imgsub = NULL;

    return;
}

static void webu_context_free(struct webui_ctx *webui) {

    if (webui->hostname      != NULL) free(webui->hostname);
    if (webui->url           != NULL) free(webui->url);
    if (webui->uri_thread    != NULL) free(webui->uri_thread);
    if (webui->uri_cmd1      != NULL) free(webui->uri_cmd1);
    if (webui->uri_cmd2      != NULL) free(webui->uri_cmd2);
    if (webui->uri_parm1     != NULL) free(webui->uri_parm1);
    if (webui->uri_value1    != NULL) free(webui->uri_value1);
    if (webui->uri_parm2     != NULL) free(webui->uri_parm2);
    if (webui->uri_value2    != NULL) free(webui->uri_value2);
    if (webui->lang          != NULL) free(webui->lang);
    if (webui->lang_full     != NULL) free(webui->lang_full);
    if (webui->resp_page     != NULL) free(webui->resp_page);
    if (webui->userpass      != NULL) free(webui->userpass);
    if (webui->auth_config   != NULL) free(webui->auth_config);
    if (webui->clientip      != NULL) free(webui->clientip);
    if (webui->stream_img    != NULL) free(webui->stream_img);
    if (webui->stream_imgsub != NULL) free(webui->stream_imgsub);

    webu_context_null(webui);

    free(webui);

    return;
}

void webu_write(struct webui_ctx *webui, const char *buf) {

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
            if (scan_rslt < 1) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,_("Error decoding"));

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

static void webu_parms_edit(struct webui_ctx *webui) {

    /* Determine the thread number provided.
     * If no thread provided, assign it to -1
     * Samples:
     * http://localhost:8081/0/stream
     * http://localhost:8081/stream
     * http://localhost:8081/
     */
    int indx, is_nbr;

    if (strlen(webui->uri_thread) > 0){
        is_nbr = TRUE;
        for (indx=0; indx < (int)strlen(webui->uri_thread);indx++){
            if ((webui->uri_thread[indx] > '9') || (webui->uri_thread[indx] < '0')) is_nbr = FALSE;
        }
        if (is_nbr){
            webui->thread_nbr = atoi(webui->uri_thread);
        } else {
            webui->thread_nbr = -1;
        }
    } else {
        webui->thread_nbr = -1;
    }

    /* Set the single context pointer to thread we are answering*/
    if (webui->cntlst != NULL){
        if (webui->thread_nbr < 0){
            webui->cnt = webui->cntlst[0];
        } else {
            webui->cnt = webui->cntlst[webui->thread_nbr];
        }
    }

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
    memset(webui->uri_thread,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value2,'\0',WEBUI_LEN_PARM);

}

static int webu_parseurl(struct webui_ctx *webui) {

    /* Sample: http://localhost:8080/0/config/set?log_level=6 */

    int retcd, parm_len, last_slash;
    char *st_pos, *en_pos;

    retcd = 0;

    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Sent url: %s"),webui->url);

    webu_parseurl_reset(webui);

    if (webui->url == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid url: %s"),webui->url);
        return -1;
    }

    if (webui->url[0] != '/'){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid url: %s"),webui->url);
        return -1;
    }

    webu_url_decode(webui->url, strlen(webui->url));

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),webui->url);

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
    if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
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

    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s< parm1:>%s< val1:>%s< parm2:>%s< val2:>%s<"
               ,webui->uri_thread
               ,webui->uri_cmd1, webui->uri_cmd2
               ,webui->uri_parm1, webui->uri_value1
               ,webui->uri_parm2, webui->uri_value2);


    return retcd;

}

void webu_process_action(struct webui_ctx *webui) {

    int indx;

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    if ((webui->thread_nbr >= webui->cam_threads) || (webui->thread_nbr < 0)){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid thread specified"));
        return;
    }

    indx = 0;
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx])
            webui->cntlst[indx]->makemovie = 1;
        } else {
            webui->cntlst[webui->thread_nbr]->makemovie = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cntlst[++indx])
            webui->cntlst[indx]->snapshot = 1;
        } else {
            webui->cntlst[webui->thread_nbr]->snapshot = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"restart")){
        /* This is the legacy method...(we can do better than signals..).*/
        if (webui->thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("httpd is going to restart"));
            kill(getpid(),SIGHUP);
            webui->cntlst[0]->webcontrol_finish = TRUE;
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("httpd is going to restart thread %d"),webui->thread_nbr);
            if (webui->cntlst[webui->thread_nbr]->running) {
                webui->cntlst[webui->thread_nbr]->makemovie = 1;
                webui->cntlst[webui->thread_nbr]->finish = 1;
            }
            webui->cntlst[webui->thread_nbr]->restart = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"start")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cntlst[indx]->pause = 0;
            } while (webui->cntlst[++indx]);
        } else {
            webui->cntlst[webui->thread_nbr]->pause = 0;
        }
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cntlst[indx]->pause = 1;
            } while (webui->cntlst[++indx]);
        } else {
            webui->cntlst[webui->thread_nbr]->pause = 1;
        }
    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        conf_print(webui->cntlst);
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: %s"),webui->uri_cmd2);
        return;
    }
}

void webu_process_config(struct webui_ctx *webui) {

    int indx;

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    if ((webui->thread_nbr >= webui->cam_threads) || (webui->thread_nbr < 0)){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid thread specified"));
        return;
    }

    if (strcmp(webui->uri_cmd2, "set")) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid command request: %s"),webui->uri_cmd2);
        return;
    }

    indx=0;
    while (config_params[indx].param_name != NULL) {
        if (((webui->thread_nbr != 0) && (config_params[indx].main_thread)) ||
            (config_params[indx].webui_level > webui->cntlst[0]->conf.webcontrol_parms) ||
            (config_params[indx].webui_level == WEBUI_LEVEL_NEVER) ) {
            indx++;
            continue;
        }
        if (!strcmp(webui->uri_parm1, config_params[indx].param_name)) break;
        indx++;
    }
    if (config_params[indx].param_name != NULL){
        if (strlen(webui->uri_parm1) > 0){
            /* This is legacy assumption on the pointers being sequential*/
            conf_cmdparse(webui->cntlst + webui->thread_nbr
                , config_params[indx].param_name, webui->uri_value1);

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
    }

    return;

}

void webu_process_track(struct webui_ctx *webui) {

    struct coord cent;

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    if ((webui->thread_nbr >= webui->cam_threads) || (webui->thread_nbr < 0)){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid thread specified"));
        return;
    }

    if (!strcmp(webui->uri_cmd2, "center")) {
        webui->cntlst[webui->thread_nbr]->moved = track_center(webui->cntlst[webui->thread_nbr], 0, 1, 0, 0);
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

        } else if (!strcasecmp(webui->uri_parm1, "x")) {
            webui->cntlst[webui->thread_nbr]->moved = track_center(webui->cntlst[webui->thread_nbr]
                , webui->cntlst[webui->thread_nbr]->video_dev, 1
                , atoi(webui->uri_value1), atoi(webui->uri_value2));
        }
    }

    return;

}

static void webu_mhd_clientip(struct webui_ctx *webui) {

    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ipv6;
    struct sockaddr_in6 *con_socket;

    /* We are using MHD_DUAL_STACK which puts everything in IPV6 */
    con_info = MHD_get_connection_info(webui->connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    con_socket = (struct sockaddr_in6 *)con_info->client_addr;
    ipv6 = inet_ntop(AF_INET6, &con_socket->sin6_addr,client,WEBUI_LEN_URLI);
    if (ipv6 == NULL){
        snprintf(webui->clientip,WEBUI_LEN_URLI,"%s","Unknown");
    } else {
        if (strncmp(client,"::ffff:",7) == 0){
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client + 7);
        } else {
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client);
        }
    }

    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),webui->clientip);

}

static void webu_mhd_hostname(struct webui_ctx *webui, int ctrl) {

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
        en_pos = strstr(webui->hostname, ":");
        if (en_pos != NULL){
            host_len = en_pos - webui->hostname + 1;
            snprintf(webui->hostname, host_len, "%s", hdr);
        }
    } else {
        gethostname(webui->hostname, WEBUI_LEN_PARM - 1);
    }

    if (ctrl){
        if (webui->cnt->conf.webcontrol_ssl){
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    } else {
        if (webui->cnt->conf.stream_ssl){
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    }

    return;
}

static int webu_mhd_digest(struct webui_ctx *webui) {
    int retcd,len_userpass;
    char *user, *pass, *col_pos;
    struct MHD_Response *response;
    const char *denied = "<html><head><title>Access denied</title></head><body>Access denied</body></html>";
    const char *opaque = "80fb23a1e3760a3d1c91cd060bd07f7a90877334";
    const char *realm = "Motion";

    if (webui->auth_config == NULL){
        len_userpass = 0;
    } else {
        len_userpass = strlen(webui->auth_config);
    }
    if (len_userpass == 0){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No user:pass provided"));
        snprintf(webui->userpass, webui->userpass_size,"%s","digest");
        return MHD_YES;
    }
    col_pos = strstr(webui->auth_config,":");
    if (col_pos == NULL){
        pass = NULL;
        user = mymalloc(len_userpass+1);
        snprintf(user,len_userpass+1,"%s",webui->auth_config);
    } else {
        user = mymalloc(len_userpass - strlen(col_pos) + 1);
        pass = mymalloc(strlen(col_pos));
        snprintf(user, len_userpass - strlen(col_pos) + 1, "%s", webui->auth_config);
        snprintf(pass, strlen(col_pos), "%s", col_pos + 1);
    }

    user = MHD_digest_auth_get_username(webui->connection);
    if (user == NULL) {
        response = MHD_create_response_from_buffer(strlen(denied)
            ,(void *)denied, MHD_RESPMEM_PERSISTENT);
        retcd = MHD_queue_auth_fail_response(webui->connection, realm
            ,opaque, response, MHD_NO);
        MHD_destroy_response(response);
        free(user);
        free(pass);
        return retcd;
    }

    retcd = MHD_digest_auth_check(webui->connection, realm, user, pass, 300);
    free(user);
    free(pass);

    if (retcd == MHD_NO) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), webui->clientip);
    }

    if ( (retcd == MHD_INVALID_NONCE) || (retcd == MHD_NO) )  {
        response = MHD_create_response_from_buffer(strlen (denied)
            ,(void *)denied, MHD_RESPMEM_PERSISTENT);
        if (response == NULL){
            return MHD_NO;
        }
        retcd = MHD_queue_auth_fail_response(webui->connection, realm
            ,opaque, response
            ,(retcd == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);
        MHD_destroy_response(response);
        return retcd;
    }

    snprintf(webui->userpass, webui->userpass_size,"%s","digest");

    return MHD_YES;

}

static int webu_mhd_basic(struct webui_ctx *webui) {
    /* Basic Authentication */
    int retcd;
    char *user, *pass;
    struct MHD_Response *response;
    const char *denied = "<html><head><title>Access denied</title></head><body>Access denied</body></html>";

    pass = NULL;
    user = NULL;
    retcd = MHD_YES;

    if (webui->auth_config == NULL){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No user:pass provided"));
        snprintf(webui->userpass, webui->userpass_size,"%s","basic");
        return MHD_YES;
    }

    user = MHD_basic_auth_get_username_password (webui->connection, &pass);
    if ((user != NULL) && (pass != NULL)){
        if ((int)(strlen(pass) + strlen(user)+2) > webui->userpass_size){
            free(webui->userpass);
            webui->userpass_size = (strlen(pass) + strlen(user)+2);
            webui->userpass = malloc(webui->userpass_size);
        }
        snprintf(webui->userpass, webui->userpass_size,"%s:%s",user,pass);
    }

    if (strlen(webui->userpass) == 0){
        response = MHD_create_response_from_buffer (strlen (denied),
						  (void *) denied, MHD_RESPMEM_PERSISTENT);
        if (!response){
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
            retcd = MHD_NO;
        } else {
            retcd = MHD_queue_basic_auth_fail_response (webui->connection,"Motion",response);
            MHD_destroy_response (response);
        }
    } else {
        if (strcmp(webui->userpass, webui->auth_config) != 0){
            MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
                ,_("Failed authentication from %s"),webui->clientip);
            response = MHD_create_response_from_buffer (strlen (denied),
						  (void *) denied, MHD_RESPMEM_PERSISTENT);
            if (!response){
                MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
                retcd = MHD_NO;
            } else {
                retcd = MHD_queue_basic_auth_fail_response (webui->connection,"Motion",response);
                MHD_destroy_response (response);
            }
        } else {
            retcd = MHD_YES;
        }
    }

    if (pass != NULL) free(pass);
    if (user != NULL) free(user);

    if (retcd == MHD_NO){
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"),webui->clientip);
    }

    return retcd;

}

static int webu_mhd_auth(struct webui_ctx *webui, int ctrl){
    int retcd;

    retcd = 2;  /* Motion specific return code to signal we fell through */
    if (ctrl){        /* Authentication for the webcontrol*/
        if ((webui->cnt->conf.webcontrol_authentication != NULL) &&
            (webui->auth_config == NULL)){
            webui->auth_config = mymalloc(strlen(webui->cnt->conf.webcontrol_authentication)+1);
            snprintf(webui->auth_config
                ,strlen(webui->cnt->conf.webcontrol_authentication)+1
                ,"%s",webui->cnt->conf.webcontrol_authentication);
        }

        if ((webui->cnt->conf.webcontrol_auth_method == 1) &&
            (strlen(webui->userpass) == 0)){
            retcd = webu_mhd_basic(webui);
            return retcd;
        } else if ((webui->cnt->conf.webcontrol_auth_method == 2) &&
            (strlen(webui->userpass) == 0)){
            retcd = webu_mhd_digest(webui);
            return retcd;
        }
    } else {        /* Authentication for the streams*/
        if ((webui->cnt->conf.stream_authentication != NULL) &&
            (webui->auth_config == NULL)){
            webui->auth_config = mymalloc(strlen(webui->cnt->conf.stream_authentication)+1);
            snprintf(webui->auth_config
                ,strlen(webui->cnt->conf.stream_authentication)+1
                ,"%s",webui->cnt->conf.stream_authentication);
        }

        if ((webui->cnt->conf.stream_auth_method == 1) &&
            (strlen(webui->userpass) == 0)){
            retcd = webu_mhd_basic(webui);
            return retcd;
        } else if ((webui->cnt->conf.stream_auth_method == 2) &&
            (strlen(webui->userpass) == 0)){
            retcd = webu_mhd_digest(webui);
            return retcd;
        }
    }

    return retcd;
}

static int webu_mhd_send(struct webui_ctx *webui, int ctrl) {

    int retcd;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen(webui->resp_page)
        ,(void *)webui->resp_page, MHD_RESPMEM_PERSISTENT);
    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

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

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

static int webu_ans_ctrl(void *cls
        , struct MHD_Connection *connection
        , const char *url
        , const char *method
        , const char *version
        , const char *upload_data
        , size_t     *upload_data_size
        , void **ptr) {

    /* Answer the request for the webcontrol*/
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

    util_threadname_set("wu", 0,NULL);

    webui->connection = connection;
    if (strlen(webui->clientip) == 0){
        webu_mhd_clientip(webui);
    }

    webu_mhd_hostname(webui, TRUE);

    retcd = webu_mhd_auth(webui, TRUE);
    if (retcd != 2) return retcd;

    retcd = 0;
    if (webui->cntlst[0]->conf.webcontrol_interface == 1){
        if (retcd == 0) retcd = webu_text_main(webui);
        if (retcd <  0) webu_text_badreq(webui);
        retcd = webu_mhd_send(webui, TRUE);
    } else {
        if (retcd == 0) retcd = webu_html_main(webui);
        if (retcd <  0) webu_html_badreq(webui);
        retcd = webu_mhd_send(webui, TRUE);
    }

    if (retcd == MHD_NO){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed %d"),retcd);
    }
    return retcd;

}

static int webu_ans_strm(void *cls
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
    if (strlen(webui->clientip) == 0){
        webu_mhd_clientip(webui);
    }

    webu_mhd_hostname(webui, FALSE);

    retcd = webu_mhd_auth(webui, FALSE);
    if (retcd != 2) return retcd;

    retcd = 0;
    if ((strcmp(webui->uri_cmd1,"stream") == 0) ||
        (strcmp(webui->uri_cmd1,"substream") == 0) ||
        (strcmp(webui->uri_thread,"stream") == 0) ||
        (strcmp(webui->uri_thread,"substream") == 0) ||
        (strlen(webui->uri_thread) == 0)){
            retcd = webu_stream_mjpeg(webui);
            if (retcd == MHD_NO){
                webu_html_badreq(webui);
                retcd = webu_mhd_send(webui, FALSE);
            }
    } else if ((strcmp(webui->uri_cmd1,"current") == 0) ||
        (strcmp(webui->uri_thread,"current") == 0)){
            retcd = webu_stream_static(webui);
            if (retcd == MHD_NO){
                webu_html_badreq(webui);
                retcd = webu_mhd_send(webui, FALSE);
            }
    } else {
        webu_html_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
    }

    if (retcd == MHD_NO){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed %d"),retcd);
    }
    return retcd;

}

static void *webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection) {

    struct context **cnt = cls;
    struct webui_ctx *webui;

    (void)connection;

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(cnt, NULL, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    webu_parseurl(webui);

    webu_parms_edit(webui);

    memset(webui->hostname,'\0',WEBUI_LEN_PARM);
    memset(webui->resp_page,'\0',webui->resp_size);
    memset(webui->userpass,'\0',webui->userpass_size);

    return webui;
}

static void *webu_mhd_init_one(void *cls, const char *uri, struct MHD_Connection *connection) {

    struct context *cnt = cls;
    struct webui_ctx *webui;

    (void)connection;

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(NULL, cnt, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    webu_parseurl(webui);

    webu_parms_edit(webui);

    memset(webui->hostname,'\0',WEBUI_LEN_PARM);
    memset(webui->resp_page,'\0',webui->resp_size);
    memset(webui->userpass,'\0',webui->userpass_size);

    return webui;
}

static void webu_mhd_deinit(void *cls
    , struct MHD_Connection *connection
    , void **con_cls
    , enum MHD_RequestTerminationCode toe) {

    struct webui_ctx *webui = *con_cls;

    /* Eliminate compiler warnings */
    (void)connection;
    (void)cls;
    (void)toe;

    webu_context_free(webui);

    return;
}

static void webu_mhd_features_basic(struct mhdstart_ctx *mhdst){
#if MHD_VERSION < 0x00094400
    (void)mhdst;
#else
    int retcd;
    retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: enabled"));
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
#if MHD_VERSION < 0x00094400
    (void)mhdst;
#else
    int retcd;
    retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: enabled"));
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

static void webu_mhd_features_ipv6(void){
#if MHD_VERSION >= 0x00094400
    int retcd;
    retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("IPV6: enabled"));
    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("IPV6: disabled"));
    }
#endif
}

static void webu_mhd_features_ssl(struct mhdstart_ctx *mhdst){
#if MHD_VERSION < 0x00094400
    if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl)){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL disabled"));
        mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl = 0;
    } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl)) {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL disabled"));
        mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl = 0;
    }
#else
    int retcd;
    retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL: enabled"));
    } else {
        if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl)){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL: disabled"));
            mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl = 0;
        } else if ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl)){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL: disabled"));
            mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl = 0;
        } else {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL: disabled"));
        }
    }
#endif
}

static void webu_mhd_features(struct mhdstart_ctx *mhdst){
    /* sample format: 0x01093001 = 1.9.30-1 , 0x00094400 ships with 16.04 */

    webu_mhd_features_basic(mhdst);

    webu_mhd_features_digest(mhdst);

    webu_mhd_features_ipv6();

    webu_mhd_features_ssl(mhdst);

}

static char *webu_mhd_loadfile(const char *fname){
    FILE *infile;
    size_t file_size, read_size;
    char * file_char;

    if (fname == NULL) {
        file_char = NULL;
    } else {
        infile = fopen(fname, "rb");
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
                    ,_("Error reading file for SSL support."));
            }
        } else {
            file_char = NULL;
        }
        fclose(infile);
    }
    return file_char;
}

static void webu_mhd_checkssl(struct mhdstart_ctx *mhdst){

    if (mhdst->ctrl){
        if (mhdst->cnt[0]->conf.webcontrol_ssl){
            if ((mhdst->cnt[0]->conf.webcontrol_cert == NULL) || (mhdst->ssl_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL requested but no cert file provided.  SSL disabled"));
                mhdst->cnt[0]->conf.webcontrol_ssl = 0;
            }
            if ((mhdst->cnt[0]->conf.webcontrol_key == NULL) || (mhdst->ssl_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL requested but no key file provided.  SSL disabled"));
                mhdst->cnt[0]->conf.webcontrol_ssl = 0;
            }
        }
    } else {
        if (mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl){
            if ((mhdst->cnt[0]->conf.webcontrol_cert == NULL) || (mhdst->ssl_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL requested but no cert file provided.  SSL disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl = 0;
            }
            if ((mhdst->cnt[0]->conf.webcontrol_key == NULL) || (mhdst->ssl_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL requested but no key file provided.  SSL disabled"));
                mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl = 0;
            }
        }
    }

}

static void webu_mhd_opts_init(struct mhdstart_ctx *mhdst){

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

    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

static void webu_mhd_opts_localhost(struct mhdstart_ctx *mhdst){

    struct sockaddr_in loopback_addr;

    if ((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_localhost)){
        memset(&loopback_addr, 0, sizeof(loopback_addr));
        loopback_addr.sin_family = AF_INET;
        loopback_addr.sin_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_port);
        loopback_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&loopback_addr);
        mhdst->mhd_opt_nbr++;

    } else if((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_localhost)){
        memset(&loopback_addr, 0, sizeof(loopback_addr));
        loopback_addr.sin_family = AF_INET;
        loopback_addr.sin_port = htons(mhdst->cnt[mhdst->indxthrd]->conf.stream_port);
        loopback_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&loopback_addr);
        mhdst->mhd_opt_nbr++;
    }

}

static void webu_mhd_opts_digest(struct mhdstart_ctx *mhdst){

    unsigned int randnbr;
    char randchar[8];

    if (((mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_auth_method == 2)) ||
        ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_auth_method == 2))) {

        srand(time(NULL));
        randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
        snprintf(randchar,sizeof(randchar),"%d",randnbr);

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(randchar);
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = randchar;
        mhdst->mhd_opt_nbr++;

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

static void webu_mhd_opts_ssl(struct mhdstart_ctx *mhdst){

    if ((( mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl)) ||
        ((!mhdst->ctrl) && (mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl))) {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_CERT;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->ssl_cert;
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_KEY;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->ssl_key;
        mhdst->mhd_opt_nbr++;
    }

}

static void webu_mhd_opts(struct mhdstart_ctx *mhdst){


    mhdst->mhd_opt_nbr = 0;

    webu_mhd_checkssl(mhdst);

    webu_mhd_opts_deinit(mhdst);

    webu_mhd_opts_init(mhdst);

    webu_mhd_opts_localhost(mhdst);

    webu_mhd_opts_digest(mhdst);

    webu_mhd_opts_ssl(mhdst);

    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_END;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

static void webu_mhd_flags(struct mhdstart_ctx *mhdst){


    if (mhdst->ctrl){
        if (mhdst->cnt[mhdst->indxthrd]->conf.webcontrol_ssl){
            mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                MHD_USE_POLL|
                MHD_USE_DUAL_STACK |
                MHD_USE_SELECT_INTERNALLY|
                MHD_USE_SSL;
        } else {
            mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                MHD_USE_POLL|
                MHD_USE_DUAL_STACK |
                MHD_USE_SELECT_INTERNALLY;
        }
    } else {
        if (mhdst->cnt[mhdst->indxthrd]->conf.stream_ssl){
            mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                MHD_USE_POLL|
                MHD_USE_DUAL_STACK |
                MHD_USE_SELECT_INTERNALLY|
                MHD_USE_SSL;
        } else {
            mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                MHD_USE_POLL|
                MHD_USE_DUAL_STACK |
                MHD_USE_SELECT_INTERNALLY;
        }
    }

}

static void webu_start_ctrl(struct context **cnt){

    struct mhdstart_ctx mhdst;

    mhdst.ssl_cert = webu_mhd_loadfile(cnt[0]->conf.webcontrol_cert);
    mhdst.ssl_key  = webu_mhd_loadfile(cnt[0]->conf.webcontrol_key);
    mhdst.ctrl = TRUE;
    mhdst.indxthrd = 0;
    mhdst.cnt = cnt;

    cnt[0]->webcontrol_daemon = NULL;
    if (cnt[0]->conf.webcontrol_port != 0 ){
        mhdst.mhd_ops = malloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
        webu_mhd_features(&mhdst);
        webu_mhd_opts(&mhdst);
        webu_mhd_flags(&mhdst);
        cnt[0]->webcontrol_daemon = MHD_start_daemon (mhdst.mhd_flags
            ,cnt[0]->conf.webcontrol_port
            ,NULL, NULL
            ,&webu_ans_ctrl, cnt
            ,MHD_OPTION_ARRAY, mhdst.mhd_ops
            ,MHD_OPTION_END);
        free(mhdst.mhd_ops);
        if (cnt[0]->webcontrol_daemon == NULL){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD"));
        }
    }

    if (mhdst.ssl_cert != NULL) free(mhdst.ssl_cert);
    if (mhdst.ssl_key  != NULL) free(mhdst.ssl_key);

    return;
}

static void webu_start_strm(struct context **cnt){

    struct mhdstart_ctx mhdst;

    mhdst.ssl_cert = webu_mhd_loadfile(cnt[0]->conf.webcontrol_cert);
    mhdst.ssl_key  = webu_mhd_loadfile(cnt[0]->conf.webcontrol_key);
    mhdst.ctrl = FALSE;
    mhdst.indxthrd = 0;
    mhdst.cnt = cnt;

    mhdst.indxthrd = 0;
    while (cnt[mhdst.indxthrd] != NULL){
        cnt[mhdst.indxthrd]->webstream_daemon = NULL;
        if (cnt[mhdst.indxthrd]->conf.stream_port != 0 ){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Starting MHD stream %d on port %d")
                ,mhdst.indxthrd, cnt[mhdst.indxthrd]->conf.stream_port);
            mhdst.mhd_ops= malloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
            webu_mhd_features(&mhdst);
            webu_mhd_opts(&mhdst);
            webu_mhd_flags(&mhdst);
            if (mhdst.indxthrd == 0){
                cnt[mhdst.indxthrd]->webstream_daemon = MHD_start_daemon (mhdst.mhd_flags
                    ,cnt[mhdst.indxthrd]->conf.stream_port
                    ,NULL, NULL
                    ,&webu_ans_strm, cnt
                    ,MHD_OPTION_ARRAY, mhdst.mhd_ops
                    ,MHD_OPTION_END);
            } else {
                cnt[mhdst.indxthrd]->webstream_daemon = MHD_start_daemon (mhdst.mhd_flags
                    ,cnt[mhdst.indxthrd]->conf.stream_port
                    ,NULL, NULL
                    ,&webu_ans_strm, cnt[mhdst.indxthrd]
                    ,MHD_OPTION_ARRAY, mhdst.mhd_ops
                    ,MHD_OPTION_END);
            }
            free(mhdst.mhd_ops);
            if (cnt[mhdst.indxthrd]->webstream_daemon == NULL){
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD %d"),mhdst.indxthrd);
            }
        }
        mhdst.indxthrd++;
    }
    if (mhdst.ssl_cert != NULL) free(mhdst.ssl_cert);
    if (mhdst.ssl_key  != NULL) free(mhdst.ssl_key);

    return;
}

void webu_stop(struct context **cnt) {
    int indxthrd;

    if (cnt[0]->webcontrol_daemon != NULL){
        MHD_stop_daemon (cnt[0]->webcontrol_daemon);
    }

    indxthrd = 0;
    while (cnt[indxthrd] != NULL){
        if (cnt[indxthrd]->webstream_daemon != NULL){
            MHD_stop_daemon (cnt[indxthrd]->webstream_daemon);
        }
        indxthrd++;
    }
}

void webu_start(struct context **cnt) {

    struct sigaction act;

    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    webu_start_ctrl(cnt);

    webu_start_strm(cnt);

    return;

}


