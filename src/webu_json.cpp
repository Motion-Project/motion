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
#include "webu_json.hpp"

static void webu_json_config_item(struct webui_ctx *webui, int indx_cam, int indx_parm)
{
    int indx;
    std::string parm_orig, parm_val, parm_list;

    parm_orig = "";
    parm_val = "";
    parm_list = "";

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
            ",\"enabled\":true" +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";

    } else if (config_parms[indx_parm].parm_type == PARM_TYP_BOOL) {
        if (parm_val == "on") {
            webui->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":true" +
                ",\"enabled\":true" +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\""+
                "}";
        } else {
            webui->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":false" +
                ",\"enabled\":true" +
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
            ",\"enabled\":true" +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            ",\"list\":" + parm_list +
            "}";

    } else {
        webui->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":true"+
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";
    }

}

static void webu_json_config_parms(struct webui_ctx *webui, int indx_cam)
{
    int indx_parm, first;
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
        if (config_parms[indx_parm].webui_level > webui->motapp->cam_list[0]->conf->webcontrol_parms) {
            webui->resp_page +=
                "\""+config_parms[indx_parm].parm_name+"\"" +
                ":{" +
                " \"value\":\"\"" +
                ",\"enabled\":false" +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\""+ conf_type_desc(config_parms[indx_parm].parm_type) + "\"" +
                "}";
        } else {
           webu_json_config_item(webui, indx_cam, indx_parm);
        }
        indx_parm++;
    }
    webui->resp_page += "}";

}

static void webu_json_config_cam_parms(struct webui_ctx *webui)
{
    int indx_cam, first;

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
            std::to_string(webui->motapp->cam_list[indx_cam]->conf->camera_id) + "\": ";

        webu_json_config_parms(webui, indx_cam);

        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_cam_list(struct webui_ctx *webui)
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
                std::to_string(webui->motapp->cam_list[indx_cam]->conf->camera_id) + "\"";
        } else {
            webui->resp_page += "{\"name\": \"" +
                webui->motapp->cam_list[indx_cam]->conf->camera_name + "\"";
        }

        webui->resp_page += ",\"id\": " +
            std::to_string(webui->motapp->cam_list[indx_cam]->conf->camera_id);

        if (indx_cam == 0) {
            webui->resp_page += "}";
        } else {
            webui->resp_page +=
                ",\"url\": \"" + webui->hostfull +
                "/" + std::to_string(webui->motapp->cam_list[indx_cam]->conf->camera_id) +
                "/\"} ";
        }

        indx_cam++;
    }
    webui->resp_page += "}";

    return;

}

static void webu_json_config_categories(struct webui_ctx *webui)
{
    int indx_cat;

    webui->resp_page += "{";

    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            webui->resp_page += ",";
        }
        webui->resp_page += "\"" + std::to_string(indx_cat) + "\": ";

        if (indx_cat == PARM_CAT_00) {
            webui->resp_page += "{\"name\":\"system\",\"display\":\"System\"}";
        } else if (indx_cat == PARM_CAT_01) {
            webui->resp_page += "{\"name\":\"camera\",\"display\":\"Camera\"}";
        } else if (indx_cat == PARM_CAT_02) {
            webui->resp_page += "{\"name\":\"source\",\"display\":\"Camera Source\"}";
        } else if (indx_cat == PARM_CAT_03) {
            webui->resp_page += "{\"name\":\"image\",\"display\":\"Image\"}";
        } else if (indx_cat == PARM_CAT_04) {
            webui->resp_page += "{\"name\":\"overlay\",\"display\":\"Overlays\"}";
        } else if (indx_cat == PARM_CAT_05) {
            webui->resp_page += "{\"name\":\"method\",\"display\":\"Method\"}";
        } else if (indx_cat == PARM_CAT_06) {
            webui->resp_page += "{\"name\":\"masks\",\"display\":\"Masks\"}";
        } else if (indx_cat == PARM_CAT_07) {
            webui->resp_page += "{\"name\":\"detect\",\"display\":\"Detection\"}";
        } else if (indx_cat == PARM_CAT_08) {
            webui->resp_page += "{\"name\":\"scripts\",\"display\":\"Scripts\"}";
        } else if (indx_cat == PARM_CAT_09) {
            webui->resp_page += "{\"name\":\"picture\",\"display\":\"Picture\"}";
        } else if (indx_cat == PARM_CAT_10) {
            webui->resp_page += "{\"name\":\"movie\",\"display\":\"Movie\"}";
        } else if (indx_cat == PARM_CAT_11) {
            webui->resp_page += "{\"name\":\"timelapse\",\"display\":\"Timelapse\"}";
        } else if (indx_cat == PARM_CAT_12) {
            webui->resp_page += "{\"name\":\"pipes\",\"display\":\"Pipes\"}";
        } else if (indx_cat == PARM_CAT_13) {
            webui->resp_page += "{\"name\":\"webcontrol\",\"display\":\"Web Control\"}";
        } else if (indx_cat == PARM_CAT_14) {
            webui->resp_page += "{\"name\":\"streams\",\"display\":\"Web Stream\"}";
        } else if (indx_cat == PARM_CAT_15) {
            webui->resp_page += "{\"name\":\"database\",\"display\":\"Database\"}";
        } else if (indx_cat == PARM_CAT_16) {
            webui->resp_page += "{\"name\":\"sql\",\"display\":\"SQL\"}";
        } else if (indx_cat == PARM_CAT_17) {
            webui->resp_page += "{\"name\":\"track\",\"display\":\"Tracking\"}";
        } else {
            webui->resp_page += "{\"name\":\"unk\",\"display\":\"Unknown\"}";
        }
        indx_cat++;
    }

    webui->resp_page += "}";

    return;

}

void webu_json_config(struct webui_ctx *webui)
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
