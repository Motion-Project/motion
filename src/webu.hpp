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

#ifndef _INCLUDE_WEBU_HPP_
#define _INCLUDE_WEBU_HPP_

    /* Some defines of lengths for our buffers */
    #define WEBUI_LEN_PARM 512          /* Parameters specified */
    #define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
    #define WEBUI_LEN_RESP 1024         /* Initial response size */
    #define WEBUI_MHD_OPTS 10           /* Maximum number of options permitted for MHD */

    #define WEBUI_POST_BFRSZ  512

    enum WEBUI_METHOD {
        WEBUI_METHOD_GET    = 0,
        WEBUI_METHOD_POST   = 1
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
        WEBUI_RESP_TEXT     = 2
    };

    struct ctx_webu_clients {
        std::string                 clientip;
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

    class cls_webu {
        public:
            cls_webu(cls_motapp *p_app);
            ~cls_webu();
            bool                        wb_finish;
            ctx_params                  *wb_headers;
            ctx_params                  *wb_actions;
            char                        wb_digest_rand[12];
            struct MHD_Daemon           *wb_daemon;
            struct MHD_Daemon           *wb_daemon2;
            std::list<ctx_webu_clients> wb_clients;
            int                         cnct_cnt;
            bool restart;
            void startup();
            void shutdown();

        private:
            ctx_mhdstart *mhdst;
            cls_motapp *app;
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
