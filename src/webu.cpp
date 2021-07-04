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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_html.hpp"
#include "webu_stream.hpp"
#include "webu_json.hpp"
#include "webu_post.hpp"
#include "video_v4l2.hpp"

/* Context to pass the parms to functions to start mhd */
struct mhdstart_ctx {
    struct ctx_motapp       *motapp;
    std::string             tls_cert;
    std::string             tls_key;
    struct MHD_OptionItem   *mhd_ops;
    int                     mhd_opt_nbr;
    unsigned int            mhd_flags;
    int                     ipv6;
    struct sockaddr_in      lpbk_ipv4;
    struct sockaddr_in6     lpbk_ipv6;
};

/* Set defaults for the webui context */
static void webu_context_init(struct ctx_motapp *motapp, struct ctx_webui *webui)
{
    int indx;
    char *tmplang;

    webui->url           = "";
    webui->uri_camid     = "";
    webui->uri_cmd1      = "";
    webui->uri_cmd2      = "";
    webui->clientip      = "";
    webui->lang          = "";                          /* Two digit lang code */

    webui->auth_opaque   = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->auth_realm    = (char*)mymalloc(WEBUI_LEN_PARM);
    webui->auth_user     = NULL;                        /* Buffer to hold the user name*/
    webui->auth_pass     = NULL;                        /* Buffer to hold the password */
    webui->authenticated = false;                       /* boolean for whether we are authenticated*/
    webui->resp_size     = WEBUI_LEN_RESP * 10;         /* The size of the resp_page buffer.  May get adjusted */
    webui->resp_used     = 0;                           /* How many bytes used so far in resp_page*/
    webui->resp_image    = NULL;                        /* Buffer for sending the images */
    webui->stream_pos    = 0;                           /* Stream position of image being sent */
    webui->stream_fps    = 1;                           /* Stream rate */
    webui->resp_page     = "";                          /* The response being constructed */
    webui->post_info     = NULL;
    webui->post_sz       = 0;
    webui->motapp        = motapp;                      /* The motion application context */
    webui->cam           = NULL;                        /* The context pointer for a single camera */
    webui->cnct_type     = WEBUI_CNCT_UNKNOWN;
    webui->resp_type     = WEBUI_RESP_HTML;             /* Default to html response */
    webui->cnct_method   = WEBUI_METHOD_GET;

    /* get the number of cameras and threads */
    indx = 0;
    if (webui->motapp->cam_list != NULL) {
        while (webui->motapp->cam_list[++indx]) {
            continue;
        };
    }
    webui->cam_threads = indx;

    webui->cam_count = indx;
    if (indx > 1) {
        webui->cam_count--;
    }

    tmplang = setlocale(LC_ALL, NULL);
    if (tmplang == NULL) {
        webui->lang = "en";
    } else {
        webui->lang.assign(tmplang, 2);
    }

    return;
}

/* Free the variables in the webui context */
static void webu_context_free(struct ctx_webui *webui)
{
    int indx;

    if (webui->auth_user != NULL) {
        free(webui->auth_user);
    }
    webui->auth_user = NULL;

    if (webui->auth_pass != NULL) {
        free(webui->auth_pass);
    }
    webui->auth_pass = NULL;

    if (webui->auth_opaque != NULL) {
        free(webui->auth_opaque);
    }
    webui->auth_opaque = NULL;

    if (webui->auth_realm != NULL) {
        free(webui->auth_realm);
    }
    webui->auth_realm = NULL;

    if (webui->resp_image != NULL) {
        free(webui->resp_image);
    }
    webui->resp_image = NULL;

    for (indx = 0; indx<webui->post_sz; indx++) {
        if (webui->post_info[indx].key_nm != NULL) {
            free(webui->post_info[indx].key_nm);
        }
        webui->post_info[indx].key_nm = NULL;
        if (webui->post_info[indx].key_val != NULL) {
            free(webui->post_info[indx].key_val);
        }
        webui->post_info[indx].key_val = NULL;
    }
    if (webui->post_info != NULL) {
        free(webui->post_info);
    }
    webui->post_info = NULL;

    delete webui;

    return;
}

/* Edit the parameters specified in the url sent */
static void webu_parms_edit(struct ctx_webui *webui)
{
    int indx, is_nbr;

    if (webui->uri_camid.length() > 0) {
        is_nbr = true;
        for (indx=0; indx < (int)webui->uri_camid.length(); indx++) {
            if ((webui->uri_camid[indx] > '9') || (webui->uri_camid[indx] < '0')) {
                is_nbr = false;
            }
        }
        if (is_nbr) {
            webui->threadnbr = atoi(webui->uri_camid.c_str());
        } else {
            webui->threadnbr = -1;
        }
    } else {
        webui->threadnbr = -1;
    }

    if (webui->threadnbr < 0) {
        webui->cam = webui->motapp->cam_list[0];
        webui->threadnbr = 0;
    } else {
        indx = 0;
        while (webui->motapp->cam_list[indx] != NULL) {
            if (webui->motapp->cam_list[indx]->camera_id == webui->threadnbr) {
                webui->threadnbr = indx;
                break;
            }
            indx++;
        }
        /* This may be null, in which case we will not answer the request */
        webui->cam = webui->motapp->cam_list[indx];
    }

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "camid: >%s< thread: >%d< cmd1: >%s< cmd2: >%s<"
        , webui->uri_camid.c_str(), webui->threadnbr
        , webui->uri_cmd1.c_str(), webui->uri_cmd2.c_str());


}

/* Extract the camid and cmds from the url */
static int webu_parseurl(struct ctx_webui *webui)
{
    int retcd;
    char *tmpurl;
    size_t  pos_slash1, pos_slash2;

    /* Example:  /camid/cmd1/cmd2   */
    retcd = 0;
    webui->uri_camid = "";
    webui->uri_cmd1 = "";
    webui->uri_cmd2 = "";

    if (webui->url.length() == 0) {
        return -1;
    }

    if (webui->url ==  "/favicon.ico") {
        return -1;
    }

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Sent url: %s"),webui->url.c_str());

    tmpurl = (char*)mymalloc(webui->url.length()+1);
    memcpy(tmpurl, webui->url.c_str(), webui->url.length());

    MHD_http_unescape(tmpurl);

    webui->url.assign(tmpurl);
    free(tmpurl);

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),webui->url.c_str());

    if (webui->url.length() == 1) {
        return 0;
    }

    pos_slash1 = webui->url.find("/", 1);
    if (pos_slash1 != std::string::npos) {
        webui->uri_camid = webui->url.substr(1, pos_slash1 - 1);
    } else {
        webui->uri_camid = webui->url.substr(1);
        return 0;
    }

    pos_slash1++;
    if (pos_slash1 >= webui->url.length()) {
        return 0;
    }

    pos_slash2 = webui->url.find("/", pos_slash1);
    if (pos_slash2 != std::string::npos) {
        webui->uri_cmd1 = webui->url.substr(pos_slash1, pos_slash2 - pos_slash1);
    } else {
        webui->uri_cmd1 = webui->url.substr(pos_slash1);
        return 0;
    }

    pos_slash1 = ++pos_slash2;
    if (pos_slash1 >= webui->url.length()) {
        return 0;
    }

    pos_slash2 = webui->url.find("/", pos_slash1);
    if (pos_slash2 != std::string::npos) {
        webui->uri_cmd2 = webui->url.substr(pos_slash1, pos_slash2 - pos_slash1);
    } else {
        webui->uri_cmd2 = webui->url.substr(pos_slash1);
        return 0;
    }

    return retcd;

}

/* Log the ip of the client connecting*/
static void webu_clientip(struct ctx_webui *webui)
{
    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ip_dst;
    struct sockaddr_in6 *con_socket6;
    struct sockaddr_in *con_socket4;
    int is_ipv6;

    is_ipv6 = false;
    if (webui->motapp->cam_list[0]->conf->webcontrol_ipv6) {
        is_ipv6 = true;
    }

    con_info = MHD_get_connection_info(webui->connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (is_ipv6) {
        con_socket6 = (struct sockaddr_in6 *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET6, &con_socket6->sin6_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            webui->clientip = "Unknown";
        } else {
            webui->clientip.assign(client);
            if (webui->clientip.substr(0, 7) == "::ffff:") {
                webui->clientip = webui->clientip.substr(7);
            }
        }
    } else {
        con_socket4 = (struct sockaddr_in *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET, &con_socket4->sin_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            webui->clientip = "Unknown";
        } else {
            webui->clientip.assign(client);
        }
    }

}

/* Get the hostname */
static void webu_hostname(struct ctx_webui *webui)
{
    const char *hdr;
    std::string hostname;

    hostname = "localhost";

    hdr = MHD_lookup_connection_value(webui->connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_HOST);
    if (hdr != NULL) {
        hostname.assign(hdr);
        if ((hdr[0] == '[') && (hostname.find("]") != std::string::npos)) {
            hostname = hostname.substr(0, hostname.find("]") + 1);
        } else if ((hdr[0] != '[') && (hostname.find(":") != std::string::npos)) {
            hostname = hostname.substr(0, hostname.find(":"));
        }
    }

    if (webui->motapp->cam_list[0]->conf->webcontrol_tls) {
        webui->hostfull = "https";
    } else {
        webui->hostfull = "http";
    }
    webui->hostfull += "://" + hostname + ":" +
        std::to_string(webui->motapp->cam_list[0]->conf->webcontrol_port);

    MOTION_LOG(DBG,TYPE_ALL, NO_ERRNO, _("Full Host:  %s"), webui->hostfull.c_str());

    return;
}

/* Log the failed authentication check */
static void webu_failauth_log(struct ctx_webui *webui)
{
    timespec                            tm_cnct;
    struct ctx_failauth                 failauth;
    std::list<ctx_failauth>::iterator   it;

    MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("Failed authentication from %s"), webui->clientip.c_str());

    clock_gettime(CLOCK_REALTIME, &tm_cnct);

    it = webui->motapp->webcontrol_failauth.begin();
    while (it != webui->motapp->webcontrol_failauth.end()) {
        if (it->clientip == webui->clientip) {
            it->attempt_nbr++;
            it->attempt_time.tv_sec =tm_cnct.tv_sec;
            return;
        }
        it++;
    }

    failauth.clientip = webui->clientip;
    failauth.attempt_nbr = 1;
    failauth.attempt_time = tm_cnct;

    webui->motapp->webcontrol_failauth.push_back(failauth);

    return;

}

/* Log the failed authentication check */
static void webu_failauth_reset(struct ctx_webui *webui)
{
    timespec                            tm_cnct;
    std::list<ctx_failauth>::iterator   it;

    clock_gettime(CLOCK_REALTIME, &tm_cnct);
    it = webui->motapp->webcontrol_failauth.begin();
    while (it != webui->motapp->webcontrol_failauth.end()) {
        if ((it->clientip == webui->clientip) ||
            ((tm_cnct.tv_sec - it->attempt_time.tv_sec) >= 600)) {
            it = webui->motapp->webcontrol_failauth.erase(it);
        } else {
            it++;
        }
    }

    return;

}

/* Check for ips with excessive failed authentication attempts */
static mhdrslt webu_failauth_check(struct ctx_webui *webui)
{
    timespec    tm_cnct;
    std::list<ctx_failauth>::iterator it;

    if (webui->motapp->webcontrol_failauth.size() == 0) {
        return MHD_YES;
    }

    clock_gettime(CLOCK_REALTIME, &tm_cnct);
    it = webui->motapp->webcontrol_failauth.begin();
    while (it != webui->motapp->webcontrol_failauth.end()) {
        if ((it->clientip == webui->clientip) &&
            ((tm_cnct.tv_sec - it->attempt_time.tv_sec) < 600) &&
            (it->attempt_nbr > 5)) {
MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
            ,_("ignoring attempt from %s"), webui->clientip.c_str());

            it->attempt_time = tm_cnct;
            return MHD_NO;
        } else if ((tm_cnct.tv_sec - it->attempt_time.tv_sec) >= 600) {
            it = webui->motapp->webcontrol_failauth.erase(it);
        } else {
            it++;
        }
    }

    return MHD_YES;

}

/* Create a authorization denied response to user*/
static mhdrslt webu_mhd_digest_fail(struct ctx_webui *webui,int signal_stale)
{
    struct MHD_Response *response;
    mhdrslt retcd;

    webui->authenticated = false;

    webui->resp_page = "<html><head><title>Access denied</title>"
        "</head><body>Access denied</body></html>";

    response = MHD_create_response_from_buffer(webui->resp_page.length()
        ,(void *)webui->resp_page.c_str(), MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_auth_fail_response(webui->connection, webui->auth_realm
        ,webui->auth_opaque, response
        ,(signal_stale == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);

    MHD_destroy_response(response);

    return retcd;
}

/* Perform digest authentication */
static mhdrslt webu_mhd_digest(struct ctx_webui *webui)
{
    /* This function gets called a couple of
     * times by MHD during the authentication process.
     */
    int retcd;
    char *user;

    /*Get username or prompt for a user/pass */
    user = MHD_digest_auth_get_username(webui->connection);
    if (user == NULL) {
        return webu_mhd_digest_fail(webui, MHD_NO);
    }

    /* Check for valid user name */
    if (mystrne(user, webui->auth_user)) {
        webu_failauth_log(webui);
        if (user != NULL) {
            free(user);
        }
        user = NULL;
        return webu_mhd_digest_fail(webui, MHD_NO);
    }
    if (user != NULL) {
        free(user);
    }
    user = NULL;

    /* Check the password as well*/
    retcd = MHD_digest_auth_check(webui->connection, webui->auth_realm
        , webui->auth_user, webui->auth_pass, 300);

    if (retcd == MHD_NO) {
        webu_failauth_log(webui);
    }

    if ( (retcd == MHD_INVALID_NONCE) || (retcd == MHD_NO) )  {
        return webu_mhd_digest_fail(webui, retcd);
    }

    webui->authenticated = true;
    return MHD_YES;

}

/* Create a authorization denied response to user*/
static mhdrslt webu_mhd_basic_fail(struct ctx_webui *webui)
{
    struct MHD_Response *response;
    int retcd;

    webui->authenticated = false;

    webui->resp_page = "<html><head><title>Access denied</title>"
        "</head><body>Access denied</body></html>";

    response = MHD_create_response_from_buffer(webui->resp_page.length()
        ,(void *)webui->resp_page.c_str(), MHD_RESPMEM_PERSISTENT);

    if (response == NULL) {
        return MHD_NO;
    }

    retcd = MHD_queue_basic_auth_fail_response (webui->connection, webui->auth_realm, response);

    MHD_destroy_response(response);

    if (retcd == MHD_YES) {
        return MHD_YES;
    } else {
        return MHD_NO;
    }

}

/* Perform Basic Authentication.  */
static mhdrslt webu_mhd_basic(struct ctx_webui *webui)
{
    char *user, *pass;

    pass = NULL;
    user = NULL;

    user = MHD_basic_auth_get_username_password (webui->connection, &pass);
    if ((user == NULL) || (pass == NULL)) {
        if (user != NULL) {
            free(user);
        }
        user = NULL;
        if (pass != NULL) {
            free(pass);
        }
        pass = NULL;
        return webu_mhd_basic_fail(webui);
    }

    if ((mystrne(user, webui->auth_user)) || (mystrne(pass, webui->auth_pass))) {
        webu_failauth_log(webui);
        if (user != NULL) {
            free(user);
        }
        user = NULL;
        if (pass != NULL) {
            free(pass);
        }
        pass = NULL;
        return webu_mhd_basic_fail(webui);
    }

    if (user != NULL) {
        free(user);
    }
    user = NULL;
    if (pass != NULL) {
        free(pass);
    }
    pass = NULL;

    webui->authenticated = true;

    return MHD_YES;

}

/* Parse apart the user:pass provided*/
static void webu_mhd_auth_parse(struct ctx_webui *webui)
{
    int auth_len;
    char *col_pos;

    if (webui->auth_user != NULL) {
        free(webui->auth_user);
    }
    webui->auth_user = NULL;
    if (webui->auth_pass != NULL) {
        free(webui->auth_pass);
    }
    webui->auth_pass = NULL;

    auth_len = webui->motapp->cam_list[0]->conf->webcontrol_authentication.length();
    col_pos =(char*) strstr(webui->motapp->cam_list[0]->conf->webcontrol_authentication.c_str() ,":");
    if (col_pos == NULL) {
        webui->auth_user = (char*)mymalloc(auth_len+1);
        webui->auth_pass = (char*)mymalloc(2);
        snprintf(webui->auth_user, auth_len + 1, "%s"
            ,webui->motapp->cam_list[0]->conf->webcontrol_authentication.c_str());
        snprintf(webui->auth_pass, 2, "%s","");
    } else {
        webui->auth_user = (char*)mymalloc(auth_len - strlen(col_pos) + 1);
        webui->auth_pass =(char*)mymalloc(strlen(col_pos));
        snprintf(webui->auth_user, auth_len - strlen(col_pos) + 1, "%s"
            ,webui->motapp->cam_list[0]->conf->webcontrol_authentication.c_str());
        snprintf(webui->auth_pass, strlen(col_pos), "%s", col_pos + 1);
    }

}

/* Initialize for authorization */
static mhdrslt webu_mhd_auth(struct ctx_webui *webui)
{
    unsigned int rand1,rand2;

    srand(time(NULL));
    rand1 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    rand2 = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(webui->auth_opaque, WEBUI_LEN_PARM, "%08x%08x", rand1, rand2);

    snprintf(webui->auth_realm, WEBUI_LEN_PARM, "%s","Motion");

    if (webui->motapp->cam_list[0]->conf->webcontrol_authentication == "") {
        webui->authenticated = true;
        if (webui->motapp->cam_list[0]->conf->webcontrol_auth_method != "none") {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No webcontrol user:pass provided"));
        }
        return MHD_YES;
    }

    if (webui->auth_user == NULL) {
        webu_mhd_auth_parse(webui);
    }

    if (webui->motapp->cam_list[0]->conf->webcontrol_auth_method == "basic") {
        return webu_mhd_basic(webui);
    } else if (webui->motapp->cam_list[0]->conf->webcontrol_auth_method == "digest") {
        return webu_mhd_digest(webui);
    }


    webui->authenticated = true;
    return MHD_YES;

}

/* Send the response that we created back to the user.  */
static mhdrslt webu_mhd_send(struct ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    response = MHD_create_response_from_buffer(webui->resp_page.length()
        ,(void *)webui->resp_page.c_str(), MHD_RESPMEM_PERSISTENT);
    if (!response) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->cam != NULL) {
        if (webui->motapp->webcontrol_headers->params_count > 0) {
            for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
                MHD_add_response_header (response
                    , webui->motapp->webcontrol_headers->params_array[indx].param_name
                    , webui->motapp->webcontrol_headers->params_array[indx].param_value
                );
            }
        }
        if (webui->resp_type == WEBUI_RESP_TEXT) {
            MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
        } else if (webui->resp_type == WEBUI_RESP_JSON) {
            MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json;");
        } else {
            MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
        }
    }

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}


/* Process the post data command */
static mhdrslt webu_answer_post(struct ctx_webui *webui)
{
    mhdrslt retcd;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"processing post");

    pthread_mutex_lock(&webui->motapp->mutex_post);
        webu_post_main(webui);
    pthread_mutex_unlock(&webui->motapp->mutex_post);

    if (webui->motapp->cam_list[0]->conf->webcontrol_interface == "user") {
        webu_html_user(webui);
    } else {
        webu_html_page(webui);
    }

    retcd = webu_mhd_send(webui);
    if (retcd == MHD_NO) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"send post page failed");
    }

    return MHD_YES;

}

/*Append more data on to an existing entry in the post info structure */
static void webu_iterate_post_append(struct ctx_webui *webui, int indx
        , const char *data, size_t datasz)
{

    webui->post_info[indx].key_val = (char*)realloc(
        webui->post_info[indx].key_val
        , webui->post_info[indx].key_sz + datasz + 1);

    memset(webui->post_info[indx].key_val +
        webui->post_info[indx].key_sz, 0, datasz + 1);

    if (datasz > 0) {
        memcpy(webui->post_info[indx].key_val +
            webui->post_info[indx].key_sz, data, datasz);
    }

    webui->post_info[indx].key_sz += datasz;

}

/*Create new entry in the post info structure */
static void webu_iterate_post_new(struct ctx_webui *webui, const char *key
        , const char *data, size_t datasz)
{
    int retcd;

    webui->post_sz++;
    if (webui->post_sz == 1) {
        webui->post_info = (ctx_key *)malloc(sizeof(struct ctx_key));
    } else {
        webui->post_info = (ctx_key *)realloc(webui->post_info
            , webui->post_sz * sizeof(struct ctx_key));
    }

    webui->post_info[webui->post_sz-1].key_nm = (char*)malloc(strlen(key)+1);
    retcd = snprintf(webui->post_info[webui->post_sz-1].key_nm, strlen(key)+1, "%s", key);

    webui->post_info[webui->post_sz-1].key_val = (char*)malloc(datasz+1);
    memset(webui->post_info[webui->post_sz-1].key_val,0,datasz+1);
    if (datasz > 0) {
        memcpy(webui->post_info[webui->post_sz-1].key_val, data, datasz);
    }

    webui->post_info[webui->post_sz-1].key_sz = datasz;

    if (retcd < 0) {
        printf("Error processing post data\n");
    }

}

static mhdrslt webu_iterate_post (void *ptr, enum MHD_ValueKind kind
        , const char *key, const char *filename, const char *content_type
        , const char *transfer_encoding, const char *data, uint64_t off, size_t datasz)
{
    (void) kind;
    (void) filename;
    (void) content_type;
    (void) transfer_encoding;
    (void) off;

    struct ctx_webui *webui = (ctx_webui *)ptr;
    int indx;

    for (indx=0; indx < webui->post_sz; indx++) {
        if (mystreq(webui->post_info[indx].key_nm, key)) {
            break;
        }
    }
    if (indx < webui->post_sz) {
        webu_iterate_post_append(webui, indx, data, datasz);
    } else {
        webu_iterate_post_new(webui, key, data, datasz);
    }

    return MHD_YES;
}

/* Answer the get request from the user */
static mhdrslt webu_answer_get(struct ctx_webui *webui)
{
    int retcd;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,"processing get");

    retcd = MHD_NO;
    if ((webui->uri_cmd1 == "mjpg") ||
        (webui->uri_cmd1 == "static")) {

        retcd = webu_stream_main(webui);
        if (retcd == MHD_NO) {
            webu_html_badreq(webui);
            retcd = webu_mhd_send(webui);
        }

    } else if ((webui->uri_camid == "config.json") ||
               (webui->uri_cmd1 == "config.json")) {

        pthread_mutex_lock(&webui->motapp->mutex_post);
            webu_json_config(webui);
        pthread_mutex_unlock(&webui->motapp->mutex_post);

        retcd = webu_mhd_send(webui);
        if (retcd == MHD_NO) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed."));
        }

    } else {
        pthread_mutex_lock(&webui->motapp->mutex_post);
            if (webui->motapp->cam_list[0]->conf->webcontrol_interface == "user") {
                webu_html_user(webui);
            } else {
                webu_html_page(webui);
            }
        pthread_mutex_unlock(&webui->motapp->mutex_post);

        retcd = webu_mhd_send(webui);
        if (retcd == MHD_NO) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed."));
        }
    }

    return retcd;

}


/* Answer the connection request for the webcontrol*/
static mhdrslt webu_answer(void *cls, struct MHD_Connection *connection, const char *url
        , const char *method, const char *version, const char *upload_data, size_t *upload_data_size
        , void **ptr)
{
    (void)cls;
    (void)url;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;

    mhdrslt retcd;
    ctx_webui *webui =(struct ctx_webui *) *ptr;

    webui->cnct_type = WEBUI_CNCT_CONTROL;
    webui->connection = connection;

    /* Throw bad URLS back to user*/
    if ((webui->cam ==  NULL) || (webui->url.length() == 0)) {
        webu_html_badreq(webui);
        retcd = webu_mhd_send(webui);
        return retcd;
    }

    if (webui->cam->finish_cam) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Shutting down camera"));
        return MHD_NO;
    }

    if (webui->clientip.length() == 0) {
        webu_clientip(webui);
    }

    if (webu_failauth_check(webui) == MHD_NO) {
        return MHD_NO;
    }

    webu_hostname(webui);

    if (!webui->authenticated) {
        retcd = webu_mhd_auth(webui);
        if (!webui->authenticated) {
            return retcd;
        }
    }

    webu_failauth_reset(webui);

    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),webui->clientip.c_str());

    if (webui->mhd_first) {
        webui->mhd_first = false;
        if (mystreq(method,"POST")) {
            webui->post_processor = MHD_create_post_processor (webui->connection
                , WEBUI_POST_BFRSZ, webu_iterate_post, (void *)webui);
            if (webui->post_processor == NULL) {
                return MHD_NO;
            }
            webui->cnct_method = WEBUI_METHOD_POST;
        } else {
            webui->cnct_method = WEBUI_METHOD_GET;
        }

        return MHD_YES;
    }

    if (mystreq(method,"POST")) {
        if (*upload_data_size != 0) {
            retcd = MHD_post_process (webui->post_processor, upload_data, *upload_data_size);
            *upload_data_size = 0;
        } else {
            retcd = webu_answer_post(webui);
        }
    } else {
        retcd = webu_answer_get(webui);
    }

    return retcd;

}

/* Initialize the MHD answer */
static void *webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection)
{
    struct ctx_motapp *motapp = (struct ctx_motapp *)cls;
    struct ctx_webui *webui;
    int retcd;

    (void)connection;

    mythreadname_set("wc", 0, NULL);

    webui = new ctx_webui;

    webu_context_init(motapp, webui);

    webui->mhd_first = true;

    webui->url.assign(uri);

    retcd = webu_parseurl(webui);
    if (retcd != 0) {
        webui->uri_camid = "";
        webui->uri_cmd1 = "";
        webui->url = "";
    }

    webu_parms_edit(webui);

    return webui;
}

/* Clean up our variables when the MHD connection closes */
static void webu_mhd_deinit(void *cls, struct MHD_Connection *connection
        , void **con_cls, enum MHD_RequestTerminationCode toe)
{
    struct ctx_webui *webui =(struct ctx_webui *) *con_cls;

    (void)connection;
    (void)cls;
    (void)toe;
    /* Sometimes we can shutdown after we have initiated a connection but yet
     * before the connection counter has been incremented.  So we check the
     * connection counter before we decrement
     */
    if (webui->cnct_type == WEBUI_CNCT_FULL ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            if (webui->cam->stream.norm.cnct_count > 0) {
                webui->cam->stream.norm.cnct_count--;
            }
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SUB ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            if (webui->cam->stream.sub.cnct_count > 0) {
                webui->cam->stream.sub.cnct_count--;
            }
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_MOTION ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            if (webui->cam->stream.motion.cnct_count > 0) {
                webui->cam->stream.motion.cnct_count--;
            }
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SOURCE ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            if (webui->cam->stream.source.cnct_count > 0) {
                webui->cam->stream.source.cnct_count--;
            }
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    } else if (webui->cnct_type == WEBUI_CNCT_SECONDARY ) {
        pthread_mutex_lock(&webui->cam->stream.mutex);
            if (webui->cam->stream.secondary.cnct_count > 0) {
                webui->cam->stream.secondary.cnct_count--;
            }
        pthread_mutex_unlock(&webui->cam->stream.mutex);

    }

    if (webui != NULL) {
        if (webui->cnct_method == WEBUI_METHOD_POST) {
            MHD_destroy_post_processor (webui->post_processor);
        }
        webu_context_free(webui);
    }

    return;
}

/* Validate that the MHD version installed can process basic authentication */
static void webu_mhd_features_basic(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: available"));
        } else {
            if (mhdst->motapp->cam_list[0]->conf->webcontrol_auth_method == "basic") {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                mhdst->motapp->cam_list[0]->conf->webcontrol_auth_method = "none";
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process digest authentication */
static void webu_mhd_features_digest(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: available"));
        } else {
            if (mhdst->motapp->cam_list[0]->conf->webcontrol_auth_method == "digest") {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                mhdst->motapp->cam_list[0]->conf->webcontrol_auth_method = "none";
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process IPV6 */
static void webu_mhd_features_ipv6(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        if (mhdst->ipv6) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old ipv6 disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("IPV6: available"));
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("IPV6: disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #endif
}

/* Validate that the MHD version installed can process tls */
static void webu_mhd_features_tls(struct mhdstart_ctx *mhdst)
{
    #if MHD_VERSION < 0x00094400
        if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
        if (retcd == MHD_YES) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: available"));
        } else {
            if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
            } else {
                MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
            }
        }
    #endif
}

/* Validate the features that MHD can support */
static void webu_mhd_features(struct mhdstart_ctx *mhdst)
{
    webu_mhd_features_basic(mhdst);

    webu_mhd_features_digest(mhdst);

    webu_mhd_features_ipv6(mhdst);

    webu_mhd_features_tls(mhdst);

}
/* Load a either the key or cert file for MHD*/
static std::string webu_mhd_loadfile(std::string fname)
{
    /* This needs conversion to c++ stream */
    FILE        *infile;
    size_t      file_size, read_size;
    char        *file_char;
    std::string filestr;

    filestr = "";
    if (fname != "") {
        infile = fopen(fname.c_str() , "rb");
        if (infile != NULL) {
            fseek(infile, 0, SEEK_END);
            file_size = ftell(infile);
            if (file_size > 0 ) {
                file_char = (char*)mymalloc(file_size +1);
                fseek(infile, 0, SEEK_SET);
                read_size = fread(file_char, file_size, 1, infile);
                if (read_size > 0 ) {
                    file_char[file_size] = 0;
                    filestr.assign(file_char, file_size);
                } else {
                    MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                        ,_("Error reading file for SSL/TLS support."));
                }
                free(file_char);
            }
            fclose(infile);
        }
    }
    return filestr;

}

/* Validate that we have the files needed for tls*/
static void webu_mhd_checktls(struct mhdstart_ctx *mhdst)
{

    if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {
        if ((mhdst->motapp->cam_list[0]->conf->webcontrol_cert == "") || (mhdst->tls_cert == "")) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
            mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
        }
        if ((mhdst->motapp->cam_list[0]->conf->webcontrol_key == "") || (mhdst->tls_key == "")) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
            mhdst->motapp->cam_list[0]->conf->webcontrol_tls = 0;
        }
    }

}

/* Set the initialization function for MHD to call upon getting a connection */
static void webu_mhd_opts_init(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp;
    mhdst->mhd_opt_nbr++;
}

/* Set the MHD option on the function to call when the connection closes */
static void webu_mhd_opts_deinit(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

/* Set the MHD option on acceptable connections */
static void webu_mhd_opts_localhost(struct mhdstart_ctx *mhdst)
{
    if (mhdst->motapp->cam_list[0]->conf->webcontrol_localhost) {
        if (mhdst->ipv6) {
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons(mhdst->motapp->cam_list[0]->conf->webcontrol_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;

        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons(mhdst->motapp->cam_list[0]->conf->webcontrol_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    }

}

/* Set the mhd digest options */
static void webu_mhd_opts_digest(struct mhdstart_ctx *mhdst)
{

    if (mhdst->motapp->cam_list[0]->conf->webcontrol_auth_method == "digest") {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(mhdst->motapp->webcontrol_digest_rand);
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = mhdst->motapp->webcontrol_digest_rand;
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NONCE_NC_SIZE;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 300;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_CONNECTION_TIMEOUT;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (unsigned int) 120;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
        mhdst->mhd_opt_nbr++;
    }

}

/* Set the MHD options needed when we want TLS connections */
static void webu_mhd_opts_tls(struct mhdstart_ctx *mhdst)
{
    if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_CERT;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (void *)mhdst->tls_cert.c_str();
        mhdst->mhd_opt_nbr++;

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_KEY;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (void *)mhdst->tls_key.c_str();
        mhdst->mhd_opt_nbr++;
    }

}

/* Set all the MHD options based upon the configuration parameters*/
static void webu_mhd_opts(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_opt_nbr = 0;

    webu_mhd_checktls(mhdst);

    webu_mhd_opts_deinit(mhdst);

    webu_mhd_opts_init(mhdst);

    webu_mhd_opts_localhost(mhdst);

    webu_mhd_opts_digest(mhdst);

    webu_mhd_opts_tls(mhdst);

    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_END;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

/* Set the mhd start up flags */
static void webu_mhd_flags(struct mhdstart_ctx *mhdst)
{
    mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION;

    if (mhdst->ipv6) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_DUAL_STACK;
    }

    if (mhdst->motapp->cam_list[0]->conf->webcontrol_tls) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    }

}

/* Start the webcontrol */
static void webu_init_webcontrol(struct ctx_motapp *motapp)
{
    struct mhdstart_ctx mhdst;
    unsigned int randnbr;

    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , _("Starting webcontrol on port %d")
        , motapp->cam_list[0]->conf->webcontrol_port);

    motapp->webcontrol_headers = (ctx_params*)mymalloc(sizeof(struct ctx_params));
    motapp->webcontrol_headers->update_params = true;
    util_parms_parse(motapp->webcontrol_headers, motapp->cam_list[0]->conf->webcontrol_headers);

    mhdst.tls_cert = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_cert);
    mhdst.tls_key  = webu_mhd_loadfile(motapp->cam_list[0]->conf->webcontrol_key);
    mhdst.motapp = motapp;
    mhdst.ipv6 = motapp->cam_list[0]->conf->webcontrol_ipv6;

    /* Set the rand number for webcontrol digest if needed */
    srand(time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(motapp->webcontrol_digest_rand
        ,sizeof(motapp->webcontrol_digest_rand),"%d",randnbr);

    mhdst.mhd_ops =(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
    webu_mhd_features(&mhdst);
    webu_mhd_opts(&mhdst);
    webu_mhd_flags(&mhdst);

    motapp->webcontrol_daemon = MHD_start_daemon (
        mhdst.mhd_flags
        , motapp->cam_list[0]->conf->webcontrol_port
        , NULL, NULL
        , &webu_answer, motapp->cam_list
        , MHD_OPTION_ARRAY, mhdst.mhd_ops
        , MHD_OPTION_END);

    free(mhdst.mhd_ops);
    if (motapp->webcontrol_daemon == NULL) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD"));
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started webcontrol on port %d")
            ,motapp->cam_list[0]->conf->webcontrol_port);
    }

    return;
}

/* Shut down the webcontrol */
void webu_deinit(struct ctx_motapp *motapp)
{

    if (motapp->webcontrol_daemon != NULL) {
        motapp->webcontrol_finish = true;
        MHD_stop_daemon (motapp->webcontrol_daemon);
    }

    util_parms_free(motapp->webcontrol_headers);
    if (motapp->webcontrol_headers != NULL) {
        free(motapp->webcontrol_headers);
    }
    motapp->webcontrol_headers = NULL;

}

/* Start the webcontrol and streams */
void webu_init(struct ctx_motapp *motapp)
{
    struct sigaction act;

    /* We need to block some signals otherwise MHD will not function correctly. */
    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    motapp->webcontrol_daemon = NULL;
    motapp->webcontrol_finish = false;

     /* Start the webcontrol */
    if (motapp->cam_list[0]->conf->webcontrol_port != 0 ) {
        webu_init_webcontrol(motapp);
    }

    return;

}
