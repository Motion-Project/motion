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
 * webu_ans.hpp - HTTP Response Management Interface
 *
 * Header file defining the HTTP response class for constructing and
 * delivering responses via libmicrohttpd, handling headers, status codes,
 * content types, and streaming callbacks.
 *
 */

#ifndef _INCLUDE_WEBU_ANS_HPP_
#define _INCLUDE_WEBU_ANS_HPP_
    class cls_webu_ans {
        public:
            cls_webu_ans(cls_motapp *p_motapp, const char *uri);
            ~cls_webu_ans();

            cls_motapp      *app;
            cls_webu        *webu;
            cls_camera      *cam;

            struct MHD_Connection   *connection;

            FILE            *req_file;      /* requested file*/
            std::string     lang;           /* Two character abbreviation for locale language*/
            std::string     auth_role;      /* User role: "admin" or "user" (empty if unauthenticated) */
            std::string     session_token;  /* Session token from X-Session-Token header */

            std::string     url;            /* The URL sent from the client */
            std::string     uri_cmd0;       /* Parsed command from the url eg /cmd0/cmd1/cmd2/cmd3/cmd4 */
            std::string     uri_cmd1;       /* Parsed command from the url eg /cmd0/cmd1/cmd2/cmd3/cmd4 */
            std::string     uri_cmd2;       /* Parsed command from the url eg /cmd0/cmd1/cmd2/cmd3/cmd4 */
            std::string     uri_cmd3;       /* Parsed command from the url eg /cmd0/cmd1/cmd2/cmd3/cmd4 */
            std::string     uri_cmd4;       /* Parsed command from the url eg /cmd0/cmd1/cmd2/cmd3/cmd4 */

            enum WEBUI_RESP resp_type;      /* indicator for the type of response to provide. */
            std::string     resp_page;      /* The response that will be sent */
            std::string     raw_body;       /* Accumulated POST/PATCH body for JSON endpoints */
            int             resp_code;      /* HTTP response code (default 200) */

            int             camindx;        /* Index number of the cam */
            int             device_id;      /* Device id number requested */
            enum WEBUI_CNCT cnct_type;      /* Type of connection we are processing */
            std::string     clientip;       /* IP of the connecting client */
            std::string     hostfull;       /* Full http name for host with port number */
            bool            gzip_encode;    /* Bool for whether to gzip response */

            void mhd_send();
            void bad_request();
            bool valid_request();
            enum WEBUI_METHOD get_method() const { return cnct_method; }
            void failauth_log(bool userid_fail, const std::string &username = "");

            mhdrslt answer_main(struct MHD_Connection *connection, const char *method
                , const char *upload_data, size_t *upload_data_size);

        private:
            cls_webu_file   *webu_file;
            cls_webu_json   *webu_json;
            cls_webu_stream *webu_stream;

            int             mhd_first;      /* Boolean for whether it is the first connection*/
            char            *auth_opaque;   /* Opaque string for digest authentication*/
            char            *auth_realm;    /* Realm string for digest authentication*/
            char            *auth_user;     /* Parsed admin user from config authentication string*/
            char            *auth_pass;     /* Parsed admin password from config authentication string*/
            char            *user_auth_user; /* Parsed view-only user from config user_authentication string*/
            char            *user_auth_pass; /* Parsed view-only password from config user_authentication string*/
            bool            authenticated;  /* Boolean for whether authentication has been passed */
            bool            auth_is_ha1;    /* Boolean for whether auth_pass is HA1 hash (32 hex chars) */
            bool            user_auth_is_ha1; /* Boolean for whether user_auth_pass is HA1 hash */
            enum WEBUI_METHOD   cnct_method;    /* Connection method.  Get or Post */
            u_char  *gzip_resp;     /* Response in gzip format */
            unsigned long    gzip_size;     /* Size of response in gzip format */

            int check_tls();
            void parms_edit();
            int parseurl();
            void clientip_get();
            void hostname_get();
            void client_connect();
            mhdrslt failauth_check();
            mhdrslt mhd_digest_fail(int signal_stale);
            mhdrslt mhd_digest();
            mhdrslt mhd_basic_fail();
            mhdrslt mhd_basic();
            void mhd_auth_parse();
            mhdrslt mhd_auth();
            void deinit_counter();
            void answer_get();
            void answer_delete();
            void gzip_deflate();

    };

#endif