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
#include "webu_html.hpp"
#include "webu_common.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "webu_json.hpp"
#include "webu_post.hpp"
#include "webu_file.hpp"
#include "video_v4l2.hpp"


/* Extract the camid and cmds from the url */
int cls_webu_ans::parseurl()
{
    char *tmpurl;
    size_t  pos_slash1, pos_slash2, baselen;

    /* Example:  /camid/cmd1/cmd2/cmd3   */
    uri_camid = "";
    uri_cmd1 = "";
    uri_cmd2 = "";
    uri_cmd3 = "";

    MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Sent url: %s"),url.c_str());

    tmpurl = (char*)mymalloc(url.length()+1);
    memcpy(tmpurl, url.c_str(), url.length());

    MHD_http_unescape(tmpurl);

    url.assign(tmpurl);
    free(tmpurl);

    MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),url.c_str());

    baselen = app->cfg->webcontrol_base_path.length();

    if (url.length() < baselen) {
        return -1;
    }

    if (url.substr(baselen) ==  "/favicon.ico") {
        return -1;
    }

    if (url.substr(0, baselen) !=
        app->cfg->webcontrol_base_path) {
        return -1;
    }

    if (url == "/") {
        return 0;
    }

    /* Remove any trailing slash to keep parms clean */
    if (url.substr(url.length()-1,1) == "/") {
        url = url.substr(0, url.length()-1);
    }

    if (url.length() == baselen) {
        return 0;
    }

    pos_slash1 = url.find("/", baselen+1);
    if (pos_slash1 != std::string::npos) {
        uri_camid = url.substr(baselen+1, pos_slash1-baselen- 1);
    } else {
        uri_camid = url.substr(baselen+1);
        return 0;
    }

    pos_slash1++;
    if (pos_slash1 >= url.length()) {
        return 0;
    }

    pos_slash2 = url.find("/", pos_slash1);
    if (pos_slash2 != std::string::npos) {
        uri_cmd1 = url.substr(pos_slash1, pos_slash2 - pos_slash1);
    } else {
        uri_cmd1 = url.substr(pos_slash1);
        return 0;
    }

    pos_slash1 = ++pos_slash2;
    if (pos_slash1 >= url.length()) {
        return 0;
    }

    if (uri_cmd1 == "movies") {
        /* Whole remaining url is the movie name and possibly subdir */
        uri_cmd2 = url.substr(pos_slash1);
        return 0;
    } else {
        pos_slash2 = url.find("/", pos_slash1);
        if (pos_slash2 != std::string::npos) {
            uri_cmd2 = url.substr(pos_slash1, pos_slash2 - pos_slash1);
        } else {
            uri_cmd2 = url.substr(pos_slash1);
            return 0;
        }

        pos_slash1 = ++pos_slash2;
        if (pos_slash1 >= url.length()) {
            return 0;
        }
        uri_cmd3 = url.substr(pos_slash1);
    }
    return 0;

}
/* Edit the parameters specified in the url sent */
void cls_webu_ans::parms_edit()
{
    int indx, is_nbr;

    if (parseurl() != 0) {
        uri_camid = "";
        uri_cmd1 = "";
        uri_cmd2 = "";
        uri_cmd3 = "";
        url = "";
    }

    if (uri_camid.length() > 0) {
        is_nbr = true;
        for (indx=0; indx < (int)uri_camid.length(); indx++) {
            if ((uri_camid[(uint)indx] > '9') || (uri_camid[(uint)indx] < '0')) {
                is_nbr = false;
            }
        }
        if (is_nbr) {
            device_id = atoi(uri_camid.c_str());
        }
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->device_id == device_id) {
            camindx = indx;
            cam = app->cam_list[indx];
        }
    }

    MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "camid: >%s< camindx: >%d< cmd1: >%s< cmd2: >%s< cmd3: >%s<"
        , uri_camid.c_str(), camindx
        , uri_cmd1.c_str(), uri_cmd2.c_str()
        , uri_cmd3.c_str());

}

/* Log the ip of the client connecting*/
void cls_webu_ans::clientip_get()
{
    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ip_dst;
    struct sockaddr_in6 *con_socket6;
    struct sockaddr_in *con_socket4;
    int is_ipv6;

    is_ipv6 = false;
    if (app->cfg->webcontrol_ipv6) {
        is_ipv6 = true;
    }

    con_info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (is_ipv6) {
        con_socket6 = (struct sockaddr_in6 *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET6, &con_socket6->sin6_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            clientip = "Unknown";
        } else {
            clientip.assign(client);
            if (clientip.substr(0, 7) == "::ffff:") {
                clientip = clientip.substr(7);
            }
        }
    } else {
        con_socket4 = (struct sockaddr_in *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET, &con_socket4->sin_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            clientip = "Unknown";
        } else {
            clientip.assign(client);
        }
    }

}

/* Get the hostname */
void cls_webu_ans::hostname_get()
{
    const char *hdr;

    hdr = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_HOST);
    if (hdr == NULL) {
        hostfull = "//localhost:" +
            std::to_string(app->cfg->webcontrol_port) +
            app->cfg->webcontrol_base_path;
    } else {
        hostfull = "//" +  std::string(hdr) +
            app->cfg->webcontrol_base_path;
    }

    MOTPLS_LOG(DBG,TYPE_ALL, NO_ERRNO, _("Full Host:  %s"), hostfull.c_str());

    return;
}

/* Log the failed authentication check */
void cls_webu_ans::failauth_log(bool userid_fail)
{
    timespec            tm_cnct;
    ctx_webu_clients    clients;
    std::list<ctx_webu_clients>::iterator   it;

    MOTPLS_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), clientip.c_str());

    clock_gettime(CLOCK_MONOTONIC, &tm_cnct);

    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if (it->clientip == clientip) {
            it->conn_nbr++;
            it->conn_time.tv_sec =tm_cnct.tv_sec;
            it->authenticated = false;
            if (userid_fail) {
                it->userid_fail_nbr++;
            }
            return;
        }
        it++;
    }

    clients.clientip = clientip;
    clients.conn_nbr = 1;
    clients.conn_time = tm_cnct;
    clients.authenticated = false;
    if (userid_fail) {
        clients.userid_fail_nbr = 1;
    } else {
        clients.userid_fail_nbr = 0;
    }

    webu->wb_clients.push_back(clients);

    return;

}

void cls_webu_ans::client_connect()
{
    timespec                                tm_cnct;
    ctx_webu_clients                 clients;
    std::list<ctx_webu_clients>::iterator   it;

    clock_gettime(CLOCK_MONOTONIC, &tm_cnct);

    /* First we need to clean out any old IPs from the list*/
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if ((tm_cnct.tv_sec - it->conn_time.tv_sec) >=
            (app->cfg->webcontrol_lock_minutes*60)) {
            it = webu->wb_clients.erase(it);
        }
        it++;
    }

    /* When this function is called, we know that we are authenticated
     * so we reset the info and as needed print a message that the
     * ip is connected.
     */
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if (it->clientip == clientip) {
            if (it->authenticated == false) {
                MOTPLS_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),clientip.c_str());
            }
            it->authenticated = true;
            it->conn_nbr = 1;
            it->userid_fail_nbr = 0;
            it->conn_time.tv_sec = tm_cnct.tv_sec;
            return;
        }
        it++;
    }

    /* The ip was not already in our list. */
    clients.clientip = clientip;
    clients.conn_nbr = 1;
    clients.userid_fail_nbr = 0;
    clients.conn_time = tm_cnct;
    clients.authenticated = true;
    webu->wb_clients.push_back(clients);

    MOTPLS_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),clientip.c_str());

    return;

}

/* Check for ips with excessive failed authentication attempts */
mhdrslt cls_webu_ans::failauth_check()
{
    timespec                                tm_cnct;
    std::list<ctx_webu_clients>::iterator   it;
    std::string                             tmp;

    if (webu->wb_clients.size() == 0) {
        return MHD_YES;
    }

    clock_gettime(CLOCK_MONOTONIC, &tm_cnct);
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if ((it->clientip == clientip) &&
            ((tm_cnct.tv_sec - it->conn_time.tv_sec) <
             (app->cfg->webcontrol_lock_minutes*60)) &&
            (it->authenticated == false) &&
            (it->conn_nbr > app->cfg->webcontrol_lock_attempts)) {
            MOTPLS_LOG(EMG, TYPE_STREAM, NO_ERRNO
                , "Ignoring connection from: %s"
                , clientip.c_str());
            it->conn_time = tm_cnct;
            if (app->cfg->webcontrol_lock_script != "") {
                tmp = app->cfg->webcontrol_lock_script + " " +
                    std::to_string(it->userid_fail_nbr) + " " +  clientip;
                util_exec_command(cam, tmp.c_str(), NULL);
            }
            return MHD_NO;
        } else if ((tm_cnct.tv_sec - it->conn_time.tv_sec) >=
            (app->cfg->webcontrol_lock_minutes*60)) {
            it = webu->wb_clients.erase(it);
        } else {
            it++;
        }
    }

    return MHD_YES;

}

/* Create a authorization denied response to user*/
mhdrslt cls_webu_ans::mhd_digest_fail(int signal_stale)
{
    struct MHD_Response *response;
    mhdrslt retcd;

    authenticated = false;

    resp_page = "<html><head><title>Access denied</title>"
        "</head><body>Access denied</body></html>";

    response = MHD_create_response_from_buffer(resp_page.length()
        ,(void *)resp_page.c_str(), MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_auth_fail_response(connection, auth_realm
        ,auth_opaque, response
        ,(signal_stale == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);

    MHD_destroy_response(response);

    return retcd;
}

/* Perform digest authentication */
mhdrslt cls_webu_ans::mhd_digest()
{
    /* This function gets called a couple of
     * times by MHD during the authentication process.
     */
    int retcd;
    char *user;

    /*Get username or prompt for a user/pass */
    user = MHD_digest_auth_get_username(connection);
    if (user == NULL) {
        return mhd_digest_fail(MHD_NO);
    }

    /* Check for valid user name */
    if (mystrne(user, auth_user)) {
        failauth_log(true);
        myfree(user);
        return mhd_digest_fail(MHD_NO);
    }
    myfree(user);

    /* Check the password as well*/
    retcd = MHD_digest_auth_check(connection, auth_realm
        , auth_user, auth_pass, 300);

    if (retcd == MHD_NO) {
        failauth_log(false);
    }

    if ( (retcd == MHD_INVALID_NONCE) || (retcd == MHD_NO) )  {
        return mhd_digest_fail(retcd);
    }

    authenticated = true;
    return MHD_YES;
}

/* Create a authorization denied response to user*/
mhdrslt cls_webu_ans::mhd_basic_fail()
{
    struct MHD_Response *response;
    int retcd;

    authenticated = false;

    resp_page = "<html><head><title>Access denied</title>"
        "</head><body>Access denied</body></html>";

    response = MHD_create_response_from_buffer(resp_page.length()
        ,(void *)resp_page.c_str(), MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_basic_auth_fail_response (connection, auth_realm, response);

    MHD_destroy_response(response);

    if (retcd == MHD_YES) {
        return MHD_YES;
    } else {
        return MHD_NO;
    }
}

/* Perform Basic Authentication.  */
mhdrslt cls_webu_ans::mhd_basic()
{
    char *user, *pass;

    pass = NULL;
    user = NULL;

    user = MHD_basic_auth_get_username_password (connection, &pass);
    if ((user == NULL) || (pass == NULL)) {
        myfree(user);
        myfree(pass);
        return mhd_basic_fail();
    }

    if ((mystrne(user, auth_user)) || (mystrne(pass, auth_pass))) {
        failauth_log(mystrne(user, auth_user));
        myfree(user);
        myfree(pass);
        return mhd_basic_fail();
    }

    myfree(user);
    myfree(pass);

    authenticated = true;

    return MHD_YES;

}

/* Parse apart the user:pass provided*/
void cls_webu_ans::mhd_auth_parse()
{
    int auth_len;
    char *col_pos;

    myfree(auth_user);
    myfree(auth_pass);

    auth_len = (int)app->cfg->webcontrol_authentication.length();
    col_pos =(char*) strstr(app->cfg->webcontrol_authentication.c_str() ,":");
    if (col_pos == NULL) {
        auth_user = (char*)mymalloc((uint)(auth_len+1));
        auth_pass = (char*)mymalloc(2);
        snprintf(auth_user, (uint)auth_len + 1, "%s"
            ,app->cfg->webcontrol_authentication.c_str());
        snprintf(auth_pass, 2, "%s","");
    } else {
        auth_user = (char*)mymalloc((uint)auth_len - strlen(col_pos) + 1);
        auth_pass =(char*)mymalloc(strlen(col_pos));
        snprintf(auth_user, (uint)auth_len - strlen(col_pos) + 1, "%s"
            ,app->cfg->webcontrol_authentication.c_str());
        snprintf(auth_pass, strlen(col_pos), "%s", col_pos + 1);
    }
}

/* Initialize for authorization */
mhdrslt cls_webu_ans::mhd_auth()
{
    unsigned int rand1,rand2;

    srand((unsigned int)time(NULL));
    rand1 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    rand2 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(auth_opaque, WEBUI_LEN_PARM, "%08x%08x", rand1, rand2);

    snprintf(auth_realm, WEBUI_LEN_PARM, "%s","Motion");

    if (app->cfg->webcontrol_authentication == "") {
        authenticated = true;
        if (app->cfg->webcontrol_auth_method != "none") {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No webcontrol user:pass provided"));
        }
        return MHD_YES;
    }

    if (auth_user == NULL) {
        mhd_auth_parse();
    }

    if (app->cfg->webcontrol_auth_method == "basic") {
        return mhd_basic();
    } else if (app->cfg->webcontrol_auth_method == "digest") {
        return mhd_digest();
    }


    authenticated = true;
    return MHD_YES;

}

/* Send the response that we created back to the user.  */
void cls_webu_ans::mhd_send()
{
    mhdrslt retcd;
    struct MHD_Response *response;
    p_lst *lst = &webu->wb_headers->params_array;
    p_it it;

    response = MHD_create_response_from_buffer(resp_page.length()
        ,(void *)resp_page.c_str(), MHD_RESPMEM_PERSISTENT);
    if (!response) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return;
    }

    if (webu->wb_headers->params_count > 0) {
        for (it = lst->begin(); it != lst->end(); it++) {
            MHD_add_response_header (response
                , it->param_name.c_str(),it->param_value.c_str());
        }
    }

    if (resp_type == WEBUI_RESP_TEXT) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
    } else if (resp_type == WEBUI_RESP_JSON) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json;");
    } else {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
    }

    retcd = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    if (retcd == MHD_NO) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed."));
    }
}

void cls_webu_ans::bad_request()
{
    resp_page =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "<p>Bad Request</p>\n"
        "<p>The server did not understand your request.</p>\n"
        "</body>\n"
        "</html>\n";
    mhd_send();
}

/* Answer the get request from the user */
void cls_webu_ans::answer_get()
{
    MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"processing get");

    if ((uri_cmd1 == "mjpg") || (uri_cmd1 == "mpegts") ||
        (uri_cmd1 == "static")) {
        if (webu_stream == nullptr) {
            webu_stream  = new cls_webu_stream(this);
        }
        webu_stream->main();

    } else if (uri_cmd1 == "movies") {
        if (webu_file == nullptr) {
            webu_file = new cls_webu_file(this);
        }
        webu_file->main();

    } else if ((uri_cmd1 == "config.json") ||
        (uri_cmd1 == "movies.json") || (uri_cmd1 == "status.json")) {
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }
        webu_json->main();

    } else {
        if (webu_html == nullptr) {
            webu_html = new cls_webu_html(this);
        }
        webu_html->main();
    }
}

/* Answer the connection request for the webcontrol*/
mhdrslt cls_webu_ans::answer_main(struct MHD_Connection *p_connection
    , const char *method, const char *upload_data, size_t *upload_data_size)
{
    mhdrslt retcd;

    cnct_type = WEBUI_CNCT_CONTROL;
    connection = p_connection;

    if (url.length() == 0) {
        bad_request();
        return MHD_YES;
    }

    if (cam != NULL) {
        if (cam->handler_stop) {
           MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Shutting down camera"));
           return MHD_NO;
        }
    }

    if (clientip.length() == 0) {
        clientip_get();
    }

    if (failauth_check() == MHD_NO) {
        return MHD_NO;
    }

    if (authenticated == false) {
        retcd = mhd_auth();
        if (authenticated == false) {
            return retcd;
        }
    }

    client_connect();

    if (mhd_first) {
        mhd_first = false;
        if (mystreq(method,"POST")) {
            if (webu_post == nullptr) {
                webu_post = new cls_webu_post(this);
            }
            cnct_method = WEBUI_METHOD_POST;
            retcd = webu_post->processor_init();
        } else {
            cnct_method = WEBUI_METHOD_GET;
            retcd = MHD_YES;
        }

        return retcd;
    }

    hostname_get();

    if (mystreq(method,"POST")) {
        retcd = webu_post->processor_start(upload_data, upload_data_size);
    } else {
        answer_get();
        retcd = MHD_YES;
    }
    return retcd;

}

cls_webu_ans::cls_webu_ans(ctx_motapp *p_motapp, const char *uri)
{
    app = p_motapp;
    webu = p_motapp->webu;

    char *tmplang;

    url           = "";
    uri_camid     = "";
    uri_cmd1      = "";
    uri_cmd2      = "";
    uri_cmd3      = "";
    clientip      = "";
    lang          = "";                          /* Two digit lang code */

    auth_opaque   = (char*)mymalloc(WEBUI_LEN_PARM);
    auth_realm    = (char*)mymalloc(WEBUI_LEN_PARM);
    auth_user     = nullptr;                        /* Buffer to hold the user name*/
    auth_pass     = nullptr;                        /* Buffer to hold the password */
    authenticated = false;                       /* boolean for whether we are authenticated*/

    resp_page     = "";                          /* The response being constructed */
    req_file      = nullptr;

    cnct_type     = WEBUI_CNCT_UNKNOWN;
    resp_type     = WEBUI_RESP_HTML;             /* Default to html response */
    cnct_method   = WEBUI_METHOD_GET;
    camindx       = -1;
    device_id     = -1;

    tmplang = setlocale(LC_ALL, NULL);
    if (tmplang == nullptr) {
        lang = "en";
    } else {
        lang.assign(tmplang, 2);
    }
    mhd_first = true;

    cam       = nullptr;
    webu_file = nullptr;
    webu_html = nullptr;
    webu_json = nullptr;
    webu_post = nullptr;
    webu_stream = nullptr;

    url.assign(uri);

    parms_edit();
    webu->cnct_cnt++;
}

void cls_webu_ans::deinit_counter()
{
    ctx_stream_data *strm;
    cls_camera *p_cam;
    int indx, cam_max, cam_min;

    if (cnct_type < WEBUI_CNCT_JPG_MIN) {
        return;
    }

    if (device_id == 0) {
        cam_min = 0;
        cam_max = app->cam_cnt;
    } else if ((device_id > 0) && (camindx >= 0)) {
        cam_min = camindx;
        cam_max = cam_min +1;
    } else {
        cam_min = 1;
        cam_max = 0;
    }

    for (indx=cam_min; indx<cam_max; indx++) {
        p_cam = app->cam_list[indx];
        pthread_mutex_lock(&p_cam->stream.mutex);
            if ((cnct_type == WEBUI_CNCT_JPG_FULL) ||
                (cnct_type == WEBUI_CNCT_TS_FULL)) {
                strm = &p_cam->stream.norm;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SUB) ||
                        (cnct_type == WEBUI_CNCT_TS_SUB)) {
                strm = &p_cam->stream.sub;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_MOTION) ||
                        (cnct_type == WEBUI_CNCT_TS_MOTION )) {
                strm = &p_cam->stream.motion;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SOURCE) ||
                        (cnct_type == WEBUI_CNCT_TS_SOURCE)) {
                strm = &p_cam->stream.source;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SECONDARY) ||
                        (cnct_type == WEBUI_CNCT_TS_SECONDARY)) {
                strm = &p_cam->stream.secondary;
            } else {
                strm = &p_cam->stream.norm;
            }

            if ((cnct_type > WEBUI_CNCT_JPG_MIN) &&
                (cnct_type < WEBUI_CNCT_JPG_MAX)) {
                if ((device_id == 0) && (strm->all_cnct > 0)) {
                    strm->all_cnct--;
                } else if ((device_id > 0) && (strm->jpg_cnct > 0)) {
                    strm->jpg_cnct--;
                }
            } else if ((cnct_type > WEBUI_CNCT_TS_MIN) &&
                (cnct_type < WEBUI_CNCT_TS_MAX)) {
                if ((device_id == 0) && (strm->all_cnct > 0)) {
                    strm->all_cnct--;
                } else if ((device_id > 0) && (strm->ts_cnct > 0)) {
                    strm->ts_cnct--;
                }
            }
            if ((strm->all_cnct == 0) &&
                (strm->jpg_cnct == 0) &&
                (strm->ts_cnct == 0) &&
                (p_cam->passflag)) {
                    myfree(strm->img_data);
                    myfree(strm->jpg_data);
            }
        pthread_mutex_unlock(&p_cam->stream.mutex);
    }
}

cls_webu_ans::~cls_webu_ans()
{
    deinit_counter();

    if (webu_file != nullptr) {
        delete webu_file;
    }
    if (webu_html != nullptr) {
        delete webu_html;
    }
    if (webu_json != nullptr) {
        delete webu_json;
    }
    if (webu_post != nullptr) {
        delete webu_post;
    }
    if (webu_stream != nullptr) {
        delete webu_stream;
    }

    myfree(auth_user);
    myfree(auth_pass);
    myfree(auth_opaque);
    myfree(auth_realm);
    webu->cnct_cnt--;
}