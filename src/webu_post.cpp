/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
*/

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "allcam.hpp"
#include "sound.hpp"
#include "dbse.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_html.hpp"
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

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "camera_add") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera add action disabled");
                return;
            } else {
                break;
            }
        }
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Adding camera.");

    maxcnt = 100;

    app->cam_add = true;
    indx = 0;
    while ((app->cam_add == true) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }

    if (indx == maxcnt) {
        app->cam_add = false;
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error adding camera.  Timed out");
        return;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "New camera added.");

}

/* Process the delete camera action */
void cls_webu_post::cam_delete()
{
    int indx, maxcnt;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "camera_delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Camera delete action disabled");
                return;
            } else {
                break;
            }
        }
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Deleting camera.");

    app->cam_delete = webua->camindx;

    maxcnt = 100;
    indx = 0;
    while ((app->cam_delete != -1) && (indx < maxcnt)) {
        SLEEP(0, 50000000)
        indx++;
    }
    if (indx == maxcnt) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Error stopping camera.  Timed out shutting down");
        app->cam_delete = -1;
        return;
    }
    webua->cam = nullptr;
    webua->camindx = -1;
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

        MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"key: %s  value: %s "
            , post_info[indx].key_nm
            , post_info[indx].key_val
        );
    }

    if (post_cmd == "") {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , "Invalid post request.  No command");
        return;
    }
    if (webua->device_id == -1) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
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
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
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

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "event") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Event end action disabled");
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

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "event") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Event start action disabled");
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

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "snapshot") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Snapshot action disabled");
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

void cls_webu_post::action_pause_on()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "pause") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->user_pause = "on";
        }
    } else {
        app->cam_list[webua->camindx]->user_pause = "on";
    }

}

void cls_webu_post::action_pause_off()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "pause") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->user_pause = "off";
        }
    } else {
        app->cam_list[webua->camindx]->user_pause = "off";
    }

}

void cls_webu_post::action_pause_schedule()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "pause") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Pause action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->user_pause = "schedule";
        }
    } else {
        app->cam_list[webua->camindx]->user_pause = "schedule";
    }

}

/* Process the restart action */
void cls_webu_post::action_restart()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "restart") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Restart action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all cameras"));
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->handler_stop = false;
            app->cam_list[indx]->restart = true;
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Restarting camera %d")
            , app->cam_list[webua->camindx]->cfg->device_id);
        app->cam_list[webua->camindx]->handler_stop = false;
        app->cam_list[webua->camindx]->restart = true;
    }
}

/* Process the stop action */
void cls_webu_post::action_stop()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "stop") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Stop action disabled");
                return;
            } else {
                break;
            }
        }
    }

    if (webua->device_id == 0) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , _("Stopping cam %d")
                , app->cam_list[indx]->cfg->device_id);
            app->cam_list[indx]->restart = false;
            app->cam_list[indx]->event_stop = true;
            app->cam_list[indx]->event_user = false;
            app->cam_list[indx]->handler_stop = true;
        }
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
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

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "action_user") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "User action disabled");
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
                    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                        , _("Invalid character included in action user \"%c\"")
                        , tmp.at((uint)indx2));
                    return;
                }
            }
            snprintf(cam->action_user, 40, "%s", tmp.c_str());
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
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
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    , _("Invalid character included in action user \"%c\"")
                    , tmp.at((uint)indx2));
                return;
            }
        }
        snprintf(cam->action_user, 40, "%s", tmp.c_str());

        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Executing user action on cam %d")
            , cam->cfg->device_id);
        util_exec_command(cam, cam->cfg->on_action_user.c_str(), NULL);
    }

}

/* Process the write config action */
void cls_webu_post::write_config()
{
    int indx;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "config_write") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Config write action disabled");
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
    std::string parm_nm, parm_vl_cur;
    PARM_CAT    parm_ct;
    PARM_CHG    parm_ch;
    int indx;

    parm_nm = config_parms[indx_parm].parm_name;
    parm_ct = config_parms[indx_parm].parm_cat;
    parm_ch = config_parms[indx_parm].parm_chg;

    if (webua->device_id == 0) {
        app->conf_src->edit_get(parm_nm, parm_vl_cur, parm_ct);
        if (parm_vl == parm_vl_cur) {
            return;
        }
        if (parm_ct == PARM_CAT_00) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_ra_set("log",0, parm_ch);
        } else if (parm_ct == PARM_CAT_13) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_ra_set("webu",0, parm_ch);
        } else if (parm_ct == PARM_CAT_15) {
            app->conf_src->edit_set(parm_nm, parm_vl);
            config_ra_set("dbse",0, parm_ch);
        } else {
            app->conf_src->edit_set(parm_nm, parm_vl);
            if (parm_ct == PARM_CAT_14) {
                app->conf_src->edit_set(parm_nm, parm_vl);
                app->allcam->all_sizes.reset = true;
            }
            for (indx=0;indx<app->cam_cnt;indx++){
                app->cam_list[indx]->conf_src->edit_get(
                    parm_nm, parm_vl_cur, parm_ct);
                if (parm_vl_cur != parm_vl) {
                    if (app->cam_list[indx]->handler_running == true) {
                        app->cam_list[indx]->conf_src->edit_set(
                            parm_nm, parm_vl);
                        config_ra_set("cam",indx, parm_ch);
                    } else {
                        app->cam_list[indx]->conf_src->edit_set(parm_nm, parm_vl);
                    }
                }
            }
            for (indx=0;indx<app->snd_cnt;indx++) {
                app->snd_list[indx]->conf_src->edit_get(
                    parm_nm, parm_vl_cur, parm_ct);
                if (parm_vl_cur != parm_vl) {
                    if (app->snd_list[indx]->handler_running == true) {
                        app->snd_list[indx]->conf_src->edit_set(
                            parm_nm, parm_vl);
                        config_ra_set("snd",indx, parm_ch);
                    } else {
                        app->snd_list[indx]->conf_src->edit_set(parm_nm, parm_vl);
                    }
                }
            }
        }
    } else {    /* (webua->device_id != 0) */
        if ((parm_ct == PARM_CAT_00) ||
            (parm_ct == PARM_CAT_15)) {
            return;
        }
        app->cam_list[webua->camindx]->conf_src->edit_get(
            parm_nm, parm_vl_cur, parm_ct);
        if (parm_vl == parm_vl_cur) {
            return;
        }
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Config edit set. %s:%s"
            ,parm_nm.c_str(), parm_vl.c_str());
        app->cam_list[webua->camindx]->conf_src->edit_set(
            parm_nm, parm_vl);
        if (app->cam_list[webua->camindx]->handler_running == true) {
            config_ra_set("cam", webua->camindx, parm_ch);
        }
    }
}

void cls_webu_post::config_ra_set(
    std::string p_type, int p_indx, PARM_CHG p_chg)
{
    int indx;

    for (indx=0; indx<ra_list.size();indx++) {
        if ((ra_list[indx].comp_type == p_type) &&
            (ra_list[indx].comp_indx == p_indx)) {
            if (p_chg == PARM_CHG_RESTART) {
                ra_list[indx].restart = true;
            } else {
                ra_list[indx].apply = true;
            }
            break;
        }
    }
}

void cls_webu_post::config_ra_reset()
{
    ctx_ra_item itm_res;
    int indx;

    ra_list.clear();

    itm_res.restart = false;
    itm_res.apply = false;
    itm_res.comp_indx = 0;

    itm_res.comp_type ="log";
    ra_list.push_back(itm_res);

    itm_res.comp_type ="webu";
    ra_list.push_back(itm_res);

    itm_res.comp_type ="dbse";
    ra_list.push_back(itm_res);

    for (indx = 0; indx<app->cam_cnt; indx++) {
        itm_res.comp_type ="cam";
        itm_res.comp_indx = indx;
        ra_list.push_back(itm_res);
    }
    for (indx = 0; indx<app->snd_cnt; indx++) {
        itm_res.comp_type ="snd";
        itm_res.comp_indx = indx;
        ra_list.push_back(itm_res);
    }

}
/* Process the configuration parameters */
void cls_webu_post::config()
{
    int indx, indx2;
    std::string tmpname;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "config") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Config save action disabled");
                return;
            } else {
                break;
            }
        }
    }

    config_ra_reset();

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
                if ((config_parms[indx2].parm_lvl > app->conf_src->webcontrol_parms) ||
                    (config_parms[indx2].parm_lvl == PARM_LVL_99) ) {
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

    for (indx = 0; indx < ra_list.size(); indx++) {
        if (ra_list[indx].comp_type == "log") {
            if (ra_list[indx].restart == true) {
                motlog->restart = true;
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for log");
            } else if (ra_list[indx].apply == true) {
                motlog->conf_chg = true;
            }
        } else if (ra_list[indx].comp_type == "webu") {
            if (ra_list[indx].restart == true) {
                for (indx2 = 0; indx2 < app->webu_cnt; indx2++) {
                    app->webu_list[indx2]->restart = true;
                    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                        "Restart request for webcontrol on port %d"
                        ,app->webu_list[indx2]->cfg->webcontrol_port);
                }
            } else if (ra_list[indx].apply == true) {
                for (indx2 = 0; indx2 < app->webu_cnt; indx2++) {
                    app->webu_list[indx2]->conf_chg = true;
                }
            }
        } else if (ra_list[indx].comp_type == "dbse") {
            if (ra_list[indx].restart == true) {
                app->dbse->restart = true;
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for database");
            } else if (ra_list[indx].apply == true) {
                app->dbse->conf_chg = true;
            }
        } else if (ra_list[indx].comp_type == "cam") {
            if (ra_list[indx].restart == true) {
                app->cam_list[ra_list[indx].comp_indx]->restart = true;
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for camera %d"
                    , app->cam_list[ra_list[indx].comp_indx]->cfg->device_id);
            } else if (ra_list[indx].apply == true) {
                app->cam_list[ra_list[indx].comp_indx]->conf_chg = true;
            }
        } else if (ra_list[indx].comp_type == "snd") {
            if (ra_list[indx].restart == true) {
                app->snd_list[ra_list[indx].comp_indx]->restart = true;
                MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,
                    "Restart request for sound %d"
                    , app->cam_list[ra_list[indx].comp_indx]->cfg->device_id);
            } else if (ra_list[indx].apply == true) {
                app->snd_list[ra_list[indx].comp_indx]->conf_chg = true;
            }
        } else {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, "Bad programming");
        }
    }

}

/* Process the ptz action */
void cls_webu_post::ptz()
{
    cls_camera *cam;
    int indx;

    if (webua->camindx == -1) {
        return;
    }

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "ptz") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "PTZ actions disabled");
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
    if (webua->is_admin == false) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , "Actions not permitted for user");
        return;
    }

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
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("pause action deprecated.  Use pause_on"));
        action_pause_on();

    } else if (post_cmd == "unpause") {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("unpause action deprecated.  Use pause_off"));
        action_pause_off();

    } else if (post_cmd == "pause_on") {
        action_pause_on();

    } else if (post_cmd == "pause_off") {
        action_pause_off();

    } else if (post_cmd == "pause_schedule") {
        action_pause_schedule();

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
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO
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
    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "Index: %d Key: >%s< val: >%s< "
        , indx, post_info[indx].key_nm, post_info[indx].key_val);
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

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "Indx: %d Key: >%s< val: >%s< "
        , post_sz-1
        , post_info[post_sz-1].key_nm
        , post_info[post_sz-1].key_val);

    if (retcd < 0) {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, _("Error processing post data"));
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