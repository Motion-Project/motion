/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
*/


/*
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
 *      The tracking is "best effort" since developer does not have tracking camera.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_html.hpp"
#include "webu_text.hpp"
#include "webu_stream.hpp"
#include "track.hpp"
#include "video_v4l2.hpp"


/* Context to pass the parms to functions to start mhd */
struct mhdstart_ctx {
    struct ctx_motapp       *motapp;
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

static void webu_context_init(struct ctx_motapp *motapp, struct ctx_cam *cam, struct webui_ctx *webui)
{

    int indx;

    webui->url           = (char*)mymalloc(WEBUI_LEN_URLI);
    webui->uri_camid     = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd1      = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd2      = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm1     = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_value1    = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm2     = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->uri_value2    = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->clientip      = (char*)mymalloc(WEBUI_LEN_URLI);
    webui->hostname      = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->auth_denied   = (char*)mymalloc(WEBUI_LEN_RESP);
    webui->auth_opaque   = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->auth_realm    = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->text_eol      = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->auth_user     = NULL;    /* Buffer to hold the user name*/
    webui->auth_pass     = NULL;    /* Buffer to hold the password */
    webui->authenticated = FALSE;   /* boolean for whether we are authenticated*/
    webui->lang          = (char*)mymalloc(3);         /* Two digit lang code plus null terminator */
    webui->lang_full     = (char*)mymalloc(6);         /* lang code, e.g US_en */
    webui->resp_size     = WEBUI_LEN_RESP * 10; /* The size of the resp_page buffer.  May get adjusted */
    webui->resp_used     = 0;                   /* How many bytes used so far in resp_page*/
    webui->stream_pos    = 0;                   /* Stream position of image being sent */
    webui->stream_fps    = 1;                   /* Stream rate */
    webui->resp_page     = (char*)mymalloc(webui->resp_size);      /* The response being constructed */
    webui->post_info     = NULL;
    webui->post_sz       = 0;
    webui->motapp        = motapp;              /* The motion application context */
    webui->cam           = cam;                 /* The context pointer for a single camera */
    webui->cnct_type     = WEBUI_CNCT_UNKNOWN;
    webui->resptype      = 0;   /* Default to html response */

    /* get the number of cameras and threads */
    indx = 0;
    if (webui->motapp->cam_list != NULL) {
        while (webui->motapp->cam_list[++indx]) {
            continue;
        };
    }
    webui->cam_threads = indx;

    webui->cam_count = indx;
    if (indx > 1) {
        webui->cam_count--;
    }

    /* 1 thread, 1 camera = just motion.conf.
     * 2 thread, 1 camera, then using motion.conf plus a separate camera file */
    snprintf(webui->lang_full, 6,"%s", getenv("LANGUAGE"));
    snprintf(webui->lang, 3,"%s",webui->lang_full);

    memset(webui->hostname,'\0',WEBUI_LEN_PARM);
    memset(webui->resp_page,'\0',webui->resp_size);

    return;
}

static void webu_free_var(char *parm)
{
    if (parm != NULL) {
        free(parm);
    }
    parm = NULL;
}

static void webu_context_free(struct webui_ctx *webui)
{
    int indx;

    webu_free_var(webui->hostname);
    webu_free_var(webui->url);
    webu_free_var(webui->uri_camid);
    webu_free_var(webui->uri_cmd1);
    webu_free_var(webui->uri_cmd2);
    webu_free_var(webui->uri_parm1);
    webu_free_var(webui->uri_value1);
    webu_free_var(webui->uri_parm2);
    webu_free_var(webui->uri_value2);
    webu_free_var(webui->lang);
    webu_free_var(webui->lang_full);
    webu_free_var(webui->resp_page);
    webu_free_var(webui->auth_user);
    webu_free_var(webui->auth_pass);
    webu_free_var(webui->auth_denied);
    webu_free_var(webui->auth_opaque);
    webu_free_var(webui->auth_realm);
    webu_free_var(webui->clientip);
    webu_free_var(webui->text_eol);

    for (indx = 0; indx<webui->post_sz; indx++) {
        webu_free_var(webui->post_info[indx].key_nm);
        webu_free_var(webui->post_info[indx].key_val);
    }
    free(webui->post_info);
    webui->post_info = NULL;

    free(webui);

    return;
}

static void webu_badreq(struct webui_ctx *webui)
{
    /* This function is used in this webu module as a central function when there is a bad
     * request.  Since sometimes we will be unable to determine what camera context (stream
     * or camera) originated the request and we have NULL for camlist and cam, we default the
     * response to be HTML.  Otherwise, we do know the type and we send back to the user the
     * bad request response either with or without the HTML tags.
     */
    if (webui->resptype == 1) {
        webu_text_badreq(webui);
    } else {
        webu_html_badreq(webui);
    }
}

void webu_write(struct webui_ctx *webui, const char *buf)
{
    /* Copy the buf data to our response buffer.  If the response buffer is not large enough to
     * accept our new data coming in, then expand it in chunks of 10
     */
    int      resp_len;
    char    *temp_resp;
    size_t   temp_size;

    resp_len = strlen(buf);

    temp_size = webui->resp_size;
    while ((resp_len + webui->resp_used) > temp_size) {
        temp_size = temp_size + (WEBUI_LEN_RESP * 10);
    }

    if (temp_size > webui->resp_size) {
        temp_resp = (char*)mymalloc(webui->resp_size);
        memcpy(temp_resp, webui->resp_page, webui->resp_size);
        free(webui->resp_page);
        webui->resp_page = (char*)mymalloc(temp_size);
        memset(webui->resp_page,'\0',temp_size);
        memcpy(webui->resp_page, temp_resp, webui->resp_size);
        webui->resp_size = temp_size;
        free(temp_resp);
    }

    memcpy(webui->resp_page + webui->resp_used, buf, resp_len);
    webui->resp_used = webui->resp_used + resp_len;

    return;
}

static void webu_parms_edit(struct webui_ctx *webui)
{

    /* Determine the thread number provided.
     * If no thread provided, assign it to -1
     * Samples:
     * http://localhost:8081/0/stream (camlist will be populated and this function will set cam)
     * http://localhost:8081/stream (camlist will be null, cam will be populated)
     * http://localhost:8081/   (camlist will be null, cam will be populated)
     */
    int indx, is_nbr;

    if (strlen(webui->uri_camid) > 0) {
        is_nbr = TRUE;
        for (indx=0; indx < (int)strlen(webui->uri_camid); indx++) {
            if ((webui->uri_camid[indx] > '9') || (webui->uri_camid[indx] < '0')) {
                is_nbr = FALSE;
            }
        }
        if (is_nbr) {
            webui->thread_nbr = atoi(webui->uri_camid);
        } else {
            webui->thread_nbr = -1;
        }
    } else {
        webui->thread_nbr = -1;
    }

    /* Set the single context pointer to thread we are answering
     * If the connection is for a single stream (legacy method of a port
     * per stream), then the camlist will be null and the camera context
     * will already be assigned into webui->cam.  This is part of the
     * init function which is called for MHD and it has the different
     * variations depending upon how the port and cameras were specified.
     * Also set/convert the camid into the thread number.
    */

    if (webui->motapp->cam_list != NULL) {
        if (webui->thread_nbr < 0) {
            webui->cam = webui->motapp->cam_list[0];
            webui->thread_nbr = 0;
        } else {
            indx = 0;
            while (webui->motapp->cam_list[indx] != NULL) {
                if (webui->motapp->cam_list[indx]->camera_id == webui->thread_nbr) {
                    webui->thread_nbr = indx;
                    break;
                }
                indx++;
            }
            /* This may be null, in which case we will not answer the request */
            webui->cam = webui->motapp->cam_list[indx];
        }
    }

    if (webui->cam != NULL) {
        if (webui->cam->conf->webcontrol_interface == 1) {
            webui->resptype = 1;
        } else {
            webui->resptype = 0;
        }
    }

}

static void webu_parseurl_parms(struct webui_ctx *webui, char *st_pos)
{

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
    if (en_pos != NULL) {
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_PARM) {
            return;
        }
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);

        /* Get the parameter name */
        st_pos = st_pos + parm_len; /* Move past the command */
        en_pos = strstr(st_pos,"=");
        if (en_pos == NULL) {
            parm_len = strlen(webui->url) - parm_len;
            last_parm = TRUE;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) {
            return;
        }
        snprintf(webui->uri_parm1, parm_len,"%s", st_pos);

        if (!last_parm) {
            /* Get the parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL) {
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) {
                return;
            }
            snprintf(webui->uri_value1, parm_len,"%s", st_pos);
        }

        if (!last_parm) {
            /* Get the next parameter name */
            st_pos = st_pos + parm_len; /* Move past the previous command */
            en_pos = strstr(st_pos,"=");
            if (en_pos == NULL) {
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) {
                return;
            }
            snprintf(webui->uri_parm2, parm_len,"%s", st_pos);
        }

        if (!last_parm) {
            /* Get the next parameter value */
            st_pos = st_pos + parm_len;     /* Move past the equals sign */
            if (!strcasecmp(webui->uri_parm1,"x") || !strcasecmp(webui->uri_parm1,"pan")) {
                en_pos = strstr(st_pos,"&");
            } else {
                en_pos = NULL;
            }
            if (en_pos == NULL) {
                parm_len = strlen(webui->url) - parm_len;
                last_parm = TRUE;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) {
                return;
            }
            snprintf(webui->uri_value2, parm_len,"%s", st_pos);
        }

    }

}

static void webu_parseurl_reset(struct webui_ctx *webui)
{

    /* Reset the variables to empty strings*/

    memset(webui->uri_camid,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value2,'\0',WEBUI_LEN_PARM);

}

static int webu_parseurl(struct webui_ctx *webui)
{
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

    if (strlen(webui->url) == 0) {
        return -1;
    }

    MHD_http_unescape(webui->url);

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),webui->url);

    /* Home page */
    if (strlen(webui->url) == 1) {
        return 0;
    }

    last_slash = 0;

    /* Get the camid number and which sometimes this will contain an action if the user
     * is setting the port for a particular camera and requests the
     * stream by using http://localhost:port/stream
     */
    st_pos = webui->url + 1; /* Move past the first "/" */
    if (*st_pos == '-') {
        /* Never allow a negative number */
        return -1;
    }
    en_pos = strstr(st_pos,"/");
    if (en_pos == NULL) {
        parm_len = strlen(webui->url);
        last_slash = 1;
    } else {
        parm_len = en_pos - st_pos + 1;
    }
    if (parm_len >= WEBUI_LEN_PARM) {
        /* var was malloc'd to WEBUI_LEN_PARM */
        return -1;
    }
    snprintf(webui->uri_camid, parm_len,"%s", st_pos);

    if (!last_slash) {
        /* Get cmd1 or action */
        st_pos = st_pos + parm_len; /* Move past the camid */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL) {
            parm_len = strlen(webui->url) - parm_len ;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) {
            /* var was malloc'd to WEBUI_LEN_PARM */
            return -1;
        }
        snprintf(webui->uri_cmd1, parm_len,"%s", st_pos);
    }

    if (!last_slash) {
        /* Get cmd2 or action */
        st_pos = st_pos + parm_len; /* Move past the first command */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL) {
            parm_len = strlen(webui->url) - parm_len;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) {
            /* var was malloc'd to WEBUI_LEN_PARM */
            return -1;
        }
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);
    }

    if ((mystreq(webui->uri_cmd1,"config") ||
         mystreq(webui->uri_cmd1,"track")) &&
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

static void webu_process_add(struct webui_ctx *webui)
{
    int indx, maxcnt;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Adding camera.");

    maxcnt = 100;

    /* webu_html_main locks it at the top of that function but for this action, we need to unlock it */
    pthread_mutex_unlock(&webui->motapp->mutex_camlst);

    webui->motapp->cam_add = TRUE;
    indx = 0;
    while ((webui->motapp->cam_add == TRUE) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }

    if (indx == maxcnt) {
        webui->motapp->cam_add = TRUE;
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error adding camera.  Timed out");
        return;
    }

    /* Now lock it back up so that webu_html_main can unlock it upon exit from that function */
    pthread_mutex_lock(&webui->motapp->mutex_camlst);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "New camera added.");

}

static void webu_process_delete(struct webui_ctx *webui)
{
    int indx, maxcnt;

    if (webui->thread_nbr == 0) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "No camera specified for deletion." );
        return;
    } else {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Deleting camera.");
    }

    maxcnt = 100;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
        _("Stopping cam %d"),webui->cam->camera_id);
    webui->cam->restart_cam = FALSE;
    webui->cam->finish_cam = TRUE;

    indx = 0;
    while ((webui->cam->running_cam) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        webui->motapp->cam_delete = 0;
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error deleting camera.  Timed out shutting down");
        return;
    }
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Camera stopped");

    /* webu_html_main and webu_text_main lock the mutex at beginning of their functions.
     * We need it unlocked so that the add/delete will work
    */
    pthread_mutex_unlock(&webui->motapp->mutex_camlst);

    webui->motapp->cam_delete = webui->thread_nbr;

    indx = 0;
    while ((webui->motapp->cam_delete > 0) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        webui->motapp->cam_delete = 0;
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error deleting camera.  Timed out");
        return;
    }
    webui->cam = NULL;

    /* We now lock the mutex so that the "unlock" at the bottom of
     * webu_html_main, webu_text_main will work.
    */
    pthread_mutex_lock(&webui->motapp->mutex_camlst);

}

/* Process the actions from the webcontrol that the user requested */
void webu_process_action(struct webui_ctx *webui)
{
    int indx;

    if ((mystreq(webui->uri_cmd2,"makemovie")) ||
        (mystreq(webui->uri_cmd2,"eventend"))) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 1;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->event_stop = TRUE;
                indx++;
            }
        } else {
            webui->cam->event_stop = TRUE;
        }

    } else if (mystreq(webui->uri_cmd2,"eventstart")) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 1;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->event_user = TRUE;
                indx++;
            }
        } else {
            webui->cam->event_user = TRUE;
        }

    } else if (mystreq(webui->uri_cmd2,"snapshot")) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 1;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->snapshot = 1;
                indx++;
            }
        } else {
            webui->cam->snapshot = 1;
        }


    } else if (mystreq(webui->uri_cmd2,"restart")) {
        if (webui->thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all threads"));
            indx = 1;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->restart_cam = TRUE;
                if (webui->motapp->cam_list[indx]->running_cam) {
                    webui->motapp->cam_list[indx]->event_stop = TRUE;
                    webui->motapp->cam_list[indx]->finish_cam = TRUE;
                }
                indx++;
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Restarting thread %d"),webui->thread_nbr);
            webui->cam->restart_cam = TRUE;
            if (webui->cam->running_cam) {
                webui->cam->event_stop = TRUE;
                webui->cam->finish_cam = TRUE;
            }

        }

    } else if (mystreq(webui->uri_cmd2,"stop")) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 1;
            while (webui->motapp->cam_list[indx]) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                    _("Stopping cam %d"),webui->motapp->cam_list[indx]->camera_id);
                webui->motapp->cam_list[indx]->restart_cam = FALSE;
                webui->motapp->cam_list[indx]->event_stop = TRUE;
                webui->motapp->cam_list[indx]->event_user = TRUE;
                webui->motapp->cam_list[indx]->finish_cam = TRUE;
                indx++;
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Stopping cam %d"),webui->cam->camera_id);
            webui->cam->restart_cam = FALSE;
            webui->cam->event_stop = TRUE;
            webui->cam->event_user = TRUE;
            webui->cam->finish_cam = TRUE;
        }

    } else if (mystreq(webui->uri_cmd2,"end")) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Motion terminating"));
            indx = 0;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->restart_cam = FALSE;
                webui->motapp->cam_list[indx]->event_stop = TRUE;
                webui->motapp->cam_list[indx]->event_user = TRUE;
                webui->motapp->cam_list[indx]->finish_cam = TRUE;
                indx++;
            }

    } else if (mystreq(webui->uri_cmd2,"resume")) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 0;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->pause = 0;
                indx++;
            };
        } else {
            webui->cam->pause = 0;
        }

    } else if (mystreq(webui->uri_cmd2,"pause")) {
        if (webui->thread_nbr == 0 && webui->cam_threads > 1) {
            indx = 0;
            while (webui->motapp->cam_list[indx]) {
                webui->motapp->cam_list[indx]->pause = 1;
                indx++;
            };
        } else {
            webui->cam->pause = 1;
        }

    } else if (mystreq(webui->uri_cmd2,"connection")) {
        webu_text_connection(webui);

    } else if (mystreq(webui->uri_cmd2,"status")) {
        webu_text_status(webui);

    } else if (mystreq(webui->uri_cmd2,"write") ||
               mystreq(webui->uri_cmd2,"writeyes")) {
        conf_parms_write(webui->motapp->cam_list);

    } else if (mystreq(webui->uri_cmd2,"add")) {
        webu_process_add(webui);

    } else if (mystreq(webui->uri_cmd2,"delete")) {
        webu_process_delete(webui);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            , webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        return;
    }
}

static int webu_process_config_set(struct webui_ctx *webui)
{
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
    while (config_parms_depr[indx].parm_name != "") {
        if (mystreq(config_parms_depr[indx].parm_name.c_str() ,webui->uri_parm1)) {
            snprintf(temp_name, WEBUI_LEN_PARM, "%s", config_parms_depr[indx].newname.c_str());
            break;
        }
        indx++;
    }
    /* Ignore any request to change an option that is designated above the
     * webcontrol_parms level.
     */
    indx=0;
    while (config_parms[indx].parm_name != "") {
        if (((webui->thread_nbr != 0) && (config_parms[indx].main_thread)) ||
            (config_parms[indx].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx].webui_level == WEBUI_LEVEL_NEVER) ) {
            indx++;
            continue;
        }
        if (mystreq(temp_name, config_parms[indx].parm_name.c_str())) {
            break;
        }
        indx++;
    }
    /* If we found the parm, assign it.  If the loop above did not find the parm
     * then we ignore the request
     */
    if (config_parms[indx].parm_name != "") {
        if (strlen(webui->uri_value1) > 0) {
            conf_edit_set(webui->motapp, false, webui->thread_nbr
                ,config_parms[indx].parm_name, webui->uri_value1);

            /*If we are updating vid parms, set the flag to update the device.*/
            if ((config_parms[indx].parm_name == "v4l2_parms") &&
                (webui->motapp->cam_list[webui->thread_nbr]->v4l2cam->params != NULL)) {
                webui->motapp->cam_list[webui->thread_nbr]->v4l2cam->params->update_params = TRUE;
            }

            /* If changing language, do it now */
            if (config_parms[indx].parm_name == "native_language") {
                if (webui->motapp->cam_list[webui->thread_nbr]->motapp->native_language) {
                    mytranslate_text("", 1);
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : on"));
                } else {
                    mytranslate_text("", 0);
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

static void webu_process_json_parm(struct webui_ctx *webui, int indx_cam, int indx_parm)
{
    int indx_orig, indx_val;
    char response[WEBUI_LEN_RESP];
    char parm_orig[PATH_MAX], parm_val[PATH_MAX], parm_list[PATH_MAX];

    memset(parm_orig, '\0',PATH_MAX);
    memset(parm_val, '\0',PATH_MAX);
    memset(parm_list, '\0',PATH_MAX);

    conf_edit_get(webui->motapp->cam_list[indx_cam]
        , config_parms[indx_parm].parm_name.c_str()
        , parm_orig
        , config_parms[indx_parm].parm_cat);

    if (strstr(parm_orig, "\"")) {
        indx_val = 0;
        for (indx_orig = 0; indx_orig < (int)strlen(parm_orig); indx_orig++){
            if (parm_orig[indx_orig] == '"') {
                parm_val[indx_val] = '\\';
                indx_val++;
            }
            parm_val[indx_val] = parm_orig[indx_orig];
            indx_val++;
            if (indx_val == PATH_MAX) {
                break;
            }
        }
    } else {
        memcpy(parm_val, parm_orig, PATH_MAX);
    }

    if (config_parms[indx_parm].parm_type == PARM_TYP_INT) {
        snprintf(response, sizeof (response)
            , "\"%s\":{\"value\": %s,\"enabled\":true"
              ",\"category\":%d, \"type\":\"%s\"}"
            , config_parms[indx_parm].parm_name.c_str()
            , parm_val
            , config_parms[indx_parm].parm_cat
            , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
            );
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_BOOL) {
        if (mystreq(parm_val, "on")) {
            snprintf(response, sizeof (response)
                , "\"%s\":{\"value\": true,\"enabled\":true"
                  ",\"category\":%d, \"type\":\"%s\"}"
                , config_parms[indx_parm].parm_name.c_str()
                , config_parms[indx_parm].parm_cat
                , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
                );
        } else {
            snprintf(response, sizeof (response)
                , "\"%s\":{\"value\": false,\"enabled\":true"
                  ",\"category\":%d, \"type\":\"%s\"}"
                , config_parms[indx_parm].parm_name.c_str()
                , config_parms[indx_parm].parm_cat
                , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
                );
        }
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
        conf_edit_list(webui->motapp->cam_list[indx_cam]
            , config_parms[indx_parm].parm_name.c_str()
            , parm_list
            , config_parms[indx_parm].parm_cat);

        snprintf(response, sizeof (response)
            , "\"%s\":{\"value\": \"%s\",\"enabled\":true"
              ",\"category\":%d, \"type\":\"%s\",\"list\":%s }"
            , config_parms[indx_parm].parm_name.c_str()
            , parm_val
            , config_parms[indx_parm].parm_cat
            , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
            , parm_list
            );

    } else {
        snprintf(response, sizeof (response)
            , "\"%s\":{\"value\": \"%s\",\"enabled\":true"
              ",\"category\":%d, \"type\":\"%s\"}"
            , config_parms[indx_parm].parm_name.c_str()
            , parm_val
            , config_parms[indx_parm].parm_cat
            , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
            );
    }
    webu_write(webui, response);

}

static void webu_process_json_parms(struct webui_ctx *webui, int indx_cam)
{
    int indx_parm, first;
    char response[WEBUI_LEN_RESP];

    indx_parm = 0;
    first = true;
    while ((config_parms[indx_parm].parm_name != "") ) {
        if ((config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER)) {
            indx_parm++;
            continue;
        }
        if (first) {
            first = false;
            snprintf(response, sizeof (response), "%s","{");
            webu_write(webui, response);
        } else {
            snprintf(response, sizeof (response), "%s",",");
            webu_write(webui, response);
        }
        if (config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) {
            snprintf(response, sizeof (response)
                , "\"%s\":{\"value\":\"\",\"enabled\":false"
                  ",\"category\":%d, \"type\":\"%s\"}"
                , config_parms[indx_parm].parm_name.c_str()
                , config_parms[indx_parm].parm_cat
                , conf_type_desc(config_parms[indx_parm].parm_type).c_str()
                );
            webu_write(webui, response);
        } else {
           webu_process_json_parm(webui, indx_cam, indx_parm);
        }
        indx_parm++;
    }
    snprintf(response, sizeof (response), "%s","}");
    webu_write(webui, response);

}

static void webu_process_json_config(struct webui_ctx *webui)
{
    int indx_cam, first;
    char response[WEBUI_LEN_RESP];

    indx_cam = 0;
    first = true;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        if (first) {
            first = false;
            snprintf(response, sizeof (response), "%s","{");
            webu_write(webui, response);
        } else {
            snprintf(response, sizeof (response), "%s",",");
            webu_write(webui, response);
        }
        snprintf(response, sizeof (response)
            , "\"cam%d\": ",webui->motapp->cam_list[indx_cam]->conf->camera_id);
        webu_write(webui, response);

        webu_process_json_parms(webui, indx_cam);

        indx_cam++;
    }
    snprintf(response, sizeof (response), "%s","}");
    webu_write(webui, response);

    return;

}

static void webu_process_json_cameras(struct webui_ctx *webui)
{
    int indx_cam;
    char response[WEBUI_LEN_RESP];

    /* Get the count */
    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }
    snprintf(response, sizeof (response)
        ,"{\"count\" : %d", indx_cam - 1);
    webu_write(webui, response);

    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        snprintf(response, sizeof(response), ",\"%d\": ", indx_cam) ;
        webu_write(webui, response);

        if (indx_cam == 0) {
            snprintf(response, sizeof(response), "{\"name\": \"default\" ");
        } else if (webui->motapp->cam_list[indx_cam]->conf->camera_name == "") {
            snprintf(response, sizeof(response), "{\"name\": \"camera %d\" "
                , webui->motapp->cam_list[indx_cam]->conf->camera_id);
        } else {
            snprintf(response, sizeof(response), "{\"name\": \"%s\" "
                , webui->motapp->cam_list[indx_cam]->conf->camera_name.c_str());
        }
        webu_write(webui, response);

        snprintf(response, sizeof(response), ",\"id\": %d "
            , webui->motapp->cam_list[indx_cam]->conf->camera_id);
        webu_write(webui, response);

        if (indx_cam == 0) {
            snprintf(response, sizeof(response), "%s","}");
        } else {
            if (webui->motapp->cam_list[0]->conf->stream_port != 0) {
                snprintf(response, sizeof(response)
                    , ",\"url\": \"%s://%s:%d/%d/\"} "
                    , webui->hostproto, webui->hostname
                    , webui->motapp->cam_list[0]->conf->stream_port
                    , webui->motapp->cam_list[indx_cam]->conf->camera_id);
            } else {
                snprintf(response, sizeof(response)
                    , ",\"url\": \"%s://%s:%d/\"} "
                    , webui->hostproto
                    , webui->hostname
                    , webui->motapp->cam_list[indx_cam]->conf->stream_port);
            }
        }
        webu_write(webui, response);

        indx_cam++;
    }
    snprintf(response, sizeof (response), "%s","}");
    webu_write(webui, response);

    return;

}

static void webu_process_json_categories(struct webui_ctx *webui)
{
    int indx_cat;
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof(response),"%s","{");
    webu_write(webui, response);

    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            snprintf(response, sizeof(response),"%s",",");
            webu_write(webui, response);
        }
        snprintf(response, sizeof(response), "\"%d\": ", indx_cat) ;
        webu_write(webui, response);

        if (indx_cat == PARM_CAT_00) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"system\",\"display\":\"System\"}");
        } else if (indx_cat == PARM_CAT_01) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"camera\",\"display\":\"Camera\"}");
        } else if (indx_cat == PARM_CAT_02) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"source\",\"display\":\"Camera Source\"}");
        } else if (indx_cat == PARM_CAT_03) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"image\",\"display\":\"Image\"}");
        } else if (indx_cat == PARM_CAT_04) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"overlay\",\"display\":\"Overlays\"}");
        } else if (indx_cat == PARM_CAT_05) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"method\",\"display\":\"Method\"}");
        } else if (indx_cat == PARM_CAT_06) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"masks\",\"display\":\"Masks\"}");
        } else if (indx_cat == PARM_CAT_07) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"detect\",\"display\":\"Detection\"}");
        } else if (indx_cat == PARM_CAT_08) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"scripts\",\"display\":\"Scripts\"}");
        } else if (indx_cat == PARM_CAT_09) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"picture\",\"display\":\"Picture\"}");
        } else if (indx_cat == PARM_CAT_10) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"movie\",\"display\":\"Movie\"}");
        } else if (indx_cat == PARM_CAT_11) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"timelapse\",\"display\":\"Timelapse\"}");
        } else if (indx_cat == PARM_CAT_12) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"pipes\",\"display\":\"Pipes\"}");
        } else if (indx_cat == PARM_CAT_13) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"webcontrol\",\"display\":\"Web Control\"}");
        } else if (indx_cat == PARM_CAT_14) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"streams\",\"display\":\"Web Stream\"}");
        } else if (indx_cat == PARM_CAT_15) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"database\",\"display\":\"Database\"}");
        } else if (indx_cat == PARM_CAT_16) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"sql\",\"display\":\"SQL\"}");
        } else if (indx_cat == PARM_CAT_17) {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"track\",\"display\":\"Tracking\"}");
        } else {
            snprintf(response, sizeof(response),"%s"
                , "{\"name\":\"unk\",\"display\":\"Unknown\"}");
        }
        webu_write(webui, response);

        indx_cat++;
    }

    snprintf(response, sizeof(response), "%s","}");
    webu_write(webui, response);

    return;

}

int webu_process_json(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webui->resptype = 2;

    snprintf(response, sizeof (response),"%s"
        ,"{\"version\" : \"" VERSION "\"");
    webu_write(webui, response);

    snprintf(response, sizeof (response), "%s",",\"cameras\" : ");
    webu_write(webui, response);

    webu_process_json_cameras(webui);

    snprintf(response, sizeof (response), "%s",",\"configuration\" : ");
    webu_write(webui, response);

    webu_process_json_config(webui);

    snprintf(response, sizeof (response), "%s",",\"categories\" : ");
    webu_write(webui, response);

    webu_process_json_categories(webui);

    snprintf(response, sizeof (response), "%s","}");
    webu_write(webui, response);

    return 0;

}

int webu_process_config(struct webui_ctx *webui)
{

    int retcd;

    retcd = 0;

    if (mystreq(webui->uri_cmd1,"config") &&
        mystreq(webui->uri_cmd2,"set")) {
        retcd = webu_process_config_set(webui);

    } else if (mystreq(webui->uri_cmd1,"config") &&
               mystreq(webui->uri_cmd2,"get")) {
        webu_text_get_query(webui);

    } else if (mystreq(webui->uri_cmd1,"config") &&
               mystreq(webui->uri_cmd2,"list")) {
        webu_text_list(webui);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            , webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);

    }

    return retcd;

}

int webu_process_track(struct webui_ctx *webui)
{
    /* Call the tracking move functions as requested */
    struct ctx_coord cent;
    int retcd;

    if (mystreq(webui->uri_cmd2, "center")) {
        webui->motapp->cam_list[webui->thread_nbr]->frame_skip = track_center(webui->motapp->cam_list[webui->thread_nbr], 0, 1, 0, 0);
        retcd = 0;
    } else if (mystreq(webui->uri_cmd2, "set")) {
        if (mystreq(webui->uri_parm1, "pan")) {
            cent.width = webui->motapp->cam_list[webui->thread_nbr]->imgs.width;
            cent.height = webui->motapp->cam_list[webui->thread_nbr]->imgs.height;
            cent.x = atoi(webui->uri_value1);
            cent.y = 0;
            webui->motapp->cam_list[webui->thread_nbr]->frame_skip = track_move(webui->motapp->cam_list[webui->thread_nbr]
                ,webui->motapp->cam_list[webui->thread_nbr]->video_dev
                ,&cent, &webui->motapp->cam_list[webui->thread_nbr]->imgs, 1);

            cent.width = webui->motapp->cam_list[webui->thread_nbr]->imgs.width;
            cent.height = webui->motapp->cam_list[webui->thread_nbr]->imgs.height;
            cent.x = 0;
            cent.y = atoi(webui->uri_value2);
            webui->motapp->cam_list[webui->thread_nbr]->frame_skip = track_move(webui->motapp->cam_list[webui->thread_nbr]
                ,webui->motapp->cam_list[webui->thread_nbr]->video_dev
                ,&cent, &webui->motapp->cam_list[webui->thread_nbr]->imgs, 1);
            retcd = 0;
        } else if (mystrceq(webui->uri_parm1, "x")) {
            webui->motapp->cam_list[webui->thread_nbr]->frame_skip = track_center(webui->motapp->cam_list[webui->thread_nbr]
                , webui->motapp->cam_list[webui->thread_nbr]->video_dev, 1
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

static void webu_clientip(struct webui_ctx *webui)
{
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
    if (webui->cam != NULL ) {
        if (webui->cam->conf->webcontrol_ipv6) {
            is_ipv6 = TRUE;
        }
    } else {
        if (webui->motapp->cam_list[0]->conf->webcontrol_ipv6) {
            is_ipv6 = TRUE;
        }
    }

    con_info = MHD_get_connection_info(webui->connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (is_ipv6) {
        con_socket6 = (struct sockaddr_in6 *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET6, &con_socket6->sin6_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", "Unknown");
        } else {
            if (strncmp(client,"::ffff:",7) == 0) {
                snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", client + 7);
            } else {
                snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", client);
            }
        }
    } else {
        con_socket4 = (struct sockaddr_in *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET, &con_socket4->sin_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            snprintf(webui->clientip, WEBUI_LEN_URLI, "%s", "Unknown");
        } else {
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client);
        }
    }
    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),webui->clientip);

}

static void webu_hostname(struct webui_ctx *webui, int ctrl)
{

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
    if (hdr != NULL) {
        snprintf(webui->hostname, WEBUI_LEN_PARM, "%s", hdr);
        /* IPv6 addresses have :'s in them so special case them */
        if (webui->hostname[0] == '[') {
            en_pos = strstr(webui->hostname, "]");
            if (en_pos != NULL) {
                host_len = en_pos - webui->hostname + 2;
                snprintf(webui->hostname, host_len, "%s", hdr);
            }
        } else {
            en_pos = strstr(webui->hostname, ":");
            if (en_pos != NULL) {
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
    if (ctrl) {
        if (webui->cam->conf->webcontrol_tls) {
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    } else {
        if (webui->cam->conf->stream_tls) {
            snprintf(webui->hostproto,6,"%s","https");
        } else {
            snprintf(webui->hostproto,6,"%s","http");
        }
    }

    return;
}

static mhdrslt webu_mhd_digest_fail(struct webui_ctx *webui,int signal_stale)
{
    /* Create a denied response to user*/
    struct MHD_Response *response;
    mhdrslt retcd;

    webui->authenticated = FALSE;

    response = MHD_create_response_from_buffer(strlen(webui->auth_denied)
        ,(void *)webui->auth_denied, MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_auth_fail_response(webui->connection, webui->auth_realm
        ,webui->auth_opaque, response
        ,(signal_stale == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);

    MHD_destroy_response(response);

    return retcd;
}

static mhdrslt webu_mhd_digest(struct webui_ctx *webui)
{
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
    if (mystrne(user, webui->auth_user)) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), webui->clientip);
        if (user != NULL) {
            free(user);
        }
        return webu_mhd_digest_fail(webui, MHD_NO);
    }
    if (user != NULL) {
        free(user);
    }

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

static mhdrslt webu_mhd_basic_fail(struct webui_ctx *webui)
{
    /* Create a denied response to user*/
    struct MHD_Response *response;
    int retcd;

    webui->authenticated = FALSE;

    response = MHD_create_response_from_buffer(strlen(webui->auth_denied)
        ,(void *)webui->auth_denied, MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_basic_auth_fail_response (webui->connection, webui->auth_realm, response);

    MHD_destroy_response(response);

    if (retcd == MHD_YES) {
        return MHD_YES;
    } else {
        return MHD_NO;
    }

}

static mhdrslt webu_mhd_basic(struct webui_ctx *webui)
{
    /* Perform Basic Authentication.  */
    char *user, *pass;

    pass = NULL;
    user = NULL;

    user = MHD_basic_auth_get_username_password (webui->connection, &pass);
    if ((user == NULL) || (pass == NULL)) {
        webu_free_var(user);
        webu_free_var(pass);
        return webu_mhd_basic_fail(webui);
    }

    if ((mystrne(user, webui->auth_user)) || (mystrne(pass, webui->auth_pass))) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"),webui->clientip);
        webu_free_var(user);
        webu_free_var(pass);
        return webu_mhd_basic_fail(webui);
    }

    webu_free_var(user);
    webu_free_var(pass);

    webui->authenticated = TRUE;

    return MHD_YES;

}

static void webu_mhd_auth_parse(struct webui_ctx *webui, int ctrl)
{
    int auth_len;
    char *col_pos;

    /* Parse apart the user:pass provided*/
    webu_free_var(webui->auth_user);
    webu_free_var(webui->auth_pass);

    if (ctrl) {
        auth_len = webui->cam->conf->webcontrol_authentication.length();
        col_pos =(char*) strstr(webui->cam->conf->webcontrol_authentication.c_str() ,":");
        if (col_pos == NULL) {
            webui->auth_user = (char*)mymalloc(auth_len+1);
            webui->auth_pass = (char*)mymalloc(2);
            snprintf(webui->auth_user, auth_len + 1, "%s"
                ,webui->cam->conf->webcontrol_authentication.c_str());
            snprintf(webui->auth_pass, 2, "%s","");
        } else {
            webui->auth_user = (char*)mymalloc(auth_len - strlen(col_pos) + 1);
            webui->auth_pass =(char*)mymalloc(strlen(col_pos));
            snprintf(webui->auth_user, auth_len - strlen(col_pos) + 1, "%s"
                ,webui->cam->conf->webcontrol_authentication.c_str());
            snprintf(webui->auth_pass, strlen(col_pos), "%s", col_pos + 1);
        }
    } else {
        auth_len = strlen(webui->cam->conf->stream_authentication.c_str());
        col_pos =(char*) strstr(webui->cam->conf->stream_authentication.c_str(),":");
        if (col_pos == NULL) {
            webui->auth_user = (char*)mymalloc(auth_len+1);
            webui->auth_pass = (char*)mymalloc(2);
            snprintf(webui->auth_user, auth_len + 1, "%s"
                ,webui->cam->conf->stream_authentication.c_str());
            snprintf(webui->auth_pass, 2, "%s","");
        } else {
            webui->auth_user = (char*)mymalloc(auth_len - strlen(col_pos) + 1);
            webui->auth_pass = (char*)mymalloc(strlen(col_pos));
            snprintf(webui->auth_user, auth_len - strlen(col_pos) + 1, "%s"
                ,webui->cam->conf->stream_authentication.c_str());
            snprintf(webui->auth_pass, strlen(col_pos), "%s", col_pos + 1);
        }
    }

}

static mhdrslt webu_mhd_auth(struct webui_ctx *webui, int ctrl)
{

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

    if (ctrl) {
        /* Authentication for the webcontrol*/
        if (webui->cam->conf->webcontrol_authentication == "") {
            webui->authenticated = TRUE;
            if (webui->cam->conf->webcontrol_auth_method != 0) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No webcontrol user:pass provided"));
            }
            return MHD_YES;
        }

        if (webui->auth_user == NULL) {
            webu_mhd_auth_parse(webui, ctrl);
        }

        if (webui->cam->conf->webcontrol_auth_method == 1) {
            return webu_mhd_basic(webui);
        } else if (webui->cam->conf->webcontrol_auth_method == 2) {
            return webu_mhd_digest(webui);
        }

    } else {
        /* Authentication for the streams */
        if (webui->cam->conf->stream_authentication == "") {
            webui->authenticated = TRUE;
            if (webui->cam->conf->stream_auth_method != 0) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No stream user:pass provided"));
            }
            return MHD_YES;
        }

        if (webui->auth_user == NULL) {
            webu_mhd_auth_parse(webui, ctrl);
        }

        if (webui->cam->conf->stream_auth_method == 1) {
            return webu_mhd_basic(webui);
        } else if (webui->cam->conf->stream_auth_method == 2) {
            return webu_mhd_digest(webui);
        }
    }

    webui->authenticated = TRUE;
    return MHD_YES;

}

/* Send the response that we created back to the user.  */
static mhdrslt webu_mhd_send(struct webui_ctx *webui, int ctrl)
{
    /* If the user
     * provided a really bad URL, then we couldn't determine which Motion context
     * they were wanting.  In this situation, we have a webui->cam = NULL and we
     * don't know whether it came from a html or text request.  In this situation
     * we use the MHD defaults and skip adding CORS/Content type.  (There isn't any
     * Motion context so we can't tell where to look)
     * The ctrl parameter is a boolean which just says whether the request is for
     * the webcontrol versus stream
     */
    mhdrslt retcd;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen(webui->resp_page)
        ,(void *)webui->resp_page, MHD_RESPMEM_PERSISTENT);
    if (!response) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->cam != NULL) {
        if (ctrl) {
            if (webui->cam->conf->webcontrol_cors_header != "") {
                MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
                    , webui->cam->conf->webcontrol_cors_header.c_str());
            }

            if (webui->resptype == 0) {
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
            } else if (webui->resptype == 1) {
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
            } else if (webui->resptype == 2) {
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json;");
            } else {
                MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
            }
        } else {
            if (webui->cam->conf->stream_cors_header !="") {
                MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
                    , webui->cam->conf->stream_cors_header.c_str());
            }
            MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
        }
    }

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Assign the type of stream that is being answered*/
static void webu_answer_strm_type(struct webui_ctx *webui)
{
    if ((mystreq(webui->uri_cmd1,"stream")) ||
        (mystreq(webui->uri_camid,"stream")) ||
        (strlen(webui->uri_camid) == 0)) {
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else if ((mystreq(webui->uri_cmd1,"substream")) ||
        (mystreq(webui->uri_camid,"substream"))) {
        webui->cnct_type = WEBUI_CNCT_SUB;

    } else if ((mystreq(webui->uri_cmd1,"motion")) ||
        (mystreq(webui->uri_camid,"motion"))) {
        webui->cnct_type = WEBUI_CNCT_MOTION;

    } else if ((mystreq(webui->uri_cmd1,"source")) ||
        (mystreq(webui->uri_camid,"source"))) {
        webui->cnct_type = WEBUI_CNCT_SOURCE;

    } else if ((mystreq(webui->uri_cmd1,"secondary")) ||
        (mystreq(webui->uri_camid,"secondary"))) {
        if (webui->cam->algsec_inuse) {
            webui->cnct_type = WEBUI_CNCT_SECONDARY;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        }

    } else if ((mystreq(webui->uri_cmd1,"current")) ||
        (mystreq(webui->uri_camid,"current"))) {
        webui->cnct_type = WEBUI_CNCT_STATIC;

    } else if ((strlen(webui->uri_camid) > 0) &&
        (strlen(webui->uri_cmd1) == 0)) {
        webui->cnct_type = WEBUI_CNCT_FULL;

    } else {
        webui->cnct_type = WEBUI_CNCT_UNKNOWN;
    }

}

/* Process the post data command */
static mhdrslt webu_answer_ctrl_post(struct webui_ctx *webui)
{
    mhdrslt retcd;
    int indx;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"processing post");

    /* TODO:  Handle more commands sent from the web page */

    if (webui->post_cmd == 1) {
        for (indx = 0; indx < webui->post_sz; indx++) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"key: %s  value: %s  size: %ld "
                , webui->post_info[indx].key_nm
                , webui->post_info[indx].key_val
                , webui->post_info[indx].key_sz
            );
        }
    }

    webu_html_main(webui);

    retcd = webu_mhd_send(webui, true);
    if (retcd == MHD_NO) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"send post page failed");
    }

    return MHD_YES;

}

/*Append more data on to an existing entry in the post info structure */
static void webu_iterate_post_append(struct webui_ctx *webui, int indx
        , const char *data, size_t datasz)
{

    webui->post_info[indx].key_val = (char*)realloc(
        webui->post_info[indx].key_val
        , webui->post_info[indx].key_sz + datasz + 1);

    memset(webui->post_info[indx].key_val +
        webui->post_info[indx].key_sz, 0, datasz + 1);

    if (datasz > 0) {
        memcpy(webui->post_info[indx].key_val +
            webui->post_info[indx].key_sz, data, datasz);
    }

    webui->post_info[indx].key_sz += datasz;

}

/*Create new entry in the post info structure */
static void webu_iterate_post_new(struct webui_ctx *webui, const char *key
        , const char *data, size_t datasz)
{
    int retcd;

    webui->post_sz++;
    if (webui->post_sz == 1) {
        webui->post_info = (ctx_key *)malloc(sizeof(struct ctx_key));
    } else {
        webui->post_info = (ctx_key *)realloc(webui->post_info
            , webui->post_sz * sizeof(struct ctx_key));
    }

    webui->post_info[webui->post_sz-1].key_nm = (char*)malloc(strlen(key)+1);
    retcd = snprintf(webui->post_info[webui->post_sz-1].key_nm, strlen(key)+1, "%s", key);

    webui->post_info[webui->post_sz-1].key_val = (char*)malloc(datasz+1);
    memset(webui->post_info[webui->post_sz-1].key_val,0,datasz+1);
    if (datasz > 0) {
        memcpy(webui->post_info[webui->post_sz-1].key_val, data, datasz);
    }

    webui->post_info[webui->post_sz-1].key_sz = datasz;

    if (retcd < 0) {
        printf("Error processing post data\n");
    }

}

static mhdrslt webu_iterate_post (void *ptr, enum MHD_ValueKind kind
        , const char *key, const char *filename, const char *content_type
        , const char *transfer_encoding, const char *data, uint64_t off, size_t datasz)
{
    struct webui_ctx *webui = (webui_ctx *)ptr;
    (void) kind;               /* Unused. Silent compiler warning. */
    (void) filename;           /* Unused. Silent compiler warning. */
    (void) content_type;       /* Unused. Silent compiler warning. */
    (void) transfer_encoding;  /* Unused. Silent compiler warning. */
    (void) off;                /* Unused. Silent compiler warning. */
    int indx;

    if (mystreq(key, "cmdid")) {
        webui->post_cmd = atoi(data);
    } else if (mystreq(key, "trailer") && (datasz ==0)) {
        return MHD_YES;
    }

    for (indx=0; indx < webui->post_sz; indx++) {
        if (mystreq(webui->post_info[indx].key_nm, key)) {
            break;
        }
    }
    if (indx < webui->post_sz) {
        webu_iterate_post_append(webui, indx, data, datasz);
    } else {
        webu_iterate_post_new(webui, key, data, datasz);
    }

    return MHD_YES;
}


static mhdrslt webu_answer_ctrl_get(struct webui_ctx *webui)
{
    int retcd;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"processing get");

    /* Throw bad URLS back to user*/
    if ((webui->cam ==  NULL) || (strlen(webui->url) == 0)) {
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
        return retcd;
    }

    if (webui->cam->finish_cam) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Shutting down camera"));
        return MHD_NO;
    }

    if (strlen(webui->clientip) == 0) {
        webu_clientip(webui);
    }

    webu_hostname(webui, TRUE);

    if (!webui->authenticated) {
        retcd = webu_mhd_auth(webui, TRUE);
        if (!webui->authenticated) {
            return retcd;
        }
    }

    if ((webui->motapp->cam_list[0]->conf->webcontrol_interface == 0) ||
        (webui->motapp->cam_list[0]->conf->webcontrol_interface == 3)) {
        webu_html_main(webui);

    } else if ((webui->motapp->cam_list[0]->conf->webcontrol_interface == 1) ||
        (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2)) {
        webu_text_main(webui);

    } else {    /* Default */
        webu_html_main(webui);

    }

    retcd = webu_mhd_send(webui, TRUE);
    if (retcd == MHD_NO) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed."));
    }

    return retcd;

}

/* Answer the connection request for the webcontrol*/
static mhdrslt webu_answer_ctrl(void *cls, struct MHD_Connection *connection, const char *url
        , const char *method, const char *version, const char *upload_data, size_t *upload_data_size
        , void **ptr)
{
    mhdrslt retcd;
    struct webui_ctx *webui =(struct webui_ctx *) *ptr;

    /* Eliminate compiler warnings */
    (void)cls;
    (void)url;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;

    /* Per MHD docs, this is called twice and we should process the second call */
    webui->cnct_type = WEBUI_CNCT_CONTROL;

    mythreadname_set("wu", 0,NULL);

    webui->connection = connection;

    if (webui->mhd_first) {
        webui->mhd_first = FALSE;
        if (mystreq(method,"POST")) {
            webui->postprocessor = MHD_create_post_processor (webui->connection
                , POSTBUFFERSIZE, webu_iterate_post, (void *)webui);
            if (webui->postprocessor == NULL) {
                return MHD_NO;
            }
            webui->cnct_method = WEBUI_METHOD_POST;
        } else {
            webui->cnct_method = WEBUI_METHOD_GET;
        }

        return MHD_YES;
    }

    if (mystreq(method,"POST")) {
        if (*upload_data_size != 0) {
            retcd = MHD_post_process (webui->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
        } else {
            retcd = webu_answer_ctrl_post(webui);
        }
    } else {
        retcd = webu_answer_ctrl_get(webui);
    }

    return retcd;

}

/* Answer the connection request for a stream */
static mhdrslt webu_answer_strm(void *cls, struct MHD_Connection *connection, const char *url
        , const char *method, const char *version, const char *upload_data, size_t *upload_data_size
        , void **ptr)
{
    mhdrslt retcd;
    struct webui_ctx *webui =(struct webui_ctx *) *ptr;

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

    if (mystrne(method, "GET")) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Invalid Method requested: %s"),method);
        return MHD_NO;
    }

    mythreadname_set("st", 0,NULL);

    webui->connection = connection;

    /* Throw bad URLS back to user*/
    if ((webui->cam ==  NULL) || (strlen(webui->url) == 0)) {
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
        return retcd;
    }

    if (webui->cam->finish_cam) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Shutting down camera"));
        return MHD_NO;
    }


    /* Do not answer a request until the motion loop has completed at least once */
    if (webui->cam->passflag == 0) {
        return MHD_NO;
    }

    if (strlen(webui->clientip) == 0) {
        webu_clientip(webui);
    }

    webu_hostname(webui, FALSE);

    if (!webui->authenticated) {
        retcd = webu_mhd_auth(webui, FALSE);
        if (!webui->authenticated) {
            return retcd;
        }
    }

    webu_answer_strm_type(webui);

    if (webui->cnct_type == WEBUI_CNCT_STATIC) {
        retcd = webu_stream_static(webui);
        if (retcd == MHD_NO) {
            webu_badreq(webui);
            retcd = webu_mhd_send(webui, FALSE);
        }
    } else if (webui->cnct_type != WEBUI_CNCT_UNKNOWN) {
        retcd = webu_stream_mjpeg(webui);
        if (retcd == MHD_NO) {
            webu_badreq(webui);
            retcd = webu_mhd_send(webui, FALSE);
        }
    } else {
        webu_badreq(webui);
        retcd = webu_mhd_send(webui, FALSE);
    }

    if (retcd == MHD_NO) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed %d"),retcd);
    }
    return retcd;

}

/* Initialize the MHD answer when it is the webcontrol or all streams on a single port */
static void *webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection)
{
    /* This is called at the very start of getting a request before the "answer"
     * is processed.  There are two variations of this and the difference is how
     * we call the webu_context_init.  When we are processing for the webcontrol or
     * the stream port specified in the motion.conf file, we pass into the init function
     * the motion application context.  The other version of the init is used when the
     * user specifies a unique port for each camera.  In this situation, the full list
     * context is passed in as a null and the context of the camera desired is passed
     * instead.
     * When this function is processed, we basically only have the URL that the user requested
     * so we initialize everything and then parse out the URL to determine what the user is
     * asking.
     */

    struct ctx_motapp *motapp = (struct ctx_motapp *)cls;
    struct webui_ctx *webui;
    int retcd;

    (void)connection;

    /* Set the thread name to connection until we know whether control or stream answers*/
    mythreadname_set("cn", 0,NULL);

    webui =(struct webui_ctx* ) malloc(sizeof(struct webui_ctx));

    webu_context_init(motapp, NULL, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    retcd = webu_parseurl(webui);
    if (retcd != 0) {
        webu_parseurl_reset(webui);
        memset(webui->url,'\0',WEBUI_LEN_URLI);
    }

    webu_parms_edit(webui);

    return webui;
}

/* Initialize the MHD answer when it is a connection for each camera */
static void *webu_mhd_init_one(void *cls, const char *uri, struct MHD_Connection *connection)
{
    /* This function initializes all the webui variables as we are getting a request.  This
     * variation of the init is the one used when the user has specified a unique port number
     * for each camera.  The variation is in how the webu_context_init is invoked.  This passes
     * in a NULL for the motion app and instead assigns the particular camera context to webui->cam
     */
    struct ctx_cam *cam =(struct ctx_cam *) cls;
    struct webui_ctx *webui;
    int retcd;

    (void)connection;

    /* Set the thread name to connection until we know whether control or stream answers*/
    mythreadname_set("cn", 0,NULL);

    webui =(struct webui_ctx*) malloc(sizeof(struct webui_ctx));

    webu_context_init(NULL, cam, webui);
    webui->mhd_first = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    retcd = webu_parseurl(webui);
    if (retcd != 0) {
        webu_parseurl_reset(webui);
        memset(webui->url,'\0',WEBUI_LEN_URLI);
    }

    webu_parms_edit(webui);

    return webui;
}

/* Clean up our variables when the MHD connection closes */
static void webu_mhd_deinit(void *cls, struct MHD_Connection *connection
        , void **con_cls, enum MHD_RequestTerminationCode toe)
{
    struct webui_ctx *webui =(struct webui_ctx *) *con_cls;

    /* Eliminate compiler warnings */
    (void)connection;
    (void)cls;
    (void)toe;

    if (webui->cnct_type == WEBUI_CNCT_FULL ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.norm.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SUB ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.sub.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.motion.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.source.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SECONDARY ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.secondary.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_STATIC ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            webui->cam->stream.norm.cnct_count--;
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    }

    if (webui != NULL) {
        if (webui->cnct_method == WEBUI_METHOD_POST) {
            MHD_destroy_post_processor (webui->postprocessor);
        }
        webu_context_free(webui);
    }

    return;
}

/* Validate that the MHD version installed can process basic authentication */
static void webu_mhd_features_basic(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: available"));
        } else {
            if ((mhdst->ctrl) &&
                (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_auth_method == 1)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_auth_method = 0;
            } else if ((!mhdst->ctrl) &&
                (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_auth_method == 1)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_auth_method = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process digest authentication */
static void webu_mhd_features_digest(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: available"));
        } else {
            if ((mhdst->ctrl) &&
                (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_auth_method == 2)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_auth_method = 0;
            } else if ((!mhdst->ctrl) &&
                (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_auth_method == 2)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_auth_method = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process IPV6 */
static void webu_mhd_features_ipv6(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        if (mhdst->ipv6) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old ipv6 disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("IPV6: available"));
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("IPV6: disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #endif
}

/* Validate that the MHD version installed can process tls */
static void webu_mhd_features_tls(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        if ((mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls)) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls = 0;
        } else if ((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls)) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls = 0;
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: available"));
        } else {
            if ((mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls = 0;
            } else if ((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
            }
        }
    #endif
}

/* Validate the features that MHD can support */
static void webu_mhd_features(struct mhdstart_ctx *mhdst)
{
    webu_mhd_features_basic(mhdst);

    webu_mhd_features_digest(mhdst);

    webu_mhd_features_ipv6(mhdst);

    webu_mhd_features_tls(mhdst);

}
/* Load a either the key or cert file for MHD*/
static char *webu_mhd_loadfile(const char *fname)
{
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
        if (infile != NULL) {
            fseek(infile, 0, SEEK_END);
            file_size = ftell(infile);
            if (file_size > 0 ) {
                file_char = (char*)mymalloc(file_size +1);
                fseek(infile, 0, SEEK_SET);
                read_size = fread(file_char, file_size, 1, infile);
                if (read_size > 0 ) {
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

/* Validate that we have the files needed for tls*/
static void webu_mhd_checktls(struct mhdstart_ctx *mhdst)
{
    if (mhdst->ctrl) {
        if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {
            if ((mhdst->motapp->cam_list[0]->conf->webcontrol_cert == "") || (mhdst->tls_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
                mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
            }
            if ((mhdst->motapp->cam_list[0]->conf->webcontrol_key == "") || (mhdst->tls_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
                mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
            }
        }
    } else {
        if (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls) {
            if ((mhdst->motapp->cam_list[0]->conf->webcontrol_cert == "") || (mhdst->tls_cert == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls = 0;
            }
            if ((mhdst->motapp->cam_list[0]->conf->webcontrol_key == "") || (mhdst->tls_key == NULL)) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
                mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls = 0;
            }
        }
    }

}

/* Set the initialization function for MHD to call upon getting a connection */
static void webu_mhd_opts_init(struct mhdstart_ctx *mhdst)
{
    /* If the connection is related to the webcontrol or the stream specified in the
     * motion.conf file, then we pass in the full motion application context.  If
     * the MHD connection is only going to be for a single camera (a unique port for
     * each camera), then we call a different init function which only wants the single
     * motion context for that particular camera.
     */
    if ((!mhdst->ctrl) && (mhdst->indxthrd != 0)) {
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init_one;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp->cam_list[mhdst->indxthrd];
        mhdst->mhd_opt_nbr++;
    } else {
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp;
        mhdst->mhd_opt_nbr++;
    }

}

/* Set the MHD option on the function to call when the connection closes */
static void webu_mhd_opts_deinit(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

/* Set the MHD option on acceptable connections */
static void webu_mhd_opts_localhost(struct mhdstart_ctx *mhdst)
{
    if ((mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_localhost)) {
        if (mhdst->ipv6) {
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons(mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;

        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons(mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    } else if((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_localhost)) {
        if (mhdst->ipv6) {
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons(mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;
        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons(mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    }

}

/* Set the mhd digest options */
static void webu_mhd_opts_digest(struct mhdstart_ctx *mhdst)
{

    if (((mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_auth_method == 2)) ||
        ((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_auth_method == 2))) {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
        if (mhdst->ctrl) {
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->motapp->webcontrol_digest_rand);
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp->webcontrol_digest_rand;
        } else if (mhdst->indxthrd == 0) {
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->motapp->webstream_digest_rand);
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp->webstream_digest_rand;
        } else {
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->motapp->cam_list[mhdst->indxthrd]->stream.digest_rand);
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp->cam_list[mhdst->indxthrd]->stream.digest_rand;
        }
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

/* Set the MHD options needed when we want TLS connections */
static void webu_mhd_opts_tls(struct mhdstart_ctx *mhdst)
{
    if ((( mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls)) ||
        ((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls))) {

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

/* Set all the MHD options based upon the configuration parameters*/
static void webu_mhd_opts(struct mhdstart_ctx *mhdst)
{
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

/* Set the mhd start up flags */
static void webu_mhd_flags(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION;

    if (mhdst->ipv6) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_DUAL_STACK;
    }

    if ((mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->webcontrol_tls)) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    } else if ((!mhdst->ctrl) && (mhdst->motapp->cam_list[mhdst->indxthrd]->conf->stream_tls)) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    }

}

/* Print to log what ports and urls are being used for the streams*/
static void webu_strm_ntc(struct ctx_cam **camlst, int indxthrd)
{
    int indx;

    if (indxthrd == 0 ) {
        if (camlst[1] != NULL) {
            indx = 1;
            while (camlst[indx] != NULL) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    ,_("Started camera %d stream on port/camera_id %d/%d")
                    ,camlst[indx]->camera_id
                    ,camlst[indxthrd]->conf->stream_port
                    ,camlst[indx]->camera_id);
                indx++;
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("Started camera %d stream on port %d")
                ,camlst[indxthrd]->camera_id,camlst[indxthrd]->conf->stream_port);
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started camera %d stream on port %d")
            ,camlst[indxthrd]->camera_id,camlst[indxthrd]->conf->stream_port);
    }
}

/* Start the webcontrol */
static void webu_init_ctrl(struct ctx_motapp *motapp)
{
    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        ,_("Starting webcontrol on port %d")
        ,motapp->cam_list[0]->conf->webcontrol_port);

    mhdst.tls_cert = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_cert.c_str());
    mhdst.tls_key  = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_key.c_str());
    mhdst.ctrl = TRUE;
    mhdst.indxthrd = 0;
    mhdst.motapp = motapp;
    mhdst.ipv6 = motapp->cam_list[0]->conf->webcontrol_ipv6;

    /* Set the rand number for webcontrol digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(motapp->webcontrol_digest_rand
        ,sizeof(motapp->webcontrol_digest_rand),"%d",randnbr);

    mhdst.mhd_ops =(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
    webu_mhd_features(&mhdst);
    webu_mhd_opts(&mhdst);
    webu_mhd_flags(&mhdst);

    motapp->webcontrol_daemon = MHD_start_daemon (mhdst.mhd_flags
        ,motapp->cam_list[0]->conf->webcontrol_port
        ,NULL, NULL
        ,&webu_answer_ctrl, motapp->cam_list
        ,MHD_OPTION_ARRAY, mhdst.mhd_ops
        ,MHD_OPTION_END);

    free(mhdst.mhd_ops);
    if (motapp->webcontrol_daemon == NULL) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD"));
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started webcontrol on port %d")
            ,motapp->cam_list[0]->conf->webcontrol_port);
    }

    webu_free_var(mhdst.tls_cert);
    webu_free_var(mhdst.tls_key);

    return;
}

/* Start the webstreams for all cameras on a single port */
static void webu_init_strm_oneport(struct ctx_motapp *motapp)
{
    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , _("Starting all camera streams on port %d")
        , motapp->cam_list[0]->conf->stream_port);

    mhdst.tls_cert = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_cert.c_str());
    mhdst.tls_key  = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_key.c_str());
    mhdst.ctrl = FALSE;
    mhdst.indxthrd = 0;
    mhdst.motapp = motapp;
    mhdst.ipv6 = motapp->cam_list[0]->conf->webcontrol_ipv6;


    /* Set the rand number for webstream digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(motapp->webstream_digest_rand
        ,sizeof(motapp->webstream_digest_rand),"%d",randnbr);

    mhdst.mhd_ops=(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
    webu_mhd_features(&mhdst);
    webu_mhd_opts(&mhdst);
    webu_mhd_flags(&mhdst);

    motapp->webstream_daemon = MHD_start_daemon (
        mhdst.mhd_flags
        , motapp->cam_list[mhdst.indxthrd]->conf->stream_port
        , NULL, NULL
        , &webu_answer_strm, motapp->cam_list
        , MHD_OPTION_ARRAY, mhdst.mhd_ops
        , MHD_OPTION_END);
    free(mhdst.mhd_ops);
    if (motapp->webstream_daemon == NULL) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Unable to start stream for cameras"));
    } else {
        webu_strm_ntc(motapp->cam_list, mhdst.indxthrd);
    }
    webu_free_var(mhdst.tls_cert);
    webu_free_var(mhdst.tls_key);


}

/* Start the webstream with each camera on a different port*/
static void webu_init_strm_multport(struct ctx_motapp *motapp, int indx)
{
    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , _("Starting camera %d stream on port %d")
        , motapp->cam_list[indx]->camera_id
        , motapp->cam_list[indx]->conf->stream_port);

    mhdst.tls_cert = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_cert.c_str());
    mhdst.tls_key  = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_key.c_str());
    mhdst.ctrl = FALSE;
    mhdst.indxthrd = indx;
    mhdst.motapp = motapp;
    mhdst.ipv6 = motapp->cam_list[0]->conf->webcontrol_ipv6;

    /* Set the rand number for webstream digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(motapp->cam_list[mhdst.indxthrd]->stream.digest_rand
        ,sizeof(motapp->cam_list[mhdst.indxthrd]->stream.digest_rand),"%d",randnbr);

    mhdst.mhd_ops=(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
    webu_mhd_features(&mhdst);
    webu_mhd_opts(&mhdst);
    webu_mhd_flags(&mhdst);

    motapp->cam_list[mhdst.indxthrd]->stream.daemon = MHD_start_daemon (
        mhdst.mhd_flags
        , motapp->cam_list[mhdst.indxthrd]->conf->stream_port
        , NULL, NULL
        , &webu_answer_strm, motapp->cam_list[mhdst.indxthrd]
        , MHD_OPTION_ARRAY, mhdst.mhd_ops
        , MHD_OPTION_END);

    free(mhdst.mhd_ops);
    if (motapp->cam_list[mhdst.indxthrd]->stream.daemon == NULL) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Unable to start stream for camera %d")
            , motapp->cam_list[mhdst.indxthrd]->camera_id);
    } else {
        webu_strm_ntc(motapp->cam_list,mhdst.indxthrd);
    }

    webu_free_var(mhdst.tls_cert);
    webu_free_var(mhdst.tls_key);

}

/*Perform validation of the user specified ports for streams and webcontrol*/
static void webu_init_ports(struct ctx_motapp *motapp)
{
    int indx, indx2;

    if (motapp->cam_list[0]->conf->webcontrol_port != 0) {
        indx = 0;
        while (motapp->cam_list[indx] != NULL){
            if ((motapp->cam_list[0]->conf->webcontrol_port == motapp->cam_list[indx]->conf->webcontrol_port)
                && (indx > 0)) {
                motapp->cam_list[indx]->conf->webcontrol_port = 0;
            }

            if (motapp->cam_list[0]->conf->webcontrol_port == motapp->cam_list[indx]->conf->stream_port) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    , _("Duplicate port requested %d")
                    , motapp->cam_list[indx]->conf->stream_port);
                motapp->cam_list[indx]->conf->stream_port = 0;
            }

            indx++;
        }
    }

    /* Now check on the stream ports */
    indx = 0;
    while (motapp->cam_list[indx] != NULL){
        if (motapp->cam_list[indx]->conf->stream_port != 0) {
            indx2 = indx + 1;
            while (motapp->cam_list[indx2] != NULL){
                if (motapp->cam_list[indx]->conf->stream_port == motapp->cam_list[indx2]->conf->stream_port) {
                    if (indx != 0) {
                        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                            , _("Duplicate port requested %d")
                            , motapp->cam_list[indx2]->conf->stream_port);
                    }
                    motapp->cam_list[indx2]->conf->stream_port = 0;
                }
                indx2++;
            }
        }
        indx++;
    }
}

/* Shut down the webcontrol and streams */
void webu_deinit(struct ctx_motapp *motapp)
{
    int indx;

    if (motapp->webcontrol_daemon != NULL) {
        motapp->webcontrol_finish = TRUE;
        MHD_stop_daemon (motapp->webcontrol_daemon);
    }

    if (motapp->webstream_daemon != NULL) {
        motapp->webstream_finish = TRUE;
        MHD_stop_daemon (motapp->webstream_daemon);
    }


    indx = 0;
    while (motapp->cam_list[indx] != NULL){
        if (motapp->cam_list[indx]->stream.daemon != NULL) {
            MHD_stop_daemon (motapp->cam_list[indx]->stream.daemon);
        }
        motapp->cam_list[indx]->stream.daemon = NULL;
        indx++;
    }
}

/* Start the webcontrol and streams */
void webu_init(struct ctx_motapp *motapp)
{
    struct sigaction act;
    int indx;

    /* We need to block some signals otherwise MHD will not function correctly. */
    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    motapp->webcontrol_daemon = NULL;
    motapp->webcontrol_finish = FALSE;

    motapp->webstream_daemon = NULL;
    motapp->webstream_finish = FALSE;

    indx = 0;
    while (motapp->cam_list[indx] != NULL) {
        motapp->cam_list[indx]->stream.daemon = NULL;
        motapp->cam_list[indx]->stream.finish = FALSE;
        indx++;
    }

    webu_init_ports(motapp);

    /* Start the streams */
    if (motapp->cam_list[0]->conf->stream_port != 0 ) {
        webu_init_strm_oneport(motapp);
    } else {
        indx = 1;
        while (motapp->cam_list[indx] != NULL) {
            if (motapp->cam_list[indx]->conf->stream_port != 0 ) {
                webu_init_strm_multport(motapp, indx);
            }
            indx++;
        }
    }

    /* Start the webcontrol */
    if (motapp->cam_list[0]->conf->webcontrol_port != 0 ) {
        webu_init_ctrl(motapp);
    }

    return;

}
