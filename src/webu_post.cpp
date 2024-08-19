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
#include "util.hpp"
#include "camera.hpp"
#include "sound.hpp"
#include "dbse.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_html.hpp"
#include "webu_common.hpp"
#include "webu_post.hpp"

/**************Callback functions for MHD **********************/

mhdrslt webup_iterate_post (void *ptr, enum MHD_ValueKind kind
        , const char *key, const char *filename, const char *content_type
        , const char *transfer_encoding, const char *data, uint64_t off, size_t datasz)
{
    (void) kind;
    (void) filename;
    (void) content_type;
    (void) transfer_encoding;
    (void) off;
    cls_webu_post *webu_post;

    webu_post = (cls_webu_post *)ptr;
    return webu_post->iterate_post(key, data, datasz);
}

/**************Class methods**********************/

/* Process the add camera action */
void cls_webu_post::cam_add()
{
    int indx, maxcnt;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "camera_add") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera add action disabled");
                return;
            } else {
                break;
            }
        }
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Adding camera.");

    maxcnt = 100;

    app->cam_add = true;
    indx = 0;
    while ((app->cam_add == true) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }

    if (indx == maxcnt) {
        app->cam_add = false;
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error adding camera.  Timed out");
        return;
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "New camera added.");

}

/* Process the delete camera action */
void cls_webu_post::cam_delete()
{
    int indx, maxcnt;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "camera_delete") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera delete action disabled");
                return;
            } else {
                break;
            }
        }
    }

    MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Deleting camera.");

    app->cam_delete = webua->camindx;

    maxcnt = 100;
    indx = 0;
    while ((app->cam_delete != -1) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        app->cam_delete = -1;
        return;
    }
}

/* Get the command, device_id and camera index from the post data */
void cls_webu_post::parse_cmd()
{
    int indx;

    post_cmd = "";
    webua->camindx = -1;
    webua->device_id = -1;

    for (indx = 0; indx < post_sz; indx++) {
        if (mystreq(post_info[indx].key_nm, "command")) {
            post_cmd = post_info[indx].key_val;
        }
        if (mystreq(post_info[indx].key_nm, "camid")) {
            webua->device_id = atoi(post_info[indx].key_val);
        }

        MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"key: %s  value: %s "
            , post_info[indx].key_nm
            , post_info[indx].key_val
        );
    }

    if (post_cmd == "") {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No command");
        return;
    }
    if (webua->device_id == -1) {
        MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No camera id provided");
        return;
    }

    if (webua->device_id != 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            if (app->cam_list[indx]->cfg->device_id == webua->device_id) {
                webua->camindx = indx;
                break;
            }
        }
        if (webua->camindx == -1) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , "Invalid request.  Device id %d not found"
                , webua->device_id);
            webua->device_id = -1;
            return;
        }
    }
}

/* Process the event end action */
void cls_webu_post::action_eventend()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "event") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Event end action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->event_stop = true;
        }
    } else {
        app->cam_list[webua->camindx]->event_stop = true;
    }

}

/* Process the event start action */
void cls_webu_post::action_eventstart()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "event") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Event start action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->event_user = true;
        }
    } else {
        app->cam_list[webua->camindx]->event_user = true;
    }

}

/* Process the snapshot action */
void cls_webu_post::action_snapshot()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "snapshot") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Snapshot action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->action_snapshot = true;
        }
    } else {
        app->cam_list[webua->camindx]->action_snapshot = true;
    }

}

/* Process the pause action */
void cls_webu_post::action_pause()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "pause") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->pause = true;
        }
    } else {
        app->cam_list[webua->camindx]->pause = true;
    }

}

/* Process the unpause action */
void cls_webu_post::action_unpause()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "pause") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->pause = false;
        }
    } else {
        app->cam_list[webua->camindx]->pause = false;
    }

}

/* Process the restart action */
void cls_webu_post::action_restart()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "restart") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Restart action disabled");
                return;
            } else {
                break;
            }
        }
    }
    if (webua->device_id == 0) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all cameras"));
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->restart = true;
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Restarting camera %d")
            , app->cam_list[webua->camindx]->cfg->device_id);
        app->cam_list[webua->camindx]->restart = true;
    }
}

/* Process the stop action */
void cls_webu_post::action_stop()
{
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "stop") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Stop action disabled");
                return;
            } else {
                break;
            }
        }
    }
    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , _("Stopping cam %d")
                , app->cam_list[indx]->cfg->device_id);
            app->cam_list[indx]->restart = false;
            app->cam_list[indx]->event_stop = true;
            app->cam_list[indx]->event_user = false;
            app->cam_list[indx]->handler_stop = true;
        }
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Stopping cam %d")
            , app->cam_list[webua->camindx]->cfg->device_id);
        app->cam_list[webua->camindx]->restart = false;
        app->cam_list[webua->camindx]->event_stop = true;
        app->cam_list[webua->camindx]->event_user = false;
        app->cam_list[webua->camindx]->handler_stop = true;
    }

}

/* Process the action_user */
void cls_webu_post::action_user()
{
    int indx, indx2;
    cls_camera *cam;
    std::string tmp;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "action_user") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "User action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            cam = app->cam_list[indx];
            cam->action_user[0] = '\0';
            for (indx2 = 0; indx2 < post_sz; indx2++) {
                if (mystreq(post_info[indx2].key_nm, "user")) {
                    tmp = std::string(post_info[indx2].key_val);
                }
            }
            for (indx2 = 0; indx2<(int)tmp.length(); indx2++) {
                if (isalnum(tmp.at((uint)indx2)) == false) {
                    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                        , _("Invalid character included in action user \"%c\"")
                        , tmp.at((uint)indx2));
                    return;
                }
            }
            snprintf(cam->action_user, 40, "%s", tmp.c_str());
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , _("Executing user action on cam %d")
                , cam->cfg->device_id);
            util_exec_command(cam, cam->cfg->on_action_user.c_str(), NULL);
        }
    } else {
        cam = app->cam_list[webua->camindx];
        cam->action_user[0] = '\0';
        for (indx2 = 0; indx2 < post_sz; indx2++) {
            if (mystreq(post_info[indx2].key_nm, "user")) {
                tmp = std::string(post_info[indx2].key_val);
            }
        }
        for (indx2 = 0; indx2<(int)tmp.length(); indx2++) {
            if (isalnum(tmp.at((uint)indx2)) == false) {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    , _("Invalid character included in action user \"%c\"")
                    , tmp.at((uint)indx2));
                return;
            }
        }
        snprintf(cam->action_user, 40, "%s", tmp.c_str());

        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Executing user action on cam %d")
            , cam->cfg->device_id);
        util_exec_command(cam, cam->cfg->on_action_user.c_str(), NULL);
    }

}

/* Process the write config action */
void cls_webu_post::write_config()
{
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "config_write") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Config write action disabled");
                return;
            } else {
                break;
            }
        }
    }

    app->conf_src->parms_write();

}

void cls_webu_post::config_set(int indx_parm, std::string parm_vl)
{
    std::string parm_nm, parm_vl_dflt, parm_vl_dev;
    PARM_CAT    parm_ct;
    int indx;

    parm_nm = config_parms[indx_parm].parm_name;
    parm_ct = config_parms[indx_parm].parm_cat;

    if (webua->device_id == 0) {
        app->conf_src->edit_get(parm_nm, parm_vl_dflt, parm_ct);
        if (parm_vl == parm_vl_dflt) {
            return;
        }
        if (parm_ct == PARM_CAT_00) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_restart_set("log",0);
        } else if (parm_ct == PARM_CAT_13) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_restart_set("webu",0);
        } else if (parm_ct == PARM_CAT_15) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_restart_set("dbse",0);
        } else {
            for (indx=0;indx<app->cam_cnt;indx++){
                app->cam_list[indx]->conf_src->edit_get(
                    parm_nm, parm_vl_dev, parm_ct);
                if (parm_vl_dev == parm_vl_dflt) {
                    app->cam_list[indx]->conf_src->edit_set(
                        parm_nm, parm_vl);
                    config_restart_set("cam",indx);
                }
            }
            for (indx=0;indx<app->snd_cnt;indx++) {
                app->snd_list[indx]->conf_src->edit_get(
                    parm_nm, parm_vl_dev, parm_ct);
                if (parm_vl_dev == parm_vl_dflt) {
                    app->snd_list[indx]->conf_src->edit_set(
                        parm_nm, parm_vl);
                    config_restart_set("snd",indx);
                }
            }
            app->conf_src->edit_set(parm_nm, parm_vl);
        }
    } else {
        if ((parm_ct == PARM_CAT_00) ||
            (parm_ct == PARM_CAT_13) ||
            (parm_ct == PARM_CAT_15)) {
            return;
        }
        app->cam_list[webua->camindx]->conf_src->edit_set(
            parm_nm, parm_vl);
        config_restart_set("cam", webua->camindx);
    }

}

void cls_webu_post::config_restart_set(std::string p_type, int p_indx)
{
    int indx;

    for (indx=0; indx<restart_list.size();indx++) {
        if ((restart_list[indx].comp_type == p_type) &&
            (restart_list[indx].comp_indx == p_indx)) {
            restart_list[indx].restart = true;
            break;
        }
    }
}

void cls_webu_post::config_restart_reset()
{
    ctx_restart_item itm_res;
    int indx;

    restart_list.clear();

    itm_res.restart = false;
    itm_res.comp_indx = 0;

    itm_res.comp_type ="log";
    restart_list.push_back(itm_res);

    itm_res.comp_type ="webu";
    restart_list.push_back(itm_res);

    itm_res.comp_type ="dbse";
    restart_list.push_back(itm_res);

    for (indx = 0; indx<app->cam_cnt; indx++) {
        itm_res.comp_type ="cam";
        itm_res.comp_indx = indx;
        restart_list.push_back(itm_res);
    }
    for (indx = 0; indx<app->snd_cnt; indx++) {
        itm_res.comp_type ="snd";
        itm_res.comp_indx = indx;
        restart_list.push_back(itm_res);
    }

}
/* Process the configuration parameters */
void cls_webu_post::config()
{
    int indx, indx2;
    std::string tmpname;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "config") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Config save actions disabled");
                return;
            } else {
                break;
            }
        }
    }

    config_restart_reset();

    for (indx = 0; indx < post_sz; indx++) {
        if (mystrne(post_info[indx].key_nm, "command") &&
            mystrne(post_info[indx].key_nm, "camid")) {

            tmpname = post_info[indx].key_nm;
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
                if ((config_parms[indx2].webui_level > app->conf_src->webcontrol_parms) ||
                    (config_parms[indx2].webui_level == PARM_LEVEL_NEVER) ) {
                    indx2++;
                    continue;
                }
                if (tmpname == config_parms[indx2].parm_name) {
                    break;
                }
                indx2++;
            }

            if (config_parms[indx2].parm_name != "") {
                config_set(indx2, post_info[indx].key_val);
            }
        }
    }

    for (indx = 0; indx < restart_list.size(); indx++) {
        if (restart_list[indx].restart == true) {
            if (restart_list[indx].comp_type == "log") {
                motlog->restart = true;
                MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for log");
            } else if (restart_list[indx].comp_type == "webu") {
                app->webu->restart = true;
                MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for webcontrol");
            } else if (restart_list[indx].comp_type == "dbse") {
                app->dbse->restart = true;
                MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for database");
            } else if (restart_list[indx].comp_type == "cam") {
                app->cam_list[restart_list[indx].comp_indx]->restart = true;
                MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for camera %d"
                    , app->cam_list[restart_list[indx].comp_indx]->cfg->device_id);
            } else if (restart_list[indx].comp_type == "snd") {
                app->snd_list[restart_list[indx].comp_indx]->restart = true;
                MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for sound %d"
                    , app->cam_list[restart_list[indx].comp_indx]->cfg->device_id);
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO, "Bad programming");
            }
        }
    }

}

/* Process the ptz action */
void cls_webu_post::ptz()
{
    cls_camera *cam;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    if (webua->camindx == -1) {
        return;
    }

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "ptz") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "PTZ actions disabled");
                return;
            } else {
                break;
            }
        }
    }
    cam = app->cam_list[webua->camindx];

    if ((post_cmd == "pan_left") &&
        (cam->cfg->ptz_pan_left != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_pan_left.c_str(), NULL);

    } else if ((post_cmd == "pan_right") &&
        (cam->cfg->ptz_pan_right != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_pan_right.c_str(), NULL);

    } else if ((post_cmd == "tilt_up") &&
        (cam->cfg->ptz_tilt_up != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_tilt_up.c_str(), NULL);

    } else if ((post_cmd == "tilt_down") &&
        (cam->cfg->ptz_tilt_down != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_tilt_down.c_str(), NULL);

    } else if ((post_cmd == "zoom_in") &&
        (cam->cfg->ptz_zoom_in != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_zoom_in.c_str(), NULL);

    } else if ((post_cmd == "zoom_out") &&
        (cam->cfg->ptz_zoom_out != "")) {
        cam->frame_skip = cam->cfg->ptz_wait;
        util_exec_command(cam, cam->cfg->ptz_zoom_out.c_str(), NULL);

    } else {
        return;
    }

}

/* Process the actions from the webcontrol that the user requested */
void cls_webu_post::process_actions()
{
    parse_cmd();

    if ((post_cmd == "") || (webua->device_id == -1)) {
        return;
    }

    if (post_cmd == "eventend") {
        action_eventend();

    } else if (post_cmd == "eventstart") {
        action_eventstart();

    } else if (post_cmd == "snapshot") {
        action_snapshot();

    } else if (post_cmd == "pause") {
        action_pause();

    } else if (post_cmd == "unpause") {
        action_unpause();

    } else if (post_cmd == "restart") {
        action_restart();

    } else if (post_cmd == "stop") {
        action_stop();

    } else if (post_cmd == "config_write") {
        write_config();

    } else if (post_cmd == "camera_add") {
        cam_add();

    } else if (post_cmd == "camera_delete") {
        cam_delete();

    } else if (post_cmd == "config") {
        config();

    } else if (post_cmd == "action_user") {
        action_user();

    } else if (
        (post_cmd == "pan_left") ||
        (post_cmd == "pan_right") ||
        (post_cmd == "tilt_up") ||
        (post_cmd == "tilt_down") ||
        (post_cmd == "zoom_in") ||
        (post_cmd == "zoom_out")) {
        ptz();

    } else {
        MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO
            , _("Invalid action requested: command: >%s< camindx : >%d< ")
            , post_cmd.c_str(), webua->camindx);
    }

}

/*Append more data on to an existing entry in the post info structure */
void cls_webu_post::iterate_post_append(int indx
        , const char *data, size_t datasz)
{
    post_info[indx].key_val = (char*)realloc(
        post_info[indx].key_val
        , post_info[indx].key_sz + datasz + 1);

    memset(post_info[indx].key_val +
        post_info[indx].key_sz, 0, datasz + 1);

    if (datasz > 0) {
        memcpy(post_info[indx].key_val +
            post_info[indx].key_sz, data, datasz);
    }

    post_info[indx].key_sz += datasz;
}

/*Create new entry in the post info structure */
void cls_webu_post::iterate_post_new(const char *key
        , const char *data, size_t datasz)
{
    int retcd;

    post_sz++;
    if (post_sz == 1) {
        post_info = (ctx_key *)malloc(sizeof(ctx_key));
    } else {
        post_info = (ctx_key *)realloc(post_info
            , (uint)post_sz * sizeof(ctx_key));
    }

    post_info[post_sz-1].key_nm = (char*)malloc(strlen(key)+1);
    retcd = snprintf(post_info[post_sz-1].key_nm, strlen(key)+1, "%s", key);

    post_info[post_sz-1].key_val = (char*)malloc(datasz+1);
    memset(post_info[post_sz-1].key_val,0,datasz+1);
    if (datasz > 0) {
        memcpy(post_info[post_sz-1].key_val, data, datasz);
    }

    post_info[post_sz-1].key_sz = datasz;

    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error processing post data"));
    }
}

mhdrslt cls_webu_post::iterate_post (const char *key, const char *data, size_t datasz)
{
    int indx;

    for (indx=0; indx < post_sz; indx++) {
        if (mystreq(post_info[indx].key_nm, key)) {
            break;
        }
    }
    if (indx < post_sz) {
        iterate_post_append(indx, data, datasz);
    } else {
        iterate_post_new(key, data, datasz);
    }

    return MHD_YES;
}

mhdrslt cls_webu_post::processor_init()
{
    post_processor = MHD_create_post_processor (webua->connection
        , WEBUI_POST_BFRSZ, webup_iterate_post, (void *)this);
    if (post_processor == NULL) {
        return MHD_NO;
    }
    return MHD_YES;
}

mhdrslt cls_webu_post::processor_start(const char *upload_data, size_t *upload_data_size)
{
     mhdrslt    retcd;

    if (*upload_data_size != 0) {
        retcd = MHD_post_process (post_processor, upload_data, *upload_data_size);
        *upload_data_size = 0;
    } else {
        pthread_mutex_lock(&app->mutex_post);
            process_actions();
        pthread_mutex_unlock(&app->mutex_post);
        /* Send updated page back to user */
        webu_html = new cls_webu_html(webua);
        webu_html->main();
        delete webu_html;
        retcd = MHD_YES;
    }
    return retcd;
}

cls_webu_post::cls_webu_post(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webua  = p_webua;

    post_processor  = nullptr;
    post_info   = nullptr;
    post_sz     = 0;

}

cls_webu_post::~cls_webu_post()
{
    int indx;

    if (post_processor != nullptr) {
        MHD_destroy_post_processor (post_processor);
    }

    for (indx = 0; indx<post_sz; indx++) {
        myfree(post_info[indx].key_nm);
        myfree(post_info[indx].key_val);
    }
    myfree(post_info);
}