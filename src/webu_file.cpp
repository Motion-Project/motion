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
#include "picture.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_file.hpp"
#include "dbse.hpp"

/* Callback for the file reader response*/
static ssize_t webu_file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
    cls_webu_ans *webu_ans =(cls_webu_ans *)cls;
    (void)fseek (webu_ans->req_file, (long)pos, SEEK_SET);
    return (ssize_t)fread (buf, 1, max, webu_ans->req_file);
}

void cls_webu_file::movies() {
    mhdrslt retcd;
    struct stat statbuf;
    struct MHD_Response *response;
    std::string full_nm;
    vec_files flst;
    int indx;
    std::string sql;

    /*If we have not fully started yet, simply return*/
    if (app->dbse == NULL) {
        webua->bad_request();
        return;
    }

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "movies") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webua->bad_request();
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
    if (flst.size() == 0) {
        webua->bad_request();
        return;
    }

    full_nm = "";
    for (indx=0;indx<flst.size();indx++) {
        if (flst[indx].file_nm == webua->uri_cmd2) {
            full_nm = flst[indx].full_nm;
        }
    }

    if (stat(full_nm.c_str(), &statbuf) == 0) {
        webua->req_file = myfopen(full_nm.c_str(), "rbe");
    } else {
        webua->req_file = nullptr;
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,"Security warning: Client IP %s requested file: %s"
            ,webua->clientip.c_str(), webua->uri_cmd2.c_str());
    }

    if (webua->req_file == nullptr) {
        webua->resp_page = "<html><head><title>Bad File</title>"
            "</head><body>Bad File</body></html>";
        webua->resp_type = WEBUI_RESP_HTML;
        webua->mhd_send();
        retcd = MHD_YES;
    } else {
        response = MHD_create_response_from_callback (
            (size_t)statbuf.st_size, 32 * 1024
            , &webu_file_reader
            , webua, NULL);
        if (response == NULL) {
            if (webua->req_file != nullptr) {
                myfclose(webua->req_file);
                webua->req_file = nullptr;
            }
            webua->bad_request();
            return;
        }
        retcd = MHD_queue_response (webua->connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
    }
    if (retcd == MHD_NO) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Error processing file request");
    }

}
void cls_webu_file::user_page()
{
    char response[PATH_MAX];
    std::string fullname, fname, ext;
    size_t pos, indx;
    FILE *fp = NULL;

    webua->resp_page = "";
    webua->resp_type = WEBUI_RESP_HTML;

    pos = app->cfg->conf_filename.find("/",0);
    if (pos == std::string::npos) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Unable to determine base path for: %s")
            , app->cfg->conf_filename.c_str());
        return;
    }
    fullname = app->cfg->conf_filename.substr(0,
                app->cfg->conf_filename.find_last_of("/"));
    fullname += "/webcontrol/";

    if (webua->uri_cmd0 == "") {
        fname = app->cfg->webcontrol_html;
    } else {
        fname = webua->uri_cmd0;
    }

    pos = fname.find(".", 0);
    if (pos == std::string::npos) {
        ext = "";
    } else {
        pos = fname.find_last_of(".");
        ext = fname.substr(pos + 1);
        fname = fname.substr(0, pos);
    }

    /*sanitize*/
    for (indx=0;indx < fname.length(); indx++) {
        if ((std::isalnum(fname[indx]) == false) &&
            (fname[indx] != '_') &&
            (fname[indx] != '-')) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("File names are restricted to -_ and alpha numberic characters: %s")
                , fname.c_str());
            return;
        }
    }

    mylower(ext);
    if (ext == "json") {
        webua->resp_type = WEBUI_RESP_JSON;
    } else if ((ext == "js") ) {
        webua->resp_type = WEBUI_RESP_JS;
    } else if ((ext == "css") ) {
        webua->resp_type = WEBUI_RESP_CSS;
    } else if (ext == "html") {
        webua->resp_type = WEBUI_RESP_HTML;
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid file extension requested: %s")
            , fname.c_str());
        return;
    }

    fullname += fname + "." + ext;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , _("Retrieving file: %s type/extension: %d/%s")
        , fullname.c_str(), webua->resp_type, ext.c_str());

    fp = myfopen(fullname.c_str(), "re");
    if (fp == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid user requested file: %s")
            , fullname.c_str());
        webua->resp_type = WEBUI_RESP_HTML;
        return;
    } else {
        while (fgets(response, PATH_MAX-1, fp)) {
            webua->resp_page += response;
        }
        myfclose(fp);
    }
}

void cls_webu_file::main()
{
    webua->gzip_encode = false;
    if (webua->uri_cmd1 == "movies") {
        movies();
    } else {
        pthread_mutex_lock(&app->mutex_post);
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "Getting user page");
            user_page();
        pthread_mutex_unlock(&app->mutex_post);
    }
    if (webua->resp_page == "") {
        webua->bad_request();
    } else {
        webua->mhd_send();
    }
}

cls_webu_file::cls_webu_file(cls_webu_ans *p_webua)
{
    app     = p_webua->app;
    webu    = p_webua->webu;
    webua   = p_webua;
}

cls_webu_file::~cls_webu_file()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
}