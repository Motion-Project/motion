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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_json.hpp"

static void webu_json_config_item(struct ctx_webui *webui, int indx_cam, int indx_parm)
{
    int indx;
    std::string parm_orig, parm_val, parm_list, parm_enable;

    parm_orig = "";
    parm_val = "";
    parm_list = "";

    if (webui->motapp->cam_list[0]->conf->webcontrol_parms < WEBUI_LEVEL_LIMITED) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    conf_edit_get(webui->motapp->cam_list[indx_cam]
        , config_parms[indx_parm].parm_name
        , parm_orig
        , config_parms[indx_parm].parm_cat);

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
        conf_edit_list(webui->motapp->cam_list[indx_cam]
            , config_parms[indx_parm].parm_name
            , parm_list
            , config_parms[indx_parm].parm_cat);

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

static void webu_json_config_parms(struct ctx_webui *webui, int indx_cam)
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
                webui->motapp->cam_list[0]->conf->webcontrol_parms) &&
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
           webu_json_config_item(webui, indx_cam, indx_parm);
        }
        indx_parm++;
    }
    webui->resp_page += "}";

}

static void webu_json_config_cam_parms(struct ctx_webui *webui)
{
    int indx_cam;
    bool first;

    indx_cam = 0;
    first = true;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        if (first) {
            first = false;
            webui->resp_page += "{";
        } else {
            webui->resp_page += ",";
        }
        webui->resp_page += "\"cam" +
            std::to_string(webui->motapp->cam_list[indx_cam]->camera_id) + "\": ";

        webu_json_config_parms(webui, indx_cam);

        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_cam_list(struct ctx_webui *webui)
{
    int indx_cam;
    std::string response;

    /* Get the count */
    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }
    webui->resp_page += "{\"count\" : " + std::to_string(indx_cam - 1);

    indx_cam = 0;
    while (webui->motapp->cam_list[indx_cam] != NULL) {
        webui->resp_page += ",\""+ std::to_string(indx_cam) + "\":";

        if (indx_cam == 0) {
            webui->resp_page += "{\"name\": \"default\" ";
        } else if (webui->motapp->cam_list[indx_cam]->conf->camera_name == "") {
            webui->resp_page += "{\"name\": \"camera " +
                std::to_string(webui->motapp->cam_list[indx_cam]->camera_id) + "\"";
        } else {
            webui->resp_page += "{\"name\": \"" +
                webui->motapp->cam_list[indx_cam]->conf->camera_name + "\"";
        }

        webui->resp_page += ",\"id\": " +
            std::to_string(webui->motapp->cam_list[indx_cam]->camera_id);

        if (indx_cam == 0) {
            webui->resp_page += "}";
        } else {
            webui->resp_page +=
                ",\"url\": \"" + webui->hostfull +
                "/" + std::to_string(webui->motapp->cam_list[indx_cam]->camera_id) +
                "/\"} ";
        }

        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_categories(struct ctx_webui *webui)
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

void webu_json_config(struct ctx_webui *webui)
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
