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
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_post.hpp"

/* Process the add camera action */
static void webu_post_cam_add(ctx_webui *webui)
{
    int indx, maxcnt;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"camera_add")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera add action disabled");
                return;
            } else {
                break;
            }
        }
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Adding camera.");

    maxcnt = 100;

    webui->motapp->cam_add = true;
    indx = 0;
    while ((webui->motapp->cam_add == true) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }

    if (indx == maxcnt) {
        webui->motapp->cam_add = false;
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error adding camera.  Timed out");
        return;
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "New camera added.");

}
/* Process the delete camera action */
static void webu_post_cam_delete(ctx_webui *webui)
{
    int indx, maxcnt;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"camera_delete")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera delete action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->camindx == -1) {
        MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "No camera specified for deletion." );
        return;
    } else {
        MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Deleting camera.");
    }

    webui->motapp->cam_delete = webui->camindx;

    maxcnt = 100;
    indx = 0;
    while ((webui->motapp->cam_delete != -1) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        webui->motapp->cam_delete = -1;
        return;
    }

}

/* Get the command, device_id and camera index from the post data */
void webu_post_cmdindx(ctx_webui *webui)
{
    int indx;

    webui->post_cmd = "";
    webui->camindx = -1;
    webui->device_id = -1;

    for (indx = 0; indx < webui->post_sz; indx++) {
        if (mystreq(webui->post_info[indx].key_nm, "command")) {
            webui->post_cmd = webui->post_info[indx].key_val;
        }
        if (mystreq(webui->post_info[indx].key_nm, "camid")) {
            webui->device_id = atoi(webui->post_info[indx].key_val);
        }

        MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"key: %s  value: %s "
            , webui->post_info[indx].key_nm
            , webui->post_info[indx].key_val
        );
    }

    if (webui->post_cmd == "") {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No command");
        return;
    }
    if (webui->device_id == -1) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No camera id provided");
        return;
    }

    for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
        if (webui->motapp->cam_list[indx]->device_id == webui->device_id) {
            webui->camindx = indx;
            break;
        }
    }

}

/* Process the event end action */
void webu_post_action_eventend(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"event")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Event end action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->event_stop = true;
        }
    } else {
        webui->motapp->cam_list[webui->camindx]->event_stop = true;
    }

}

/* Process the event start action */
void webu_post_action_eventstart(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"event")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Event start action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->event_user = true;
        }
    } else {
        webui->motapp->cam_list[webui->camindx]->event_user = true;
    }

}

/* Process the snapshot action */
void webu_post_action_snapshot(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"snapshot")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Snapshot action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->snapshot = true;
        }
    } else {
        webui->motapp->cam_list[webui->camindx]->snapshot = true;
    }

}

/* Process the pause action */
void webu_post_action_pause(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"pause")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->pause = true;
        }
    } else {
        webui->motapp->cam_list[webui->camindx]->pause = true;
    }

}

/* Process the unpause action */
void webu_post_action_unpause(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"pause")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->pause = false;
        }
    } else {
        webui->motapp->cam_list[webui->camindx]->pause = false;
    }

}

/* Process the restart action */
void webu_post_action_restart(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"restart")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Restart action disabled");
                return;
            } else {
                break;
            }
        }
    }
    if (webui->device_id == 0) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all cameras"));
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            webui->motapp->cam_list[indx]->restart_dev = true;
            webui->motapp->cam_list[indx]->finish_dev = true;
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Restarting camera %d")
            , webui->motapp->cam_list[webui->camindx]->device_id);
        webui->motapp->cam_list[webui->camindx]->restart_dev = true;
        webui->motapp->cam_list[webui->camindx]->finish_dev = true;
    }
}

/* Process the stop action */
void webu_post_action_stop(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"stop")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Stop action disabled");
                return;
            } else {
                break;
            }
        }
    }
    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , _("Stopping cam %d")
                , webui->motapp->cam_list[indx]->device_id);
            webui->motapp->cam_list[indx]->restart_dev = false;
            webui->motapp->cam_list[indx]->event_stop = true;
            webui->motapp->cam_list[indx]->event_user = false;
            webui->motapp->cam_list[indx]->finish_dev = true;
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Stopping cam %d")
            , webui->motapp->cam_list[webui->camindx]->device_id);
        webui->motapp->cam_list[webui->camindx]->restart_dev = false;
        webui->motapp->cam_list[webui->camindx]->event_stop = true;
        webui->motapp->cam_list[webui->camindx]->event_user = false;
        webui->motapp->cam_list[webui->camindx]->finish_dev = true;
    }

}

/* Process the action_user */
void webu_post_action_user(ctx_webui *webui)
{
    int indx, indx2;
    ctx_params *wact;
    ctx_dev *cam;
    std::string tmp;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"action_user")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "User action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webui->device_id == 0) {
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            cam = webui->motapp->cam_list[indx];
            cam->action_user[0] = '\0';
            for (indx2 = 0; indx2 < webui->post_sz; indx2++) {
                if (mystreq(webui->post_info[indx2].key_nm, "user")) {
                    tmp = std::string(webui->post_info[indx2].key_val);
                }
            }
            for (indx2 = 0; indx2<(int)tmp.length(); indx2++) {
                if (isalnum(tmp.at(indx2)) == false) {
                    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                        , _("Invalid character included in action user \"%c\"")
                        , tmp.at(indx2));
                    return;
                }
            }
            snprintf(cam->action_user, 40, "%s", tmp.c_str());
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , _("Executing user action on cam %d")
                , cam->device_id);
            util_exec_command(cam, cam->conf->on_action_user.c_str(), NULL);
        }
    } else {
        cam = webui->motapp->cam_list[webui->camindx];
        cam->action_user[0] = '\0';
        for (indx2 = 0; indx2 < webui->post_sz; indx2++) {
            if (mystreq(webui->post_info[indx2].key_nm, "user")) {
                tmp = std::string(webui->post_info[indx2].key_val);
            }
        }
        for (indx2 = 0; indx2<(int)tmp.length(); indx2++) {
            if (isalnum(tmp.at(indx2)) == false) {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    , _("Invalid character included in action user \"%c\"")
                    , tmp.at(indx2));
                return;
            }
        }
        snprintf(cam->action_user, 40, "%s", tmp.c_str());

        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Executing user action on cam %d")
            , cam->device_id);
        util_exec_command(cam, cam->conf->on_action_user.c_str(), NULL);
    }

}

/* Process the write config action */
void webu_post_write_config(ctx_webui *webui)
{
    int indx;
    ctx_params *wact;

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"config_write")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Config write action disabled");
                return;
            } else {
                break;
            }
        }
    }

    conf_parms_write(webui->motapp);

}

/* Process the configuration parameters */
static void webu_post_config(ctx_webui *webui)
{
    int indx, indx2;
    std::string tmpname;
    ctx_params *wact;

    if (webui->camindx == -1) {
        return;
    }

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"config")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Config save actions disabled");
                return;
            } else {
                break;
            }
        }
    }

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
                if ((config_parms[indx2].webui_level > webui->motapp->conf->webcontrol_parms) ||
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
                    conf_edit_set(webui->motapp->conf
                        , config_parms[indx2].parm_name
                        , webui->post_info[indx].key_val);
                } else {
                    conf_edit_set(webui->motapp->cam_list[webui->camindx]->conf
                        , config_parms[indx2].parm_name
                        , webui->post_info[indx].key_val);
                }
            }
        }
    }

}

/* Process the ptz action */
void webu_post_ptz(ctx_webui *webui)
{
    int indx;
    ctx_dev *cam;
    ctx_params *wact;

    if (webui->camindx == -1) {
        return;
    }

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"ptz")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "PTZ actions disabled");
                return;
            } else {
                break;
            }
        }
    }
    cam = webui->motapp->cam_list[webui->camindx];

    if ((webui->post_cmd == "pan_left") &&
        (cam->conf->ptz_pan_left != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_pan_left.c_str(), NULL);

    } else if ((webui->post_cmd == "pan_right") &&
        (cam->conf->ptz_pan_right != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_pan_right.c_str(), NULL);

    } else if ((webui->post_cmd == "tilt_up") &&
        (cam->conf->ptz_tilt_up != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_tilt_up.c_str(), NULL);

    } else if ((webui->post_cmd == "tilt_down") &&
        (cam->conf->ptz_tilt_down != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_tilt_down.c_str(), NULL);

    } else if ((webui->post_cmd == "zoom_in") &&
        (cam->conf->ptz_zoom_in != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_zoom_in.c_str(), NULL);

    } else if ((webui->post_cmd == "zoom_out") &&
        (cam->conf->ptz_zoom_out != "")) {
        cam->frame_skip = cam->conf->ptz_wait;
        util_exec_command(cam, cam->conf->ptz_zoom_out.c_str(), NULL);

    } else {
        return;
    }

}

/* Process the actions from the webcontrol that the user requested */
void webu_post_main(ctx_webui *webui)
{

    webu_post_cmdindx(webui);

    if (webui->post_cmd == "")  {
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

    } else if (webui->post_cmd == "config_write") {
        webu_post_write_config(webui);

    } else if (webui->post_cmd == "camera_add") {
        webu_post_cam_add(webui);

    } else if (webui->post_cmd == "camera_delete") {
        webu_post_cam_delete(webui);

    } else if (webui->post_cmd == "config") {
        webu_post_config(webui);

    } else if (webui->post_cmd == "action_user") {
        webu_post_action_user(webui);

    } else if (
        (webui->post_cmd == "pan_left") ||
        (webui->post_cmd == "pan_right") ||
        (webui->post_cmd == "tilt_up") ||
        (webui->post_cmd == "tilt_down") ||
        (webui->post_cmd == "zoom_in") ||
        (webui->post_cmd == "zoom_out")) {
        webu_post_ptz(webui);

    } else {
        MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO
            , _("Invalid action requested: command: >%s< camindx : >%d< ")
            , webui->post_cmd.c_str(), webui->camindx);
    }

}

