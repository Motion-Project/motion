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
 *    This module processes the requests associated with the text inferface
 *    of the webcontrol.  This interface is intended to be used by programs
 *    and does not have any user interface to navigate.  The same actions
 *    are available as the HTML as well as a few more.
 *      Additional functions not directly available via HTML
 *          get:    Returns the value of a parameter.
 *          stop:   Stops the camera thread
 *          list:   Lists all the configuration parameters and values
 *          status  Whether the camera is in pause mode.
 *          connection  Whether the camera connection is working
 *
 */

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_text.hpp"

static void webu_text_seteol(struct webui_ctx *webui)
{
    /* Set the end of line character for text interface */
    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
        snprintf(webui->text_eol, WEBUI_LEN_PARM,"%s","<br>");
    } else {
        snprintf(webui->text_eol, WEBUI_LEN_PARM,"%s","");
    }

}

static void webu_text_camera_name(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    if (webui->motapp->cam_list[webui->thread_nbr]->conf->camera_name == ""){
        snprintf(response,sizeof(response),
            "Camera %s %s\n"
            ,webui->uri_camid,webui->text_eol
        );
    } else {
        snprintf(response,sizeof(response),
            "Camera %s %s\n"
            ,webui->motapp->cam_list[webui->thread_nbr]->conf->camera_name.c_str()
            ,webui->text_eol
        );
    }
    webu_write(webui, response);

}

static void webu_text_back(struct webui_ctx *webui, const char *prevuri)
{
    char response[WEBUI_LEN_RESP];

    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
        snprintf(response,sizeof(response),
            "<a href=/%s%s><- back</a><br><br>\n"
            ,webui->uri_camid, prevuri
        );
        webu_write(webui, response);
    }

}

static void webu_text_header(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
        snprintf(response, sizeof (response),"%s",
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>MotionPlus " VERSION " </title></head>\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
            "<body>\n");
        webu_write(webui, response);
    }
}

static void webu_text_trailer(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
        snprintf(response, sizeof (response),"%s",
            "</body>\n"
            "</html>\n");
        webu_write(webui, response);
    }

}

void webu_text_badreq(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);
    snprintf(response, sizeof (response),
        "Bad Request %s\n"
        "The server did not understand your request. %s\n"
        ,webui->text_eol, webui->text_eol
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

    return;
}

static void webu_text_page_raw(struct webui_ctx *webui)
{
    /* Write the main page text */
    char response[WEBUI_LEN_RESP];
    int indx;

    snprintf(response, sizeof (response),
        "MotionPlus " VERSION " Running [%d] Camera%s \n"
        ,webui->cam_count
        ,(webui->cam_count > 1 ? "s" : "")
    );
    webu_write(webui, response);

    if (webui->cam_threads > 1){
        for (indx = 1; indx < webui->cam_threads; indx++) {
            snprintf(response, sizeof (response),
                "%d \n"
                ,webui->motapp->cam_list[indx]->camera_id
            );
            webu_write(webui, response);
        }
    }

}

static void webu_text_page_basic(struct webui_ctx *webui)
{
    /* Write the main page text */
    char response[WEBUI_LEN_RESP];
    int indx;

    webu_text_header(webui);
    snprintf(response, sizeof (response),
        "MotionPlus " VERSION " Running [%d] Camera%s<br>\n"
        "<a href='/%d/'>All</a><br>\n"
        ,webui->cam_count, (webui->cam_count > 1 ? "s" : "")
        ,webui->motapp->cam_list[0]->camera_id);
    webu_write(webui, response);

    if (webui->cam_threads > 1){
        for (indx = 1; indx < webui->cam_threads; indx++) {
            if (webui->motapp->cam_list[indx]->conf->camera_name == ""){
                snprintf(response, sizeof (response),
                    "<a href='/%d/'>Camera %d</a><br>\n"
                    , webui->motapp->cam_list[indx]->camera_id
                    , indx);
                webu_write(webui, response);
            } else {
                snprintf(response, sizeof (response),
                    "<a href='/%d/'>Camera %s</a><br>\n"
                    , webui->motapp->cam_list[indx]->camera_id
                    ,webui->motapp->cam_list[indx]->conf->camera_name.c_str());
                webu_write(webui, response);
            }
        }
    }
    webu_text_trailer(webui);

}

static void webu_text_list_raw(struct webui_ctx *webui)
{
    /* Write out the options and values */
    char response[WEBUI_LEN_RESP];
    int indx_parm,retcd;
    char val_parm[PATH_MAX];

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_parms[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        conf_edit_get(webui->motapp->cam_list[webui->thread_nbr], config_parms[indx_parm].parm_name
            , val_parm, config_parms[indx_parm].parm_cat);
        if (val_parm == NULL){
            conf_edit_get(webui->motapp->cam_list[0], config_parms[indx_parm].parm_name
                , val_parm, config_parms[indx_parm].parm_cat);
        }
        retcd = snprintf(response, sizeof (response),
            "  %s = %s \n"
            ,config_parms[indx_parm].parm_name.c_str()
            ,val_parm
        );
        if (retcd <0) MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        webu_write(webui, response);

        indx_parm++;
    }

}

static void webu_text_list_basic(struct webui_ctx *webui)
{
    /* Write out the options and values */
    char response[WEBUI_LEN_RESP];
    int indx_parm,retcd;
    char val_parm[PATH_MAX];

    webu_text_header(webui);

    snprintf(response,sizeof(response),
        "<a href=/%s/config><- back</a><br><br>"
        ,webui->uri_camid
    );
    webu_write(webui, response);

    webu_text_camera_name(webui);

    snprintf(response,sizeof(response),"%s","<ul>\n");
    webu_write(webui, response);

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_parms[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        conf_edit_get(webui->motapp->cam_list[webui->thread_nbr], config_parms[indx_parm].parm_name
            , val_parm, config_parms[indx_parm].parm_cat);
        if (val_parm == NULL){
            conf_edit_get(webui->motapp->cam_list[0], config_parms[indx_parm].parm_name
                , val_parm, config_parms[indx_parm].parm_cat);
        }
        retcd = snprintf(response, sizeof (response),
            "  <li><a href=/%s/config/set?%s>%s</a> = %s</li>\n"
            ,webui->uri_camid
            ,config_parms[indx_parm].parm_name.c_str()
            ,config_parms[indx_parm].parm_name.c_str()
            ,val_parm);
        if (retcd <0) MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        webu_write(webui, response);

        indx_parm++;
    }

    snprintf(response,sizeof(response),"%s","</ul>\n");
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_set_menu(struct webui_ctx *webui)
{

    /* Write out the options and values to allow user to set them*/
    char response[WEBUI_LEN_RESP];
    int indx_parm;
    char val_parm[PATH_MAX];

    webu_text_header(webui);

    webu_text_back(webui,"/config");

    webu_text_camera_name(webui);

    snprintf(response, sizeof (response),"%s",
        "<script language='javascript'>function show() {\n"
        " top.location.href='set?'\n"
        " +document.n.onames.options[document.n.onames.selectedIndex].value\n"
        " +'='+document.s.valor.value;}\n"
        " </script>\n"
        "<form name='n'> \n"
        "<select name='onames'>\n"
    );
    webu_write(webui, response);

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_parms[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        conf_edit_get(webui->motapp->cam_list[webui->thread_nbr], config_parms[indx_parm].parm_name
            , val_parm, config_parms[indx_parm].parm_cat);
        if (val_parm == NULL){
            conf_edit_get(webui->motapp->cam_list[0], config_parms[indx_parm].parm_name
                , val_parm, config_parms[indx_parm].parm_cat);
        }
        snprintf(response, sizeof(response),
            "<option value='%s'>%s</option>\n"
            ,config_parms[indx_parm].parm_name.c_str()
            ,config_parms[indx_parm].parm_name.c_str()
        );
        webu_write(webui, response);

        indx_parm++;
    }

    snprintf(response, sizeof (response),"%s",
        "</select>\n"
        "</form>\n"
        "<form action=set name='s'ONSUBMIT='if (!this.submitted) return false; else return true;'>\n"
        "<input type=text name='valor' value=''>\n"
        "<input type='button' value='set' onclick='javascript:show()'>\n"
        "</form>\n"
    );
    webu_write(webui, response);

    webu_text_trailer(webui);


}

static void webu_text_set_query(struct webui_ctx *webui)
{

    /* Write out the options and values to allow user to set them*/
    char response[WEBUI_LEN_RESP];
    int indx_parm,retcd;
    char val_parm[PATH_MAX];

    webu_text_header(webui);

    webu_text_back(webui,"/config/list");

    webu_text_camera_name(webui);

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_parms[indx_parm].main_thread != 0)) ||
            (mystrne(webui->uri_parm1, config_parms[indx_parm].parm_name.c_str()))) {
            indx_parm++;
            continue;
        }

        conf_edit_get(webui->motapp->cam_list[webui->thread_nbr], config_parms[indx_parm].parm_name
            , val_parm, config_parms[indx_parm].parm_cat);
        if (val_parm == NULL){
            conf_edit_get(webui->motapp->cam_list[0], config_parms[indx_parm].parm_name
                , val_parm, config_parms[indx_parm].parm_cat);
        }
        retcd = snprintf(response, sizeof (response),
            "<form action=set?>\n"
            "%s <input type=text name='%s' value='%s' size=60>\n"
            "<input type='submit' value='set'>\n"
            ,config_parms[indx_parm].parm_name.c_str()
            ,config_parms[indx_parm].parm_name.c_str()
            ,val_parm
        );
        if (retcd <0) MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        webu_write(webui, response);

        break;

        indx_parm++;
    }

    webu_text_trailer(webui);

}

static void webu_text_set_assign(struct webui_ctx *webui)
{
    /* Set a particular configuration parameter to desired value */

    char response[WEBUI_LEN_RESP];
    int retcd;

    retcd = webu_process_config(webui);

    if (retcd == 0){
        webu_text_header(webui);

        webu_text_back(webui,"/config");

        snprintf(response,sizeof(response),
            "%s = %s %s\n"
            "Done %s\n"
            ,webui->uri_parm1
            ,webui->uri_value1
            ,webui->text_eol, webui->text_eol
        );
        webu_write(webui, response);

        webu_text_trailer(webui);
    } else {
        webu_text_badreq(webui);
    }

}

static void webu_text_get_menu(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];
    int indx_parm;

    webu_text_header(webui);

    webu_text_back(webui,"/config");

    webu_text_camera_name(webui);

    snprintf(response, sizeof (response),"%s",
        "<form action=get>\n"
        "<select name='query'>\n"
    );
    webu_write(webui, response);

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((webui->thread_nbr != 0) && (config_parms[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        snprintf(response, sizeof(response),
            "<option value='%s'>%s</option>\n"
            ,config_parms[indx_parm].parm_name.c_str()
            ,config_parms[indx_parm].parm_name.c_str()
        );
        webu_write(webui, response);

        indx_parm++;
    }

    snprintf(response, sizeof (response),"%s",
        "</select>\n"
        "<input type='submit' value='get'>\n"
        "</form>\n"
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_stop(struct webui_ctx *webui)
{
    /* Shut down MotionPlus or the associated thread */
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_back(webui,"/action");

    webu_text_header(webui);
    snprintf(response,sizeof(response),
        "Stopping camera ... bye %s\nDone %s\n"
        ,webui->text_eol, webui->text_eol
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_action_makemovie(struct webui_ctx *webui)
{
    /* end the event.  Legacy api name*/

    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_back(webui,"/action");

    webu_text_header(webui);

    snprintf(response,sizeof(response)
        ,"makemovie for camera %d %s\nDone%s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_eventstart(struct webui_ctx *webui)
{
    /* Start the event*/

    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"Start event for camera %d %s\nDone%s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_eventend(struct webui_ctx *webui)
{
    /* End any active event*/

    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"End event for camera %d %s\nDone %s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

/* trigger a snapshot*/
static void webu_text_action_snapshot(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"Snapshot for camera %d %s\nDone%s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

/* Restart*/
static void webu_text_action_restart(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"Restart in progress ...%s\nDone %s\n"
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_add(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"Camera added %s\nDone%s\n"
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_delete(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/action");

    snprintf(response,sizeof(response)
        ,"Camera delete %s\nDone%s\n"
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action_resume(struct webui_ctx *webui)
{
    /* Resume detection on the camera*/

    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/detection");

    snprintf(response,sizeof(response)
        ,"Camera %d Detection resumed%s\nDone %s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

/* pause the motion detection on the camera*/
static void webu_text_action_pause(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/detection");

    snprintf(response,sizeof(response)
        ,"Camera %d Detection paused%s\nDone %s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

/* write the parms to file*/
static void webu_text_action_write(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    webu_text_header(webui);

    webu_text_back(webui,"/config");

    snprintf(response,sizeof(response)
        ,"Camera %d write %s\nDone %s\n"
        ,webui->cam->camera_id
        ,webui->text_eol,webui->text_eol
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_action(struct webui_ctx *webui)
{
    /* Call the action functions */

    if (mystreq(webui->uri_cmd2,"makemovie")){
        webu_text_action_makemovie(webui);

    } else if (mystreq(webui->uri_cmd2,"eventstart")){
        webu_text_action_eventstart(webui);

    } else if (mystreq(webui->uri_cmd2,"eventend")){
        webu_text_action_eventend(webui);

    } else if (mystreq(webui->uri_cmd2,"snapshot")){
        webu_text_action_snapshot(webui);

    } else if (mystreq(webui->uri_cmd2,"restart")){
        webu_text_action_restart(webui);

    } else if (mystreq(webui->uri_cmd2,"resume")){
        webu_text_action_resume(webui);

    } else if (mystreq(webui->uri_cmd2,"pause")){
        webu_text_action_pause(webui);

    } else if ((mystreq(webui->uri_cmd2,"stop")) ||
               (mystreq(webui->uri_cmd2,"end"))){
        webu_text_action_stop(webui);

    } else if ((mystreq(webui->uri_cmd2,"write")) ||
               (mystreq(webui->uri_cmd2,"writeyes"))){
        webu_text_action_write(webui);

    } else if (mystreq(webui->uri_cmd2,"add")){
        webu_text_action_add(webui);

    } else if (mystreq(webui->uri_cmd2,"delete")){
        webu_text_action_delete(webui);

    } else {
        webu_text_badreq(webui);
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        return;
    }

}

static void webu_text_track_pantilt(struct webui_ctx *webui)
{
    /* Call the track function */
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    webu_text_back(webui,"/track");

    webu_text_camera_name(webui);

    snprintf(response,sizeof(response),"%s",
        "<form action='set'>\n"
        "Pan<input type=text name='pan' value=''>\n"
        "Tilt<input type=text name='tilt' value=''>\n"
        "<input type=submit value='set relative'>\n"
        "</form>\n"
        "<form action='set'>\n"
        "X<input type=text name='x' value=''>\n"
        "Y<input type=text name='y' value=''>\n"
        "<input type=submit value='set absolute'>\n"
        "</form>\n"
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_track(struct webui_ctx *webui)
{
    /* Call the track function */
    char response[WEBUI_LEN_RESP];
    int retcd;

    retcd = webu_process_track(webui);
    if (retcd == 0){
        webu_text_header(webui);

        webu_text_back(webui,"/track");

        webu_text_camera_name(webui);

        snprintf(response,sizeof(response)
            ,"Track %s %s\n"
            "Done %s\n"
            ,webui->uri_cmd2,webui->text_eol
            ,webui->text_eol
        );
        webu_write(webui, response);

        webu_text_trailer(webui);
    } else {
        webu_text_badreq(webui);
    }

}

static void webu_text_menu(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    snprintf(response,sizeof(response),
        "<a href=/><- back</a><br><br>"
    );
    webu_write(webui, response);

    webu_text_camera_name(webui);
    snprintf(response,sizeof(response),
            "<a href='/%s/config'>config</a><br>\n"
            "<a href='/%s/action'>action</a><br>\n"
            "<a href='/%s/detection'>detection</a><br>\n"
            "<a href='/%s/track'>track</a><br>\n"
        ,webui->uri_camid, webui->uri_camid
        ,webui->uri_camid, webui->uri_camid
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_menu_config(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    webu_text_back(webui,"/");

    webu_text_camera_name(webui);
    snprintf(response,sizeof(response),
        "<a href=/%s/config/list>list</a><br>"
        "<a href=/%s/config/write>write</a><br>"
        "<a href=/%s/config/set>set</a><br>"
        "<a href=/%s/config/get>get</a><br>"
        ,webui->uri_camid, webui->uri_camid
        ,webui->uri_camid, webui->uri_camid
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_menu_action(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    webu_text_back(webui,"/");

    webu_text_camera_name(webui);
    snprintf(response,sizeof(response),
        "<a href=/%s/action/eventstart>eventstart</a><br>"
        "<a href=/%s/action/eventend>eventend</a><br>"
        "<a href=/%s/action/snapshot>snapshot</a><br>"
        "<a href=/%s/action/restart>restart</a><br>"
        "<a href=/%s/action/stop>stop</a><br>"
        "<a href=/%s/action/end>end</a><br>"
        ,webui->uri_camid, webui->uri_camid, webui->uri_camid
        ,webui->uri_camid, webui->uri_camid, webui->uri_camid
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_menu_detection(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    webu_text_back(webui,"/");

    webu_text_camera_name(webui);

    snprintf(response,sizeof(response),
        "<a href=/%s/detection/status>status</a><br>"
        "<a href=/%s/detection/resume>resume</a><br>"
        "<a href=/%s/detection/pause>pause</a><br>"
        "<a href=/%s/detection/connection>connection</a><br>"
        ,webui->uri_camid, webui->uri_camid
        ,webui->uri_camid, webui->uri_camid
    );
    webu_write(webui, response);
    webu_text_trailer(webui);

}

static void webu_text_menu_track(struct webui_ctx *webui)
{
    char response[WEBUI_LEN_RESP];

    webu_text_header(webui);

    webu_text_back(webui,"/");

    webu_text_camera_name(webui);

    snprintf(response,sizeof(response),
        "<a href=/%s/track/set>track set pan/tilt</a><br>"
        "<a href=/%s/track/center>track center</a><br>"
        ,webui->uri_camid, webui->uri_camid
    );
    webu_write(webui, response);

    webu_text_trailer(webui);

}

static void webu_text_submenu(struct webui_ctx *webui)
{

    if ((mystreq(webui->uri_cmd1,"config")) &&
        (strlen(webui->uri_cmd2) == 0)) {
        webu_text_menu_config(webui);

    } else if ((mystreq(webui->uri_cmd1,"action")) &&
                (strlen(webui->uri_cmd2) == 0)) {
        webu_text_menu_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"detection")) &&
                (strlen(webui->uri_cmd2) == 0)) {
        webu_text_menu_detection(webui);

    } else if ((mystreq(webui->uri_cmd1,"track")) &&
                (strlen(webui->uri_cmd2) == 0)) {
        webu_text_menu_track(webui);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        webu_text_badreq(webui);
    }

}

void webu_text_get_query(struct webui_ctx *webui)
{
    /* Write out the option value for one parm */
    char response[WEBUI_LEN_RESP];
    int indx_parm, retcd;
    char val_parm[PATH_MAX];
    char temp_name[WEBUI_LEN_PARM];


    /* Search through the depreciated parms and if applicable,
     * get the new parameter name so we can check its webcontrol_parms level
     */
    snprintf(temp_name, WEBUI_LEN_PARM, "%s", webui->uri_value1);
    indx_parm=0;
    while (config_parms_depr[indx_parm].parm_name != "") {
        if (mystreq(config_parms_depr[indx_parm].parm_name.c_str(), webui->uri_value1)){
            snprintf(temp_name, WEBUI_LEN_PARM, "%s", config_parms_depr[indx_parm].newname.c_str());
            break;
        }
        indx_parm++;
    }

    indx_parm = 0;
    while (config_parms[indx_parm].parm_name != ""){

        if ((config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
            (config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            mystrne(webui->uri_parm1,"query") ||
            mystrne(temp_name, config_parms[indx_parm].parm_name.c_str())){
            indx_parm++;
            continue;
        }

        conf_edit_get(webui->motapp->cam_list[webui->thread_nbr], config_parms[indx_parm].parm_name
            , val_parm, config_parms[indx_parm].parm_cat);
        if (val_parm == NULL){
            conf_edit_get(webui->motapp->cam_list[0], config_parms[indx_parm].parm_name
                , val_parm, config_parms[indx_parm].parm_cat);
        }
        if (mystrne(webui->uri_value1, config_parms[indx_parm].parm_name.c_str())){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("'%s' option is depreciated.  New option name is `%s'")
            ,webui->uri_value1, config_parms[indx_parm].parm_name.c_str());
        }

        webu_text_header(webui);

        webu_text_back(webui,"/config");

        webu_text_camera_name(webui);

        if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
            retcd = snprintf(response, sizeof (response),
                "<ul>\n"
                "  <li>%s = %s </li>\n"
                "</ul>\n"
                ,config_parms[indx_parm].parm_name.c_str()
                ,val_parm
            );
            if (retcd <0) MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        } else {
            retcd = snprintf(response, sizeof (response),
                "%s = %s %s\n"
                "Done %s\n"
                ,config_parms[indx_parm].parm_name.c_str()
                ,val_parm
                ,webui->text_eol, webui->text_eol
            );
            if (retcd <0) MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error option"));
        }
        webu_write(webui, response);
        webu_text_trailer(webui);

        break;
    }

    if (config_parms[indx_parm].parm_name == ""){
        webu_text_badreq(webui);
    }

}

void webu_text_status(struct webui_ctx *webui)
{
    /* Write out the pause/active status */

    char response[WEBUI_LEN_RESP];
    int indx, indx_st;

    webu_text_header(webui);

    webu_text_back(webui,"/detection");

    if (webui->thread_nbr == 0){
        indx_st = 1;
        if (webui->cam_threads == 1) indx_st = 0;

        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            snprintf(response, sizeof(response),
                "Camera %d Detection status %s %s\n"
                ,webui->motapp->cam_list[indx]->camera_id
                ,(!webui->motapp->cam_list[indx]->running_cam)? "NOT RUNNING":
                (webui->motapp->cam_list[indx]->pause)? "PAUSE":"ACTIVE"
                ,webui->text_eol
            );
            webu_write(webui, response);
        }
    } else {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s %s\n"
            ,webui->cam->camera_id
            ,(!webui->cam->running_cam)? "NOT RUNNING":
            (webui->cam->pause)? "PAUSE":"ACTIVE"
            ,webui->text_eol
        );
        webu_write(webui, response);
    }
    webu_text_trailer(webui);
}

void webu_text_connection(struct webui_ctx *webui)
{
    /* Write out the connection status */
    char response[WEBUI_LEN_RESP];
    int indx, indx_st;

    webu_text_header(webui);

    webu_text_back(webui,"/detection");

    webu_text_camera_name(webui);

    if (webui->thread_nbr == 0){
        indx_st = 1;
        if (webui->cam_threads == 1) indx_st = 0;

        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            snprintf(response,sizeof(response)
                , "Camera %d%s%s %s %s\n"
                ,webui->motapp->cam_list[indx]->camera_id
                ,webui->motapp->cam_list[indx]->conf->camera_name!="" ? " -- " : ""
                ,webui->motapp->cam_list[indx]->conf->camera_name!="" ? webui->motapp->cam_list[indx]->conf->camera_name.c_str() : ""
                ,(!webui->motapp->cam_list[indx]->running_cam)? "NOT RUNNING" :
                (webui->motapp->cam_list[indx]->lost_connection)? "Lost connection": "Connection OK"
                ,webui->text_eol
            );
            webu_write(webui, response);
        }
    } else {
        snprintf(response,sizeof(response)
            , "Camera %d%s%s %s %s\n"
            ,webui->cam->camera_id
            ,webui->cam->conf->camera_name!="" ? " -- " : ""
            ,webui->cam->conf->camera_name!="" ? webui->cam->conf->camera_name.c_str() : ""
            ,(!webui->cam->running_cam)? "NOT RUNNING" :
             (webui->cam->lost_connection)? "Lost connection": "Connection OK"
            ,webui->text_eol
        );
        webu_write(webui, response);
    }
    webu_text_trailer(webui);
}

void webu_text_list(struct webui_ctx *webui)
{

    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
        webu_text_list_basic(webui);
    } else {
        webu_text_list_raw(webui);
    }

}

void webu_text_main(struct webui_ctx *webui)
{

    /* Main entry point for processing requests for the text interface */

    webu_text_seteol(webui);

    pthread_mutex_lock(&webui->motapp->mutex_camlst);

    if (strlen(webui->uri_camid) == 0) {
        if (webui->motapp->cam_list[0]->conf->webcontrol_interface == 2) {
            webu_text_page_basic(webui);
        } else {
            webu_text_page_raw(webui);
        }

    } else if (strlen(webui->uri_cmd1) == 0) {
        webu_text_menu(webui);

    } else if (strlen(webui->uri_cmd2) == 0) {
        webu_text_submenu(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"set")) &&
               (strlen(webui->uri_parm1) == 0)) {
        webu_text_set_menu(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"set")) &&
               (strlen(webui->uri_parm1) > 0) &&
               (strlen(webui->uri_value1) == 0) ) {
        webu_text_set_query(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"set"))) {
        webu_text_set_assign(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"write"))) {
        webu_text_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"list"))) {
        webu_text_list(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"get")) &&
               (strlen(webui->uri_parm1) == 0)) {
        webu_text_get_menu(webui);

    } else if ((mystreq(webui->uri_cmd1,"config")) &&
               (mystreq(webui->uri_cmd2,"get"))) {
        webu_text_get_query(webui);

    } else if ((mystreq(webui->uri_cmd1,"detection")) &&
               (mystreq(webui->uri_cmd2,"status"))) {
        webu_text_status(webui);

    } else if ((mystreq(webui->uri_cmd1,"detection")) &&
               (mystreq(webui->uri_cmd2,"connection"))) {
        webu_text_connection(webui);

    } else if ((mystreq(webui->uri_cmd1,"detection")) &&
               (mystreq(webui->uri_cmd2,"resume"))) {
        webu_text_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"detection")) &&
               (mystreq(webui->uri_cmd2,"pause"))) {
        webu_text_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"action")) &&
               (mystreq(webui->uri_cmd2,"stop"))){
        webu_text_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"action")) &&
               (mystreq(webui->uri_cmd2,"end"))){
        webu_text_action(webui);

    } else if (mystreq(webui->uri_cmd1,"action")) {
        webu_text_action(webui);

    } else if ((mystreq(webui->uri_cmd1,"track")) &&
               (mystreq(webui->uri_cmd2,"set")) &&
               (strlen(webui->uri_parm1) == 0)) {
        webu_text_track_pantilt(webui);

    } else if ((mystreq(webui->uri_cmd1,"track"))){
        webu_text_track(webui);

    } else{
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        webu_text_badreq(webui);
    }
    pthread_mutex_unlock(&webui->motapp->mutex_camlst);

    return;
}
