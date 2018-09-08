/*
 *    webu_text.c
 *
 *    Create the text(programatic) interface Motion
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    This module processes the requests associated with the text inferface
 *    of the webcontrol.  This interface is intended to be used by programs
 *    and does not have any user interface to navigate.  The same actions
 *    are available as the HTML as well as a few more.
 *      Additional functions not directly available via HTML
 *          get:    Returns the value of a parameter.
 *          quit:   Terminates motion
 *          list:   Lists all the configuration parameters and values
 *          status  Whether the camera is in pause mode.
 *          connection  Whether the camera connection is working
 *
 */

#include "motion.h"
#include "webu.h"
#include "webu_text.h"
#include "translate.h"

void webu_text_badreq(struct webui_ctx *webui) {
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "Bad Request\n"
        "The server did not understand your request.\n");
    webu_write(webui, response);

    return;
}

static void webu_text_page(struct webui_ctx *webui) {
    /* Write the main page text */
    char response[WEBUI_LEN_RESP];
    int indx;

    snprintf(response, sizeof (response),
        "Motion "VERSION" Running [%d] Camera%s\n0\n",
        webui->cam_count, (webui->cam_count > 1 ? "s" : ""));
    webu_write(webui, response);

    if (webui->cam_threads > 1){
        for (indx = 1; indx < webui->cam_threads; indx++) {
            snprintf(response, sizeof (response), "%d\n", webui->cntlst[indx]->camera_id);
            webu_write(webui, response);
        }
    }
}

static void webu_text_list(struct webui_ctx *webui) {
    /* Write out the options and values */
    char response[WEBUI_LEN_RESP];
    int indx_parm;
    const char *val_parm;

    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cntlst[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_params[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, webui->thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, 0);
        }
        snprintf(response, sizeof (response),"  %s = %s \n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        webu_write(webui, response);

        indx_parm++;
    }

}

static void webu_text_get(struct webui_ctx *webui) {
    /* Write out the option value for one parm */
    char response[WEBUI_LEN_RESP];
    int indx_parm;
    const char *val_parm;
    char temp_name[WEBUI_LEN_PARM];

    /* Search through the depreciated parms and if applicable,
     * get the new parameter name so we can check its webcontrol_parms level
     */
    snprintf(temp_name, WEBUI_LEN_PARM, "%s", webui->uri_value1);
    indx_parm=0;
    while (dep_config_params[indx_parm].name != NULL) {
        if (strcmp(dep_config_params[indx_parm].name, webui->uri_value1) == 0){
            snprintf(temp_name, WEBUI_LEN_PARM, "%s", dep_config_params[indx_parm].newname);
            break;
        }
        indx_parm++;
    }

    indx_parm = 0;

    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cntlst[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            strcmp(webui->uri_parm1,"query") ||
            strcmp(temp_name, config_params[indx_parm].param_name)){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, webui->thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, 0);
        }

        if (strcmp(webui->uri_value1, config_params[indx_parm].param_name) != 0){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("'%s' option is depreciated.  New option name is `%s'")
            ,webui->uri_value1, config_params[indx_parm].param_name);
        }

        snprintf(response, sizeof (response),"%s = %s \nDone\n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        webu_write(webui, response);

        break;
    }

    if (config_params[indx_parm].param_name == NULL) webu_text_badreq(webui);

}

static void webu_text_quit(struct webui_ctx *webui) {
    /* Shut down motion or the associated thread */
    char response[WEBUI_LEN_RESP];

    /* This is the legacy method...(we can do better than signals..).*/
    if (webui->thread_nbr == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("httpd quits"));
        kill(getpid(),SIGQUIT);
        webui->cntlst[0]->webcontrol_finish = TRUE;
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
            _("httpd quits thread %d"),webui->thread_nbr);
        webui->cnt->restart = 0;
        webui->cnt->event_stop = TRUE;
        webui->cnt->finish = 1;
    }

    snprintf(response,sizeof(response)
        ,"quit in progress ... bye \nDone\n");
    webu_write(webui, response);

}

static void webu_text_status(struct webui_ctx *webui) {
    /* Write out the pause/active status */

    char response[WEBUI_LEN_RESP];
    int indx, indx_st;

    if (webui->thread_nbr == 0){
        indx_st = 1;
        if (webui->cam_threads == 1) indx_st = 0;

        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            snprintf(response, sizeof(response),
                "Camera %d Detection status %s\n"
                ,webui->cntlst[indx]->camera_id
                ,(!webui->cntlst[indx]->running)? "NOT RUNNING":
                (webui->cntlst[indx]->pause)? "PAUSE":"ACTIVE");
                webu_write(webui, response);
        }
    } else {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s\n"
            ,webui->cnt->camera_id
            ,(!webui->cnt->running)? "NOT RUNNING":
            (webui->cnt->pause)? "PAUSE":"ACTIVE");
        webu_write(webui, response);
    }

}

static void webu_text_connection(struct webui_ctx *webui) {
    /* Write out the connection status */
    char response[WEBUI_LEN_RESP];
    int indx, indx_st;

    if (webui->thread_nbr == 0){
        indx_st = 1;
        if (webui->cam_threads == 1) indx_st = 0;

        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            snprintf(response,sizeof(response)
                , "Camera %d%s%s %s\n"
                ,webui->cntlst[indx]->camera_id
                ,webui->cntlst[indx]->conf.camera_name ? " -- " : ""
                ,webui->cntlst[indx]->conf.camera_name ? webui->cntlst[indx]->conf.camera_name : ""
                ,(!webui->cntlst[indx]->running)? "NOT RUNNING" :
                (webui->cntlst[indx]->lost_connection)? "Lost connection": "Connection OK");
            webu_write(webui, response);
        }
    } else {
        snprintf(response,sizeof(response)
            , "Camera %d%s%s %s\n"
            ,webui->cnt->camera_id
            ,webui->cnt->conf.camera_name ? " -- " : ""
            ,webui->cnt->conf.camera_name ? webui->cnt->conf.camera_name : ""
            ,(!webui->cnt->running)? "NOT RUNNING" :
             (webui->cnt->lost_connection)? "Lost connection": "Connection OK");
        webu_write(webui, response);
    }

}

static void webu_text_set(struct webui_ctx *webui) {
    /* Set a particular configuration parameter to desired value */

    char response[WEBUI_LEN_RESP];
    int retcd;

    retcd = webu_process_config(webui);

    if (retcd == 0){
        snprintf(response,sizeof(response)
            , "%s = %s\nDone \n"
            ,webui->uri_parm1
            ,webui->uri_value1
        );
        webu_write(webui, response);
    } else {
        webu_text_badreq(webui);
    }

}

static void webu_text_action(struct webui_ctx *webui) {
    /* Call the action functions */
    char response[WEBUI_LEN_RESP];

    /* Send response message for action */
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"makemovie for camera %d \nDone\n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if (strcmp(webui->uri_cmd2,"eventstart") == 0){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Start event for camera %d \nDone\n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if (!strcmp(webui->uri_cmd2,"eventend")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"End event for camera %d \nDone\n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Snapshot for camera %d \nDone\n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if (!strcmp(webui->uri_cmd2,"restart")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Restart in progress ...\nDone\n"
        );
        webu_write(webui, response);

    } else if (!strcmp(webui->uri_cmd2,"start")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Camera %d Detection resumed\nDone \n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if (!strcmp(webui->uri_cmd2,"pause")){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Camera %d Detection paused\nDone \n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        webu_process_action(webui);
        snprintf(response,sizeof(response)
            ,"Camera %d write\nDone \n"
            ,webui->cnt->camera_id
        );
        webu_write(webui, response);

    } else {
        webu_text_badreq(webui);
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        return;
    }

}

static void webu_text_track(struct webui_ctx *webui) {
    /* Call the track function */
    char response[WEBUI_LEN_RESP];
    int retcd;

    retcd = webu_process_track(webui);
    if (retcd == 0){
        snprintf(response,sizeof(response)
            ,"Camera %d \nTrack set %s\nDone \n"
            ,webui->cnt->camera_id
            ,webui->uri_cmd2
        );
        webu_write(webui, response);
    } else {
        webu_text_badreq(webui);
    }

}

void webu_text_main(struct webui_ctx *webui) {

    /* Main entry point for processing requests for the text interface */

    if (strlen(webui->uri_camid) == 0){
        webu_text_page(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"set"))) {
        webu_text_set(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_text_action(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"list"))) {
        webu_text_list(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"get"))) {
        webu_text_get(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"status"))) {
        webu_text_status(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"connection"))) {
        webu_text_connection(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"start"))) {
        webu_text_action(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"pause"))) {
        webu_text_action(webui);

    } else if ((strcmp(webui->uri_cmd1,"action") == 0) &&
               (strcmp(webui->uri_cmd2,"quit") == 0)){
        webu_text_quit(webui);

    } else if (!strcmp(webui->uri_cmd1,"action")) {
        webu_text_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        webu_text_track(webui);

    } else{
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        webu_text_badreq(webui);
    }

    return;
}

