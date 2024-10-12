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
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_common.hpp"
#include "webu_json.hpp"
#include "dbse.hpp"

std::string cls_webu_json::escstr(std::string invar)
{
    std::string  outvar;
    size_t indx;
    for (indx = 0; indx <invar.length(); indx++) {
        if (invar[indx] == '\\' ||
            invar[indx] == '\"') {
                outvar += '\\';
            }
        outvar += invar[indx];
    }
    return outvar;
}

void cls_webu_json::parms_item(cls_config *conf, int indx_parm)
{
    std::string parm_orig, parm_val, parm_list, parm_enable;

    parm_orig = "";
    parm_val = "";
    parm_list = "";

    if (app->cfg->webcontrol_parms < PARM_LEVEL_LIMITED) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    conf->edit_get(config_parms[indx_parm].parm_name
        , parm_orig, config_parms[indx_parm].parm_cat);

    parm_val = escstr(parm_orig);

    if (config_parms[indx_parm].parm_type == PARM_TYP_INT) {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":" + parm_val +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";

    } else if (config_parms[indx_parm].parm_type == PARM_TYP_BOOL) {
        if (parm_val == "on") {
            webua->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":true" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\""+
                "}";
        } else {
            webua->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":false" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
                "}";
        }
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
        conf->edit_list(config_parms[indx_parm].parm_name
            , parm_list, config_parms[indx_parm].parm_cat);

        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\": \"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            ",\"list\":" + parm_list +
            "}";

    } else {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";
    }
}

void cls_webu_json::parms_one(cls_config *conf)
{
    int indx_parm;
    bool first;
    std::string response;

    indx_parm = 0;
    first = true;
    while ((config_parms[indx_parm].parm_name != "") ) {
        if (config_parms[indx_parm].webui_level == PARM_LEVEL_NEVER) {
            indx_parm++;
            continue;
        }
        if (first) {
            first = false;
            webua->resp_page += "{";
        } else {
            webua->resp_page += ",";
        }
        /* Allow limited parameters to be read only to the web page */
        if ((config_parms[indx_parm].webui_level >
                app->cfg->webcontrol_parms) &&
            (config_parms[indx_parm].webui_level > PARM_LEVEL_LIMITED)) {

            webua->resp_page +=
                "\""+config_parms[indx_parm].parm_name+"\"" +
                ":{" +
                " \"value\":\"\"" +
                ",\"enabled\":false" +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"";

            if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
                webua->resp_page += ",\"list\":[\"na\"]";
            }
            webua->resp_page +="}";
        } else {
           parms_item(conf, indx_parm);
        }
        indx_parm++;
    }
    webua->resp_page += "}";
}

void cls_webu_json::parms_all()
{
    int indx_cam;

    webua->resp_page += "{";
    webua->resp_page += "\"default\": ";
    parms_one(app->cfg);

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        webua->resp_page += ",\"cam" +
            std::to_string(app->cam_list[indx_cam]->cfg->device_id) + "\": ";
        parms_one(app->cam_list[indx_cam]->cfg);
    }
    webua->resp_page += "}";
}

void cls_webu_json::cameras_list()
{
    int indx_cam;
    std::string response;
    std::string strid;
    cls_camera     *cam;

    webua->resp_page += "{\"count\" : " + std::to_string(app->cam_cnt);

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        cam = app->cam_list[indx_cam];
        strid =std::to_string(cam->cfg->device_id);
        webua->resp_page += ",\"" + std::to_string(indx_cam) + "\":";
        if (cam->cfg->device_name == "") {
            webua->resp_page += "{\"name\": \"camera " + strid + "\"";
        } else {
            webua->resp_page += "{\"name\": \"" + escstr(cam->cfg->device_name) + "\"";
        }
        webua->resp_page += ",\"id\": " + strid;
        webua->resp_page += ",\"url\": \"" + webua->hostfull + "/" + strid + "/\"} ";
    }
    webua->resp_page += "}";

}

void cls_webu_json::categories_list()
{
    int indx_cat;
    std::string catnm_short, catnm_long;

    webua->resp_page += "{";

    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            webua->resp_page += ",";
        }
        webua->resp_page += "\"" + std::to_string(indx_cat) + "\": ";

        catnm_long = webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, false);
        catnm_short = webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, true);

        webua->resp_page += "{\"name\":\"" + catnm_short + "\",\"display\":\"" + catnm_long + "\"}";

        indx_cat++;
    }

    webua->resp_page += "}";
}

void cls_webu_json::config()
{
    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"version\" : \"" VERSION "\"";

    webua->resp_page += ",\"cameras\" : ";
    cameras_list();

    webua->resp_page += ",\"configuration\" : ";
    parms_all();

    webua->resp_page += ",\"categories\" : ";
    categories_list();

    webua->resp_page += "}";
}

void cls_webu_json::movies_list()
{
    int indx;
    std::string response;
    char fmt[PATH_MAX];
    lst_movies movielist;
    it_movies m_it;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "movies") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webua->resp_page += "{\"count\" : 0} ";
                webua->resp_page += ",\"device_id\" : ";
                webua->resp_page += std::to_string(webua->cam->cfg->device_id);
                webua->resp_page += "}";
                return;
            } else {
                break;
            }
        }
    }

    app->dbse->movielist_get(webua->cam->cfg->device_id, &movielist);

    webua->resp_page += "{";
    indx = 0;
    for (m_it = movielist.begin(); m_it != movielist.end(); m_it++){
        if (m_it->found == true) {
            if ((m_it->movie_sz/1000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fKB"
                    ,((double)m_it->movie_sz/1000));
            } else if ((m_it->movie_sz/1000000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fMB"
                    ,((double)m_it->movie_sz/1000000));
            } else {
                snprintf(fmt,PATH_MAX,"%.1fGB"
                    ,((double)m_it->movie_sz/1000000000));
            }
            webua->resp_page += "\""+ std::to_string(indx) + "\":";

            webua->resp_page += "{\"name\": \"";
            webua->resp_page += escstr(m_it->movie_nm) + "\"";

            webua->resp_page += ",\"size\": \"";
            webua->resp_page += std::string(fmt) + "\"";

            webua->resp_page += ",\"date\": \"";
            webua->resp_page += std::to_string(m_it->movie_dtl) + "\"";

            webua->resp_page += ",\"time\": \"";
            webua->resp_page += m_it->movie_tmc + "\"";

            webua->resp_page += ",\"diff_avg\": \"";
            webua->resp_page += std::to_string(m_it->diff_avg) + "\"";

            webua->resp_page += ",\"sdev_min\": \"";
            webua->resp_page += std::to_string(m_it->sdev_min) + "\"";

            webua->resp_page += ",\"sdev_max\": \"";
            webua->resp_page += std::to_string(m_it->sdev_max) + "\"";

            webua->resp_page += ",\"sdev_avg\": \"";
            webua->resp_page += std::to_string(m_it->sdev_avg) + "\"";

            webua->resp_page += "}";
            webua->resp_page += ",";
            indx++;
        }
    }
    webua->resp_page += "\"count\" : " + std::to_string(indx);
    webua->resp_page += ",\"device_id\" : ";
    webua->resp_page += std::to_string(webua->cam->cfg->device_id);
    webua->resp_page += "}";
}

void cls_webu_json::movies()
{
    int indx_cam, indx_req;

    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"movies\" : ";
    if (webua->cam == NULL) {
        webua->resp_page += "{\"count\" :" + std::to_string(app->cam_cnt);

        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            webua->cam = app->cam_list[indx_cam];
            webua->resp_page += ",\""+ std::to_string(indx_cam) + "\":";
            movies_list();
        }
        webua->resp_page += "}";
        webua->cam = NULL;
    } else {
        indx_req = -1;
        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            if (webua->cam->cfg->device_id == app->cam_list[indx_cam]->cfg->device_id){
                indx_req = indx_cam;
            }
        }
        webua->resp_page += "{\"count\" : 1";
        webua->resp_page += ",\""+ std::to_string(indx_req) + "\":";
        movies_list();
        webua->resp_page += "}";
    }
    webua->resp_page += "}";
}

void cls_webu_json::status_vars(int indx_cam)
{
    char buf[32];
    struct tm timestamp_tm;
    struct timespec curr_ts;
    cls_camera *cam;

    cam = app->cam_list[indx_cam];

    webua->resp_page += "{";

    webua->resp_page += "\"name\":\"" + escstr(cam->cfg->device_name)+"\"";
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

    webua->resp_page += "}";
}

void cls_webu_json::status()
{
    int indx_cam;

    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"version\" : \"" VERSION "\"";
    webua->resp_page += ",\"status\" : ";

    webua->resp_page += "{\"count\" : " + std::to_string(app->cam_cnt);
        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            webua->resp_page += ",\"cam" +
                std::to_string(app->cam_list[indx_cam]->cfg->device_id) + "\": ";
            status_vars(indx_cam);
        }
    webua->resp_page += "}";

    webua->resp_page += "}";
}

void cls_webu_json::loghistory()
{
    int indx, cnt;
    bool frst;

    webua->resp_type = WEBUI_RESP_JSON;

    pthread_mutex_lock(&motlog->mutex_log);
        frst = true;
        cnt = 0;
        for (indx=0; indx<motlog->log_vec.size();indx++) {
            if (motlog->log_vec[indx].log_nbr > mtoi(webua->uri_cmd2)) {
                if (frst == true) {
                    webua->resp_page += "{";
                    frst = false;
                } else {
                    webua->resp_page += ",";
                }
                webua->resp_page += "\"" + std::to_string(indx) +"\" : {";
                webua->resp_page += "\"lognbr\" :\"" +
                    std::to_string(motlog->log_vec[indx].log_nbr) + "\", ";
                webua->resp_page += "\"logmsg\" :\"" +
                    escstr(motlog->log_vec[indx].log_msg.substr(0,
                        motlog->log_vec[indx].log_msg.length()-1)) + "\" ";
                webua->resp_page += "}";
                cnt++;
            }
        }
    pthread_mutex_unlock(&motlog->mutex_log);
    if (frst == true) {
        webua->resp_page += "{\"0\": \"\" ";
    }
    webua->resp_page += ",\"count\":\""+std::to_string(cnt)+"\"}";
}

void cls_webu_json::main()
{
    pthread_mutex_lock(&app->mutex_post);
        if (webua->uri_cmd1 == "config.json") {
            config();
        } else if (webua->uri_cmd1 == "movies.json") {
            movies();
        } else if (webua->uri_cmd1 == "status.json") {
            status();
        } else if (webua->uri_cmd1 == "log") {
            loghistory();
        } else {
            webua->bad_request();
            pthread_mutex_unlock(&app->mutex_post);
            return;
        }
    pthread_mutex_unlock(&app->mutex_post);
    webua->mhd_send();
}

cls_webu_json::cls_webu_json(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webua  = p_webua;
}

cls_webu_json::~cls_webu_json()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
}