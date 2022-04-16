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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_file.hpp"
#include "dbse.hpp"


/* Callback for the file reader response*/
static ssize_t webu_file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
    struct ctx_webui *webui =(struct ctx_webui *)cls;

    (void)fseek (webui->req_file, pos, SEEK_SET);
    return fread (buf, 1, max, webui->req_file);
}

/* Close the requested file */
static void webu_file_free (void *cls)
{
    struct ctx_webui *webui =(struct ctx_webui *)cls;
    myfclose(webui->req_file);
}

/* Entry point for answering file request*/
mhdrslt webu_file_main(struct ctx_webui *webui)
{
    mhdrslt retcd;
    struct stat statbuf;
    struct MHD_Response *response;
    std::string full_nm;
    int indx;
    struct ctx_params *wact;

    /*If we have not fully started yet, simply return*/
    if (webui->cam->dbsemp == NULL) {
        return MHD_NO;
    }

    if (webui->cam->dbsemp->movie_cnt == 0) {
        if (dbse_motpls_getlist(webui->cam) != 0) {
            return MHD_NO;
        }
    }

    wact = webui->motapp->webcontrol_actions;
    for (indx = 0; indx < wact->params_count; indx++) {
        if (mystreq(wact->params_array[indx].param_name,"movies")) {
            if (mystreq(wact->params_array[indx].param_value,"off")) {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                return MHD_NO;
            } else {
                break;
            }
        }
    }

    full_nm = "";
    for (indx=0; indx < webui->cam->dbsemp->movie_cnt; indx++) {
        if (mystreq(webui->cam->dbsemp->movie_list[indx].movie_nm
            , webui->uri_cmd2.c_str())) {
            full_nm = webui->cam->dbsemp->movie_list[indx].full_nm;
        }
    }

    if (stat(full_nm.c_str(), &statbuf) == 0) {
        webui->req_file = myfopen(full_nm.c_str(), "rbe");
    } else {
        webui->req_file = NULL;
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,"Security warning: Client IP %s requested file: %s"
            ,webui->clientip.c_str(), webui->uri_cmd2.c_str());
    }

    if (webui->req_file == NULL) {
        webui->resp_page = "<html><head><title>Bad File</title>"
            "</head><body>Bad File</body></html>";

        response = MHD_create_response_from_buffer(webui->resp_page.length()
            ,(void *)webui->resp_page.c_str(), MHD_RESPMEM_PERSISTENT);
        retcd = MHD_queue_response (webui->connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response (response);

    } else {
        response = MHD_create_response_from_callback (
            statbuf.st_size, 32 * 1024
            , &webu_file_reader
            , webui
            , &webu_file_free);
        if (response == NULL) {
            myfclose(webui->req_file);
            return MHD_NO;
        }
        retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
    }

    return retcd;
}

