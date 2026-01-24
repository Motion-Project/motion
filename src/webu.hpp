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
 * webu.hpp - Web Server Interface Definitions
 *
 * Header file defining web server structures, HTTP request handling,
 * authentication, and the web interface class for Motion's embedded
 * HTTP server using libmicrohttpd.
 *
 */

#ifndef _INCLUDE_WEBU_HPP_
#define _INCLUDE_WEBU_HPP_

    #include <mutex>
    #include <map>
    #include <string>

    /* Some defines of lengths for our buffers */
    #define WEBUI_LEN_PARM 512          /* Parameters specified */
    #define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
    #define WEBUI_LEN_RESP 1024         /* Initial response size */
    #define WEBUI_MHD_OPTS 10           /* Maximum number of options permitted for MHD */

    #define WEBUI_POST_BFRSZ  512

    /* Security: Maximum tracked clients for rate limiting (prevents memory exhaustion) */
    #define WEBUI_MAX_CLIENTS 10000
    /* Security: TTL for stale client entries in seconds */
    #define WEBUI_CLIENT_TTL  3600

    enum WEBUI_METHOD {
        WEBUI_METHOD_GET    = 0,
        WEBUI_METHOD_POST   = 1,
        WEBUI_METHOD_DELETE = 2,
        WEBUI_METHOD_PATCH  = 3
    };

    enum WEBUI_CNCT {
        WEBUI_CNCT_CONTROL,
        WEBUI_CNCT_FILE,
        WEBUI_CNCT_JPG_MIN,
        WEBUI_CNCT_JPG_FULL,
        WEBUI_CNCT_JPG_SUB,
        WEBUI_CNCT_JPG_MOTION,
        WEBUI_CNCT_JPG_SOURCE,
        WEBUI_CNCT_JPG_SECONDARY,
        WEBUI_CNCT_JPG_MAX,
        WEBUI_CNCT_TS_MIN,
        WEBUI_CNCT_TS_FULL,
        WEBUI_CNCT_TS_SUB,
        WEBUI_CNCT_TS_MOTION,
        WEBUI_CNCT_TS_SOURCE,
        WEBUI_CNCT_TS_SECONDARY,
        WEBUI_CNCT_TS_MAX,
        WEBUI_CNCT_UNKNOWN
    };

    enum WEBUI_RESP {
        WEBUI_RESP_HTML     = 0,
        WEBUI_RESP_JSON     = 1,
        WEBUI_RESP_TEXT     = 2,
        WEBUI_RESP_JS       = 3,
        WEBUI_RESP_CSS      = 4
    };

    struct ctx_webu_clients {
        std::string                 clientip;
        std::string                 username;       /* Track username for lockout */
        bool                        authenticated;
        int                         conn_nbr;
        struct timespec             conn_time;
        int                         userid_fail_nbr;
    };

    struct ctx_key {
        char                        *key_nm;        /* Name of the key item */
        char                        *key_val;       /* Value of the key item */
        size_t                      key_sz;         /* The size of the value */
    };

    /* Context to pass the parms to functions to start mhd */
    struct ctx_mhdstart {
        std::string             tls_cert;
        std::string             tls_key;
        bool                    tls_use;
        struct MHD_OptionItem   *mhd_ops;
        int                     mhd_opt_nbr;
        unsigned int            mhd_flags;
        int                     ipv6;
        struct sockaddr_in      lpbk_ipv4;
        struct sockaddr_in6     lpbk_ipv6;
    };

    #define CSRF_TOKEN_LENGTH 64    /* 32 bytes hex-encoded */

    /* Session data structure for session-based authentication */
    struct ctx_session {
        std::string     token;          /* 64-char hex session token */
        std::string     csrf_token;     /* Per-session CSRF token */
        std::string     role;           /* "admin" or "user" */
        std::string     client_ip;      /* IP that created session */
        time_t          created;        /* Creation timestamp */
        time_t          last_access;    /* Last activity timestamp */
        time_t          expires;        /* Expiration timestamp */
    };

    class cls_webu {
        public:
            cls_webu(cls_motapp *p_app);
            ~cls_webu();
            bool                        finish;
            ctx_params                  *wb_headers;
            ctx_params                  *wb_actions;
            char                        wb_digest_rand[12];
            struct MHD_Daemon           *wb_daemon;
            struct MHD_Daemon           *wb_daemon2;
            std::list<ctx_webu_clients> wb_clients;
            std::string                 info_tls;
            int                         cnct_cnt;
            bool                        restart;
            std::string                 csrf_token;     /* CSRF protection token */

            /* Session management */
            std::map<std::string, ctx_session> sessions; /* token -> session */
            std::mutex                  sessions_mutex;  /* Thread-safe access */

            void startup();
            void shutdown();
            void csrf_generate();
            bool csrf_validate(const std::string &token);
            bool csrf_validate_request(const std::string &csrf_token, const std::string &session_token);

            /* Session management functions */
            std::string session_generate_token();
            std::string session_create(const std::string& role, const std::string& client_ip);
            std::string session_validate(const std::string& token, const std::string& client_ip);
            std::string session_get_csrf(const std::string& token);
            void session_destroy(const std::string& token);
            void session_cleanup_expired();

        private:
            ctx_mhdstart    *mhdst;
            cls_motapp      *app;
            void init_actions();
            void start_daemon_port1();
            void start_daemon_port2();
            void mhd_features_basic();
            void mhd_features_digest();
            void mhd_features_ipv6();
            void mhd_features_tls();
            void mhd_features();
            void mhd_loadfile(std::string fname, std::string &filestr);
            void mhd_checktls();
            void mhd_opts_init();
            void mhd_opts_deinit();
            void mhd_opts_localhost();
            void mhd_opts_digest();
            void mhd_opts_tls();
            void mhd_opts();
            void mhd_flags();
    };

#endif
