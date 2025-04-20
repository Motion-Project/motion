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
#include "conf.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_text.hpp"
#include "webu_post.hpp"
#include "dbse.hpp"

void cls_webu_text::status_vars(int indx_cam)
{
    char buf[32];
    struct tm timestamp_tm;
    struct timespec curr_ts;
    cls_camera *cam;

    cam = app->cam_list[indx_cam];

    webua->resp_page += "{";

    webua->resp_page += ",\"id\":" + std::to_string(cam->cfg->device_id);
    webua->resp_page += ",\"width\":" + std::to_string(cam->imgs.width);
    webua->resp_page += ",\"height\":" + std::to_string(cam->imgs.height);
    webua->resp_page += ",\"fps\":" + std::to_string(cam->lastrate);

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    localtime_r(&curr_ts.tv_sec, &timestamp_tm);
    strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
    webua->resp_page += ",\"current_time\":\"" + std::string(buf)+"\"";

    webua->resp_page += ",\"missing_frame_counter\":" +
        std::to_string(cam->missing_frame_counter);

    if (cam->lost_connection) {
        webua->resp_page += ",\"lost_connection\":true";
    } else {
        webua->resp_page += ",\"lost_connection\":false";
    }

    if (cam->connectionlosttime.tv_sec != 0) {
        localtime_r(&cam->connectionlosttime.tv_sec, &timestamp_tm);
        strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
        webua->resp_page += ",\"connection_lost_time\":\"" + std::string(buf)+"\"";
    } else {
        webua->resp_page += ",\"connection_lost_time\":\"\"" ;
    }
    if (cam->detecting_motion) {
        webua->resp_page += ",\"detecting\":true";
    } else {
        webua->resp_page += ",\"detecting\":false";
    }

    if (cam->pause) {
        webua->resp_page += ",\"pause\":true";
    } else {
        webua->resp_page += ",\"pause\":false";
    }

    webua->resp_page += ",\"user_pause\":\"" + cam->user_pause +"\"";

    webua->resp_page += "}";
}

void cls_webu_text::status()
{
    int indx_cam, st, en;

    if (webua->device_id == 0) {
        st = 0;
        en = app->cam_cnt;
    } else {
        st = webua->camindx;
        en = webua->camindx+1;
    }

    webua->resp_type = WEBUI_RESP_TEXT;
    webua->resp_page="";
    for (indx_cam=st; indx_cam<en; indx_cam++) {
        webua->resp_page += "Camera ";
        webua->resp_page += std::to_string(app->cam_list[indx_cam]->cfg->device_id);
        webua->resp_page += " Detection status ";
        if (app->cam_list[indx_cam]->handler_running == false) {
            webua->resp_page += "NOT RUNNING";
        } else {
            if (app->cam_list[indx_cam]->pause) {
                webua->resp_page += "PAUSE";
            } else {
                webua->resp_page += "ACTIVE";
            }
        }
        webua->resp_page += "\n";
    }

}

void cls_webu_text::connection()
{
    int indx_cam, st, en;

    if (webua->device_id == 0) {
        st = 0;
        en = app->cam_cnt;
    } else {
        st = webua->camindx;
        en = webua->camindx+1;
    }
    webua->resp_type = WEBUI_RESP_TEXT;
    webua->resp_page = "";
    for (indx_cam=st; indx_cam<en; indx_cam++) {
        webua->resp_page += "Camera ";
        webua->resp_page += std::to_string(app->cam_list[indx_cam]->cfg->device_id);
        if (app->cam_list[indx_cam]->cfg->device_name != "") {
            webua->resp_page += " -- ";
            webua->resp_page += app->cam_list[indx_cam]->cfg->device_name;
            webua->resp_page += " -- ";
        }
        if (app->cam_list[indx_cam]->handler_running == false) {
            webua->resp_page += "NOT RUNNING";
        } else {
            if (app->cam_list[indx_cam]->lost_connection) {
                webua->resp_page += "Lost connection";
            } else {
                webua->resp_page += "Connection OK";
            }
        }
        webua->resp_page += "\n";
    }
    MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
        , "Action/Detection: >%s< camindx : >%d< "
        , webua->uri_cmd2.c_str(), webua->camindx);

}

void cls_webu_text::main()
{
    pthread_mutex_lock(&app->mutex_post);
        if ((webua->uri_cmd1 == "detection") &&
            (webua->uri_cmd2 == "status")) {
            status();
        } else if (
            (webua->uri_cmd1 == "detection") &&
            (webua->uri_cmd2 == "connection")) {
            connection();
        } else if (
            (webua->uri_cmd1 == "detection") &&
            (webua->uri_cmd2 == "pause")) {
            webu_post->action_pause_on();
        } else if (
            (webua->uri_cmd1 == "detection") &&
            (webua->uri_cmd2 == "start")) {
            webu_post->action_pause_off();
        } else if (
            (webua->uri_cmd1 == "action") &&
            (webua->uri_cmd2 == "eventend")) {
            webu_post->action_eventend();
        } else if (
            (webua->uri_cmd1 == "action") &&
            (webua->uri_cmd2 == "eventstart")) {
            webu_post->action_eventstart();
        } else if (
            (webua->uri_cmd1 == "action") &&
            (webua->uri_cmd2 == "snapshot")) {
            webu_post->action_snapshot();
        } else if (
            (webua->uri_cmd1 == "action") &&
            (webua->uri_cmd2 == "restart")) {
            webu_post->action_restart();
        } else if (
            (webua->uri_cmd1 == "action") &&
            ((webua->uri_cmd2 == "quit") ||
                (webua->uri_cmd2 == "end"))) {
            webu_post->action_stop();
        } else {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO
                , _("Invalid request: cmd1: >%s< cmd2: >%s< camindx : >%d< ")
                , webua->uri_cmd1.c_str()
                , webua->uri_cmd2.c_str()
                , webua->camindx);
        }
    pthread_mutex_unlock(&app->mutex_post);
    webua->mhd_send();
}

cls_webu_text::cls_webu_text(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webua  = p_webua;
    webu_post = new cls_webu_post(webua);
}

cls_webu_text::~cls_webu_text()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
    mydelete(webu_post);
}