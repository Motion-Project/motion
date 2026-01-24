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
 * webu_ans.cpp - HTTP Response Management
 *
 * This module manages HTTP response construction and delivery for the
 * web server, handling response headers, status codes, content types,
 * and streaming responses via libmicrohttpd callbacks.
 *
 */

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "allcam.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "webu_json.hpp"
#include "webu_file.hpp"
#include "video_v4l2.hpp"

static mhdrslt webua_connection_values (void *cls
    , enum MHD_ValueKind kind, const char *src_key, const char *src_val)
{
    (void) kind;
    std::string parm_val;
    cls_webu_ans *webua =(cls_webu_ans *) cls;

    if (mystreq(src_key,"Accept-Encoding")) {
        parm_val = src_val;
        if (parm_val.find("gzip") != std::string::npos) {
            webua->gzip_encode = true;
        }
    }

  return MHD_YES;
}

int cls_webu_ans::check_tls()
{
    struct stat file_attrib;
    char        file_time[50];
    std::string file_chk;

    if (webu->info_tls == "") {
        return 0;
    }

    file_chk = "";
    if (app->cfg->webcontrol_cert != "") {
        memset(&file_attrib, 0, sizeof(struct stat));
        if (stat(app->cfg->webcontrol_cert.c_str(), &file_attrib) == 0) {
            strftime(file_time, 50, "%Y%m%d-%H%M%S-"
                , localtime(&file_attrib.st_mtime));
            file_chk.append(file_time);
            file_chk.append(std::to_string(file_attrib.st_size));
        } else {
            file_chk += "FileError";
        }
    }

    if (app->cfg->webcontrol_key != "") {
        memset(&file_attrib, 0, sizeof(struct stat));
        if (stat(app->cfg->webcontrol_key.c_str(), &file_attrib) == 0) {
            strftime(file_time, 50, "%Y%m%d-%H%M%S-"
                , localtime(&file_attrib.st_mtime));
            file_chk.append(file_time);
            file_chk.append(std::to_string(file_attrib.st_size));
        } else {
            file_chk += "FileError";
        }
    }

    if (file_chk != webu->info_tls) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
            , _("Webcontrol files have changed.  Restarting webcontrol"));
        webu->restart = true;
        return -1;
    }

    return 0;

}

/* Extract the camid and cmds from the url */
int cls_webu_ans::parseurl()
{
    char *tmpurl;
    size_t  pos_slash1, pos_slash2, baselen;

    /* Example:  /camid/cmd1/cmd2/cmd3/cmd4   */
    uri_cmd0 = "";
    uri_cmd1 = "";
    uri_cmd2 = "";
    uri_cmd3 = "";
    uri_cmd4 = "";

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Sent url: %s"),url.c_str());

    tmpurl = (char*)mymalloc(url.length()+1);
    memcpy(tmpurl, url.c_str(), url.length());

    MHD_http_unescape(tmpurl);

    url.assign(tmpurl);
    free(tmpurl);

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("Decoded url: %s"),url.c_str());

    /* Strip query string from URL path before parsing
     * Query parameters are still accessible via MHD_lookup_connection_value() */
    size_t query_pos = url.find('?');
    if (query_pos != std::string::npos) {
        url = url.substr(0, query_pos);
    }

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

    pos_slash1 = url.find("/", baselen);
    if (pos_slash1 != std::string::npos) {
        uri_cmd0 = url.substr(baselen, pos_slash1-baselen);
    } else {
        uri_cmd0 = url.substr(baselen);
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

        pos_slash2 = url.find("/", pos_slash1);
        if (pos_slash2 != std::string::npos) {
            uri_cmd3 = url.substr(pos_slash1, pos_slash2 - pos_slash1);
            pos_slash1 = ++pos_slash2;
            if (pos_slash1 < url.length()) {
                uri_cmd4 = url.substr(pos_slash1);
            }
        } else {
            uri_cmd3 = url.substr(pos_slash1);
        }
    }
    return 0;

}
/* Edit the parameters specified in the url sent */
void cls_webu_ans::parms_edit()
{
    int indx, is_nbr;

    if (parseurl() != 0) {
        uri_cmd0 = "";
        uri_cmd1 = "";
        uri_cmd2 = "";
        uri_cmd3 = "";
        uri_cmd4 = "";
        url = "";
    }

    if (uri_cmd0.length() > 0) {
        is_nbr = true;
        for (indx=0; indx < (int)uri_cmd0.length(); indx++) {
            if ((uri_cmd0[(uint)indx] > '9') || (uri_cmd0[(uint)indx] < '0')) {
                is_nbr = false;
            }
        }
        if (is_nbr) {
            device_id = atoi(uri_cmd0.c_str());
        }
    } else if (uri_cmd0 == "") {
        device_id = 0;
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->cfg->device_id == device_id) {
            camindx = indx;
            cam = app->cam_list[indx];
        }
    }

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "cmd0: >%s< cmd1: >%s< cmd2: >%s< cmd3: >%s< cmd4: >%s< camindx: >%d< "
        , uri_cmd0.c_str(), uri_cmd1.c_str()
        , uri_cmd2.c_str(), uri_cmd3.c_str()
        , uri_cmd4.c_str(), camindx );

}

/**
 * Check if an IP address is in the trusted proxies list
 * Supports comma-separated list of IPs or CIDR ranges
 */
static bool is_trusted_proxy(const std::string &ip, const std::string &trusted_list)
{
    if (trusted_list.empty()) {
        return false;
    }

    /* Parse comma-separated list of trusted IPs */
    std::string list = trusted_list;
    size_t pos;
    while ((pos = list.find(',')) != std::string::npos) {
        std::string trusted = list.substr(0, pos);
        /* Trim whitespace */
        while (!trusted.empty() && trusted[0] == ' ') trusted = trusted.substr(1);
        while (!trusted.empty() && trusted.back() == ' ') trusted.pop_back();
        if (trusted == ip) {
            return true;
        }
        list = list.substr(pos + 1);
    }
    /* Check last/only entry */
    while (!list.empty() && list[0] == ' ') list = list.substr(1);
    while (!list.empty() && list.back() == ' ') list.pop_back();
    return (list == ip);
}

/**
 * Extract the first IP from X-Forwarded-For header
 * Format: "client, proxy1, proxy2, ..."
 */
static std::string parse_xff_first_ip(const char *xff)
{
    if (xff == nullptr || xff[0] == '\0') {
        return "";
    }

    std::string header(xff);
    size_t comma = header.find(',');
    std::string first_ip;
    if (comma != std::string::npos) {
        first_ip = header.substr(0, comma);
    } else {
        first_ip = header;
    }

    /* Trim whitespace */
    while (!first_ip.empty() && first_ip[0] == ' ') first_ip = first_ip.substr(1);
    while (!first_ip.empty() && first_ip.back() == ' ') first_ip.pop_back();

    return first_ip;
}

/* Get the client IP, with X-Forwarded-For support for reverse proxies */
void cls_webu_ans::clientip_get()
{
    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ip_dst;
    struct sockaddr_in6 *con_socket6;
    struct sockaddr_in *con_socket4;
    int is_ipv6;
    std::string direct_ip;

    is_ipv6 = false;
    if (app->cfg->webcontrol_ipv6) {
        is_ipv6 = true;
    }

    /* First, get the direct connection IP */
    con_info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (is_ipv6) {
        con_socket6 = (struct sockaddr_in6 *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET6, &con_socket6->sin6_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            direct_ip = "Unknown";
        } else {
            direct_ip.assign(client);
            if (direct_ip.substr(0, 7) == "::ffff:") {
                direct_ip = direct_ip.substr(7);
            }
        }
    } else {
        con_socket4 = (struct sockaddr_in *)con_info->client_addr;
        ip_dst = inet_ntop(AF_INET, &con_socket4->sin_addr, client, WEBUI_LEN_URLI);
        if (ip_dst == NULL) {
            direct_ip = "Unknown";
        } else {
            direct_ip.assign(client);
        }
    }

    /* Check if connection is from a trusted proxy */
    if (is_trusted_proxy(direct_ip, app->cfg->webcontrol_trusted_proxies)) {
        /* Get client IP from X-Forwarded-For header */
        const char *xff = MHD_lookup_connection_value(connection,
            MHD_HEADER_KIND, "X-Forwarded-For");
        if (xff != nullptr) {
            std::string real_ip = parse_xff_first_ip(xff);
            if (!real_ip.empty()) {
                MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO,
                    _("Trusted proxy %s forwarding for %s"),
                    direct_ip.c_str(), real_ip.c_str());
                clientip = real_ip;
                return;
            }
        }
    }

    clientip = direct_ip;
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

    MOTION_LOG(DBG,TYPE_ALL, NO_ERRNO, _("Full Host:  %s"), hostfull.c_str());

    return;
}

/* Log the failed authentication check
 * Tracks by (IP + username) combination to prevent distributed brute-force attacks
 * where attacker uses multiple IPs to guess usernames
 */
void cls_webu_ans::failauth_log(bool userid_fail, const std::string &username)
{
    timespec            tm_cnct;
    ctx_webu_clients    clients;
    std::list<ctx_webu_clients>::iterator   it;

    if (username.empty()) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
                ,_("Failed authentication from %s"), clientip.c_str());
    } else {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO
                ,_("Failed authentication from %s for user '%s'"), clientip.c_str(), username.c_str());
    }

    clock_gettime(CLOCK_MONOTONIC, &tm_cnct);

    /* Track by (IP + username) combination for stronger brute-force protection */
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if ((it->clientip == clientip) && (it->username == username)) {
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
    clients.username = username;
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
    std::string current_user;

    /* Get current authenticated username for tracking */
    if (auth_user != nullptr) {
        current_user = std::string(auth_user);
    }

    clock_gettime(CLOCK_MONOTONIC, &tm_cnct);

    /* SECURITY: Clean out stale entries (TTL-based cleanup)
     * This prevents memory exhaustion from attackers creating many entries
     */
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        /* Use configurable lock_minutes or fallback to WEBUI_CLIENT_TTL */
        int ttl = (app->cfg->webcontrol_lock_minutes > 0) ?
            (app->cfg->webcontrol_lock_minutes * 60) : WEBUI_CLIENT_TTL;
        if ((tm_cnct.tv_sec - it->conn_time.tv_sec) >= ttl) {
            it = webu->wb_clients.erase(it);
        } else {
            it++;
        }
    }

    /* SECURITY: Enforce bounded client list to prevent memory exhaustion
     * If at capacity, remove oldest entries first
     */
    while (webu->wb_clients.size() >= WEBUI_MAX_CLIENTS) {
        /* Find and remove oldest entry */
        auto oldest = webu->wb_clients.begin();
        for (it = webu->wb_clients.begin(); it != webu->wb_clients.end(); it++) {
            if (it->conn_time.tv_sec < oldest->conn_time.tv_sec) {
                oldest = it;
            }
        }
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
            _("Client tracking at capacity (%d), removing oldest entry: %s"),
            WEBUI_MAX_CLIENTS, oldest->clientip.c_str());
        webu->wb_clients.erase(oldest);
    }

    /* When this function is called, we know that we are authenticated
     * so we reset the info and as needed print a message that the
     * ip is connected. Track by (IP + username) combination.
     */
    it = webu->wb_clients.begin();
    while (it != webu->wb_clients.end()) {
        if ((it->clientip == clientip) && (it->username == current_user)) {
            if (it->authenticated == false) {
                MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),clientip.c_str());
            }
            it->authenticated = true;
            it->conn_nbr = 1;
            it->userid_fail_nbr = 0;
            it->conn_time.tv_sec = tm_cnct.tv_sec;
            return;
        }
        it++;
    }

    /* The (ip, username) combination was not already in our list. */
    clients.clientip = clientip;
    clients.username = current_user;
    clients.conn_nbr = 1;
    clients.userid_fail_nbr = 0;
    clients.conn_time = tm_cnct;
    clients.authenticated = true;
    webu->wb_clients.push_back(clients);

    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, _("Connection from: %s"),clientip.c_str());

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
            MOTION_LOG(EMG, TYPE_STREAM, NO_ERRNO
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
    int retcd = MHD_NO;
    char *user;
    bool is_admin = false;
    bool is_user = false;

    /*Get username or prompt for a user/pass */
    user = MHD_digest_auth_get_username(connection);
    if (user == NULL) {
        return mhd_digest_fail(MHD_NO);
    }

    /* Check which credential set to use */
    if (mystreq(user, auth_user)) {
        is_admin = true;
    } else if (user_auth_user != nullptr && mystreq(user, user_auth_user)) {
        is_user = true;
    } else {
        /* Unknown username */
        failauth_log(true, user ? std::string(user) : "");
        myfree(user);
        return mhd_digest_fail(MHD_NO);
    }

    std::string attempted_user(user);  /* Save for logging before freeing */
    myfree(user);

    /* Check the password based on role */
    if (is_admin) {
        if (auth_is_ha1) {
            retcd = MHD_digest_auth_check2(connection, auth_realm
                , auth_user, auth_pass, 300, MHD_DIGEST_ALG_MD5);
        } else {
            retcd = MHD_digest_auth_check(connection, auth_realm
                , auth_user, auth_pass, 300);
        }
        if (retcd == MHD_YES) {
            authenticated = true;
            auth_role = "admin";
            return MHD_YES;
        }
    } else if (is_user) {
        if (user_auth_is_ha1) {
            retcd = MHD_digest_auth_check2(connection, auth_realm
                , user_auth_user, user_auth_pass, 300, MHD_DIGEST_ALG_MD5);
        } else {
            retcd = MHD_digest_auth_check(connection, auth_realm
                , user_auth_user, user_auth_pass, 300);
        }
        if (retcd == MHD_YES) {
            authenticated = true;
            auth_role = "user";
            return MHD_YES;
        }
    }

    /* Password check failed */
    if (retcd == MHD_NO) {
        failauth_log(false, attempted_user);
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

    /* Try admin credentials first */
    if ((mystreq(user, auth_user)) && (mystreq(pass, auth_pass))) {
        myfree(user);
        myfree(pass);
        authenticated = true;
        auth_role = "admin";
        return MHD_YES;
    }

    /* Try view-only user credentials if configured */
    if (user_auth_user != nullptr && user_auth_pass != nullptr) {
        if ((mystreq(user, user_auth_user)) && (mystreq(pass, user_auth_pass))) {
            myfree(user);
            myfree(pass);
            authenticated = true;
            auth_role = "user";
            return MHD_YES;
        }
    }

    /* Both failed */
    failauth_log(true, user ? std::string(user) : "");
    myfree(user);
    myfree(pass);
    return mhd_basic_fail();

}

/* Parse apart the admin user:pass provided*/
void cls_webu_ans::mhd_auth_parse()
{
    int auth_len;
    char *col_pos;

    /* Parse admin credentials */
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
        auth_is_ha1 = false;
    } else {
        auth_user = (char*)mymalloc((uint)auth_len - strlen(col_pos) + 1);
        auth_pass =(char*)mymalloc(strlen(col_pos));
        snprintf(auth_user, (uint)auth_len - strlen(col_pos) + 1, "%s"
            ,app->cfg->webcontrol_authentication.c_str());
        snprintf(auth_pass, strlen(col_pos), "%s", col_pos + 1);

        /* Check if password is HA1 hash (32 hex characters) */
        auth_is_ha1 = false;
        if (strlen(auth_pass) == 32) {
            bool is_hex = true;
            for (int i = 0; i < 32 && is_hex; i++) {
                is_hex = isxdigit((unsigned char)auth_pass[i]);
            }
            if (is_hex) {
                auth_is_ha1 = true;
                MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                    _("Detected HA1 hash format for webcontrol authentication"));
            }
        }
    }

    /* Parse view-only user credentials (if configured) */
    myfree(user_auth_user);
    myfree(user_auth_pass);
    user_auth_user = nullptr;
    user_auth_pass = nullptr;
    user_auth_is_ha1 = false;

    if (app->cfg->webcontrol_user_authentication != "" &&
        app->cfg->webcontrol_user_authentication != "noauth") {

        auth_len = (int)app->cfg->webcontrol_user_authentication.length();
        col_pos =(char*) strstr(app->cfg->webcontrol_user_authentication.c_str() ,":");
        if (col_pos == NULL) {
            user_auth_user = (char*)mymalloc((uint)(auth_len+1));
            user_auth_pass = (char*)mymalloc(2);
            snprintf(user_auth_user, (uint)auth_len + 1, "%s"
                ,app->cfg->webcontrol_user_authentication.c_str());
            snprintf(user_auth_pass, 2, "%s","");
        } else {
            user_auth_user = (char*)mymalloc((uint)auth_len - strlen(col_pos) + 1);
            user_auth_pass =(char*)mymalloc(strlen(col_pos));
            snprintf(user_auth_user, (uint)auth_len - strlen(col_pos) + 1, "%s"
                ,app->cfg->webcontrol_user_authentication.c_str());
            snprintf(user_auth_pass, strlen(col_pos), "%s", col_pos + 1);

            /* Check if user password is HA1 hash */
            if (strlen(user_auth_pass) == 32) {
                bool is_hex = true;
                for (int i = 0; i < 32 && is_hex; i++) {
                    is_hex = isxdigit((unsigned char)user_auth_pass[i]);
                }
                if (is_hex) {
                    user_auth_is_ha1 = true;
                    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                        _("Detected HA1 hash format for user authentication"));
                }
            }
        }
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

    /* Allow certain endpoints without HTTP auth so the React SPA can load
     * and handle authentication itself via session tokens:
     * 1. Static files (device_id < 0): /assets/* etc.
     * 2. Root page and SPA routes (uri_cmd1 empty): /, /settings, etc.
     * 3. All API endpoints: Use session-based auth, not HTTP Basic/Digest
     *
     * Only streams (mjpg, mpegts, static) use HTTP auth as fallback for
     * clients that don't support session tokens.
     */
    if (device_id < 0 || uri_cmd1.empty() || uri_cmd1 == "api") {
        authenticated = true;  /* Skip HTTP auth, use session auth */
        return MHD_YES;
    }

    if (app->cfg->webcontrol_authentication == "") {
        authenticated = true;
        if (app->cfg->webcontrol_auth_method != "none") {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("No webcontrol user:pass provided"));
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

void cls_webu_ans::gzip_deflate()
{
    uint sz;
    int retcd;

    /* Add extra 1024(arbitrary number) bytes to output buffer
       The buffer MUST be large enough to do in a single call
       to deflate.  (Not currently coded for multiple calls)
    */
    sz = (uint)resp_page.length()+1024;

    myfree(gzip_resp);
    gzip_resp = (u_char*)mymalloc(sz);
    gzip_size = 0;

    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = (uint)resp_page.length();
    zs.next_in = (Bytef *)resp_page.c_str();
    zs.avail_out = sz;
    zs.next_out = (Bytef *)gzip_resp;

    deflateInit2(&zs
        , Z_DEFAULT_COMPRESSION
        , Z_DEFLATED
        , 15 | 16, 8
        , Z_DEFAULT_STRATEGY);

    retcd = deflate(&zs, Z_FINISH);
    if (retcd < Z_OK) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("deflate failed: %d") ,retcd);
        gzip_size = 0;
    } else {
        gzip_size = (unsigned long)zs.total_out;
    }

    retcd = deflateEnd(&zs);
    if (retcd < Z_OK) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("deflateEnd failed: %d"), retcd);
        gzip_size = 0;
    }

    if (zs.avail_in != 0) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("deflate failed avail in: %d"), zs.avail_in);
        gzip_size = 0;
    }

}

/* Send the response that we created back to the user.  */
void cls_webu_ans::mhd_send()
{
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    if (gzip_encode == true) {
        gzip_deflate();
        if (gzip_size == 0) {
            gzip_encode = false;
            resp_page = "Error in gzip response";
            response = MHD_create_response_from_buffer(resp_page.length()
                ,(void *)resp_page.c_str(), MHD_RESPMEM_PERSISTENT);
        } else {
            response = MHD_create_response_from_buffer(
                gzip_size, (void *)gzip_resp
                , MHD_RESPMEM_PERSISTENT);
        }
    } else {
        response = MHD_create_response_from_buffer(resp_page.length()
            ,(void *)resp_page.c_str(), MHD_RESPMEM_PERSISTENT);
    }
    if (response == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return;
    }

    /* Add default security headers (can be overridden by user config) */
    MHD_add_response_header(response, "X-Content-Type-Options", "nosniff");
    MHD_add_response_header(response, "X-Frame-Options", "SAMEORIGIN");
    MHD_add_response_header(response, "X-XSS-Protection", "1; mode=block");
    MHD_add_response_header(response, "Referrer-Policy", "strict-origin-when-cross-origin");

    /* Add Content Security Policy for HTML responses */
    if (resp_type == WEBUI_RESP_HTML) {
        MHD_add_response_header(response, "Content-Security-Policy",
            "default-src 'self'; "
            "script-src 'self' 'unsafe-inline'; "
            "style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data:; "
            "connect-src 'self'");
    }

    /* User-configured headers can override defaults */
    if (webu->wb_headers->params_cnt > 0) {
        for (indx=0;indx<webu->wb_headers->params_cnt; indx++) {
            MHD_add_response_header (response
                ,webu->wb_headers->params_array[indx].param_name.c_str()
                ,webu->wb_headers->params_array[indx].param_value.c_str());
        }
    }

    if (resp_type == WEBUI_RESP_TEXT) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
    } else if (resp_type == WEBUI_RESP_JSON) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json;");
    } else if (resp_type == WEBUI_RESP_CSS) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/css;");
    } else if (resp_type == WEBUI_RESP_JS) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/javascript;");
    } else {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
    }

    if (gzip_encode == true) {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_ENCODING, "gzip");
    }

    retcd = MHD_queue_response (connection, resp_code, response);
    MHD_destroy_response (response);

    if (retcd == MHD_NO) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("send page failed."));
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

bool cls_webu_ans::valid_request()
{
    pthread_mutex_lock(&app->mutex_camlst);
        if (device_id < 0) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), url.c_str());
            pthread_mutex_unlock(&app->mutex_camlst);
            return false;
        }
        if ((device_id > 0) && (cam == NULL)) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), url.c_str());
            pthread_mutex_unlock(&app->mutex_camlst);
            return false;
        }
    pthread_mutex_unlock(&app->mutex_camlst);

    return true;
}

/* Answer the DELETE request from the user */
void cls_webu_ans::answer_delete()
{
    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        ,"processing delete: %s",uri_cmd1.c_str());

    if (valid_request() == false) {
        bad_request();
        return;
    }

    /* Only allow DELETE for API media endpoints */
    if (uri_cmd1 == "api" && uri_cmd2 == "media") {
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }

        /* Validate CSRF token for DELETE requests (supports both session and global tokens) */
        const char* csrf_token = MHD_lookup_connection_value(
            connection, MHD_HEADER_KIND, "X-CSRF-Token");
        if (!webu->csrf_validate_request(csrf_token ? std::string(csrf_token) : "", session_token)) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
                _("CSRF token validation failed for DELETE from %s"), clientip.c_str());
            resp_type = WEBUI_RESP_JSON;
            resp_page = "{\"error\":\"CSRF validation failed\"}";
            mhd_send();
            return;
        }

        if (uri_cmd3 == "picture") {
            webu_json->api_delete_picture();
            mhd_send();
        } else if (uri_cmd3 == "movie") {
            webu_json->api_delete_movie();
            mhd_send();
        } else if (uri_cmd3 == "folders" && uri_cmd4 == "files") {
            webu_json->api_delete_folder_files();
            mhd_send();
        } else {
            bad_request();
        }
    } else if (uri_cmd1 == "api" && uri_cmd2 == "mask" && uri_cmd3 != "") {
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }
        /* CSRF validation is done inside api_mask_delete() */
        webu_json->api_mask_delete();
        mhd_send();
    } else if (uri_cmd1 == "api" && uri_cmd2 == "profiles" && !uri_cmd3.empty()) {
        /* DELETE /0/api/profiles/{id} */
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }
        /* CSRF validation is done inside api_profiles_delete() */
        webu_json->api_profiles_delete();
        mhd_send();
    } else {
        /* DELETE not allowed for other endpoints */
        resp_type = WEBUI_RESP_TEXT;
        resp_page = "HTTP 405: Method Not Allowed\n";
        mhd_send();
    }
}

/* Answer the get request from the user */
void cls_webu_ans::answer_get()
{
    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        ,"processing get: %s",uri_cmd1.c_str());

    /* Check for static file serving (React UI) before camera validation
     * This allows serving files like /assets/*, /settings, / without a camera ID */
    if (app->cfg->webcontrol_html_path != "" && device_id < 0) {
        if (webu_file == nullptr) {
            webu_file = new cls_webu_file(this);
        }
        gzip_encode = false;
        webu_file->serve_static_file();
        return;
    }

    if (valid_request() == false) {
        bad_request();
        return;
    }

    if ((uri_cmd1 == "mjpg") || (uri_cmd1 == "mpegts") ||
        (uri_cmd1 == "static")) {
        if (webu_stream == nullptr) {
            webu_stream  = new cls_webu_stream(this);
        }
        gzip_encode = false;
        webu_stream->main();

    } else if (uri_cmd1 == "movies") {
        if (webu_file == nullptr) {
            webu_file = new cls_webu_file(this);
        }
        gzip_encode = false;
        webu_file->main();

    } else if (uri_cmd1 == "api") {
        /* React UI JSON API endpoints */
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }

        /* Check session-based auth for protected API endpoints
         * Auth endpoints (status, login, logout) are exempt */
        if (uri_cmd2 != "auth" && app->cfg->webcontrol_authentication != "") {
            /* Validate session token */
            bool has_valid_session = false;
            if (!session_token.empty()) {
                auth_role = webu->session_validate(session_token, clientip);
                has_valid_session = !auth_role.empty();
            }
            if (!has_valid_session) {
                /* Return 401 JSON (no WWW-Authenticate to avoid browser popup) */
                resp_type = WEBUI_RESP_JSON;
                resp_code = 401;
                resp_page = "{\"error\":\"Authentication required\",\"auth_required\":true}";
                mhd_send();
                return;
            }
        }

        if (uri_cmd2 == "auth" && uri_cmd3 == "me") {
            webu_json->api_auth_me();
            mhd_send();
        } else if (uri_cmd2 == "auth" && uri_cmd3 == "login") {
            webu_json->api_auth_login();
            mhd_send();
        } else if (uri_cmd2 == "auth" && uri_cmd3 == "logout") {
            webu_json->api_auth_logout();
            mhd_send();
        } else if (uri_cmd2 == "auth" && uri_cmd3 == "status") {
            webu_json->api_auth_status();
            mhd_send();
        } else if (uri_cmd2 == "media" && uri_cmd3 == "pictures") {
            webu_json->api_media_pictures();
            mhd_send();
        } else if (uri_cmd2 == "media" && uri_cmd3 == "movies") {
            webu_json->api_media_movies();
            mhd_send();
        } else if (uri_cmd2 == "media" && uri_cmd3 == "dates") {
            webu_json->api_media_dates();
            mhd_send();
        } else if (uri_cmd2 == "media" && uri_cmd3 == "folders") {
            webu_json->api_media_folders();
            mhd_send();
        } else if (uri_cmd2 == "system" && uri_cmd3 == "temperature") {
            webu_json->api_system_temperature();
            mhd_send();
        } else if (uri_cmd2 == "system" && uri_cmd3 == "status") {
            webu_json->api_system_status();
            mhd_send();
        } else if (uri_cmd2 == "cameras") {
            webu_json->api_cameras();
            mhd_send();
        } else if (uri_cmd2 == "config") {
            webu_json->api_config();
            mhd_send();
        } else if (uri_cmd2 == "mask" && uri_cmd3 != "") {
            webu_json->api_mask_get();
            mhd_send();
        } else if (uri_cmd2 == "profiles") {
            if (uri_cmd3.empty()) {
                /* GET /0/api/profiles?camera_id=X */
                webu_json->api_profiles_list();
            } else {
                /* GET /0/api/profiles/{id} */
                webu_json->api_profiles_get();
            }
            mhd_send();
        } else {
            bad_request();
        }

    } else if (uri_cmd1 == "config") {
        /* Treat /config like /config.json */
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }
        webu_json->main();

    } else if ((uri_cmd1 == "config.json") || (uri_cmd1 == "log") ||
        (uri_cmd1 == "movies.json") || (uri_cmd1 == "status.json")) {
        if (webu_json == nullptr) {
            webu_json = new cls_webu_json(this);
        }
        webu_json->main();

    } else {
        /* Serve React UI static files */
        if (webu_file == nullptr) {
            webu_file = new cls_webu_file(this);
        }
        gzip_encode = false;
        webu_file->serve_static_file();
    }
}

/* Answer the connection request for the webcontrol*/
mhdrslt cls_webu_ans::answer_main(struct MHD_Connection *p_connection
    , const char *method, const char *upload_data, size_t *upload_data_size)
{
    mhdrslt retcd;

    cnct_type = WEBUI_CNCT_CONTROL;
    connection = p_connection;

    MHD_get_connection_values (p_connection
        , MHD_HEADER_KIND, webua_connection_values, this);

    if (url.length() == 0) {
        bad_request();
        return MHD_YES;
    }

    if (cam != NULL) {
        if (cam->finish) {
           MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Shutting down camera"));
           return MHD_NO;
        }
    }


    if (check_tls() != 0) {
        return MHD_NO;
    }

    if (clientip.length() == 0) {
        clientip_get();
    }

    if (failauth_check() == MHD_NO) {
        return MHD_NO;
    }

    if (authenticated == false) {
        /* Check for session token in X-Session-Token header */
        const char* token = MHD_lookup_connection_value(
            connection, MHD_HEADER_KIND, "X-Session-Token");

        /* Also check query parameter for streams (img/video tags can't send headers) */
        if (token == nullptr) {
            token = MHD_lookup_connection_value(
                connection, MHD_GET_ARGUMENT_KIND, "token");
        }

        if (token != nullptr) {
            session_token = token;

            /* Validate and get role from session */
            auth_role = webu->session_validate(session_token, clientip);
            if (!auth_role.empty()) {
                authenticated = true;
                retcd = MHD_YES;
            } else {
                /* Session invalid/expired - fall through to HTTP auth */
                retcd = mhd_auth();
                if (authenticated == false) {
                    return retcd;
                }
            }
        } else {
            /* No session token - use HTTP Basic/Digest auth */
            retcd = mhd_auth();
            if (authenticated == false) {
                return retcd;
            }
        }
    }

    client_connect();

    if (mhd_first) {
        mhd_first = false;
        if (mystreq(method,"POST")) {
            cnct_method = WEBUI_METHOD_POST;
            /* Check if this is a JSON API endpoint (mask API, power control, profiles, auth) */
            if ((uri_cmd1 == "api" && uri_cmd2 == "mask" && uri_cmd3 != "") ||
                (uri_cmd1 == "api" && uri_cmd2 == "system" &&
                 (uri_cmd3 == "reboot" || uri_cmd3 == "shutdown" || uri_cmd3 == "service-restart")) ||
                (uri_cmd1 == "api" && uri_cmd2 == "profiles") ||
                (uri_cmd1 == "api" && uri_cmd2 == "auth" &&
                 (uri_cmd3 == "login" || uri_cmd3 == "logout"))) {
                raw_body.clear();  /* Clear body buffer for JSON POST */
                retcd = MHD_YES;
            } else if (uri_cmd1 == "api" && uri_cmd2 == "config" && uri_cmd3 == "write") {
                /* Config write handled via JSON POST */
                raw_body.clear();
                retcd = MHD_YES;
            } else if (uri_cmd1 == "api" && uri_cmd2 == "camera") {
                /* Camera action endpoints handled via JSON POST */
                raw_body.clear();
                retcd = MHD_YES;
            } else {
                /* Unknown POST endpoint */
                bad_request();
                mhd_send();
                retcd = MHD_YES;
            }
        } else if (mystreq(method,"PATCH")) {
            cnct_method = WEBUI_METHOD_PATCH;
            raw_body.clear();  /* Clear body buffer for new PATCH request */
            retcd = MHD_YES;
        } else if (mystreq(method,"DELETE")) {
            cnct_method = WEBUI_METHOD_DELETE;
            retcd = MHD_YES;
        } else {
            cnct_method = WEBUI_METHOD_GET;
            retcd = MHD_YES;
        }
        return retcd;
    }

    hostname_get();

    if (mystreq(method,"POST")) {
        /* Check if this is a JSON API endpoint */
        if (uri_cmd1 == "api" && uri_cmd2 == "mask" && uri_cmd3 != "") {
            /* Accumulate raw body for JSON POST */
            if (*upload_data_size > 0) {
                raw_body.append(upload_data, *upload_data_size);
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, process mask POST */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_mask_post();
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "system" && uri_cmd3 == "reboot") {
            /* System reboot - consume body first (even if empty) */
            if (*upload_data_size > 0) {
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, process reboot */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_system_reboot();
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "system" && uri_cmd3 == "shutdown") {
            /* System shutdown - consume body first (even if empty) */
            if (*upload_data_size > 0) {
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, process shutdown */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_system_shutdown();
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "system" && uri_cmd3 == "service-restart") {
            /* Motion service restart - consume body first (even if empty) */
            if (*upload_data_size > 0) {
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, process service restart */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_system_service_restart();
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "profiles") {
            /* Profile API endpoints - accumulate raw body for JSON POST */
            if (*upload_data_size > 0) {
                raw_body.append(upload_data, *upload_data_size);
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, route to appropriate handler */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            if (uri_cmd3.empty()) {
                /* POST /0/api/profiles (create) */
                webu_json->api_profiles_create();
            } else if (uri_cmd4 == "apply") {
                /* POST /0/api/profiles/{id}/apply */
                webu_json->api_profiles_apply();
            } else if (uri_cmd4 == "default") {
                /* POST /0/api/profiles/{id}/default */
                webu_json->api_profiles_set_default();
            } else {
                bad_request();
            }
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "auth") {
            /* Auth API endpoints - accumulate raw body for JSON POST */
            if (*upload_data_size > 0) {
                raw_body.append(upload_data, *upload_data_size);
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, route to appropriate handler */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            if (uri_cmd3 == "login") {
                /* POST /0/api/auth/login */
                webu_json->api_auth_login();
            } else if (uri_cmd3 == "logout") {
                /* POST /0/api/auth/logout */
                webu_json->api_auth_logout();
            } else {
                bad_request();
            }
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "config" && uri_cmd3 == "write") {
            /* POST /0/api/config/write - save configuration to file */
            if (*upload_data_size > 0) {
                *upload_data_size = 0;
                return MHD_YES;
            }
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_config_write();
            mhd_send();
            retcd = MHD_YES;
        } else if (uri_cmd1 == "api" && uri_cmd2 == "camera") {
            /* Camera action API endpoints - accumulate body for JSON POST */
            if (*upload_data_size > 0) {
                raw_body.append(upload_data, *upload_data_size);
                *upload_data_size = 0;
                return MHD_YES;
            }
            /* Body complete, route to appropriate handler */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            if (uri_cmd3 == "restart") {
                /* POST /{camId}/api/camera/restart */
                webu_json->api_camera_restart();
            } else if (uri_cmd3 == "snapshot") {
                /* POST /{camId}/api/camera/snapshot */
                webu_json->api_camera_snapshot();
            } else if (uri_cmd3 == "pause") {
                /* POST /{camId}/api/camera/pause */
                webu_json->api_camera_pause();
            } else if (uri_cmd3 == "stop") {
                /* POST /{camId}/api/camera/stop */
                webu_json->api_camera_stop();
            } else if (uri_cmd3 == "event" && uri_cmd4 == "start") {
                /* POST /{camId}/api/camera/event/start */
                webu_json->api_camera_event_start();
            } else if (uri_cmd3 == "event" && uri_cmd4 == "end") {
                /* POST /{camId}/api/camera/event/end */
                webu_json->api_camera_event_end();
            } else if (uri_cmd3 == "ptz") {
                /* POST /{camId}/api/camera/ptz */
                webu_json->api_camera_ptz();
            } else {
                bad_request();
            }
            mhd_send();
            retcd = MHD_YES;
        } else {
            /* Unknown POST endpoint - reject */
            bad_request();
            mhd_send();
            retcd = MHD_YES;
        }
    } else if (mystreq(method,"PATCH")) {
        /* Accumulate raw body for JSON endpoints */
        if (*upload_data_size > 0) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "PATCH: Accumulating %zu bytes, total now %zu"
                , *upload_data_size, raw_body.length() + *upload_data_size);
            raw_body.append(upload_data, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        }
        /* Body complete, process request */
        MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
            , "PATCH: Body complete (%zu bytes), processing %s/%s"
            , raw_body.length(), uri_cmd1.c_str(), uri_cmd2.c_str());
        if (uri_cmd1 == "api" && uri_cmd2 == "config") {
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_config_patch();
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "PATCH: api_config_patch() completed, sending response (%zu bytes)"
                , resp_page.length());
            mhd_send();
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, "PATCH: mhd_send() completed");
        } else if (uri_cmd1 == "api" && uri_cmd2 == "profiles" && !uri_cmd3.empty()) {
            /* PATCH /0/api/profiles/{id} */
            if (webu_json == nullptr) {
                webu_json = new cls_webu_json(this);
            }
            webu_json->api_profiles_update();
            mhd_send();
        } else {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , "PATCH: Bad request - cmd1=%s cmd2=%s"
                , uri_cmd1.c_str(), uri_cmd2.c_str());
            bad_request();
        }
        retcd = MHD_YES;
    } else if (mystreq(method,"DELETE")) {
        answer_delete();
        retcd = MHD_YES;
    } else {
        answer_get();
        retcd = MHD_YES;
    }
    return retcd;

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
    if (device_id == 0) {
        pthread_mutex_lock(&app->allcam->stream.mutex);
            if ((cnct_type == WEBUI_CNCT_JPG_FULL) ||
                (cnct_type == WEBUI_CNCT_TS_FULL)) {
                strm = &app->allcam->stream.norm;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SUB) ||
                        (cnct_type == WEBUI_CNCT_TS_SUB)) {
                strm = &app->allcam->stream.sub;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_MOTION) ||
                        (cnct_type == WEBUI_CNCT_TS_MOTION )) {
                strm = &app->allcam->stream.motion;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SOURCE) ||
                        (cnct_type == WEBUI_CNCT_TS_SOURCE)) {
                strm = &app->allcam->stream.source;
            } else if ( (cnct_type == WEBUI_CNCT_JPG_SECONDARY) ||
                        (cnct_type == WEBUI_CNCT_TS_SECONDARY)) {
                strm = &app->allcam->stream.secondary;
            } else {
                strm = &app->allcam->stream.norm;
            }

            if (strm->all_cnct > 0) {
                strm->all_cnct--;
            }
        pthread_mutex_unlock(&app->allcam->stream.mutex);
    }
}

cls_webu_ans::cls_webu_ans(cls_motapp *p_app, const char *uri)
{
    app = p_app;
    webu = p_app->webu;

    char *tmplang;

    url           = "";
    uri_cmd0      = "";
    uri_cmd1      = "";
    uri_cmd2      = "";
    uri_cmd3      = "";
    uri_cmd4      = "";
    clientip      = "";
    lang          = "";                          /* Two digit lang code */

    auth_opaque   = (char*)mymalloc(WEBUI_LEN_PARM);
    auth_realm    = (char*)mymalloc(WEBUI_LEN_PARM);
    auth_user     = nullptr;                        /* Buffer to hold the admin user name*/
    auth_pass     = nullptr;                        /* Buffer to hold the admin password */
    user_auth_user = nullptr;                       /* Buffer to hold the view-only user name*/
    user_auth_pass = nullptr;                       /* Buffer to hold the view-only password */
    authenticated = false;                       /* boolean for whether we are authenticated*/
    auth_role     = "";                          /* User role (admin/user)*/
    auth_is_ha1   = false;                       /* Admin password is HA1 hash */
    user_auth_is_ha1 = false;                    /* User password is HA1 hash */

    resp_page     = "";                          /* The response being constructed */
    resp_code     = 200;                         /* Default HTTP status code */
    req_file      = nullptr;
    gzip_resp     = nullptr;
    gzip_size     = 0;
    gzip_encode   = false;

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
    webu_json = nullptr;
    webu_stream = nullptr;

    url.assign(uri);

    parms_edit();
    webu->cnct_cnt++;

}

cls_webu_ans::~cls_webu_ans()
{
    deinit_counter();

    mydelete(webu_file);
    mydelete(webu_json);
    mydelete(webu_stream);

    myfree(auth_user);
    myfree(auth_pass);
    myfree(user_auth_user);
    myfree(user_auth_pass);
    myfree(auth_opaque);
    myfree(auth_realm);
    myfree(gzip_resp);

    webu->cnct_cnt--;
}