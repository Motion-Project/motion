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
#include "webu_json.hpp"
#include "webu_post.hpp"
#include "webu_file.hpp"
#include "webu_common.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "video_v4l2.hpp"

/* Initialize the MHD answer */
static void *webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection)
{
    (void)connection;
    ctx_motapp      *p_app =(ctx_motapp *)cls;
    cls_webu_ans    *webua;

    mythreadname_set("wc", 0, NULL);

    webua = new cls_webu_ans(p_app, uri);

    return webua;
}

/* Clean up our variables when the MHD connection closes */
static void webu_mhd_deinit(void *cls, struct MHD_Connection *connection
        , void **con_cls, enum MHD_RequestTerminationCode toe)
{
    (void)connection;
    (void)cls;
    (void)toe;
    cls_webu_ans *webua =(cls_webu_ans *) *con_cls;

    if (webua != nullptr) {
        if (webua->req_file != nullptr) {
            myfclose(webua->req_file);
            webua->req_file = nullptr;
        }
        delete webua;
        webua = nullptr;
    }
}

/* Answer the connection request for the webcontrol*/
static mhdrslt mhd_answer(void *cls
        , struct MHD_Connection *connection
        , const char *url, const char *method, const char *version
        , const char *upload_data, size_t *upload_data_size
        , void **ptr)
{
    (void)cls;
    (void)url;
    (void)version;

    cls_webu_ans *webua =(cls_webu_ans *) *ptr;

    return webua->answer_main(connection, method, upload_data, upload_data_size);
}

/* Validate that the MHD version installed can process basic authentication */
void cls_webu::mhd_features_basic()
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
        if (retcd == MHD_YES) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: available"));
        } else {
            if (app->cfg->webcontrol_auth_method == "basic") {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
                app->cfg->webcontrol_auth_method = "none";
            } else {
                MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Basic authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process digest authentication */
void cls_webu::mhd_features_digest()
{
    #if MHD_VERSION < 0x00094400
        (void)mhdst;
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
        if (retcd == MHD_YES) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: available"));
        } else {
            if (app->cfg->webcontrol_auth_method == "digest") {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
                app->cfg->webcontrol_auth_method = "none";
            } else {
                MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("Digest authentication: disabled"));
            }
        }
    #endif
}

/* Validate that the MHD version installed can process IPV6 */
void cls_webu::mhd_features_ipv6()
{
    #if MHD_VERSION < 0x00094400
        if (mhdst->ipv6) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old ipv6 disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
        if (retcd == MHD_YES) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("IPV6: available"));
        } else {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("IPV6: disabled"));
            if (mhdst->ipv6) {
                mhdst->ipv6 = 0;
            }
        }
    #endif
}

/* Validate that the MHD version installed can process tls */
void cls_webu::mhd_features_tls()
{
    #if MHD_VERSION < 0x00094400
        if (mhdst->tls_use) {
            MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("libmicrohttpd libary too old SSL/TLS disabled"));
            mhdst->tls_use = false;
        }
    #else
        mhdrslt retcd;
        retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
        if (retcd == MHD_YES) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: available"));
        } else {
            if (mhdst->tls_use) {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
                mhdst->tls_use = false;
            } else {
                MOTPLS_LOG(INF, TYPE_STREAM, NO_ERRNO ,_("SSL/TLS: disabled"));
            }
        }
    #endif
}

/* Validate the features that MHD can support */
void cls_webu::mhd_features()
{
    mhd_features_basic();
    mhd_features_digest();
    mhd_features_ipv6();
    mhd_features_tls();
}

/* Load a either the key or cert file for MHD*/
void cls_webu::mhd_loadfile(std::string fname,std::string &filestr)
{
    /* This needs conversion to c++ stream */
    FILE        *infile;
    size_t      read_size, file_size;
    long        retcd;
    char        *file_char;

    filestr = "";
    if (fname != "") {
        infile = myfopen(fname.c_str() , "rbe");
        if (infile != NULL) {
            fseek(infile, 0, SEEK_END);
            retcd = ftell(infile);
            if (retcd > 0 ) {
                file_size = (size_t)retcd;
                file_char = (char*)mymalloc(file_size +1);
                fseek(infile, 0, SEEK_SET);
                read_size = fread(file_char, file_size, 1, infile);
                if (read_size > 0 ) {
                    file_char[file_size] = 0;
                    filestr.assign(file_char, file_size);
                } else {
                    MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                        ,_("Error reading file for SSL/TLS support."));
                }
                free(file_char);
            }
            myfclose(infile);
        }
    }
}

/* Validate that we have the files needed for tls*/
void cls_webu::mhd_checktls()
{
    if (mhdst->tls_use) {
        if ((app->cfg->webcontrol_cert == "") || (mhdst->tls_cert == "")) {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("SSL/TLS requested but no cert file provided.  SSL/TLS disabled"));
            mhdst->tls_use = false;
        }
        if ((app->cfg->webcontrol_key == "") || (mhdst->tls_key == "")) {
            MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,_("SSL/TLS requested but no key file provided.  SSL/TLS disabled"));
            mhdst->tls_use = false;
        }
    }

}

/* Set the initialization function for MHD to call upon getting a connection */
void cls_webu::mhd_opts_init()
{
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_init;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = app;
    mhdst->mhd_opt_nbr++;
}

/* Set the MHD option on the function to call when the connection closes */
void cls_webu::mhd_opts_deinit()
{
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

/* Set the MHD option on acceptable connections */
void cls_webu::mhd_opts_localhost()
{
    if (app->cfg->webcontrol_localhost) {
        if (mhdst->ipv6) {
            memset(&mhdst->lpbk_ipv6, 0, sizeof(struct sockaddr_in6));
            mhdst->lpbk_ipv6.sin6_family = AF_INET6;
            mhdst->lpbk_ipv6.sin6_port = htons((uint16_t)app->cfg->webcontrol_port);
            mhdst->lpbk_ipv6.sin6_addr = in6addr_loopback;

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sosockaddr *)(&mhdst->lpbk_ipv6);
            mhdst->mhd_opt_nbr++;

        } else {
            memset(&mhdst->lpbk_ipv4, 0, sizeof(struct sockaddr_in));
            mhdst->lpbk_ipv4.sin_family = AF_INET;
            mhdst->lpbk_ipv4.sin_port = htons((uint16_t)app->cfg->webcontrol_port);
            mhdst->lpbk_ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
            mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = (struct sockaddr *)(&mhdst->lpbk_ipv4);
            mhdst->mhd_opt_nbr++;
        }
    }

}

/* Set the mhd digest options */
void cls_webu::mhd_opts_digest()
{
    if (app->cfg->webcontrol_auth_method == "digest") {

        mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = sizeof(wb_digest_rand);
        mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = wb_digest_rand;
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
void cls_webu::mhd_opts_tls()
{
    if (mhdst->tls_use) {

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
void cls_webu::mhd_opts()
{
    mhdst->mhd_opt_nbr = 0;

    mhd_checktls();
    mhd_opts_deinit();
    mhd_opts_init();
    mhd_opts_localhost();
    mhd_opts_digest();
    mhd_opts_tls();

    mhdst->mhd_ops[mhdst->mhd_opt_nbr].option = MHD_OPTION_END;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].value = 0;
    mhdst->mhd_ops[mhdst->mhd_opt_nbr].ptr_value = NULL;
    mhdst->mhd_opt_nbr++;

}

/* Set the mhd start up flags */
void cls_webu::mhd_flags()
{
    mhdst->mhd_flags = MHD_USE_THREAD_PER_CONNECTION;

    if (mhdst->ipv6) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_DUAL_STACK;
    }

    if (mhdst->tls_use) {
        mhdst->mhd_flags = mhdst->mhd_flags | MHD_USE_SSL;
    }

}

/* Set the values for the action commands */
void cls_webu::init_actions()
{
    std::string parm_vl;

    wb_actions = new ctx_params;
    wb_actions->update_params = true;
    util_parms_parse(wb_actions
        ,"webcontrol_actions", app->cfg->webcontrol_actions);

    if (app->cfg->webcontrol_parms == 0) {
        parm_vl = "off";
    } else {
        parm_vl = "on";
    }

    util_parms_add_default(wb_actions,"event",parm_vl);
    util_parms_add_default(wb_actions,"snapshot",parm_vl);
    util_parms_add_default(wb_actions,"pause",parm_vl);
    util_parms_add_default(wb_actions,"restart",parm_vl);
    util_parms_add_default(wb_actions,"stop",parm_vl);
    util_parms_add_default(wb_actions,"config_write",parm_vl);
    util_parms_add_default(wb_actions,"camera_add",parm_vl);
    util_parms_add_default(wb_actions,"camera_delete",parm_vl);
    util_parms_add_default(wb_actions,"config",parm_vl);
    util_parms_add_default(wb_actions,"ptz",parm_vl);
    util_parms_add_default(wb_actions,"movies","on");
    util_parms_add_default(wb_actions,"action_user",parm_vl);
}

void cls_webu::start_daemon_port1()
{
    mhdst = new ctx_mhdstart;

    mhd_loadfile(app->cfg->webcontrol_cert, mhdst->tls_cert);
    mhd_loadfile(app->cfg->webcontrol_key, mhdst->tls_key);
    mhdst->ipv6 = app->cfg->webcontrol_ipv6;
    mhdst->tls_use = app->cfg->webcontrol_tls;

    mhdst->mhd_ops =(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem) * WEBUI_MHD_OPTS);
    mhd_features();
    mhd_opts();
    mhd_flags();

    wb_daemon = MHD_start_daemon (
        mhdst->mhd_flags
        , (uint16_t)app->cfg->webcontrol_port
        , NULL, NULL
        , &mhd_answer, app
        , MHD_OPTION_ARRAY, mhdst->mhd_ops
        , MHD_OPTION_END);

    free(mhdst->mhd_ops);
    if (wb_daemon == nullptr) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start MHD"));
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started webcontrol on port %d")
            ,app->cfg->webcontrol_port);
    }
    delete mhdst;
    mhdst = nullptr;
}

void cls_webu::start_daemon_port2()
{
    if ((app->cfg->webcontrol_port2 == 0 ) ||
        (app->cfg->webcontrol_port2 == app->cfg->webcontrol_port)) {
        return;
    }

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , _("Starting secondary webcontrol on port %d")
        , app->cfg->webcontrol_port2);

    mhdst = new ctx_mhdstart;

    mhd_loadfile(app->cfg->webcontrol_cert, mhdst->tls_cert);
    mhd_loadfile(app->cfg->webcontrol_key, mhdst->tls_key);
    mhdst->ipv6 = app->cfg->webcontrol_ipv6;
    mhdst->tls_use = false;

    if (app->cfg->webcontrol_tls) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("TLS will be disabled on webcontrol port %d")
            , app->cfg->webcontrol_port2);
    }

    mhdst->mhd_ops =(struct MHD_OptionItem*)mymalloc(sizeof(struct MHD_OptionItem)*WEBUI_MHD_OPTS);
    mhd_opts();
    mhd_flags();

    wb_daemon2 = MHD_start_daemon (
        mhdst->mhd_flags
        , (uint16_t)app->cfg->webcontrol_port2
        , NULL, NULL
        , &mhd_answer, app
        , MHD_OPTION_ARRAY, mhdst->mhd_ops
        , MHD_OPTION_END);

    free(mhdst->mhd_ops);
    if (wb_daemon2 == nullptr) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO ,_("Unable to start port2 MHD"));
    } else {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,_("Started webcontrol on port %d")
            ,app->cfg->webcontrol_port2);
    }

    delete mhdst;
    mhdst = nullptr;

}

void cls_webu::webu_start()
{
    unsigned int randnbr;
    wb_daemon = nullptr;
    wb_daemon2 = nullptr;
    wb_finish = false;
    wb_clients.clear();

    memset(wb_digest_rand, 0, sizeof(wb_digest_rand));

    if (app->cfg->webcontrol_port == 0 ) {
        return;
    }

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , _("Starting webcontrol on port %d")
        , app->cfg->webcontrol_port);

    wb_headers = new ctx_params;
    wb_headers->update_params = true;
    util_parms_parse(wb_headers
        , "webcontrol_headers", app->cfg->webcontrol_headers);

    init_actions();

    srand((unsigned int)time(NULL));
    randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
    snprintf(wb_digest_rand, sizeof(wb_digest_rand),"%d",randnbr);

    start_daemon_port1();

    start_daemon_port2();
    cnct_cnt = 0;

}

void cls_webu::webu_stop()
{
    int chkcnt;

    wb_finish = true;

    MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Closing webcontrol"));

    chkcnt = 0;
    while ((chkcnt < 1000) && (cnct_cnt >0)) {
        SLEEP(0, 5000000);
        chkcnt++;
    }

    if (chkcnt>=1000){
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO, _("Excessive wait closing webcontrol"));
    }

    if (wb_daemon != nullptr) {
        MHD_stop_daemon (wb_daemon);
        wb_daemon = nullptr;
    }

    if (wb_daemon2 != nullptr) {
        MHD_stop_daemon (wb_daemon2);
        wb_daemon2 = nullptr;
    }

    delete wb_actions;
    delete wb_headers;

}

cls_webu::cls_webu(ctx_motapp *p_app)
{
    app = p_app;
    webu_start();
}

cls_webu::~cls_webu()
{
    webu_stop();
}
