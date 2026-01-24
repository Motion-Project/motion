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

/*
 * webu_json.cpp - JSON REST API Implementation
 *
 * This module implements the JSON REST API for configuration management,
 * camera control, status queries, and profile operations, serving as the
 * primary interface between the React frontend and Motion backend.
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "conf_profile.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_auth.hpp"
#include "webu_json.hpp"
#include "dbse.hpp"
#include "libcam.hpp"
#include "json_parse.hpp"
#include <map>
#include <algorithm>
#include <vector>
#include <thread>
#include <functional>
#include <unordered_map>
#include <sys/statvfs.h>
#include <dirent.h>
#include <set>

/* CPU-efficient polygon fill using scanline algorithm
 * Fills polygon interior with specified value in bitmap
 * O(height * edges) complexity, minimal memory allocation
 */
static void fill_polygon(u_char *bitmap, int width, int height,
    const std::vector<std::pair<int,int>> &polygon, u_char fill_val)
{
    if (polygon.size() < 3) return;

    /* Find vertical bounds */
    int min_y = height, max_y = 0;
    for (const auto &pt : polygon) {
        if (pt.second < min_y) min_y = pt.second;
        if (pt.second > max_y) max_y = pt.second;
    }

    /* Clamp to image bounds */
    if (min_y < 0) min_y = 0;
    if (max_y >= height) max_y = height - 1;

    /* Scanline fill */
    std::vector<int> x_intersects;
    for (int y = min_y; y <= max_y; y++) {
        x_intersects.clear();

        /* Find intersections with polygon edges */
        size_t n = polygon.size();
        for (size_t i = 0; i < n; i++) {
            int x1 = polygon[i].first;
            int y1 = polygon[i].second;
            int x2 = polygon[(i + 1) % n].first;
            int y2 = polygon[(i + 1) % n].second;

            /* Check if edge crosses this scanline */
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                /* Compute x intersection using integer math to avoid float */
                int x = x1 + ((y - y1) * (x2 - x1)) / (y2 - y1);
                x_intersects.push_back(x);
            }
        }

        /* Sort intersections */
        std::sort(x_intersects.begin(), x_intersects.end());

        /* Fill between pairs */
        for (size_t i = 0; i + 1 < x_intersects.size(); i += 2) {
            int xs = x_intersects[i];
            int xe = x_intersects[i + 1];

            /* Clamp to image bounds */
            if (xs < 0) xs = 0;
            if (xe >= width) xe = width - 1;

            /* Fill the span */
            for (int x = xs; x <= xe; x++) {
                bitmap[y * width + x] = fill_val;
            }
        }
    }
}

/* Generate auto-path for mask file in target_dir */
static std::string build_mask_path(cls_camera *cam, const std::string &type)
{
    std::string target = cam->cfg->target_dir;
    if (target.empty()) {
        target = "/var/lib/motion";
    }
    /* Remove trailing slash */
    if (!target.empty() && target.back() == '/') {
        target.pop_back();
    }
    return target + "/cam" + std::to_string(cam->cfg->device_id) +
           "_" + type + ".pgm";
}

/* Hot-reload parameter dispatch table
 * Maps parameter names to lambda functions that apply the change to a camera
 * This replaces the 28-branch if/else chain with O(1) hash map lookup
 */
namespace {
    using HotReloadFunc = std::function<void(cls_camera*, const std::string&)>;

    const std::unordered_map<std::string, HotReloadFunc> hot_reload_map = {
        {"libcam_brightness", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_brightness(atof(val.c_str()));
        }},
        {"libcam_contrast", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_contrast(atof(val.c_str()));
        }},
        {"libcam_gain", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_gain(atof(val.c_str()));
        }},
        {"libcam_awb_enable", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_awb_enable(val == "true" || val == "1");
        }},
        {"libcam_awb_mode", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_awb_mode(atoi(val.c_str()));
        }},
        {"libcam_awb_locked", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_awb_locked(val == "true" || val == "1");
        }},
        {"libcam_colour_temp", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_colour_temp(atoi(val.c_str()));
        }},
        {"libcam_colour_gain_r", [](cls_camera *cam, const std::string &val) {
            float r = atof(val.c_str());
            float b = cam->cfg->parm_cam.libcam_colour_gain_b;
            cam->set_libcam_colour_gains(r, b);
        }},
        {"libcam_colour_gain_b", [](cls_camera *cam, const std::string &val) {
            float r = cam->cfg->parm_cam.libcam_colour_gain_r;
            float b = atof(val.c_str());
            cam->set_libcam_colour_gains(r, b);
        }},
        {"libcam_af_mode", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_af_mode(atoi(val.c_str()));
        }},
        {"libcam_lens_position", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_lens_position(atof(val.c_str()));
        }},
        {"libcam_af_range", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_af_range(atoi(val.c_str()));
        }},
        {"libcam_af_speed", [](cls_camera *cam, const std::string &val) {
            cam->set_libcam_af_speed(atoi(val.c_str()));
        }},
        {"libcam_af_trigger", [](cls_camera *cam, const std::string &val) {
            int v = atoi(val.c_str());
            if (v == 0) {
                cam->trigger_libcam_af_scan();
            } else {
                cam->cancel_libcam_af_scan();
            }
        }},
    };
}

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

void cls_webu_json::parms_item_detail(cls_config *conf, std::string pNm)
{
    ctx_params  *params;
    ctx_params_item *itm;
    int indx;

    params = new ctx_params;
    params->params_cnt = 0;
    mylower(pNm);

    if (pNm == "v4l2_params") {
        util_parms_parse(params, pNm, conf->v4l2_params);
    } else if (pNm == "netcam_params") {
        util_parms_parse(params, pNm, conf->netcam_params);
    } else if (pNm == "netcam_high_params") {
        util_parms_parse(params, pNm, conf->netcam_high_params);
    } else if (pNm == "libcam_params") {
        util_parms_parse(params, pNm, conf->libcam_params);
    } else if (pNm == "schedule_params") {
        util_parms_parse(params, pNm, conf->schedule_params);
    } else if (pNm == "picture_schedule_params") {
        util_parms_parse(params, pNm, conf->picture_schedule_params);
    } else if (pNm == "cleandir_params") {
        util_parms_parse(params, pNm, conf->cleandir_params);
    } else if (pNm == "secondary_params") {
        util_parms_parse(params, pNm, conf->secondary_params);
    } else if (pNm == "webcontrol_actions") {
        util_parms_parse(params, pNm, conf->webcontrol_actions);
    } else if (pNm == "webcontrol_headers") {
        util_parms_parse(params, pNm, conf->webcontrol_headers);
    } else if (pNm == "stream_preview_params") {
        util_parms_parse(params, pNm, conf->stream_preview_params);
    } else if (pNm == "snd_params") {
        util_parms_parse(params, pNm, conf->snd_params);
    }

    webua->resp_page += ",\"count\":";
    webua->resp_page += std::to_string(params->params_cnt);

    if (params->params_cnt > 0) {
        webua->resp_page += ",\"parsed\" :{";
        for (indx=0; indx<params->params_cnt; indx++) {
            itm = &params->params_array[indx];
            if (indx != 0) {
                webua->resp_page += ",";
            }
            webua->resp_page += "\""+std::to_string(indx)+"\":";
            webua->resp_page += "{\"name\":\""+itm->param_name+"\",";
            webua->resp_page += "\"value\":\""+itm->param_value+"\"}";

        }
        webua->resp_page += "}";
    }

    mydelete(params);

}

void cls_webu_json::parms_item(cls_config *conf, int indx_parm)
{
    std::string parm_orig, parm_val, parm_list, parm_enable;
    std::string parm_name = config_parms[indx_parm].parm_name;
    bool password_set = false;

    parm_orig = "";
    parm_val = "";
    parm_list = "[]";  // Default to empty JSON array for valid JSON

    if (app->cfg->webcontrol_parms < PARM_LEVEL_LIMITED) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    conf->edit_get(config_parms[indx_parm].parm_name
        , parm_orig, config_parms[indx_parm].parm_cat);

    /* Mask password values for authentication parameters
     * Returns username with empty password, plus password_set flag */
    if (parm_name == "webcontrol_authentication" ||
        parm_name == "webcontrol_user_authentication") {
        size_t colon_pos = parm_orig.find(':');
        if (colon_pos != std::string::npos) {
            std::string username = parm_orig.substr(0, colon_pos);
            std::string password = parm_orig.substr(colon_pos + 1);
            password_set = !password.empty();
            /* Return username with empty password portion */
            parm_val = escstr(username) + ":";
        } else {
            parm_val = "";
        }
    } else {
        parm_val = escstr(parm_orig);
    }

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
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_PARAMS) {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"";
        parms_item_detail(conf, config_parms[indx_parm].parm_name);
        webua->resp_page += "}";
    } else {
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"";
        /* Add password_set flag for authentication parameters */
        if (parm_name == "webcontrol_authentication" ||
            parm_name == "webcontrol_user_authentication") {
            webua->resp_page += ",\"password_set\":" + std::string(password_set ? "true" : "false");
        }
        webua->resp_page += "}";
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
        webua->resp_page += ",\"all_xpct_st\": " + std::to_string(cam->all_loc.xpct_st);
        webua->resp_page += ",\"all_xpct_en\": " + std::to_string(cam->all_loc.xpct_en);
        webua->resp_page += ",\"all_ypct_st\": " + std::to_string(cam->all_loc.ypct_st);
        webua->resp_page += ",\"all_ypct_en\": " + std::to_string(cam->all_loc.ypct_en);
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
    int indx, indx2;
    std::string response;
    char fmt[PATH_MAX];
    vec_files flst;
    std::string sql;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "movies") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
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

    sql  = " select * from motion ";
    sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " order by file_dtl, file_tml;";
    app->dbse->filelist_get(sql, flst);

    webua->resp_page += "{";
    indx = 0;
    for (indx2=0;indx2<flst.size();indx2++){
        if (flst[indx2].found == true) {
            if ((flst[indx2].file_sz/1000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fKB"
                    ,((double)flst[indx2].file_sz/1000));
            } else if ((flst[indx2].file_sz/1000000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fMB"
                    ,((double)flst[indx2].file_sz/1000000));
            } else {
                snprintf(fmt,PATH_MAX,"%.1fGB"
                    ,((double)flst[indx2].file_sz/1000000000));
            }
            webua->resp_page += "\""+ std::to_string(indx) + "\":";

            webua->resp_page += "{\"name\": \"";
            webua->resp_page += escstr(flst[indx2].file_nm) + "\"";

            webua->resp_page += ",\"size\": \"";
            webua->resp_page += std::string(fmt) + "\"";

            webua->resp_page += ",\"date\": \"";
            webua->resp_page += std::to_string(flst[indx2].file_dtl) + "\"";

            webua->resp_page += ",\"time\": \"";
            webua->resp_page += flst[indx2].file_tmc + "\"";

            webua->resp_page += ",\"diff_avg\": \"";
            webua->resp_page += std::to_string(flst[indx2].diff_avg) + "\"";

            webua->resp_page += ",\"sdev_min\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_min) + "\"";

            webua->resp_page += ",\"sdev_max\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_max) + "\"";

            webua->resp_page += ",\"sdev_avg\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_avg) + "\"";

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

    webua->resp_page += ",\"user_pause\":\"" + cam->user_pause +"\"";

    /* Add supportedControls for libcamera capability discovery */
    #ifdef HAVE_LIBCAM
    if (cam->has_libcam()) {
        webua->resp_page += ",\"supportedControls\":{";
        std::map<std::string, bool> caps = cam->get_libcam_capabilities();
        bool first = true;
        for (const auto& [name, supported] : caps) {
            if (!first) {
                webua->resp_page += ",";
            }
            webua->resp_page += "\"" + name + "\":" +
                               (supported ? "true" : "false");
            first = false;
        }
        webua->resp_page += "}";
    }
    #endif

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
    webua->resp_page = "";

    frst = true;
    cnt = 0;

    pthread_mutex_lock(&motlog->mutex_log);
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
        webua->resp_page += "{\"0\":\"\" ";
    }
    webua->resp_page += ",\"count\":\""+std::to_string(cnt)+"\"}";

}

/*
 * Hot Reload API: Validate parameter exists and is hot-reloadable
 * Returns true if parameter can be hot-reloaded
 * Sets parm_index to the index in config_parms[] (-1 if not found)
 */
bool cls_webu_json::validate_hot_reload(const std::string &parm_name, int &parm_index)
{
    parm_index = 0;
    while (config_parms[parm_index].parm_name != "") {
        if (config_parms[parm_index].parm_name == parm_name) {
            /* Check permission level */
            if (config_parms[parm_index].webui_level > app->cfg->webcontrol_parms) {
                return false;
            }
            /* Check hot reload flag */
            return config_parms[parm_index].hot_reload;
        }
        parm_index++;
    }
    parm_index = -1;  /* Not found */
    return false;
}

/*
 * Hot Reload Helper: Apply hot-reloadable parameter to a specific camera
 * Uses lookup table for O(1) dispatch instead of if/else chain
 */
void cls_webu_json::apply_hot_reload_to_camera(cls_camera *cam,
    const std::string &parm_name, const std::string &parm_val)
{
    auto it = hot_reload_map.find(parm_name);
    if (it != hot_reload_map.end()) {
        it->second(cam, parm_val);
    }
}

/*
 * Hot Reload API: Apply parameter change to config
 */
void cls_webu_json::apply_hot_reload(int parm_index, const std::string &parm_val)
{
    std::string parm_name = config_parms[parm_index].parm_name;

    if (webua->device_id == 0) {
        /* Update default config */
        app->cfg->edit_set(parm_name, parm_val);
        app->conf_src->edit_set(parm_name, parm_val);

        /* Update all running cameras - currently unreachable from UI but kept for
         * future "Apply to All Cameras" feature and external API clients */
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->cfg->edit_set(parm_name, parm_val);
            app->cam_list[indx]->conf_src->edit_set(parm_name, parm_val);
            apply_hot_reload_to_camera(app->cam_list[indx], parm_name, parm_val);
        }
    } else if (webua->cam != nullptr) {
        /* Update specific camera only */
        webua->cam->cfg->edit_set(parm_name, parm_val);
        webua->cam->conf_src->edit_set(parm_name, parm_val);
        apply_hot_reload_to_camera(webua->cam, parm_name, parm_val);
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Hot reload: %s = %s (camera %d)",
        parm_name.c_str(), parm_val.c_str(), webua->device_id);
}


/*
 * External API: Authentication status for HTTP Basic/Digest clients
 * GET /0/api/auth/me
 *
 * This endpoint is used by external API clients (curl, scripts, automation tools)
 * that authenticate via HTTP Basic/Digest. The React UI uses /0/api/auth/status instead.
 */
void cls_webu_json::api_auth_me()
{
    webua->resp_page = "{";

    /* Check if authentication is configured */
    if (app->cfg->webcontrol_authentication != "") {
        webua->resp_page += "\"authenticated\":true,";
        webua->resp_page += "\"auth_method\":\"digest\",";

        /* Include role from HTTP Basic/Digest auth */
        if (webua->auth_role != "") {
            webua->resp_page += "\"role\":\"" + webua->auth_role + "\"";
        } else {
            /* Default to admin if role not determined */
            webua->resp_page += "\"role\":\"admin\"";
        }
    } else {
        webua->resp_page += "\"authenticated\":false";
    }

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Login with session creation
 * POST /0/api/auth/login
 * Body: {username, password}
 * Returns: {session_token, csrf_token, role, expires_in}
 */
void cls_webu_json::api_auth_login()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Only accept POST */
    if (webua->get_method() != WEBUI_METHOD_POST) {
        webua->resp_page = "{\"error\":\"Method not allowed\"}";
        webua->resp_code = 405;
        return;
    }

    /* Parse JSON body for username/password */
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        webua->resp_page = "{\"error\":\"Invalid JSON\"}";
        webua->resp_code = 400;
        return;
    }

    std::string username = parser.getString("username");
    std::string password = parser.getString("password");

    if (username.empty() || password.empty()) {
        webua->resp_page = "{\"error\":\"Missing username or password\"}";
        webua->resp_code = 400;
        return;
    }

    /* Validate credentials against config */
    std::string role = "";

    /* Check admin credentials */
    std::string admin_auth = app->cfg->webcontrol_authentication;
    if (!admin_auth.empty()) {
        size_t colon_pos = admin_auth.find(':');
        if (colon_pos != std::string::npos) {
            std::string admin_user = admin_auth.substr(0, colon_pos);
            std::string stored_value = admin_auth.substr(colon_pos + 1);

            /* Verify username matches */
            if (username == admin_user) {
                /* Check if stored value is bcrypt hash or plaintext */
                if (cls_webu_auth::is_bcrypt_hash(stored_value)) {
                    /* Bcrypt hash - verify password */
                    if (cls_webu_auth::verify_password(password, stored_value)) {
                        role = "admin";
                    }
                } else {
                    /* Plaintext password (for initial setup compatibility) */
                    if (password == stored_value) {
                        role = "admin";

                        /* Log warning about plaintext password */
                        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
                            "Plaintext admin password detected - "
                            "run motion-setup to hash credentials");
                    }
                }
            }
        }
    }

    /* Check user credentials if admin didn't match */
    if (role.empty()) {
        std::string user_auth = app->cfg->webcontrol_user_authentication;
        if (!user_auth.empty()) {
            size_t colon_pos = user_auth.find(':');
            if (colon_pos != std::string::npos) {
                std::string user_user = user_auth.substr(0, colon_pos);
                std::string stored_value = user_auth.substr(colon_pos + 1);

                /* Verify username matches */
                if (username == user_user) {
                    /* Check if stored value is bcrypt hash or plaintext */
                    if (cls_webu_auth::is_bcrypt_hash(stored_value)) {
                        /* Bcrypt hash - verify password */
                        if (cls_webu_auth::verify_password(password, stored_value)) {
                            role = "user";
                        }
                    } else {
                        /* Plaintext password (for initial setup compatibility) */
                        if (password == stored_value) {
                            role = "user";

                            /* Log warning about plaintext password */
                            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
                                "Plaintext viewer password detected - "
                                "run motion-setup to hash credentials");
                        }
                    }
                }
            }
        }
    }

    if (role.empty()) {
        /* Log failed attempt for rate limiting */
        webua->failauth_log(true, username);

        webua->resp_page = "{\"error\":\"Invalid credentials\"}";
        webua->resp_code = 401;
        return;
    }

    /* Create session */
    std::string session_token = webu->session_create(role, webua->clientip);
    std::string csrf_token = webu->session_get_csrf(session_token);

    /* Return session info */
    webua->resp_page = "{";
    webua->resp_page += "\"session_token\":\"" + session_token + "\",";
    webua->resp_page += "\"csrf_token\":\"" + csrf_token + "\",";
    webua->resp_page += "\"role\":\"" + role + "\",";
    webua->resp_page += "\"expires_in\":" + std::to_string(app->cfg->webcontrol_session_timeout);
    webua->resp_page += "}";
}

/*
 * React UI API: Logout (destroy session)
 * POST /0/api/auth/logout
 */
void cls_webu_json::api_auth_logout()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->get_method() != WEBUI_METHOD_POST) {
        webua->resp_page = "{\"error\":\"Method not allowed\"}";
        webua->resp_code = 405;
        return;
    }

    /* Get session token from header */
    std::string session_token = webua->session_token;

    if (!session_token.empty()) {
        webu->session_destroy(session_token);
    }

    webua->resp_page = "{\"success\":true}";
}

/*
 * React UI API: Get authentication status
 * GET /0/api/auth/status
 * Returns: {auth_required, authenticated, role?, csrf_token?}
 */
void cls_webu_json::api_auth_status()
{
    webua->resp_type = WEBUI_RESP_JSON;
    webua->resp_page = "{";

    /* Check if authentication is configured */
    bool auth_required = (app->cfg->webcontrol_authentication != "");

    webua->resp_page += "\"auth_required\":" + std::string(auth_required ? "true" : "false");

    if (!auth_required) {
        /* No auth configured - full access with pseudo-session for CSRF protection */
        /* Create or reuse session for CSRF token even when auth not required */
        if (webua->session_token.empty()) {
            /* No session yet - create pseudo-session for CSRF */
            std::string new_token = webu->session_create("admin", webua->clientip);
            webua->resp_page += ",\"authenticated\":true";
            webua->resp_page += ",\"role\":\"admin\"";
            webua->resp_page += ",\"session_token\":\"" + new_token + "\"";
            webua->resp_page += ",\"csrf_token\":\"" + webu->session_get_csrf(new_token) + "\"";
        } else {
            /* Reuse existing session */
            std::string role = webu->session_validate(webua->session_token, webua->clientip);
            if (!role.empty()) {
                webua->resp_page += ",\"authenticated\":true";
                webua->resp_page += ",\"role\":\"" + role + "\"";
                webua->resp_page += ",\"csrf_token\":\"" + webu->session_get_csrf(webua->session_token) + "\"";
            } else {
                /* Session expired - create new one */
                std::string new_token = webu->session_create("admin", webua->clientip);
                webua->resp_page += ",\"authenticated\":true";
                webua->resp_page += ",\"role\":\"admin\"";
                webua->resp_page += ",\"session_token\":\"" + new_token + "\"";
                webua->resp_page += ",\"csrf_token\":\"" + webu->session_get_csrf(new_token) + "\"";
            }
        }
    } else if (!webua->session_token.empty()) {
        /* Session token provided - validate it */
        std::string role = webu->session_validate(
            webua->session_token, webua->clientip);

        if (!role.empty()) {
            webua->resp_page += ",\"authenticated\":true";
            webua->resp_page += ",\"role\":\"" + role + "\"";
            webua->resp_page += ",\"csrf_token\":\"" +
                webu->session_get_csrf(webua->session_token) + "\"";
        } else {
            webua->resp_page += ",\"authenticated\":false";
        }
    } else if (!webua->auth_role.empty()) {
        /* HTTP Basic/Digest auth for external API clients (curl, scripts, etc.) */
        webua->resp_page += ",\"authenticated\":true";
        webua->resp_page += ",\"role\":\"" + webua->auth_role + "\"";
        webua->resp_page += ",\"csrf_token\":\"" + webu->csrf_token + "\"";
    } else {
        /* Auth required but no credentials */
        webua->resp_page += ",\"authenticated\":false";
    }

    webua->resp_page += "}";
}

/*
 * React UI API: Media pictures list
 * Returns list of snapshot images for a camera
 */
void cls_webu_json::api_media_pictures()
{
    vec_files flst, flst_count;
    std::string sql, where_clause;
    int offset = 0, limit = 100;
    int64_t total_count = 0;
    const char* date_filter = nullptr;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    /* Parse query parameters */
    const char* offset_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "offset");
    const char* limit_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "limit");
    date_filter = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "date");

    if (offset_str) offset = std::max(0, atoi(offset_str));
    if (limit_str) limit = std::min(std::max(1, atoi(limit_str)), 100); // Cap at 100

    /* Build WHERE clause */
    where_clause  = " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    where_clause += " and file_typ = 'pic'";
    if (date_filter && strlen(date_filter) == 8) {
        where_clause += " and file_dtl = " + std::string(date_filter);
    }

    /* Get total count - query just record_id for efficiency */
    sql = " select record_id from motion " + where_clause + ";";
    app->dbse->filelist_get(sql, flst_count);
    total_count = flst_count.size();

    /* Get paginated results */
    sql  = " select * from motion ";
    sql += where_clause;
    sql += " order by file_dtl desc, file_tml desc";
    sql += " limit " + std::to_string(limit);
    sql += " offset " + std::to_string(offset) + ";";

    app->dbse->filelist_get(sql, flst);

    /* Build JSON response with pagination metadata */
    webua->resp_page = "{";
    webua->resp_page += "\"total_count\":" + std::to_string(total_count) + ",";
    webua->resp_page += "\"offset\":" + std::to_string(offset) + ",";
    webua->resp_page += "\"limit\":" + std::to_string(limit) + ",";
    webua->resp_page += "\"date_filter\":";
    if (date_filter) {
        webua->resp_page += "\"" + std::string(date_filter) + "\"";
    } else {
        webua->resp_page += "null";
    }
    webua->resp_page += ",\"pictures\":[";

    for (size_t i = 0; i < flst.size(); i++) {
        if (i > 0) webua->resp_page += ",";
        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + std::to_string(flst[i].record_id) + ",";
        webua->resp_page += "\"filename\":\"" + escstr(flst[i].file_nm) + "\",";
        webua->resp_page += "\"path\":\"" + escstr(flst[i].full_nm) + "\",";
        webua->resp_page += "\"date\":\"" + std::to_string(flst[i].file_dtl) + "\",";
        webua->resp_page += "\"time\":\"" + escstr(flst[i].file_tml) + "\",";
        webua->resp_page += "\"size\":" + std::to_string(flst[i].file_sz);
        webua->resp_page += "}";
    }
    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Delete a picture file
 * DELETE /{camId}/api/media/picture/{id}
 * Deletes both the file and database record
 */
void cls_webu_json::api_delete_picture()
{
    int indx;
    std::string sql, full_path;
    vec_files flst;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Check if delete action is enabled */
    for (indx=0; indx<webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Delete action disabled");
                webua->resp_page = "{\"error\":\"Delete action is disabled\"}";
                webua->resp_type = WEBUI_RESP_JSON;
                return;
            }
            break;
        }
    }

    /* Get file ID from URI: uri_cmd4 contains the record ID */
    if (webua->uri_cmd4.empty()) {
        webua->resp_page = "{\"error\":\"File ID required\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    int file_id = mtoi(webua->uri_cmd4);
    if (file_id <= 0) {
        webua->resp_page = "{\"error\":\"Invalid file ID\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Look up the file in database */
    sql  = " select * from motion ";
    sql += " where record_id = " + std::to_string(file_id);
    sql += " and device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = 'pic'";
    app->dbse->filelist_get(sql, flst);

    if (flst.empty()) {
        webua->resp_page = "{\"error\":\"File not found\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Security: Validate file path to prevent directory traversal */
    full_path = flst[0].full_nm;
    if (full_path.find("..") != std::string::npos) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            full_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid file path\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete the file from filesystem */
    if (remove(full_path.c_str()) != 0 && errno != ENOENT) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Failed to delete file: %s"), full_path.c_str());
        webua->resp_page = "{\"error\":\"Failed to delete file\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete from database */
    sql  = "delete from motion where record_id = " + std::to_string(file_id);
    app->dbse->exec_sql(sql);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Deleted picture: %s (id=%d) by %s",
        flst[0].file_nm.c_str(), file_id, webua->clientip.c_str());

    webua->resp_page = "{\"success\":true,\"deleted_id\":" + std::to_string(file_id) + "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Delete a movie file
 * DELETE /{camId}/api/media/movie/{id}
 * Deletes both the file and database record
 */
void cls_webu_json::api_delete_movie()
{
    int indx;
    std::string sql, full_path;
    vec_files flst;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Check if delete action is enabled */
    for (indx=0; indx<webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Delete action disabled");
                webua->resp_page = "{\"error\":\"Delete action is disabled\"}";
                webua->resp_type = WEBUI_RESP_JSON;
                return;
            }
            break;
        }
    }

    /* Get file ID from URI: uri_cmd4 contains the record ID */
    if (webua->uri_cmd4.empty()) {
        webua->resp_page = "{\"error\":\"File ID required\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    int file_id = mtoi(webua->uri_cmd4);
    if (file_id <= 0) {
        webua->resp_page = "{\"error\":\"Invalid file ID\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Look up the file in database */
    sql  = " select * from motion ";
    sql += " where record_id = " + std::to_string(file_id);
    sql += " and device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = 'movie'";
    app->dbse->filelist_get(sql, flst);

    if (flst.empty()) {
        webua->resp_page = "{\"error\":\"File not found\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Security: Validate file path to prevent directory traversal */
    full_path = flst[0].full_nm;
    if (full_path.find("..") != std::string::npos) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            full_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid file path\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete the file from filesystem */
    if (remove(full_path.c_str()) != 0 && errno != ENOENT) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Failed to delete file: %s"), full_path.c_str());
        webua->resp_page = "{\"error\":\"Failed to delete file\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete associated thumbnail */
    std::string thumb_path = full_path + ".thumb.jpg";
    if (remove(thumb_path.c_str()) != 0 && errno != ENOENT) {
        MOTION_LOG(NTC, TYPE_STREAM, SHOW_ERRNO,
            _("Could not delete thumbnail: %s"), thumb_path.c_str());
        /* Non-fatal - continue with database deletion */
    }

    /* Delete from database */
    sql  = "delete from motion where record_id = " + std::to_string(file_id);
    app->dbse->exec_sql(sql);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Deleted movie: %s (id=%d) by %s",
        flst[0].file_nm.c_str(), file_id, webua->clientip.c_str());

    webua->resp_page = "{\"success\":true,\"deleted_id\":" + std::to_string(file_id) + "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: List movies
 * Returns list of movie files from database
 */
void cls_webu_json::api_media_movies()
{
    vec_files flst, flst_count;
    std::string sql, where_clause, cam_id;
    int offset = 0, limit = 100;
    int64_t total_count = 0;
    const char* date_filter = nullptr;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    cam_id = std::to_string(webua->cam->cfg->device_id);

    /* Parse query parameters */
    const char* offset_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "offset");
    const char* limit_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "limit");
    date_filter = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "date");

    if (offset_str) offset = std::max(0, atoi(offset_str));
    if (limit_str) limit = std::min(std::max(1, atoi(limit_str)), 100); // Cap at 100

    /* Build WHERE clause */
    where_clause  = " where device_id = " + cam_id;
    where_clause += " and file_typ = 'movie'";
    if (date_filter && strlen(date_filter) == 8) {
        where_clause += " and file_dtl = " + std::string(date_filter);
    }

    /* Get total count - query just record_id for efficiency */
    sql = " select record_id from motion " + where_clause + ";";
    app->dbse->filelist_get(sql, flst_count);
    total_count = flst_count.size();

    /* Get paginated results */
    sql  = " select * from motion ";
    sql += where_clause;
    sql += " order by file_dtl desc, file_tml desc";
    sql += " limit " + std::to_string(limit);
    sql += " offset " + std::to_string(offset) + ";";

    app->dbse->filelist_get(sql, flst);

    /* Build JSON response with pagination metadata */
    webua->resp_page = "{";
    webua->resp_page += "\"total_count\":" + std::to_string(total_count) + ",";
    webua->resp_page += "\"offset\":" + std::to_string(offset) + ",";
    webua->resp_page += "\"limit\":" + std::to_string(limit) + ",";
    webua->resp_page += "\"date_filter\":";
    if (date_filter) {
        webua->resp_page += "\"" + std::string(date_filter) + "\"";
    } else {
        webua->resp_page += "null";
    }
    webua->resp_page += ",\"movies\":[";

    for (size_t i = 0; i < flst.size(); i++) {
        if (i > 0) webua->resp_page += ",";
        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + std::to_string(flst[i].record_id) + ",";
        webua->resp_page += "\"filename\":\"" + escstr(flst[i].file_nm) + "\",";
        /* Return URL path for browser access, not filesystem path */
        webua->resp_page += "\"path\":\"/" + cam_id + "/movies/" + escstr(flst[i].file_nm) + "\",";
        webua->resp_page += "\"date\":\"" + std::to_string(flst[i].file_dtl) + "\",";
        webua->resp_page += "\"time\":\"" + escstr(flst[i].file_tml) + "\",";
        webua->resp_page += "\"size\":" + std::to_string(flst[i].file_sz);

        /* Add thumbnail path if exists */
        std::string thumb_path = flst[i].full_nm + ".thumb.jpg";
        struct stat st;
        if (stat(thumb_path.c_str(), &st) == 0) {
            webua->resp_page += ",\"thumbnail\":\"/" + cam_id + "/movies/" +
                                escstr(flst[i].file_nm) + ".thumb.jpg\"";
        }

        webua->resp_page += "}";
    }
    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Date summary
 * Returns list of dates with counts for a media type
 * GET /{camId}/api/media/dates?type=movie
 */
void cls_webu_json::api_media_dates()
{
    vec_files flst;
    std::string sql, file_typ;
    std::map<std::string, int> date_counts;
    int64_t total_count = 0;
    const char* type_param;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    /* Parse type parameter (required) */
    type_param = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "type");

    if (!type_param || (strcmp(type_param, "pic") != 0 && strcmp(type_param, "movie") != 0)) {
        webua->resp_page = "{\"error\":\"Invalid or missing 'type' parameter. Must be 'pic' or 'movie'\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    file_typ = type_param;

    /* Query all records for this type to build date summary */
    sql  = " select record_id, file_dtl from motion ";
    sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = '" + file_typ + "'";
    sql += " order by file_dtl desc;";

    app->dbse->filelist_get(sql, flst);
    total_count = flst.size();

    /* Group by date */
    for (size_t i = 0; i < flst.size(); i++) {
        std::string date_str = std::to_string(flst[i].file_dtl);
        date_counts[date_str]++;
    }

    /* Build JSON response */
    webua->resp_page = "{";
    webua->resp_page += "\"type\":\"" + file_typ + "\",";
    webua->resp_page += "\"total_count\":" + std::to_string(total_count) + ",";
    webua->resp_page += "\"dates\":[";

    bool first = true;
    for (const auto& pair : date_counts) {
        if (!first) webua->resp_page += ",";
        webua->resp_page += "{";
        webua->resp_page += "\"date\":\"" + pair.first + "\",";
        webua->resp_page += "\"count\":" + std::to_string(pair.second);
        webua->resp_page += "}";
        first = false;
    }

    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/* Media file extension checking helpers */
static bool is_media_extension(const std::string &ext)
{
    static const std::set<std::string> media_exts = {
        ".mp4", ".mkv", ".avi", ".webm", ".mov",
        ".jpg", ".jpeg", ".png", ".gif", ".bmp"
    };
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return media_exts.find(lower_ext) != media_exts.end();
}

static bool is_thumbnail(const std::string &filename)
{
    return filename.length() > 10 &&
           filename.substr(filename.length() - 10) == ".thumb.jpg";
}

static std::string get_file_extension(const std::string &filename)
{
    size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == 0) return "";
    return filename.substr(dot_pos);
}

/* Validate path is safe (no traversal, within target_dir) */
static bool validate_folder_path(const std::string &target_dir, const std::string &rel_path,
                                 std::string &full_path)
{
    /* Check for path traversal attempts */
    if (rel_path.find("..") != std::string::npos) {
        return false;
    }

    /* Build full path */
    full_path = target_dir;
    if (!full_path.empty() && full_path.back() != '/') {
        full_path += '/';
    }
    if (!rel_path.empty()) {
        full_path += rel_path;
    }

    /* Resolve symlinks and check real path is still under target_dir */
    char resolved[PATH_MAX];
    if (realpath(full_path.c_str(), resolved) == nullptr) {
        /* Path doesn't exist - that's ok for empty folder case */
        return true;
    }

    std::string real_path(resolved);
    char target_resolved[PATH_MAX];
    if (realpath(target_dir.c_str(), target_resolved) == nullptr) {
        return false;
    }
    std::string real_target(target_resolved);

    /* Ensure resolved path starts with target_dir */
    if (real_path.length() < real_target.length() ||
        real_path.substr(0, real_target.length()) != real_target) {
        return false;
    }

    /* Ensure it's either exactly target_dir or has a / separator after */
    if (real_path.length() > real_target.length() &&
        real_path[real_target.length()] != '/') {
        return false;
    }

    return true;
}

/*
 * React UI API: Folder-based media browsing
 * GET /{camId}/api/media/folders?path=rel/path&offset=0&limit=100
 * Returns folders and media files in the specified directory
 */
void cls_webu_json::api_media_folders()
{
    vec_files flst;
    std::string sql, target_dir, full_path;
    int offset = 0, limit = 100;
    const char* path_param = nullptr;
    std::string rel_path;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    /* Get target directory for this camera */
    target_dir = webua->cam->cfg->target_dir;
    if (target_dir.empty()) {
        webua->resp_page = "{\"error\":\"Target directory not configured\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Parse query parameters */
    path_param = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "path");
    const char* offset_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "offset");
    const char* limit_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "limit");

    if (path_param) rel_path = path_param;
    if (offset_str) offset = std::max(0, atoi(offset_str));
    if (limit_str) limit = std::min(std::max(1, atoi(limit_str)), 100);

    /* Validate and build full path */
    if (!validate_folder_path(target_dir, rel_path, full_path)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            rel_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid path\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Open directory */
    DIR *dir = opendir(full_path.c_str());
    if (dir == nullptr) {
        webua->resp_page = "{\"error\":\"Directory not found\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Scan directory entries */
    struct dirent *entry;
    std::vector<std::pair<std::string, std::string>> folders; /* name, path */
    std::vector<std::string> media_files;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        /* Skip . and .. */
        if (name == "." || name == "..") continue;

        /* Skip hidden files */
        if (name[0] == '.') continue;

        std::string entry_path = full_path + "/" + name;
        struct stat st;
        if (stat(entry_path.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Directory - add to folders list */
            std::string folder_rel = rel_path.empty() ? name : rel_path + "/" + name;
            folders.push_back({name, folder_rel});
        } else if (S_ISREG(st.st_mode)) {
            /* Regular file - check if it's a media file (not thumbnail) */
            std::string ext = get_file_extension(name);
            if (is_media_extension(ext) && !is_thumbnail(name)) {
                media_files.push_back(name);
            }
        }
    }
    closedir(dir);

    /* Sort folders and files alphabetically */
    std::sort(folders.begin(), folders.end());
    std::sort(media_files.begin(), media_files.end());

    /* Calculate folder statistics (file count, total size) */
    std::string cam_id = std::to_string(webua->cam->cfg->device_id);

    /* Build JSON response */
    webua->resp_page = "{";
    webua->resp_page += "\"path\":\"" + escstr(rel_path) + "\",";

    /* Parent path for navigation */
    if (rel_path.empty()) {
        webua->resp_page += "\"parent\":null,";
    } else {
        size_t last_slash = rel_path.rfind('/');
        std::string parent = (last_slash == std::string::npos) ? "" : rel_path.substr(0, last_slash);
        webua->resp_page += "\"parent\":\"" + escstr(parent) + "\",";
    }

    /* Folders */
    webua->resp_page += "\"folders\":[";
    for (size_t i = 0; i < folders.size(); i++) {
        if (i > 0) webua->resp_page += ",";

        /* Count files in this folder (from database) */
        std::string folder_path = full_path + "/" + folders[i].first;
        int64_t file_count = 0;
        int64_t total_size = 0;

        /* Count by scanning directory */
        DIR *subdir = opendir(folder_path.c_str());
        if (subdir != nullptr) {
            struct dirent *subentry;
            while ((subentry = readdir(subdir)) != nullptr) {
                std::string subname = subentry->d_name;
                if (subname == "." || subname == "..") continue;
                std::string subpath = folder_path + "/" + subname;
                struct stat sub_st;
                if (stat(subpath.c_str(), &sub_st) == 0 && S_ISREG(sub_st.st_mode)) {
                    std::string ext = get_file_extension(subname);
                    if (is_media_extension(ext) && !is_thumbnail(subname)) {
                        file_count++;
                        total_size += sub_st.st_size;
                    }
                }
            }
            closedir(subdir);
        }

        webua->resp_page += "{";
        webua->resp_page += "\"name\":\"" + escstr(folders[i].first) + "\",";
        webua->resp_page += "\"path\":\"" + escstr(folders[i].second) + "\",";
        webua->resp_page += "\"file_count\":" + std::to_string(file_count) + ",";
        webua->resp_page += "\"total_size\":" + std::to_string(total_size);
        webua->resp_page += "}";
    }
    webua->resp_page += "],";

    /* Files with pagination */
    int total_files = (int)media_files.size();
    int start_idx = std::min(offset, total_files);
    int end_idx = std::min(offset + limit, total_files);

    webua->resp_page += "\"files\":[";
    for (int i = start_idx; i < end_idx; i++) {
        if (i > start_idx) webua->resp_page += ",";

        std::string filename = media_files[i];
        std::string file_path = full_path + "/" + filename;
        struct stat st;
        stat(file_path.c_str(), &st);

        /* Determine file type */
        std::string ext = get_file_extension(filename);
        std::string file_type = "movie";
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".gif" || ext == ".bmp") {
            file_type = "picture";
        }

        /* Look up in database for metadata */
        sql = " select * from motion ";
        sql += " where device_id = " + cam_id;
        sql += " and file_nm = '" + filename + "'";
        sql += " limit 1;";
        flst.clear();
        app->dbse->filelist_get(sql, flst);

        webua->resp_page += "{";

        if (!flst.empty()) {
            webua->resp_page += "\"id\":" + std::to_string(flst[0].record_id) + ",";
            webua->resp_page += "\"date\":\"" + std::to_string(flst[0].file_dtl) + "\",";
            webua->resp_page += "\"time\":\"" + escstr(flst[0].file_tml) + "\",";
        } else {
            webua->resp_page += "\"id\":0,";
            /* Extract date from filename if possible (common format: camera-YYYYMMDD...) */
            webua->resp_page += "\"date\":\"\",";
            webua->resp_page += "\"time\":\"\",";
        }

        webua->resp_page += "\"filename\":\"" + escstr(filename) + "\",";

        /* Build URL path for access */
        if (file_type == "movie") {
            std::string url_path = "/" + cam_id + "/movies/";
            if (!rel_path.empty()) url_path += rel_path + "/";
            url_path += filename;
            webua->resp_page += "\"path\":\"" + escstr(url_path) + "\",";

            /* Check for thumbnail */
            std::string thumb_file = file_path + ".thumb.jpg";
            struct stat thumb_st;
            if (stat(thumb_file.c_str(), &thumb_st) == 0) {
                webua->resp_page += "\"thumbnail\":\"" + escstr(url_path + ".thumb.jpg") + "\",";
            }
        } else {
            /* Pictures use direct file path */
            webua->resp_page += "\"path\":\"" + escstr(file_path) + "\",";
        }

        webua->resp_page += "\"type\":\"" + file_type + "\",";
        webua->resp_page += "\"size\":" + std::to_string(st.st_size);
        webua->resp_page += "}";
    }
    webua->resp_page += "],";

    webua->resp_page += "\"total_files\":" + std::to_string(total_files) + ",";
    webua->resp_page += "\"offset\":" + std::to_string(offset) + ",";
    webua->resp_page += "\"limit\":" + std::to_string(limit);
    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Delete all media files in a folder
 * DELETE /{camId}/api/media/folders/files?path=rel/path
 * Deletes media files only (not subfolders or non-media files)
 * Also deletes associated thumbnails
 */
void cls_webu_json::api_delete_folder_files()
{
    std::string target_dir, full_path, sql;
    const char* path_param = nullptr;
    std::string rel_path;
    int deleted_movies = 0, deleted_pictures = 0, deleted_thumbnails = 0;
    std::vector<std::string> errors;

    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    /* Require admin role */
    if (webua->auth_role != "admin") {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
            _("Delete folder files denied - requires admin role (from %s)"),
            webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Admin access required\"}";
        return;
    }

    /* Check if delete action is enabled */
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Delete action disabled");
                webua->resp_page = "{\"error\":\"Delete action is disabled\"}";
                return;
            }
            break;
        }
    }

    /* Get path parameter (required) */
    path_param = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "path");

    if (path_param == nullptr) {
        webua->resp_page = "{\"error\":\"Path parameter required\"}";
        return;
    }
    rel_path = path_param;

    /* Get target directory for this camera */
    target_dir = webua->cam->cfg->target_dir;
    if (target_dir.empty()) {
        webua->resp_page = "{\"error\":\"Target directory not configured\"}";
        return;
    }

    /* Validate and build full path */
    if (!validate_folder_path(target_dir, rel_path, full_path)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            rel_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid path\"}";
        return;
    }

    /* Open directory */
    DIR *dir = opendir(full_path.c_str());
    if (dir == nullptr) {
        webua->resp_page = "{\"error\":\"Directory not found\"}";
        return;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Delete all media files in folder '%s' requested by %s",
        rel_path.c_str(), webua->clientip.c_str());

    /* Collect media files to delete */
    struct dirent *entry;
    std::vector<std::string> files_to_delete;
    std::vector<std::string> thumbs_to_delete;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string entry_path = full_path + "/" + name;
        struct stat st;
        if (stat(entry_path.c_str(), &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            std::string ext = get_file_extension(name);
            if (is_thumbnail(name)) {
                /* Track thumbnails separately - they'll be deleted with their movie */
                continue;
            } else if (is_media_extension(ext)) {
                files_to_delete.push_back(entry_path);
                /* Check for associated thumbnail */
                std::string thumb_path = entry_path + ".thumb.jpg";
                struct stat thumb_st;
                if (stat(thumb_path.c_str(), &thumb_st) == 0) {
                    thumbs_to_delete.push_back(thumb_path);
                }
            }
        }
    }
    closedir(dir);

    std::string cam_id = std::to_string(webua->cam->cfg->device_id);

    /* Delete files */
    for (const auto& file_path : files_to_delete) {
        std::string ext = get_file_extension(file_path);
        bool is_movie = (ext == ".mp4" || ext == ".mkv" || ext == ".avi" ||
                        ext == ".webm" || ext == ".mov");

        if (remove(file_path.c_str()) == 0) {
            if (is_movie) {
                deleted_movies++;
            } else {
                deleted_pictures++;
            }

            /* Delete from database */
            size_t last_slash = file_path.rfind('/');
            std::string filename = (last_slash == std::string::npos) ?
                file_path : file_path.substr(last_slash + 1);

            sql = "delete from motion where device_id = " + cam_id +
                  " and file_nm = '" + filename + "'";
            app->dbse->exec_sql(sql);
        } else {
            errors.push_back("Failed to delete: " + file_path);
            MOTION_LOG(ERR, TYPE_STREAM, SHOW_ERRNO,
                _("Failed to delete file: %s"), file_path.c_str());
        }
    }

    /* Delete thumbnails */
    for (const auto& thumb_path : thumbs_to_delete) {
        if (remove(thumb_path.c_str()) == 0) {
            deleted_thumbnails++;
        }
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Deleted %d movies, %d pictures, %d thumbnails from '%s'",
        deleted_movies, deleted_pictures, deleted_thumbnails, rel_path.c_str());

    /* Build response */
    webua->resp_page = "{";
    webua->resp_page += "\"success\":true,";
    webua->resp_page += "\"deleted\":{";
    webua->resp_page += "\"movies\":" + std::to_string(deleted_movies) + ",";
    webua->resp_page += "\"pictures\":" + std::to_string(deleted_pictures) + ",";
    webua->resp_page += "\"thumbnails\":" + std::to_string(deleted_thumbnails);
    webua->resp_page += "},";
    webua->resp_page += "\"errors\":[";
    for (size_t i = 0; i < errors.size(); i++) {
        if (i > 0) webua->resp_page += ",";
        webua->resp_page += "\"" + escstr(errors[i]) + "\"";
    }
    webua->resp_page += "],";
    webua->resp_page += "\"path\":\"" + escstr(rel_path) + "\"";
    webua->resp_page += "}";
}

/*
 * React UI API: System temperature
 * Returns CPU temperature (Raspberry Pi)
 */
void cls_webu_json::api_system_temperature()
{
    FILE *temp_file;
    int temp_raw;
    double temp_celsius;

    webua->resp_page = "{";

    temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp_file != nullptr) {
        if (fscanf(temp_file, "%d", &temp_raw) == 1) {
            temp_celsius = temp_raw / 1000.0;
            webua->resp_page += "\"celsius\":" + std::to_string(temp_celsius) + ",";
            webua->resp_page += "\"fahrenheit\":" + std::to_string(temp_celsius * 9.0 / 5.0 + 32.0);
        }
        fclose(temp_file);
    } else {
        webua->resp_page += "\"error\":\"Temperature not available\"";
    }

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: System status
 * Returns comprehensive system information (CPU temp, disk, memory, uptime)
 */
void cls_webu_json::api_system_status()
{
    FILE *file;
    char buffer[256];
    int temp_raw;
    double temp_celsius;
    unsigned long uptime_sec, mem_total, mem_free, mem_available;
    struct statvfs fs_stat;

    webua->resp_page = "{";

    /* CPU Temperature */
    file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (file != nullptr) {
        if (fscanf(file, "%d", &temp_raw) == 1) {
            temp_celsius = temp_raw / 1000.0;
            webua->resp_page += "\"temperature\":{";
            webua->resp_page += "\"celsius\":" + std::to_string(temp_celsius) + ",";
            webua->resp_page += "\"fahrenheit\":" + std::to_string(temp_celsius * 9.0 / 5.0 + 32.0);
            webua->resp_page += "},";
        }
        fclose(file);
    }

    /* System Uptime */
    file = fopen("/proc/uptime", "r");
    if (file != nullptr) {
        if (fscanf(file, "%lu", &uptime_sec) == 1) {
            webua->resp_page += "\"uptime\":{";
            webua->resp_page += "\"seconds\":" + std::to_string(uptime_sec) + ",";
            webua->resp_page += "\"days\":" + std::to_string(uptime_sec / 86400) + ",";
            webua->resp_page += "\"hours\":" + std::to_string((uptime_sec % 86400) / 3600);
            webua->resp_page += "},";
        }
        fclose(file);
    }

    /* Memory Information */
    file = fopen("/proc/meminfo", "r");
    if (file != nullptr) {
        mem_total = mem_free = mem_available = 0;
        while (fgets(buffer, sizeof(buffer), file)) {
            if (sscanf(buffer, "MemTotal: %lu kB", &mem_total) == 1) continue;
            if (sscanf(buffer, "MemFree: %lu kB", &mem_free) == 1) continue;
            if (sscanf(buffer, "MemAvailable: %lu kB", &mem_available) == 1) break;
        }
        fclose(file);

        if (mem_total > 0) {
            unsigned long mem_used = mem_total - mem_available;
            double mem_percent = (double)mem_used / mem_total * 100.0;
            webua->resp_page += "\"memory\":{";
            webua->resp_page += "\"total\":" + std::to_string(mem_total * 1024) + ",";
            webua->resp_page += "\"used\":" + std::to_string(mem_used * 1024) + ",";
            webua->resp_page += "\"free\":" + std::to_string(mem_free * 1024) + ",";
            webua->resp_page += "\"available\":" + std::to_string(mem_available * 1024) + ",";
            webua->resp_page += "\"percent\":" + std::to_string(mem_percent);
            webua->resp_page += "},";
        }
    }

    /* Disk Usage (root filesystem) */
    if (statvfs("/", &fs_stat) == 0) {
        unsigned long long total_bytes = (unsigned long long)fs_stat.f_blocks * fs_stat.f_frsize;
        unsigned long long free_bytes = (unsigned long long)fs_stat.f_bfree * fs_stat.f_frsize;
        unsigned long long avail_bytes = (unsigned long long)fs_stat.f_bavail * fs_stat.f_frsize;
        unsigned long long used_bytes = total_bytes - free_bytes;
        double disk_percent = (double)used_bytes / total_bytes * 100.0;

        webua->resp_page += "\"disk\":{";
        webua->resp_page += "\"total\":" + std::to_string(total_bytes) + ",";
        webua->resp_page += "\"used\":" + std::to_string(used_bytes) + ",";
        webua->resp_page += "\"free\":" + std::to_string(free_bytes) + ",";
        webua->resp_page += "\"available\":" + std::to_string(avail_bytes) + ",";
        webua->resp_page += "\"percent\":" + std::to_string(disk_percent);
        webua->resp_page += "},";
    }

    /* Device Model (Raspberry Pi) */
    file = fopen("/proc/device-tree/model", "r");
    if (file != nullptr) {
        if (fgets(buffer, sizeof(buffer), file)) {
            /* Remove trailing newline/null */
            size_t len = strlen(buffer);
            while (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\0' || buffer[len-1] == '\r')) {
                buffer[--len] = '\0';
            }
            webua->resp_page += "\"device_model\":\"" + escstr(buffer) + "\",";

            /* Detect Pi generation */
            if (strstr(buffer, "Pi 5") != nullptr) {
                webua->resp_page += "\"pi_generation\":5,";
            } else if (strstr(buffer, "Pi 4") != nullptr) {
                webua->resp_page += "\"pi_generation\":4,";
            } else if (strstr(buffer, "Pi 3") != nullptr) {
                webua->resp_page += "\"pi_generation\":3,";
            } else {
                webua->resp_page += "\"pi_generation\":0,";
            }
        }
        fclose(file);
    }

    /* Hardware Encoder Availability */
    {
        const AVCodec *codec_check;
        webua->resp_page += "\"hardware_encoders\":{";

        /* Check for V4L2 M2M H.264 encoder (Pi 4 only) */
        codec_check = avcodec_find_encoder_by_name("h264_v4l2m2m");
        webua->resp_page += "\"h264_v4l2m2m\":" + std::string(codec_check ? "true" : "false");

        webua->resp_page += "},";
    }

    /* Webcontrol Actions Status */
    webua->resp_page += "\"actions\":{";

    bool service_enabled = false;
    bool power_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "service" &&
            webu->wb_actions->params_array[indx].param_value == "on") {
            service_enabled = true;
        }
        if (webu->wb_actions->params_array[indx].param_name == "power" &&
            webu->wb_actions->params_array[indx].param_value == "on") {
            power_enabled = true;
        }
    }

    webua->resp_page += "\"service\":" + std::string(service_enabled ? "true" : "false");
    webua->resp_page += ",\"power\":" + std::string(power_enabled ? "true" : "false");
    webua->resp_page += "},";

    /* Motion Version */
    webua->resp_page += "\"version\":\"" + escstr(VERSION) + "\"";

    /* Camera Status (includes FPS for each camera) */
    webua->resp_page += ",\"status\":{";
    webua->resp_page += "\"count\":" + std::to_string(app->cam_cnt);
    for (int indx_cam = 0; indx_cam < app->cam_cnt; indx_cam++) {
        webua->resp_page += ",\"cam" +
            std::to_string(app->cam_list[indx_cam]->cfg->device_id) + "\":";
        status_vars(indx_cam);
    }
    webua->resp_page += "}";

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: System reboot
 * POST /0/api/system/reboot
 * Requires CSRF token and authentication
 */
void cls_webu_json::api_system_reboot()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for reboot from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Check if power control is enabled via webcontrol_actions */
    bool power_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "power") {
            if (webu->wb_actions->params_array[indx].param_value == "on") {
                power_enabled = true;
            }
            break;
        }
    }

    if (!power_enabled) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
            "Reboot request denied - power control disabled (from %s)", webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Power control is disabled\"}";
        return;
    }

    /* Log the reboot request */
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "System reboot requested by %s", webua->clientip.c_str());

    /* Schedule reboot with 2-second delay to allow HTTP response to complete */
    std::thread([]() {
        sleep(2);
        /* Try reboot commands in sequence (like MotionEye) */
        if (system("sudo /sbin/reboot") != 0) {
            if (system("sudo /sbin/shutdown -r now") != 0) {
                if (system("sudo /usr/bin/systemctl reboot") != 0) {
                    system("sudo /sbin/init 6");
                }
            }
        }
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"reboot\",\"message\":\"System will reboot in 2 seconds\"}";
}

/*
 * React UI API: System shutdown
 * POST /0/api/system/shutdown
 * Requires CSRF token and authentication
 */
void cls_webu_json::api_system_shutdown()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for shutdown from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Check if power control is enabled via webcontrol_actions */
    bool power_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "power") {
            if (webu->wb_actions->params_array[indx].param_value == "on") {
                power_enabled = true;
            }
            break;
        }
    }

    if (!power_enabled) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
            "Shutdown request denied - power control disabled (from %s)", webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Power control is disabled\"}";
        return;
    }

    /* Log the shutdown request */
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "System shutdown requested by %s", webua->clientip.c_str());

    /* Schedule shutdown with 2-second delay to allow HTTP response to complete */
    std::thread([]() {
        sleep(2);
        /* Try shutdown commands in sequence (like MotionEye) */
        if (system("sudo /sbin/poweroff") != 0) {
            if (system("sudo /sbin/shutdown -h now") != 0) {
                if (system("sudo /usr/bin/systemctl poweroff") != 0) {
                    system("sudo /sbin/init 0");
                }
            }
        }
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"shutdown\",\"message\":\"System will shut down in 2 seconds\"}";
}

/*
 * React UI API: Restart Motion service
 * POST /0/api/system/service-restart
 * Requires CSRF token and authentication
 */
void cls_webu_json::api_system_service_restart()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for service restart from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Check if service control is enabled via webcontrol_actions */
    bool service_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "service") {
            if (webu->wb_actions->params_array[indx].param_value == "on") {
                service_enabled = true;
            }
            break;
        }
    }

    if (!service_enabled) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
            "Service restart request denied - service control disabled (from %s)", webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Service control is disabled\"}";
        return;
    }

    /* Log the restart request */
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "Motion service restart requested by %s", webua->clientip.c_str());

    /* Schedule restart with 2-second delay to allow HTTP response to complete */
    std::thread([]() {
        sleep(2);
        system("sudo /usr/bin/systemctl restart motion");
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"service-restart\",\"message\":\"Motion service will restart in 2 seconds\"}";
}

/*
 * React UI API: Cameras list
 * Returns list of configured cameras
 */
void cls_webu_json::api_cameras()
{
    int indx_cam;
    std::string strid;
    cls_camera *cam;

    webua->resp_page = "{\"cameras\":[";

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        cam = app->cam_list[indx_cam];
        strid = std::to_string(cam->cfg->device_id);

        if (indx_cam > 0) {
            webua->resp_page += ",";
        }

        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + strid + ",";

        if (cam->cfg->device_name == "") {
            webua->resp_page += "\"name\":\"camera " + strid + "\",";
        } else {
            webua->resp_page += "\"name\":\"" + escstr(cam->cfg->device_name) + "\",";
        }

        webua->resp_page += "\"url\":\"" + webua->hostfull + "/" + strid + "/\"";
        webua->resp_page += "}";
    }

    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Configuration
 * Returns full Motion configuration including parameters and categories
 * Includes CSRF token for React UI authentication
 */
void cls_webu_json::api_config()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Add CSRF token at the start of the response */
    webua->resp_page = "{\"csrf_token\":\"" + webu->csrf_token + "\"";

    /* Add version - config() normally starts with { so we skip it */
    webua->resp_page += ",\"version\" : \"" VERSION "\"";

    /* Add cameras list */
    webua->resp_page += ",\"cameras\" : ";
    cameras_list();

    /* Add configuration parameters */
    webua->resp_page += ",\"configuration\" : ";
    parms_all();

    /* Add categories */
    webua->resp_page += ",\"categories\" : ";
    categories_list();

    webua->resp_page += "}";
}

/*
 * React UI API: Batch Configuration Update
 * PATCH /0/api/config with JSON body containing multiple parameters
 * Returns detailed results for each parameter change
 */
void cls_webu_json::api_config_patch()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for PATCH from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    /* Parse JSON body */
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("JSON parse error: %s"), parser.getError().c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid JSON: " +
                          parser.getError() + "\"}";
        return;
    }

    /* Get config for this camera/device */
    cls_config *cfg;
    if (webua->cam != nullptr) {
        cfg = webua->cam->cfg;
    } else {
        cfg = app->cfg;
    }

    /* Start response */
    webua->resp_page = "{\"status\":\"ok\",\"applied\":[";
    bool first_item = true;
    int success_count = 0;
    int error_count = 0;

    /* Process each parameter */
    pthread_mutex_lock(&app->mutex_post);
    for (const auto& kv : parser.getAll()) {
        std::string parm_name = kv.first;
        std::string parm_val = parser.getString(parm_name);
        std::string old_val;
        int parm_index = -1;
        bool applied = false;
        bool hot_reload = false;
        bool unchanged = false;
        std::string error_msg;

        /* Auto-hash authentication passwords if not already hashed */
        if (parm_name == "webcontrol_authentication" ||
            parm_name == "webcontrol_user_authentication") {

            size_t colon_pos = parm_val.find(':');
            if (colon_pos != std::string::npos) {
                std::string username = parm_val.substr(0, colon_pos);
                std::string password = parm_val.substr(colon_pos + 1);

                /* If password is not already a bcrypt hash, hash it */
                if (!cls_webu_auth::is_bcrypt_hash(password)) {
                    std::string hashed = cls_webu_auth::hash_password(password);
                    if (!hashed.empty()) {
                        parm_val = username + ":" + hashed;
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                            "Auto-hashed password for %s", parm_name.c_str());
                    } else {
                        /* Hash failed - log error but allow plaintext */
                        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
                            "Failed to hash password for %s - saving plaintext", parm_name.c_str());
                    }
                }
            }
        }

        /* SECURITY: Reject SQL parameter modifications */
        if (parm_name.substr(0, 4) == "sql_") {
            error_msg = "SQL parameters cannot be modified via web interface (security restriction)";
            error_count++;
        }
        /* SECURITY: Allow initial authentication setup regardless of webcontrol_parms
         * This enables first-run configuration without requiring webcontrol_parms 3
         * Exception only applies when BOTH auth parameters are empty (fresh install)
         * Once any auth is configured, normal permission levels apply */
        else if ((parm_name == "webcontrol_authentication" ||
                  parm_name == "webcontrol_user_authentication") &&
                 cfg->webcontrol_authentication == "" &&
                 cfg->webcontrol_user_authentication == "") {
            /* Initial setup exception - find parameter without permission check */
            parm_index = 0;
            while (config_parms[parm_index].parm_name != "") {
                if (config_parms[parm_index].parm_name == parm_name) {
                    break;
                }
                parm_index++;
            }

            if (config_parms[parm_index].parm_name == "") {
                /* Parameter not found */
                parm_index = -1;
                error_msg = "Unknown parameter";
                error_count++;
            } else {
                /* Parameter exists - get current value */
                cfg->edit_get(parm_name, old_val, config_parms[parm_index].parm_cat);

                /* Check if value actually changed */
                if (old_val == parm_val) {
                    unchanged = true;
                    hot_reload = config_parms[parm_index].hot_reload;
                    success_count++;
                } else {
                    /* Authentication parameters require restart */
                    cfg->edit_set(parm_name, parm_val);
                    applied = true;
                    hot_reload = false;
                    success_count++;

                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                        "Initial setup: %s configured (restart required)",
                        parm_name.c_str());
                }
            }
        }
        /* Check if parameter exists */
        else {
            validate_hot_reload(parm_name, parm_index);

            if (parm_index < 0) {
                /* Parameter doesn't exist */
                error_msg = "Unknown parameter";
                error_count++;
            } else if (config_parms[parm_index].webui_level > app->cfg->webcontrol_parms) {
                /* Permission level too low */
                error_msg = "Insufficient permissions (requires webcontrol_parms " +
                            std::to_string(config_parms[parm_index].webui_level) + ")";
                error_count++;
            } else {
                /* Parameter exists - get current value */
                cfg->edit_get(parm_name, old_val, config_parms[parm_index].parm_cat);

                /* Check if value actually changed */
                if (old_val == parm_val) {
                    unchanged = true;
                    hot_reload = config_parms[parm_index].hot_reload;
                    success_count++;
                } else {
                    /* Check if hot-reloadable */
                    if (config_parms[parm_index].hot_reload) {
                        /* Apply immediately */
                        apply_hot_reload(parm_index, parm_val);
                        applied = true;
                        hot_reload = true;
                        success_count++;
                    } else {
                        /* Save to config - requires restart to take effect */
                        cfg->edit_set(parm_name, parm_val);

                        /* Also update source config for restart persistence */
                        if (webua->cam != nullptr) {
                            webua->cam->conf_src->edit_set(parm_name, parm_val);
                        } else {
                            app->conf_src->edit_set(parm_name, parm_val);
                        }

                        applied = true;
                        hot_reload = false;
                        success_count++;
                    }
                }
            }
        }

        /* Add this parameter to response */
        if (!first_item) {
            webua->resp_page += ",";
        }
        first_item = false;

        webua->resp_page += "{\"param\":\"" + parm_name + "\"";
        webua->resp_page += ",\"old\":\"" + escstr(old_val) + "\"";
        webua->resp_page += ",\"new\":\"" + escstr(parm_val) + "\"";

        if (unchanged) {
            webua->resp_page += ",\"unchanged\":true";
        } else if (applied) {
            webua->resp_page += ",\"hot_reload\":" + std::string(hot_reload ? "true" : "false");
        }

        if (!error_msg.empty()) {
            webua->resp_page += ",\"error\":\"" + escstr(error_msg) + "\"";
        }

        webua->resp_page += "}";
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page += "]";
    webua->resp_page += ",\"summary\":{";
    webua->resp_page += "\"total\":" + std::to_string(success_count + error_count);
    webua->resp_page += ",\"success\":" + std::to_string(success_count);
    webua->resp_page += ",\"errors\":" + std::to_string(error_count);
    webua->resp_page += "}}";
}

/*
 * React UI API: Get mask information
 * GET /{camId}/api/mask/{type}
 * type = "motion" or "privacy"
 */
void cls_webu_json::api_mask_get()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Get current mask path from config */
    std::string mask_path;
    if (type == "motion") {
        mask_path = webua->cam->cfg->mask_file;
    } else {
        mask_path = webua->cam->cfg->mask_privacy;
    }

    webua->resp_page = "{";
    webua->resp_page += "\"type\":\"" + type + "\"";

    if (mask_path.empty()) {
        webua->resp_page += ",\"exists\":false";
        webua->resp_page += ",\"path\":\"\"";
    } else {
        /* Check if file exists and get dimensions */
        FILE *f = myfopen(mask_path.c_str(), "rbe");
        if (f != nullptr) {
            char line[256];
            int w = 0, h = 0;

            /* Skip magic number P5 */
            if (fgets(line, sizeof(line), f)) {
                /* Skip comments */
                do {
                    if (!fgets(line, sizeof(line), f)) break;
                } while (line[0] == '#');

                /* Parse dimensions */
                sscanf(line, "%d %d", &w, &h);
            }
            myfclose(f);

            webua->resp_page += ",\"exists\":true";
            webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
            webua->resp_page += ",\"width\":" + std::to_string(w);
            webua->resp_page += ",\"height\":" + std::to_string(h);
        } else {
            webua->resp_page += ",\"exists\":false";
            webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
            webua->resp_page += ",\"error\":\"File not accessible\"";
        }
    }

    webua->resp_page += "}";
}

/*
 * React UI API: Save mask from polygon data
 * POST /{camId}/api/mask/{type}
 * Request body: {"polygons":[[{x,y},...]], "width":W, "height":H, "invert":bool}
 */
void cls_webu_json::api_mask_post()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Validate CSRF (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Parse JSON request body */
    std::string body = webua->raw_body;

    /* Extract dimensions - default to camera size */
    int img_width = webua->cam->imgs.width;
    int img_height = webua->cam->imgs.height;
    bool invert = false;

    /* Parse width/height from body if present */
    size_t pos = body.find("\"width\":");
    if (pos != std::string::npos) {
        img_width = atoi(body.c_str() + pos + 8);
    }
    pos = body.find("\"height\":");
    if (pos != std::string::npos) {
        img_height = atoi(body.c_str() + pos + 9);
    }
    pos = body.find("\"invert\":");
    if (pos != std::string::npos) {
        invert = (body.substr(pos + 9, 4) == "true");
    }

    /* Validate dimensions match camera */
    if (img_width != webua->cam->imgs.width || img_height != webua->cam->imgs.height) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            "Mask dimensions %dx%d differ from camera %dx%d, will be resized on load",
            img_width, img_height, webua->cam->imgs.width, webua->cam->imgs.height);
    }

    /* Allocate bitmap */
    u_char default_val = invert ? 255 : 0;  /* 255=detect, 0=mask */
    u_char fill_val = invert ? 0 : 255;
    std::vector<u_char> bitmap(img_width * img_height, default_val);

    /* Parse polygons array */
    /* Format: "polygons":[[[x,y],[x,y],...],[[x,y],...]] */
    pos = body.find("\"polygons\":");
    if (pos != std::string::npos) {
        size_t start = body.find('[', pos);
        if (start != std::string::npos) {
            start++; /* Skip outer [ */

            while (start < body.length() && body[start] != ']') {
                /* Skip whitespace */
                while (start < body.length() &&
                       (body[start] == ' ' || body[start] == '\n' || body[start] == ',')) {
                    start++;
                }

                if (body[start] == '[') {
                    /* Parse one polygon */
                    std::vector<std::pair<int,int>> polygon;
                    start++; /* Skip [ */

                    while (start < body.length() && body[start] != ']') {
                        /* Skip to { or [ */
                        while (start < body.length() &&
                               body[start] != '{' && body[start] != '[' && body[start] != ']') {
                            start++;
                        }
                        if (body[start] == ']') break;

                        /* Parse point {x:N, y:N} or [x,y] */
                        int x = 0, y = 0;
                        if (body[start] == '{') {
                            /* Object format */
                            size_t xpos = body.find("\"x\":", start);
                            size_t ypos = body.find("\"y\":", start);
                            if (xpos != std::string::npos && ypos != std::string::npos) {
                                x = atoi(body.c_str() + xpos + 4);
                                y = atoi(body.c_str() + ypos + 4);
                            }
                            start = body.find('}', start) + 1;
                        } else if (body[start] == '[') {
                            /* Array format [x,y] */
                            start++;
                            x = atoi(body.c_str() + start);
                            size_t comma = body.find(',', start);
                            if (comma != std::string::npos) {
                                y = atoi(body.c_str() + comma + 1);
                            }
                            start = body.find(']', start) + 1;
                        }

                        polygon.push_back({x, y});
                    }
                    start++; /* Skip ] */

                    /* Fill polygon */
                    if (polygon.size() >= 3) {
                        fill_polygon(bitmap.data(), img_width, img_height, polygon, fill_val);
                    }
                } else {
                    break;
                }
            }
        }
    }

    /* Generate mask path */
    std::string mask_path = build_mask_path(webua->cam, type);

    /* Write PGM file */
    FILE *f = myfopen(mask_path.c_str(), "wbe");
    if (f == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
            "Cannot write mask file: %s", mask_path.c_str());
        webua->resp_page = "{\"error\":\"Cannot write mask file\"}";
        return;
    }

    /* Write PGM P5 header */
    fprintf(f, "P5\n");
    fprintf(f, "# Motion mask - type: %s\n", type.c_str());
    fprintf(f, "%d %d\n", img_width, img_height);
    fprintf(f, "255\n");

    /* Write bitmap data */
    if (fwrite(bitmap.data(), 1, bitmap.size(), f) != bitmap.size()) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
            "Failed writing mask data to: %s", mask_path.c_str());
        myfclose(f);
        webua->resp_page = "{\"error\":\"Failed writing mask data\"}";
        return;
    }

    myfclose(f);

    /* Update config parameter */
    pthread_mutex_lock(&app->mutex_post);
    if (type == "motion") {
        webua->cam->cfg->mask_file = mask_path;
        app->cfg->edit_set("mask_file", mask_path);
    } else {
        webua->cam->cfg->mask_privacy = mask_path;
        app->cfg->edit_set("mask_privacy", mask_path);
    }
    pthread_mutex_unlock(&app->mutex_post);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Mask saved: %s (type=%s, %dx%d, polygons parsed)",
        mask_path.c_str(), type.c_str(), img_width, img_height);

    webua->resp_page = "{";
    webua->resp_page += "\"success\":true";
    webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
    webua->resp_page += ",\"width\":" + std::to_string(img_width);
    webua->resp_page += ",\"height\":" + std::to_string(img_height);
    webua->resp_page += ",\"message\":\"Mask saved. Reload camera to apply.\"";
    webua->resp_page += "}";
}

/*
 * React UI API: Delete mask file
 * DELETE /{camId}/api/mask/{type}
 */
void cls_webu_json::api_mask_delete()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Validate CSRF (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Get current mask path */
    std::string mask_path;
    if (type == "motion") {
        mask_path = webua->cam->cfg->mask_file;
    } else {
        mask_path = webua->cam->cfg->mask_privacy;
    }

    bool file_deleted = false;
    if (!mask_path.empty()) {
        /* Security: Validate path doesn't contain traversal */
        if (mask_path.find("..") != std::string::npos) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
                "Path traversal attempt blocked: %s", mask_path.c_str());
            webua->resp_page = "{\"error\":\"Invalid path\"}";
            return;
        }

        /* Delete file */
        if (remove(mask_path.c_str()) == 0) {
            file_deleted = true;
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
                "Deleted mask file: %s", mask_path.c_str());
        } else if (errno != ENOENT) {
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO,
                "Failed to delete mask file: %s", mask_path.c_str());
        }
    }

    /* Clear config parameter */
    pthread_mutex_lock(&app->mutex_post);
    if (type == "motion") {
        webua->cam->cfg->mask_file = "";
        app->cfg->edit_set("mask_file", "");
    } else {
        webua->cam->cfg->mask_privacy = "";
        app->cfg->edit_set("mask_privacy", "");
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{";
    webua->resp_page += "\"success\":true";
    webua->resp_page += ",\"deleted\":" + std::string(file_deleted ? "true" : "false");
    webua->resp_page += ",\"message\":\"Mask removed. Reload camera to apply.\"";
    webua->resp_page += "}";
}

/*
 * Configuration Profiles API: List all profiles for a camera
 * GET /0/api/profiles?camera_id=X
 */
void cls_webu_json::api_profiles_list()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Get camera_id from query params (default to 0) */
    int camera_id = 0;
    const char* cam_id_str = MHD_lookup_connection_value(
        webua->connection, MHD_GET_ARGUMENT_KIND, "camera_id");
    if (cam_id_str != nullptr) {
        camera_id = atoi(cam_id_str);
    }

    /* Get profiles from database */
    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\",\"profiles\":[]}";
        return;
    }

    std::vector<ctx_profile_info> profiles = app->profiles->list_profiles(camera_id);

    /* Build JSON response */
    webua->resp_page = "{\"status\":\"ok\",\"profiles\":[";
    bool first = true;
    for (const auto &prof : profiles) {
        if (!first) {
            webua->resp_page += ",";
        }
        first = false;

        webua->resp_page += "{";
        webua->resp_page += "\"profile_id\":" + std::to_string(prof.profile_id) + ",";
        webua->resp_page += "\"camera_id\":" + std::to_string(prof.camera_id) + ",";
        webua->resp_page += "\"name\":\"" + escstr(prof.name) + "\",";
        webua->resp_page += "\"description\":\"" + escstr(prof.description) + "\",";
        webua->resp_page += "\"is_default\":" + std::string(prof.is_default ? "true" : "false") + ",";
        webua->resp_page += "\"created_at\":" + std::to_string((int64_t)prof.created_at) + ",";
        webua->resp_page += "\"updated_at\":" + std::to_string((int64_t)prof.updated_at) + ",";
        webua->resp_page += "\"param_count\":" + std::to_string(prof.param_count);
        webua->resp_page += "}";
    }
    webua->resp_page += "]}";
}

/*
 * Configuration Profiles API: Get specific profile with parameters
 * GET /0/api/profiles/{id}
 */
void cls_webu_json::api_profiles_get()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Parse profile_id from URI */
    int profile_id = atoi(webua->uri_cmd3.c_str());
    if (profile_id <= 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid profile ID\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Get profile info */
    ctx_profile_info info;
    if (!app->profiles->get_profile_info(profile_id, info)) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile not found\"}";
        return;
    }

    /* Load profile parameters */
    std::map<std::string, std::string> params;
    if (app->profiles->load_profile(profile_id, params) != 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Failed to load profile parameters\"}";
        return;
    }

    /* Build JSON response with metadata + params */
    webua->resp_page = "{\"status\":\"ok\",";
    webua->resp_page += "\"profile_id\":" + std::to_string(info.profile_id) + ",";
    webua->resp_page += "\"camera_id\":" + std::to_string(info.camera_id) + ",";
    webua->resp_page += "\"name\":\"" + escstr(info.name) + "\",";
    webua->resp_page += "\"description\":\"" + escstr(info.description) + "\",";
    webua->resp_page += "\"is_default\":" + std::string(info.is_default ? "true" : "false") + ",";
    webua->resp_page += "\"created_at\":" + std::to_string((int64_t)info.created_at) + ",";
    webua->resp_page += "\"updated_at\":" + std::to_string((int64_t)info.updated_at) + ",";
    webua->resp_page += "\"params\":{";

    bool first = true;
    for (const auto &kv : params) {
        if (!first) {
            webua->resp_page += ",";
        }
        first = false;
        webua->resp_page += "\"" + escstr(kv.first) + "\":\"" + escstr(kv.second) + "\"";
    }
    webua->resp_page += "}}";
}

/*
 * Configuration Profiles API: Create new profile
 * POST /0/api/profiles
 * Body: {name, description?, camera_id, snapshot_current?, params?}
 */
void cls_webu_json::api_profiles_create()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for profile create from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Parse JSON body */
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("JSON parse error: %s"), parser.getError().c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid JSON: " +
                          parser.getError() + "\"}";
        return;
    }

    /* Extract required fields */
    std::string name = parser.getString("name");
    if (name.empty()) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile name is required\"}";
        return;
    }

    std::string description = parser.getString("description", "");
    int camera_id = (int)parser.getNumber("camera_id", 0);
    bool snapshot_current = parser.getBool("snapshot_current", false);

    /* Get parameters */
    std::map<std::string, std::string> params;

    if (snapshot_current) {
        /* Snapshot current configuration */
        cls_config *cfg;
        if (webua->cam != nullptr) {
            cfg = webua->cam->cfg;
        } else {
            cfg = app->cfg;
        }
        params = app->profiles->snapshot_config(cfg);
    } else {
        /* Use params from request body (TODO: parse nested params object) */
        /* For now, just create empty profile - params can be added via update */
    }

    /* Create profile */
    pthread_mutex_lock(&app->mutex_post);
    int profile_id = app->profiles->create_profile(camera_id, name, description, params);
    pthread_mutex_unlock(&app->mutex_post);

    if (profile_id < 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Failed to create profile\"}";
        return;
    }

    webua->resp_page = "{\"status\":\"ok\",\"profile_id\":" + std::to_string(profile_id) + "}";

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Profile created: id=%d, name='%s', camera=%d"), profile_id, name.c_str(), camera_id);
}

/*
 * Configuration Profiles API: Update profile parameters
 * PATCH /0/api/profiles/{id}
 * Body: {params: {...}}
 */
void cls_webu_json::api_profiles_update()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for profile update from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Parse profile_id from URI */
    int profile_id = atoi(webua->uri_cmd3.c_str());
    if (profile_id <= 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid profile ID\"}";
        return;
    }

    /* Parse JSON body */
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("JSON parse error: %s"), parser.getError().c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid JSON: " +
                          parser.getError() + "\"}";
        return;
    }

    /* Extract params (for now, simple key-value pairs) */
    std::map<std::string, std::string> params;
    for (const auto &kv : parser.getAll()) {
        params[kv.first] = parser.getString(kv.first);
    }

    /* Update profile */
    pthread_mutex_lock(&app->mutex_post);
    int retcd = app->profiles->update_profile(profile_id, params);
    pthread_mutex_unlock(&app->mutex_post);

    if (retcd < 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Failed to update profile\"}";
        return;
    }

    webua->resp_page = "{\"status\":\"ok\"}";

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Profile updated: id=%d"), profile_id);
}

/*
 * Configuration Profiles API: Delete profile
 * DELETE /0/api/profiles/{id}
 */
void cls_webu_json::api_profiles_delete()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for profile delete from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Parse profile_id from URI */
    int profile_id = atoi(webua->uri_cmd3.c_str());
    if (profile_id <= 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid profile ID\"}";
        return;
    }

    /* Delete profile */
    pthread_mutex_lock(&app->mutex_post);
    int retcd = app->profiles->delete_profile(profile_id);
    pthread_mutex_unlock(&app->mutex_post);

    if (retcd < 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Failed to delete profile\"}";
        return;
    }

    webua->resp_page = "{\"status\":\"ok\"}";

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Profile deleted: id=%d"), profile_id);
}

/*
 * Configuration Profiles API: Apply profile to camera configuration
 * POST /0/api/profiles/{id}/apply
 */
void cls_webu_json::api_profiles_apply()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for profile apply from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Parse profile_id from URI */
    int profile_id = atoi(webua->uri_cmd3.c_str());
    if (profile_id <= 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid profile ID\"}";
        return;
    }

    /* Get config for this camera/device */
    cls_config *cfg;
    if (webua->cam != nullptr) {
        cfg = webua->cam->cfg;
    } else {
        cfg = app->cfg;
    }

    /* Apply profile */
    pthread_mutex_lock(&app->mutex_post);
    std::vector<std::string> needs_restart = app->profiles->apply_profile(cfg, profile_id);
    pthread_mutex_unlock(&app->mutex_post);

    /* Build response with restart requirements */
    webua->resp_page = "{\"status\":\"ok\",\"requires_restart\":[";
    bool first = true;
    for (const auto &param : needs_restart) {
        if (!first) {
            webua->resp_page += ",";
        }
        first = false;
        webua->resp_page += "\"" + escstr(param) + "\"";
    }
    webua->resp_page += "]}";

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Profile applied: id=%d, restart_required=%s"),
        profile_id, needs_restart.empty() ? "no" : "yes");
}

/*
 * Configuration Profiles API: Set profile as default for camera
 * POST /0/api/profiles/{id}/default
 */
void cls_webu_json::api_profiles_set_default()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token (supports both session and global tokens) */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for set default from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    if (!app->profiles || !app->profiles->enabled) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Profile system not available\"}";
        return;
    }

    /* Parse profile_id from URI */
    int profile_id = atoi(webua->uri_cmd3.c_str());
    if (profile_id <= 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid profile ID\"}";
        return;
    }

    /* Set as default */
    pthread_mutex_lock(&app->mutex_post);
    int retcd = app->profiles->set_default_profile(profile_id);
    pthread_mutex_unlock(&app->mutex_post);

    if (retcd < 0) {
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Failed to set default profile\"}";
        return;
    }

    webua->resp_page = "{\"status\":\"ok\"}";

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        _("Default profile set: id=%d"), profile_id);
}

/*
 * CSRF validation helper for POST endpoints
 * Gets token from X-CSRF-Token header and validates against session or global token
 * Returns true if valid, false otherwise (also sets error response)
 */
bool cls_webu_json::validate_csrf()
{
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", webua->session_token)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        webua->resp_code = 403;
        return false;
    }
    return true;
}

/*
 * Check if an action is enabled in webcontrol_actions
 * Returns true if action is enabled (or not explicitly disabled), false if disabled
 */
bool cls_webu_json::check_action_permission(const std::string &action_name)
{
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == action_name) {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
                    "%s action disabled", action_name.c_str());
                webua->resp_page = "{\"error\":\"" + action_name + " action is disabled\"}";
                return false;
            }
            break;
        }
    }
    return true;
}

/*
 * Camera action API: Write configuration to file
 * POST /0/api/config/write
 * Saves current configuration parameters to file
 */
void cls_webu_json::api_config_write()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("config_write")) {
        return;
    }

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "Config write requested by %s", webua->clientip.c_str());

    pthread_mutex_lock(&app->mutex_post);
    app->conf_src->parms_write();
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: Restart camera(s)
 * POST /{camId}/api/camera/restart
 * If camId=0, restart all cameras; otherwise restart specific camera
 */
void cls_webu_json::api_camera_restart()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("restart")) {
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Restarting all cameras"));
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->handler_stop = false;
            app->cam_list[indx]->restart = true;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Restarting camera %d"),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->handler_stop = false;
            app->cam_list[webua->camindx]->restart = true;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: Take snapshot
 * POST /{camId}/api/camera/snapshot
 * If camId=0, snapshot all cameras; otherwise snapshot specific camera
 */
void cls_webu_json::api_camera_snapshot()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("snapshot")) {
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Snapshot requested for all cameras"));
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->action_snapshot = true;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Snapshot requested for camera %d"),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->action_snapshot = true;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: Pause/unpause detection
 * POST /{camId}/api/camera/pause
 * Body: {"action": "on"|"off"|"schedule"}
 * If camId=0, applies to all cameras; otherwise specific camera
 */
void cls_webu_json::api_camera_pause()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("pause")) {
        return;
    }

    /* Parse JSON body for action */
    std::string action = "on";  /* Default to pause on */
    if (!webua->raw_body.empty()) {
        JsonParser parser;
        if (parser.parse(webua->raw_body)) {
            std::string parsed_action = parser.getString("action");
            if (!parsed_action.empty()) {
                action = parsed_action;
            }
        }
    }

    /* Validate action value */
    if (action != "on" && action != "off" && action != "schedule") {
        webua->resp_page = "{\"error\":\"Invalid action. Use 'on', 'off', or 'schedule'\"}";
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
            _("Pause %s requested for all cameras"), action.c_str());
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->user_pause = action;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Pause %s requested for camera %d"), action.c_str(),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->user_pause = action;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\",\"action\":\"" + action + "\"}";
}

/*
 * Camera action API: Stop camera(s)
 * POST /{camId}/api/camera/stop
 * If camId=0, stop all cameras; otherwise stop specific camera
 */
void cls_webu_json::api_camera_stop()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("stop")) {
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Stopping camera %d"),
                app->cam_list[indx]->cfg->device_id);
            app->cam_list[indx]->restart = false;
            app->cam_list[indx]->event_stop = true;
            app->cam_list[indx]->event_user = false;
            app->cam_list[indx]->handler_stop = true;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Stopping camera %d"),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->restart = false;
            app->cam_list[webua->camindx]->event_stop = true;
            app->cam_list[webua->camindx]->event_user = false;
            app->cam_list[webua->camindx]->handler_stop = true;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: Trigger event start
 * POST /{camId}/api/camera/event/start
 * If camId=0, trigger for all cameras; otherwise specific camera
 */
void cls_webu_json::api_camera_event_start()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("event")) {
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Event start triggered for all cameras"));
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->event_user = true;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Event start triggered for camera %d"),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->event_user = true;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: Trigger event end
 * POST /{camId}/api/camera/event/end
 * If camId=0, trigger for all cameras; otherwise specific camera
 */
void cls_webu_json::api_camera_event_end()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("event")) {
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    if (webua->device_id == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Event end triggered for all cameras"));
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->event_stop = true;
        }
    } else {
        if (webua->camindx >= 0 && webua->camindx < app->cam_cnt) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                _("Event end triggered for camera %d"),
                app->cam_list[webua->camindx]->cfg->device_id);
            app->cam_list[webua->camindx]->event_stop = true;
        } else {
            pthread_mutex_unlock(&app->mutex_post);
            webua->resp_page = "{\"error\":\"Invalid camera ID\"}";
            return;
        }
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{\"status\":\"ok\"}";
}

/*
 * Camera action API: PTZ control
 * POST /{camId}/api/camera/ptz
 * Body: {"action": "pan_left"|"pan_right"|"tilt_up"|"tilt_down"|"zoom_in"|"zoom_out"}
 * Requires specific camera (camId != 0)
 */
void cls_webu_json::api_camera_ptz()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (!validate_csrf()) {
        return;
    }

    if (!check_action_permission("ptz")) {
        return;
    }

    /* PTZ requires a specific camera */
    if (webua->camindx < 0 || webua->camindx >= app->cam_cnt) {
        webua->resp_page = "{\"error\":\"PTZ requires a specific camera ID\"}";
        return;
    }

    /* Parse JSON body for action */
    if (webua->raw_body.empty()) {
        webua->resp_page = "{\"error\":\"Missing request body with action\"}";
        return;
    }

    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        webua->resp_page = "{\"error\":\"Invalid JSON: " + parser.getError() + "\"}";
        return;
    }

    std::string action = parser.getString("action");
    if (action.empty()) {
        webua->resp_page = "{\"error\":\"Missing 'action' field\"}";
        return;
    }

    cls_camera *cam = app->cam_list[webua->camindx];
    std::string ptz_cmd;

    /* Map action to PTZ command */
    if (action == "pan_left" && !cam->cfg->ptz_pan_left.empty()) {
        ptz_cmd = cam->cfg->ptz_pan_left;
    } else if (action == "pan_right" && !cam->cfg->ptz_pan_right.empty()) {
        ptz_cmd = cam->cfg->ptz_pan_right;
    } else if (action == "tilt_up" && !cam->cfg->ptz_tilt_up.empty()) {
        ptz_cmd = cam->cfg->ptz_tilt_up;
    } else if (action == "tilt_down" && !cam->cfg->ptz_tilt_down.empty()) {
        ptz_cmd = cam->cfg->ptz_tilt_down;
    } else if (action == "zoom_in" && !cam->cfg->ptz_zoom_in.empty()) {
        ptz_cmd = cam->cfg->ptz_zoom_in;
    } else if (action == "zoom_out" && !cam->cfg->ptz_zoom_out.empty()) {
        ptz_cmd = cam->cfg->ptz_zoom_out;
    } else {
        webua->resp_page = "{\"error\":\"Invalid or unconfigured PTZ action: " + action + "\"}";
        return;
    }

    pthread_mutex_lock(&app->mutex_post);
    cam->frame_skip = cam->cfg->ptz_wait;
    util_exec_command(cam, ptz_cmd);
    pthread_mutex_unlock(&app->mutex_post);

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
        _("PTZ %s executed for camera %d"), action.c_str(), cam->cfg->device_id);

    webua->resp_page = "{\"status\":\"ok\",\"action\":\"" + action + "\"}";
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