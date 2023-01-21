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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_json.hpp"
#include "dbse.hpp"

static void webu_json_config_item(ctx_webui *webui, ctx_config *conf, int indx_parm)
{
    int indx;
    std::string parm_orig, parm_val, parm_list, parm_enable;

    parm_orig = "";
    parm_val = "";
    parm_list = "";

    if (webui->motapp->conf->webcontrol_parms < WEBUI_LEVEL_LIMITED) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    conf_edit_get(conf, config_parms[indx_parm].parm_name
        , parm_orig, config_parms[indx_parm].parm_cat);

    if (parm_orig.find("\"") != std::string::npos) {
        for (indx = 0; indx < (int)parm_orig.length(); indx++){
            if (parm_orig[indx] == '"') {
                parm_val += '\\';
            }
            parm_val += parm_orig[indx];
        }
    } else {
        parm_val = parm_orig;
    }

    if (config_parms[indx_parm].parm_type == PARM_TYP_INT) {
        webui->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":" + parm_val +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";

    } else if (config_parms[indx_parm].parm_type == PARM_TYP_BOOL) {
        if (parm_val == "on") {
            webui->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":true" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\""+
                "}";
        } else {
            webui->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":false" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
                "}";
        }
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
        conf_edit_list(conf, config_parms[indx_parm].parm_name
            , parm_list, config_parms[indx_parm].parm_cat);

        webui->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\": \"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            ",\"list\":" + parm_list +
            "}";

    } else {
        webui->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";
    }

}

static void webu_json_config_parms(ctx_webui *webui, ctx_config *conf)
{
    int indx_parm;
    bool first;
    std::string response;

    indx_parm = 0;
    first = true;
    while ((config_parms[indx_parm].parm_name != "") ) {
        if ((config_parms[indx_parm].webui_level == WEBUI_LEVEL_NEVER)) {
            indx_parm++;
            continue;
        }
        if (first) {
            first = false;
            webui->resp_page += "{";
        } else {
            webui->resp_page += ",";
        }
        /* Allow limited parameters to be read only to the web page */
        if ((config_parms[indx_parm].webui_level >
                webui->motapp->conf->webcontrol_parms) &&
            (config_parms[indx_parm].webui_level > WEBUI_LEVEL_LIMITED)) {

            webui->resp_page +=
                "\""+config_parms[indx_parm].parm_name+"\"" +
                ":{" +
                " \"value\":\"\"" +
                ",\"enabled\":false" +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\""+ conf_type_desc(config_parms[indx_parm].parm_type) + "\"";

            if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
                webui->resp_page += ",\"list\":[\"na\"]";
            }
            webui->resp_page +="}";
        } else {
           webu_json_config_item(webui, conf, indx_parm);
        }
        indx_parm++;
    }
    webui->resp_page += "}";

}

static void webu_json_config_cam_parms(ctx_webui *webui)
{
    int indx_cam;

    webui->resp_page += "{";
    webui->resp_page += "\"default\": ";
    webu_json_config_parms(webui, webui->motapp->conf);

    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        webui->resp_page += ",\"cam" +
            std::to_string(webui->motapp->cam_list[indx_cam]->device_id) + "\": ";
        webu_json_config_parms(webui, webui->motapp->cam_list[indx_cam]->conf);
        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_cam_list(ctx_webui *webui)
{
    int indx_cam;
    std::string response;
    std::string strid;
    ctx_dev     *cam;

    webui->resp_page += "{\"count\" : " + std::to_string(webui->motapp->cam_cnt);

    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        cam = webui->motapp->cam_list[indx_cam];
        strid =std::to_string(cam->device_id);
        webui->resp_page += ",\"" + std::to_string(indx_cam) + "\":";
        if (cam->conf->camera_name == "") {
            webui->resp_page += "{\"name\": \"camera " + strid + "\"";
        } else {
            webui->resp_page += "{\"name\": \"" + cam->conf->camera_name + "\"";
        }
        webui->resp_page += ",\"id\": " + strid;
        webui->resp_page += ",\"url\": \"" + webui->hostfull + "/" + strid + "/\"} ";
        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_categories(ctx_webui *webui)
{
    int indx_cat;
    std::string catnm_short, catnm_long;

    webui->resp_page += "{";

    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            webui->resp_page += ",";
        }
        webui->resp_page += "\"" + std::to_string(indx_cat) + "\": ";

        catnm_long = conf_cat_desc((enum PARM_CAT)indx_cat, false);
        catnm_short = conf_cat_desc((enum PARM_CAT)indx_cat, true);

        webui->resp_page += "{\"name\":\"" + catnm_short + "\",\"display\":\"" + catnm_long + "\"}";

        indx_cat++;
    }

    webui->resp_page += "}";

    return;

}

void webu_json_config(ctx_webui *webui)
{
    webui->resp_type = WEBUI_RESP_JSON;

    webui->resp_page += "{\"version\" : \"" VERSION "\"";

    webui->resp_page += ",\"cameras\" : ";
    webu_json_config_cam_list(webui);

    webui->resp_page += ",\"configuration\" : ";
    webu_json_config_cam_parms(webui);

    webui->resp_page += ",\"categories\" : ";
    webu_json_config_categories(webui);

    webui->resp_page += "}";

}

static void webu_json_movies_list(ctx_webui *webui)
{
    int indx_mov, indx_cam, indx;
    int movie_cnt, indx_req;
    std::string response;
    char fmt[PATH_MAX];
    ctx_dbse_rec db;
    ctx_params *wact;

    /* Get the indx we want */
    indx_cam = 0;
    indx_req = -1;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        if (webui->cam->device_id == webui->motapp->cam_list[indx_cam]->device_id){
            indx_req = indx_cam;
        }
        indx_cam++;
    }

    webui->resp_page += "{\"count\" : 1";
    webui->resp_page += ",\""+ std::to_string(indx_req) + "\":";

    if (webui->cam == NULL) {
        webui->resp_page += "{\"count\" : 0} ";
        webui->resp_page += "}";
        return;
    }

    /* Validate movies permitted via params */
    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"movies")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webui->resp_page += "{\"count\" : 0} ";
                webui->resp_page += "}";
                return;
            } else {
                break;
            }
        }
    }

    dbse_movies_getlist(webui->motapp, webui->cam->device_id);

    movie_cnt = webui->motapp->dbse->movie_cnt;
    webui->resp_page += "{";
    indx = 0;
    for (indx_mov=0; indx_mov < movie_cnt; indx_mov++) {
        db = webui->motapp->dbse->movie_list[indx_mov];
        if (db.found == true) {
            if ((db.movie_sz/1000) < 1000) {
                snprintf(fmt,PATH_MAX,"%'.1fKB"
                    ,((double)db.movie_sz/1000));
            } else if ((db.movie_sz/1000000) < 1000) {
                snprintf(fmt,PATH_MAX,"%'.1fMB"
                    ,((double)db.movie_sz/1000000));
            } else {
                snprintf(fmt,PATH_MAX,"%'.1fGB"
                    ,((double)db.movie_sz/1000000000));
            }
            webui->resp_page += "\""+ std::to_string(indx) + "\":";

            webui->resp_page += "{\"name\": \"";
            webui->resp_page += std::string(db.movie_nm) + "\"";

            webui->resp_page += ",\"size\": \"";
            webui->resp_page += std::string(fmt) + "\"";

            webui->resp_page += ",\"date\": \"";
            webui->resp_page += std::to_string(db.movie_dtl) + "\"";

            if (db.movie_tmc != NULL) {
                webui->resp_page += ",\"time\": \"";
                webui->resp_page += std::string(db.movie_tmc) + "\"";
            }

            webui->resp_page += ",\"diff_avg\": \"";
            webui->resp_page += std::to_string(db.diff_avg) + "\"";

            webui->resp_page += ",\"sdev_min\": \"";
            webui->resp_page += std::to_string(db.sdev_min) + "\"";

            webui->resp_page += ",\"sdev_max\": \"";
            webui->resp_page += std::to_string(db.sdev_max) + "\"";

            webui->resp_page += ",\"sdev_avg\": \"";
            webui->resp_page += std::to_string(db.sdev_avg) + "\"";

            webui->resp_page += "}";
            webui->resp_page += ",";
            indx++;
        }
    }
    webui->resp_page += "\"count\" : " + std::to_string(indx);
    webui->resp_page += "}";
    webui->resp_page += "}";

    return;

}

void webu_json_movies(ctx_webui *webui)
{
    webui->resp_type = WEBUI_RESP_JSON;

    webui->resp_page += "{\"movies\" : ";
    webu_json_movies_list(webui);

    webui->resp_page += "}";

}

static void webu_json_status_vars(ctx_webui *webui, int indx_cam)
{
    char buf[32];
    struct tm timestamp_tm;
    struct timespec curr_ts;
    ctx_dev *cam;

    cam = webui->motapp->cam_list[indx_cam];

    webui->resp_page += "{";

    webui->resp_page += "\"name\":\"" + cam->conf->camera_name+"\"";
    webui->resp_page += ",\"id\":" + std::to_string(cam->device_id);
    webui->resp_page += ",\"width\":" + std::to_string(cam->imgs.width);
    webui->resp_page += ",\"height\":" + std::to_string(cam->imgs.height);
    webui->resp_page += ",\"fps\":" + std::to_string(cam->lastrate);

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    localtime_r(&curr_ts.tv_sec, &timestamp_tm);
    strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
    webui->resp_page += ",\"current_time\":\"" + std::string(buf)+"\"";

    webui->resp_page += ",\"missing_frame_counter\":" +
        std::to_string(cam->missing_frame_counter);

    if (cam->lost_connection) {
        webui->resp_page += ",\"lost_connection\":true";
    } else {
        webui->resp_page += ",\"lost_connection\":false";
    }

    if (cam->connectionlosttime.tv_sec != 0) {
        localtime_r(&cam->connectionlosttime.tv_sec, &timestamp_tm);
        strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
        webui->resp_page += ",\"connection_lost_time\":\"" + std::string(buf)+"\"";
    } else {
        webui->resp_page += ",\"connection_lost_time\":\"\"" ;
    }
    if (cam->detecting_motion) {
        webui->resp_page += ",\"detecting\":true";
    } else {
        webui->resp_page += ",\"detecting\":false";
    }

    if (cam->pause) {
        webui->resp_page += ",\"pause\":true";
    } else {
        webui->resp_page += ",\"pause\":false";
    }

    webui->resp_page += "}";

}

void webu_json_status(ctx_webui *webui)
{
    int indx_cam;

    webui->resp_type = WEBUI_RESP_JSON;

    webui->resp_page += "{\"version\" : \"" VERSION "\"";
    webui->resp_page += ",\"status\" : ";

    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }
    webui->resp_page += "{\"count\" : " + std::to_string(indx_cam - 1);
        indx_cam = 0;
        while (webui->motapp->cam_list[indx_cam] != NULL) {
            webui->resp_page += ",\"cam" +
                std::to_string(webui->motapp->cam_list[indx_cam]->device_id) + "\": ";
            webu_json_status_vars(webui, indx_cam);
            indx_cam++;
        }
    webui->resp_page += "}";

    webui->resp_page += "}";

}
