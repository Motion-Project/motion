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
#include "i10n.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "webu.hpp"
#include "webu_allcam.hpp"
#include "webu_ans.hpp"
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

void cls_webu_json::parms_item_detail(cls_config *conf, std::string pNm)
{
    std::string catstr, plst, ppfx, pi10n;
    std::string pval, pchk;
    ctx_params  *prm;
    ctx_params_item *itm;
    int indx, indx2, pcnt;
    bool first;
    PARM_PCAT pcat;

    prm = nullptr;
    pcat = PARM_PCAT_MIN;
    for (indx=0;indx<conf->params.size();indx++) {
        if (conf->params[indx]->params_desc == pNm) {
            prm = conf->params[indx];
            pcat = conf->params[indx]->params_pcat;
        }
    }
    if (prm == nullptr) {
        webua->resp_page += ",\"prefix\":\"\"";
        webua->resp_page += ",\"detail\":\"\"";
        webua->resp_page += ",\"count\": 0";
        return;
    }

    catstr = std::to_string(pcat);
    first = true;
    pcnt = 0;
    if (prm->params_cnt > 0) {
        webua->resp_page += ",\"prefix\":\"" + conf->params_prefix(pcat)+"\"";
        webua->resp_page += ",\"detail\":";
        for (indx=0; indx<prm->params_array.size(); indx++) {
            itm = &prm->params_array[indx];
            if (itm->param_lvl <= webu->cfg->webcontrol_access) {
                if (first) {
                    first = false;
                    webua->resp_page += "{";
                } else {
                    webua->resp_page += ",";
                }
                pval = itm->param_value;
                plst = "";
                for (indx2=0;indx2<itm->param_lst.size();indx2++) {
                    if (itm->param_type == PARM_TYP_INTLIST) {
                        plst += ",\""+escstr(_(itm->param_lst[indx2]))+"\"";
                        pchk = itm->param_lst[indx2];
                        pchk = pchk.substr(0, pchk.find("-"));
                        if (mtoi(pval) == mtoi(pchk)) {
                            pval = itm->param_lst[indx2];
                        }
                    } else if (itm->param_type == PARM_TYP_LIST) {
                        if (itm->param_lst[indx2] != _(itm->param_lst[indx2])) {
                            plst += ",\""+escstr(itm->param_lst[indx2])+"(";
                            plst += escstr(_(itm->param_lst[indx2])) + ")\"";
                        } else {
                            plst += ",\""+escstr(itm->param_lst[indx2])+"\"";
                        }
                    }
                }
                if (plst != "") {
                    plst="["+plst.substr(1)+"]";
                }
                pcnt++;
                parms_item(conf,itm->param_name, pval
                    ,itm->param_type, plst, catstr, itm->param_desc);
            }
        }
        if (first) {
            first = false;
            webua->resp_page += "{";
        } else {
            webua->resp_page += ",";
        }
        pcnt++;
        parms_item(conf,"extra_options","",PARM_TYP_STRING,"",catstr,"");
        webua->resp_page += "}";
        webua->resp_page += ",\"count\":" + std::to_string(pcnt);

    } else {
        webua->resp_page += ",\"prefix\":\"" + conf->params_prefix(pcat)+"\"";
        webua->resp_page += ",\"detail\":\"\"";
        webua->resp_page += ",\"count\": 0";
    }

}

void cls_webu_json::parms_item(cls_config *conf
    , std::string parm_name, std::string porig
    , enum PARM_TYP ptyp, std::string parm_list
    , std::string parm_catstr, std::string pdesc_in)
{
    std::string parm_val, parm_enable;
    std::string parm_bool, parm_typstr, pdesc;
    std::string pl10n;

    parm_val = "";
    if (webu->cfg->webcontrol_access < PARM_LVL_01) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    pl10n = _(parm_name);
    if (pl10n != parm_name){
        pdesc =escstr(pl10n + _(pdesc_in));
    } else {
        pdesc =escstr(_(pdesc_in));
    }

    parm_val = escstr(porig);
    parm_typstr = conf->type_desc(ptyp);

    if ((ptyp == PARM_TYP_INT) || (ptyp == PARM_TYP_FLOAT)) {
        if (parm_val == ""){
            parm_val = "0";
        }
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\":" + parm_val +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + parm_catstr +
            ",\"type\":\"" + parm_typstr + "\"" +
            ",\"desc\":\"" + pdesc + "\"" +
            "}";
    } else if (ptyp == PARM_TYP_BOOL) {
        parm_bool = (parm_val == "on") ? "true" : "false";
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\":" + parm_bool +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + parm_catstr +
            ",\"type\":\"" + parm_typstr + "\"" +
            ",\"desc\":\"" + pdesc + "\"" +
            "}";
    } else if ((ptyp == PARM_TYP_LIST) || (ptyp == PARM_TYP_INTLIST)) {
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\": \"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + parm_catstr +
            ",\"type\":\"" + parm_typstr + "\"" +
            ",\"desc\":\"" + pdesc + "\"" +
            ",\"list\":" + parm_list +
            "}";
    } else if (ptyp == PARM_TYP_PARAMS) {
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + parm_catstr +
            ",\"type\":\"" + parm_typstr + "\"" +
            ",\"desc\":\"" + pdesc + "\"";
            parms_item_detail(conf, parm_name);
        webua->resp_page += "}";
    } else {
        webua->resp_page +=
            "\"" + parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + parm_catstr +
            ",\"type\":\"" + parm_typstr + "\"" +
            ",\"desc\":\"" + pdesc + "\"" +
        "}";
    }
}

/* Temporary workaround until the configuration parameters get
    rewritten to allow for the list to be provided directly via
    a vector.  We parse this so that we can translate each item*/
void cls_webu_json::parms_lst(std::string &parm)
{
    std::vector<std::string>   tmp_lst;
    std::string tmp;
    int indx;

    tmp_lst.clear();
    mytrim(parm);
    parm = parm.substr(1, parm.length()-2); /* strip []*/
    mytrim(parm);
    while (parm.find(",") != std::string::npos) {
        tmp = parm.substr(0,parm.find(","));
        if (tmp.length()>=2) {
            tmp = tmp.substr(1,tmp.length()-2); /* strip " */
        }
        mytrim(tmp);
        tmp_lst.push_back(tmp);
        parm=parm.substr(parm.find(",")+1);
        mytrim(parm);
    }
    if (parm.length()>=2) {
        parm = parm.substr(1,parm.length()-2); /* strip " */
    }
    mytrim(parm);
    tmp_lst.push_back(parm);

    /* Now we put it back together with translations*/
    if (tmp_lst.size()>0) {
        parm = "";
        for (indx=0;indx<tmp_lst.size();indx++) {
            if (tmp_lst[indx] == _(tmp_lst[indx])) {
                parm += ",\""+tmp_lst[indx]+"\"";
            } else {
                parm += ",\""+tmp_lst[indx]+" (";
                parm += _(tmp_lst[indx])+")\"";
            }
        }
        parm="["+parm.substr(1)+"]";
    } else {
        parm="[\"\"]";
    }

}

void cls_webu_json::parms_one(cls_config *conf, PARM_CAT pcat)
{
    int indx_parm;
    bool first;
    std::string parm_orig, catstr, plst;

    indx_parm = 0;
    first = true;
    while ((config_parms[indx_parm].parm_name != "") ) {
        if ((config_parms[indx_parm].parm_lvl > webu->cfg->webcontrol_access) ||
            (pcat != (int)config_parms[indx_parm].parm_cat)) {
            indx_parm++;
            continue;
        }
        if (first) {
            first = false;
            webua->resp_page += "{";
        } else {
            webua->resp_page += ",";
        }
        conf->edit_get(config_parms[indx_parm].parm_name
            , parm_orig, config_parms[indx_parm].parm_cat);
        catstr = std::to_string(pcat);
        if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
            conf->edit_list(config_parms[indx_parm].parm_name, plst, pcat);
            parms_lst(plst);
        } else {
            plst="[\"\"]";
        }
        parms_item(conf, config_parms[indx_parm].parm_name
            ,parm_orig,config_parms[indx_parm].parm_type
            ,plst, catstr, "");
        indx_parm++;
    }
    if (first == false) {
        webua->resp_page += "}";
    } else {
        webua->resp_page += "\"\"";
    }
}

void cls_webu_json::parms_all()
{
    int indx_cam, indx_cat;
    cls_config *pcfg;

    webua->resp_page += "{";
    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            webua->resp_page += ",";
        }
        webua->resp_page += "\"" + std::to_string(indx_cat) + "\": ";
        webua->resp_page += "{";
        for (indx_cam=-1; indx_cam<webu->cam_cnt; indx_cam++) {
            if (indx_cam == -1) {
                pcfg = webu->cfg;
                webua->resp_page += "\"0\": ";
            } else {
                pcfg = webu->cam_list[indx_cam]->cfg;
                webua->resp_page += ",\"" +
                    std::to_string(webu->cam_list[indx_cam]->cfg->device_id) + "\": ";
            }
            parms_one(pcfg, (PARM_CAT)indx_cat);
        }
        webua->resp_page += "}";
        indx_cat++;
    }
    webua->resp_page += "}";

}

void cls_webu_json::cameras_list()
{
    int indx_cam, indx, chk;
    std::string response;
    std::string strid;
    cls_camera     *cam;

    webua->resp_page += "{\"count\" : " + std::to_string(webu->cam_cnt);

    for (indx_cam=0; indx_cam<webu->cam_cnt; indx_cam++) {
        cam = webu->cam_list[indx_cam];
        strid =std::to_string(cam->cfg->device_id);
        webua->resp_page += ",\"" + std::to_string(indx_cam) + "\":";
        if (cam->cfg->device_name == "") {
            webua->resp_page += "{\"name\": \"camera " + strid + "\"";
        } else {
            webua->resp_page += "{\"name\": \"" + escstr(cam->cfg->device_name) + "\"";
        }

        webua->resp_page += ",\"id\": " + strid;

        chk = -1;
        for (indx=0;indx<webu->allcam->active_cnt;indx++) {
            if (webu->allcam->active_cam[indx].cam == cam) {
                chk = indx;
            }
        }
        if (chk == -1) {
            webua->resp_page += ",\"all_xpct_st\": 0";
            webua->resp_page += ",\"all_xpct_en\": 0";
            webua->resp_page += ",\"all_ypct_st\": 0";
            webua->resp_page += ",\"all_ypct_en\": 0";
            webua->resp_page += ",\"ptz\": " + strid;
            webua->resp_page += ",\"ptz\": false";
        } else {
            webua->resp_page += ",\"all_xpct_st\": ";
            webua->resp_page += std::to_string(webu->allcam->active_cam[chk].xpct_st);

            webua->resp_page += ",\"all_xpct_en\": ";
            webua->resp_page += std::to_string(webu->allcam->active_cam[chk].xpct_en);

            webua->resp_page += ",\"all_ypct_st\": ";
            webua->resp_page += std::to_string(webu->allcam->active_cam[chk].ypct_st);

            webua->resp_page += ",\"all_ypct_en\": ";
            webua->resp_page += std::to_string(webu->allcam->active_cam[chk].ypct_en);
            if (cam->cfg->ptz_move_track != "" ||
                cam->cfg->ptz_pan_left != "" ||
                cam->cfg->ptz_pan_right != "" ||
                cam->cfg->ptz_tilt_up != "" ||
                cam->cfg->ptz_tilt_down != "" ||
                cam->cfg->ptz_zoom_in != "" ||
                cam->cfg->ptz_zoom_out != "" ) {
                webua->resp_page += ",\"ptz\": true";
            } else {
                webua->resp_page += ",\"ptz\": false";
            }
        }
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

        catnm_short = webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, true);
        catnm_long = escstr(_(webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, false)));

        webua->resp_page += "{\"name\":\"" + catnm_short + "\",\"display\":\"" + catnm_long + "\"}";
        indx_cat++;
    }

    webua->resp_page += "}";
}

void cls_webu_json::i10n_list()
{
    std::string catnm_short, catnm_long;

    webua->resp_page += "{";

    webua->resp_page += "\"showhide\": \""+escstr(_("Show/Hide"))+"\"";
    webua->resp_page += ",\"startevent\": \""+escstr(_("Start Event"))+"\"";
    webua->resp_page += ",\"endevent\": \""+escstr(_("End Event"))+"\"";
    webua->resp_page += ",\"snapshot\": \""+escstr(_("Snapshot"))+"\"";
    webua->resp_page += ",\"pause\": \""+escstr(_("Pause"))+"\"";
    webua->resp_page += ",\"unpause\": \""+escstr(_("UnPause"))+"\"";
    webua->resp_page += ",\"startrestart\": \""+escstr(_("Start/Restart"))+"\"";
    webua->resp_page += ",\"stop\": \""+escstr(_("Stop"))+"\"";
    webua->resp_page += ",\"saveconfig\": \""+escstr(_("Save Config"))+"\"";
    webua->resp_page += ",\"addcamera\": \""+escstr(_("Add Camera"))+"\"";
    webua->resp_page += ",\"deletecamera\": \""+escstr(_("Delete Camera"))+"\"";
    webua->resp_page += ",\"useraction\": \""+escstr(_("User Action"))+"\"";
    webua->resp_page += ",\"showhidelog\": \""+escstr(_("Show/Hide Log"))+"\"";
    webua->resp_page += ",\"submit\": \""+escstr(_("Submit"))+"\"";
    webua->resp_page += ",\"cameras\": \""+escstr(_("Cameras"))+"\"";
    webua->resp_page += ",\"camera\": \""+escstr(_("Camera"))+"\"";
    webua->resp_page += ",\"allcameras\": \""+escstr(_("All Cameras"))+"\"";
    webua->resp_page += ",\"scancameras\": \""+escstr(_("Scan Cameras"))+"\"";
    webua->resp_page += ",\"actions\": \""+escstr(_("Actions"))+"\"";
    webua->resp_page += ",\"configuration\": \""+escstr(_("Configuration"))+"\"";
    webua->resp_page += ",\"recordings\": \""+escstr(_("Recordings"))+"\"";
    webua->resp_page += ",\"parameters\": \""+escstr(_("Parameters"))+"\"";
    webua->resp_page += ",\"selectcamera\": \""+escstr(_("Select Camera"))+"\"";
    webua->resp_page += ",\"default\": \""+escstr(_("Default"))+"\"";

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

    webua->resp_page += ",\"i10n\" : ";
    i10n_list();

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
        webua->resp_page += "{\"count\" :" + std::to_string(webu->cam_cnt);

        for (indx_cam=0; indx_cam<webu->cam_cnt; indx_cam++) {
            webua->cam = webu->cam_list[indx_cam];
            webua->resp_page += ",\""+ std::to_string(indx_cam) + "\":";
            movies_list();
        }
        webua->resp_page += "}";
        webua->cam = NULL;
    } else {
        indx_req = -1;
        for (indx_cam=0; indx_cam<webu->cam_cnt; indx_cam++) {
            if (webua->cam->cfg->device_id == webu->cam_list[indx_cam]->cfg->device_id){
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

    cam = webu->cam_list[indx_cam];

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

    webua->resp_page += "}";
}

void cls_webu_json::status()
{
    int indx_cam;

    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"version\" : \"" VERSION "\"";
    webua->resp_page += ",\"status\" : ";

    webua->resp_page += "{\"count\" : " + std::to_string(webu->cam_cnt);
        for (indx_cam=0; indx_cam<webu->cam_cnt; indx_cam++) {
            webua->resp_page += ",\"" +
                std::to_string(webu->cam_list[indx_cam]->cfg->device_id) + "\": ";
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