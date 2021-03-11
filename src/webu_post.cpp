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

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_post.hpp"

/* Process the add camera action */
static void webu_post_cam_add(struct webui_ctx *webui)
{
    int indx, maxcnt;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Adding camera.");

    maxcnt = 100;

    webui->motapp->cam_add = true;
    indx = 0;
    while ((webui->motapp->cam_add == true) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }

    if (indx == maxcnt) {
        webui->motapp->cam_add = true;
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error adding camera.  Timed out");
        return;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "New camera added.");

}
/* Process the delete camera action */
static void webu_post_cam_delete(struct webui_ctx *webui)
{
    int indx, maxcnt;

    if (webui->threadnbr == 0) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "No camera specified for deletion." );
        return;
    } else {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Deleting camera.");
    }

    maxcnt = 100;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
        _("Stopping cam %d"),webui->cam->camera_id);
    webui->motapp->cam_list[webui->threadnbr]->restart_cam = false;
    webui->motapp->cam_list[webui->threadnbr]->finish_cam = true;

    indx = 0;
    while ((webui->motapp->cam_list[webui->threadnbr]->running_cam) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        return;
    }
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "Camera stopped");

    webui->motapp->cam_delete = webui->threadnbr;

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

}

/* Get the command and thread number from the post data */
void webu_post_cmdthr(struct webui_ctx *webui)
{
    int indx, camid;

    webui->post_cmd = "";
    webui->threadnbr = -1;
    camid = -1;

    for (indx = 0; indx < webui->post_sz; indx++) {
        if (mystreq(webui->post_info[indx].key_nm, "command")) {
            webui->post_cmd = webui->post_info[indx].key_val;
        }
        if (mystreq(webui->post_info[indx].key_nm, "camid")) {
            camid = atoi(webui->post_info[indx].key_val);
        }

        MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"key: %s  value: %s "
            , webui->post_info[indx].key_nm
            , webui->post_info[indx].key_val
        );
    }

    if (webui->post_cmd == "") {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No command");
        return;
    }
    if (camid == -1) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No camera id provided");
        return;
    }

    indx = 0;
    while (webui->motapp->cam_list[indx] != NULL) {
        if (webui->motapp->cam_list[indx]->camera_id == camid) {
            webui->threadnbr = indx;
            break;
        }
        indx++;
    }
    if (webui->threadnbr == -1) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  Camid: %d not found"
            , camid);
        return;
    }

}

/* Process the event end action */
void webu_post_action_eventend(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->event_stop = true;
            indx++;
        }
    } else {
        webui->motapp->cam_list[webui->threadnbr]->event_stop = true;
    }

}

/* Process the event start action */
void webu_post_action_eventstart(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->event_user = true;
            indx++;
        }
    } else {
        webui->motapp->cam_list[webui->threadnbr]->event_user = true;
    }

}

/* Process the snapshot action */
void webu_post_action_snapshot(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->snapshot = true;
            indx++;
        }
    } else {
        webui->motapp->cam_list[webui->threadnbr]->snapshot = true;
    }

}

/* Process the pause action */
void webu_post_action_pause(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->pause = true;
            indx++;
        };
    } else {
        webui->motapp->cam_list[webui->threadnbr]->pause = true;
    }

}

/* Process the unpause action */
void webu_post_action_unpause(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->pause = false;
            indx++;
        };
    } else {
        webui->motapp->cam_list[webui->threadnbr]->pause = false;
    }

}

/* Process the restart action */
void webu_post_action_restart(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all cameras"));
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            webui->motapp->cam_list[indx]->restart_cam = true;
            if (webui->motapp->cam_list[indx]->running_cam) {
                webui->motapp->cam_list[indx]->event_stop = true;
                webui->motapp->cam_list[indx]->finish_cam = true;
            }
            indx++;
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Restarting camera %d")
            , webui->motapp->cam_list[webui->threadnbr]->camera_id);
        webui->motapp->cam_list[webui->threadnbr]->restart_cam = true;
        webui->motapp->cam_list[webui->threadnbr]->finish_cam = false;
    }

}

/* Process the stop action */
void webu_post_action_stop(struct webui_ctx *webui)
{
    int indx;

    if (webui->threadnbr == 0)  {
        indx = 1;
        while (webui->motapp->cam_list[indx]) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Stopping cam %d"),webui->motapp->cam_list[indx]->camera_id);
            webui->motapp->cam_list[indx]->restart_cam = false;
            webui->motapp->cam_list[indx]->event_stop = true;
            webui->motapp->cam_list[indx]->event_user = true;
            webui->motapp->cam_list[indx]->finish_cam = true;
            indx++;
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Stopping cam %d")
            , webui->motapp->cam_list[webui->threadnbr]->camera_id);
        webui->motapp->cam_list[webui->threadnbr]->restart_cam = false;
        webui->motapp->cam_list[webui->threadnbr]->event_stop = true;
        webui->motapp->cam_list[webui->threadnbr]->event_user = true;
        webui->motapp->cam_list[webui->threadnbr]->finish_cam = true;
    }

}

/* Process the configuration parameters */
static void webu_post_config(struct webui_ctx *webui)
{
    int indx, indx2;
    bool ismotapp;
    std::string tmpname;

    for (indx = 0; indx < webui->post_sz; indx++) {
        if (mystrne(webui->post_info[indx].key_nm, "command") &&
            mystrne(webui->post_info[indx].key_nm, "camid")) {

            tmpname = webui->post_info[indx].key_nm;
            indx2=0;
            while (config_parms_depr[indx2].parm_name != "") {
                if (config_parms_depr[indx2].parm_name == tmpname) {
                    tmpname = config_parms_depr[indx2].newname;
                    break;
                }
                indx2++;
            }

            /* Ignore any requests for parms above webcontrol_parms level. */
            indx2=0;
            while (config_parms[indx2].parm_name != "") {
                if ((config_parms[indx2].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) ||
                    (config_parms[indx2].webui_level == WEBUI_LEVEL_NEVER) ) {
                    indx2++;
                    continue;
                }
                if (tmpname == config_parms[indx2].parm_name) {
                    break;
                }
                indx2++;
            }

            if (config_parms[indx2].parm_name != "") {

                if (config_parms[indx2].parm_cat == PARM_CAT_00) {
                    ismotapp = true;
                } else {
                    ismotapp = false;
                }

                conf_edit_set(webui->motapp, ismotapp
                    , webui->threadnbr
                    , config_parms[indx2].parm_name
                    , webui->post_info[indx].key_val);

                /* If changing language, do it now */
                if (config_parms[indx2].parm_name == "native_language") {
                    if (webui->motapp->native_language) {
                        mytranslate_text("", true);
                        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : on"));
                    } else {
                        mytranslate_text("", false);
                        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : off"));
                    }
                }
            }
        }
    }



}

/* Process the actions from the webcontrol that the user requested */
void webu_post_main(struct webui_ctx *webui)
{

    webu_post_cmdthr(webui);

    if ((webui->post_cmd == "") || (webui->threadnbr == -1)) {
        return;
    }

    if (webui->post_cmd == "eventend") {
        webu_post_action_eventend(webui);

    } else if (webui->post_cmd == "eventstart") {
        webu_post_action_eventstart(webui);

    } else if (webui->post_cmd == "snapshot") {
        webu_post_action_snapshot(webui);

    } else if (webui->post_cmd == "pause") {
        webu_post_action_pause(webui);

    } else if (webui->post_cmd == "unpause") {
        webu_post_action_unpause(webui);

    } else if (webui->post_cmd == "restart") {
        webu_post_action_restart(webui);

    } else if (webui->post_cmd == "stop") {
        webu_post_action_stop(webui);

    } else if (webui->post_cmd == "write") {
        conf_parms_write(webui->motapp);

    } else if (webui->post_cmd == "add") {
        webu_post_cam_add(webui);

    } else if (webui->post_cmd == "delete") {
        webu_post_cam_delete(webui);

    } else if (webui->post_cmd == "config") {
        webu_post_config(webui);

    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO
            , _("Invalid action requested: command: >%s< threadnbr : >%d< ")
            , webui->post_cmd.c_str(), webui->threadnbr);
        return;
    }

}

