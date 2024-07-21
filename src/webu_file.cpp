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
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_common.hpp"
#include "webu_file.hpp"
#include "dbse.hpp"

/* Callback for the file reader response*/
static ssize_t webu_file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
    cls_webu_ans *webu_ans =(cls_webu_ans *)cls;

    (void)fseek (webu_ans->req_file, (long)pos, SEEK_SET);
    return (ssize_t)fread (buf, 1, max, webu_ans->req_file);
}

/* Close the requested file */
static void webu_file_free (void *cls)
{
    cls_webu_ans *webu_ans =(cls_webu_ans *)cls;
    myfclose(webu_ans->req_file);
}

/****************************************************/

void cls_webu_file::main() {
    mhdrslt retcd;
    struct stat statbuf;
    struct MHD_Response *response;
    std::string full_nm;
    int indx;
    p_lst *lst = &webu->wb_actions->params_array;
    p_it it;

    /*If we have not fully started yet, simply return*/
    if (app->dbse == NULL) {
        webua->bad_request();
        return;
    }

    if (app->dbse->movie_cnt == 0) {
        dbse_movies_getlist(app, webua->cam->device_id);
    }

    for (it = lst->begin(); it != lst->end(); it++) {
        if (it->param_name == "movies") {
            if (it->param_value == "off") {
                MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webua->bad_request();
                return;
            } else {
                break;
            }
        }
    }

    full_nm = "";
    for (indx=0; indx < app->dbse->movie_cnt; indx++) {
        if (mystreq(app->dbse->movie_list[indx].movie_nm
            ,webua->uri_cmd2.c_str())) {
            full_nm = app->dbse->movie_list[indx].full_nm;
        }
    }

    if (stat(full_nm.c_str(), &statbuf) == 0) {
        webua->req_file = myfopen(full_nm.c_str(), "rbe");
    } else {
        webua->req_file = NULL;
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,"Security warning: Client IP %s requested file: %s"
            ,webua->clientip.c_str(), webua->uri_cmd2.c_str());
    }

    if (webua->req_file == NULL) {
        webua->resp_page = "<html><head><title>Bad File</title>"
            "</head><body>Bad File</body></html>";
        webua->resp_type = WEBUI_RESP_HTML;
        webua->mhd_send();
        retcd = MHD_YES;
    } else {
        response = MHD_create_response_from_callback (
            (size_t)statbuf.st_size, 32 * 1024
            , &webu_file_reader
            , this
            , &webu_file_free);
        if (response == NULL) {
            myfclose(webua->req_file);
            webua->bad_request();
            return;
        }
        retcd = MHD_queue_response (webua->connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
    }
    if (retcd == MHD_NO) {
        MOTPLS_LOG(INF, TYPE_ALL, NO_ERRNO, "Error processing file request");
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