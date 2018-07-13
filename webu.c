/*
 *    webu.c
 *
 *    Web User control interface control for motion.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    Portions of code from Angel Carpintero (motiondevelop@gmail.com)
 *    from webhttpd.c Copyright 2004-2005
 *
 *    Majority of module written by MrDave.
 *
 *    Function scheme:
 *      webu*      - All functions in this module have this prefix.
 *      webu_main  - Main entry point from the motion thread and only function exposed.
 *      webu_mhd*  - Functions related to libmicrohttd implementation
 *      webu_html* - Functions that create the display web page.
 *        webu_html_style*  - The style section of the web page
 *        webu_html_script* - The javascripts of the web page
 *        webu_html_navbar* - The navbar section of the web page
 *      webu_text* - Functions that create the text interface.
 *      webu_process_action - Performs most items under the action menu
 *      webu_process_config - Saves the parameter values into Motion.
 *
 *      Some function names are long and are not expected to contain any
 *      logger message that would display the function name to the user.
 *
 *      Functions are generally kept to under one page in length
 *
 *    To debug, run code, open page, view source and make copy of html
 *    into a local file to revise changes then determine applicable section(s)
 *    in this code to modify to match modified version.
 *
 *    Known Issues:
 *      The quit/restart uses signals and this should be reconsidered.
 *      The tracking is "best effort" since developer does not have tracking camera.
 *      The conf_cmdparse assumes that the pointers to the motion context for each
 *        camera are always sequential and enforcement of the pointers being sequential
 *        has not been observed in the other modules. (This is a legacy assumption)
 *      Need to investigate use of 'if (webui->uri_thread == NULL){'  The pointer should
 *        never be null until we exit.  We memset the memory pointed to with '\0'
 *      Should store the thread number as a number in the context
 *    Known HTML Issues:
 *      Single and double quotes are not consistently used.
 *      HTML ids do not follow any naming convention.
 *      After clicking restart/quit, do something..close page? Try to connect again?
 *
 *    Additional functionality considerations:
 *      Notification to user of items that require restart when changed.
 *      Notification to user that item successfully implemented (config change/tracking)
 *      List motion parms somewhere so they can be found by xgettext
 *
 *
 *
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "motion.h"
#include "webu.h"
#include "translate.h"
#include "picture.h"

/* Some defines of lengths for our buffers */
#define WEBUI_LEN_PARM 512          /* Parameters specified */
#define WEBUI_LEN_BUFF 1024         /* Buffer from the header */
#define WEBUI_LEN_RESP 1024         /* Our responses.  (Break up response if more space needed) */
#define WEBUI_LEN_SPRM 10           /* Shorter parameter buffer (method/protocol) */
#define WEBUI_LEN_URLI 512          /* Maximum URL permitted */
#define WEBUI_LEN_THRD 6            /* Maximum length for thread number e.g. 99999 */

struct webui_ctx {
    char *url;                   /* The URL sent from the client */
    char *uri_thread;            /* Parsed thread number from the url*/
    char *uri_cmd1;              /* Parsed command(action) from the url*/
    char *uri_cmd2;              /* Parsed command (set) from the url*/
    char *uri_parm1;             /* Parameter 1 for the command */
    char *uri_value1;            /* The value for parameter 1*/
    char *uri_parm2;             /* Parameter 2 for the command */
    char *uri_value2;            /* The value for parameter 2*/

    char *hostname;              /* Host name provided from header content*/
    char *clientip;              /* IP of the connecting client */
    char *userpass;              /* userpass as provided by the client */
    int   userpass_size;         /* Memory size of the userpass provided */
    int   cam_count;             /* Count of the number of cameras*/
    int   cam_threads;           /* Count of the number of camera threads running*/
    char *lang;                  /* Two character abbreviation for locale language*/
    char *lang_full;             /* Five character abbreviation for language-country*/

    struct context **cnt;
    char *resp_page;
    int   resp_size;
    int   resp_used;
    struct MHD_Connection *connection;
    int   mhd_first_connect;
};


static void webu_context_init(struct context **cnt, struct webui_ctx *webui) {

    int indx;

    webui->url           = NULL;
    webui->hostname      = NULL;

    webui->url         = mymalloc(WEBUI_LEN_URLI);
    webui->uri_thread  = mymalloc(WEBUI_LEN_THRD);
    webui->uri_cmd1    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_cmd2    = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm1   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value1  = mymalloc(WEBUI_LEN_PARM);
    webui->uri_parm2   = mymalloc(WEBUI_LEN_PARM);
    webui->uri_value2  = mymalloc(WEBUI_LEN_PARM);
    webui->clientip    = mymalloc(WEBUI_LEN_URLI);
    webui->hostname    = mymalloc(WEBUI_LEN_PARM);

    webui->lang          = mymalloc(3);    /* Two digit lang code plus null terminator */
    webui->lang_full     = mymalloc(6);    /* lang code, underscore, country plus null terminator */
    webui->resp_size     = WEBUI_LEN_RESP * 10;
    webui->resp_used     = 0;
    webui->resp_page     = mymalloc(webui->resp_size);
    webui->userpass_size = WEBUI_LEN_PARM;
    webui->userpass      = mymalloc(webui->userpass_size);

    /* get the number of cameras and threads */
    indx = 0;
    while (cnt[++indx]);

    webui->cam_threads = indx;

    webui->cam_count = indx;
    if (indx > 1)
        webui->cam_count--;

    /* 1 thread, 1 camera = just motion.conf.
     * 2 thread, 1 camera, then using motion.conf plus a separate camera file */

    snprintf(webui->lang_full, 6,"%s", getenv("LANGUAGE"));
    snprintf(webui->lang, 3,"%s",webui->lang_full);

    webui->cnt = cnt;

    return;
}

static void webu_context_null(struct webui_ctx *webui) {

    webui->url        = NULL;
    webui->hostname   = NULL;

    webui->uri_thread    = NULL;
    webui->uri_cmd1      = NULL;
    webui->uri_cmd2      = NULL;
    webui->uri_parm1     = NULL;
    webui->uri_value1    = NULL;
    webui->uri_parm2     = NULL;
    webui->uri_value2    = NULL;
    webui->lang          = NULL;
    webui->lang_full     = NULL;
    webui->resp_page     = NULL;
    webui->connection    = NULL;
    webui->userpass      = NULL;
    webui->clientip      = NULL;

    return;
}

static void webu_context_free(struct webui_ctx *webui) {

    if (webui->hostname      != NULL) free(webui->hostname);
    if (webui->url           != NULL) free(webui->url);
    if (webui->uri_thread    != NULL) free(webui->uri_thread);
    if (webui->uri_cmd1      != NULL) free(webui->uri_cmd1);
    if (webui->uri_cmd2      != NULL) free(webui->uri_cmd2);
    if (webui->uri_parm1     != NULL) free(webui->uri_parm1);
    if (webui->uri_value1    != NULL) free(webui->uri_value1);
    if (webui->uri_parm2     != NULL) free(webui->uri_parm2);
    if (webui->uri_value2    != NULL) free(webui->uri_value2);
    if (webui->lang          != NULL) free(webui->lang);
    if (webui->lang_full     != NULL) free(webui->lang_full);
    if (webui->resp_page     != NULL) free(webui->resp_page);
    if (webui->userpass      != NULL) free(webui->userpass);
    if (webui->clientip      != NULL) free(webui->clientip);

    webu_context_null(webui);

    free(webui);

    return;
}

static void webu_write(struct webui_ctx *webui, const char *buf) {

    int   resp_len;
    char *temp_resp;
    int   temp_size;

    resp_len = strlen(buf);

    temp_size = webui->resp_size;
    while ((resp_len + webui->resp_used) > temp_size){
        temp_size = temp_size + (WEBUI_LEN_RESP * 10);
    }

    if (temp_size > webui->resp_size){
        temp_resp = mymalloc(webui->resp_size);
        memcpy(temp_resp, webui->resp_page, webui->resp_size);
        free(webui->resp_page);
        webui->resp_page = mymalloc(temp_size);
        memset(webui->resp_page,'\0',temp_size);
        memcpy(webui->resp_page, temp_resp, webui->resp_size);
        webui->resp_size = temp_size;
        free(temp_resp);
    }

    memcpy(webui->resp_page + webui->resp_used, buf, resp_len);
    webui->resp_used = webui->resp_used + resp_len;

    return;
}

static void webu_html_badreq(struct webui_ctx *webui) {

    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "<p>Bad Request</p>\n"
        "<p>The server did not understand your request.</p>\n"
        "</body>\n"
        "</html>\n");
    webu_write(webui, response);

    return;

}

static void webu_text_badreq(struct webui_ctx *webui) {
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "Bad Request\n");
    webu_write(webui, response);

    return;
}

static void webu_url_decode(char *urlencoded, size_t length) {
    char *data = urlencoded;
    char *urldecoded = urlencoded;
    int scan_rslt;

    while (length > 0) {
        if (*data == '%') {
            char c[3];
            int i;
            data++;
            length--;
            c[0] = *data++;
            length--;
            c[1] = *data;
            c[2] = 0;

            scan_rslt = sscanf(c, "%x", &i);
            if (scan_rslt < 1) MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,"Error decoding");

            if (i < 128) {
                *urldecoded++ = (char)i;
            } else {
                *urldecoded++ = '%';
                *urldecoded++ = c[0];
                *urldecoded++ = c[1];
            }

        } else if (*data == '<' || *data == '+' || *data == '>') {
            *urldecoded++ = ' ';
        } else {
            *urldecoded++ = *data;
        }

        data++;
        length--;
    }
    *urldecoded = '\0';
}

static void webu_parseurl_parms(struct webui_ctx *webui, char *st_pos) {

    int parm_len, last_parm;
    char *en_pos;

    /* First parse out the "set","get","pan","tilt","x","y"
     * from the uri and put them into the cmd2.
     * st_pos is at the beginning of the command
     * If there is no ? then we are done parsing
     */
    last_parm = 0;
    en_pos = strstr(st_pos,"?");
    if (en_pos != NULL){
        parm_len = en_pos - st_pos + 1;
        if (parm_len >= WEBUI_LEN_PARM) return;
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);

        /* Get the parameter name */
        st_pos = st_pos + parm_len; /* Move past the command */
        en_pos = strstr(st_pos,"=");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len;
            last_parm = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return;
        snprintf(webui->uri_parm1, parm_len,"%s", st_pos);

        if (!last_parm){
            /* Get the parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value1, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter name */
            st_pos = st_pos + parm_len; /* Move past the command */
            en_pos = strstr(st_pos,"=");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_parm2, parm_len,"%s", st_pos);
        }

        if (!last_parm){
            /* Get the next parameter value */
            st_pos = st_pos + parm_len; /* Move past the equals sign */
            en_pos = strstr(st_pos,"&");
            if (en_pos == NULL){
                parm_len = strlen(webui->url) - parm_len;
                last_parm = 1;
            } else {
                parm_len = en_pos - st_pos + 1;
            }
            if (parm_len >= WEBUI_LEN_PARM) return;
            snprintf(webui->uri_value2, parm_len,"%s", st_pos);
        }

    }

}

static void webu_parseurl_reset(struct webui_ctx *webui) {

    /* Reset the variable to empty strings since they
     * are re-used across calls.  These are allocated
     * larger sizes to allow for multiple variable types.
     */
    memset(webui->uri_thread,'\0',WEBUI_LEN_THRD);
    memset(webui->uri_cmd1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_cmd2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value1,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_parm2,'\0',WEBUI_LEN_PARM);
    memset(webui->uri_value2,'\0',WEBUI_LEN_PARM);

}

static int webu_parseurl(struct webui_ctx *webui) {

    /* Sample: http://localhost:8080/0/config/set?log_level=6 */

    int retcd, parm_len, last_slash;
    char *st_pos, *en_pos;

    retcd = 0;

    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO, "Sent url: %s",webui->url);

    webu_parseurl_reset(webui);

    if (webui->url == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid url: %s",webui->url);
        return -1;
    }

    if (webui->url[0] != '/'){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid url: %s",webui->url);
        return -1;
    }

    webu_url_decode(webui->url, strlen(webui->url));

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, "Decoded url: %s",webui->url);

    /* Home page, nothing else */
    if (strlen(webui->url) == 1) return 0;

    last_slash = 0;

    /* Get thread number */
    st_pos = webui->url + 1; /* Move past the first "/" */
    if (*st_pos == '-') return -1; /* Never allow a negative thread number */
    en_pos = strstr(st_pos,"/");
    if (en_pos == NULL){
        parm_len = strlen(webui->url);
        last_slash = 1;
    } else {
        parm_len = en_pos - st_pos + 1;
    }
    if (parm_len >= WEBUI_LEN_THRD) return -1; /* var was malloc'd to WEBUI_LEN_THRD */
    snprintf(webui->uri_thread, parm_len,"%s", st_pos);

    if (!last_slash){
        /* Get cmd1 or action */
        st_pos = st_pos + parm_len; /* Move past the thread number */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len ;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
        snprintf(webui->uri_cmd1, parm_len,"%s", st_pos);
    }

    if (!last_slash){
        /* Get cmd2 or action */
        st_pos = st_pos + parm_len; /* Move past the first command */
        en_pos = strstr(st_pos,"/");
        if (en_pos == NULL){
            parm_len = strlen(webui->url) - parm_len;
            last_slash = 1;
        } else {
            parm_len = en_pos - st_pos + 1;
        }
        if (parm_len >= WEBUI_LEN_PARM) return -1; /* var was malloc'd to WEBUI_LEN_PARM */
        snprintf(webui->uri_cmd2, parm_len,"%s", st_pos);
    }

    if ((!strcmp(webui->uri_cmd1,"config") ||
         !strcmp(webui->uri_cmd1,"track") ) &&
        (strlen(webui->uri_cmd2) > 0)) {
        webu_parseurl_parms(webui, st_pos);
    }

    MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s< parm1:>%s< val1:>%s< parm2:>%s< val2:>%s<"
               ,webui->uri_thread
               ,webui->uri_cmd1, webui->uri_cmd2
               ,webui->uri_parm1, webui->uri_value1
               ,webui->uri_parm2, webui->uri_value2);


    return retcd;

}

static void webu_html_style_navbar(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    .navbar {\n"
        "      overflow: hidden;\n"
        "      background-color: #333;\n"
        "      font-family: Arial;\n"
        "    }\n"
        "    .navbar a {\n"
        "      float: left;\n"
        "      font-size: 16px;\n"
        "      color: white;\n"
        "      text-align: center;\n"
        "      padding: 14px 16px;\n"
        "      text-decoration: none;\n"
        "    }\n"
        "    .navbar a:hover, {\n"
        "      background-color: darkgray;\n"
        "    }\n");
    webu_write(webui, response);

}

static void webu_html_style_dropdown(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    .dropdown {\n"
        "      float: left;\n"
        "      overflow: hidden;\n"
        "    }\n"
        "    .dropdown .dropbtn {\n"
        "      font-size: 16px;\n"
        "      border: none;\n"
        "      outline: none;\n"
        "      color: white;\n"
        "      padding: 14px 16px;\n"
        "      background-color: inherit;\n"
        "      font-family: inherit;\n"
        "      margin: 0;\n"
        "    }\n"
        "    .dropdown-content {\n"
        "      display: none;\n"
        "      position: absolute;\n"
        "      background-color: #f9f9f9;\n"
        "      min-width: 160px;\n"
        "      box-shadow: 0px 8px 16px 0px rgba(0,0,0,0.2);\n"
        "      z-index: 1;\n"
        "    }\n"
        "    .dropdown-content a {\n"
        "      float: none;\n"
        "      color: black;\n"
        "      padding: 12px 16px;\n"
        "      text-decoration: none;\n"
        "      display: block;\n"
        "      text-align: left;\n"
        "    }\n"
        "    .dropdown-content a:hover {\n"
        "      background-color: lightgray;\n"
        "    }\n"
        "    .dropdown:hover .dropbtn {\n"
        "      background-color: darkgray;\n"
        "    }\n");
    webu_write(webui, response);
}

static void webu_html_style_input(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    input , select  {\n"
        "      width: 25%;\n"
        "      padding: 5px;\n"
        "      margin: 0;\n"
        "      display: inline-block;\n"
        "      border: 1px solid #ccc;\n"
        "      border-radius: 4px;\n"
        "      box-sizing: border-box;\n"
        "      height: 50%;\n"
        "      font-size: 75%;\n"
        "      margin-bottom: 5px;\n"
        "    }\n"
        "    .frm-input{\n"
        "      text-align:center;\n"
        "    }\n");
    webu_write(webui, response);
}

static void webu_html_style_base(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    * {margin: 0; padding: 0; }\n"
        "    body {\n"
        "      padding: 0;\n"
        "      margin: 0;\n"
        "      font-family: Arial, Helvetica, sans-serif;\n"
        "      font-size: 16px;\n"
        "      line-height: 1;\n"
        "      color: #606c71;\n"
        "      background-color: #159957;\n"
        "      background-image: linear-gradient(120deg, #155799, #159957);\n"
        "      margin-left:0.5% ;\n"
        "      margin-right:0.5% ;\n"
        "      width: device-width ;\n"
        "    }\n"
        "    img {\n"
        "      max-width: 100%;\n"
        "      max-height: 100%;\n"
        "      height: auto;\n"
        "    }\n"
        "    .page-header {\n"
        "      color: #fff;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "    }\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "    .page-header h4 {\n"
        "      height: 2px;\n"
        "      padding: 0;\n"
        "      margin: 1rem 0;\n"
        "      border: 0;\n"
        "    }\n"
        "    .main-content {\n"
        "      background-color: #000000;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "      font-size: 0.90em;\n"
        "    }\n"
        "    .header-right{\n"
        "      float: right;\n"
        "      color: white;\n"
        "    }\n"
        "    .header-center {\n"
        "      text-align: center;\n"
        "      color: white;\n"
        "      margin-top: 10px;\n"
        "      margin-bottom: 10px;\n"
        "    }\n");
    webu_write(webui, response);
}

static void webu_html_style(struct webui_ctx *webui) {
    /* Write out the style section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s", "  <style>\n");
    webu_write(webui, response);

    webu_html_style_base(webui);

    webu_html_style_navbar(webui);

    webu_html_style_input(webui);

    webu_html_style_dropdown(webui);

    snprintf(response, sizeof (response),"%s", "  </style>\n");
    webu_write(webui, response);

}

static void webu_html_head(struct webui_ctx *webui) {
    /* Write out the header section of the web page */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s","<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <title>Motion</title>\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    webu_write(webui, response);

    webu_html_style(webui);

    snprintf(response, sizeof (response),"%s", "</head>\n");
    webu_write(webui, response);

}

static void webu_html_navbar_camera(struct webui_ctx *webui) {
    /*Write out the options included in the camera dropdown */
    char response[WEBUI_LEN_RESP];
    int indx;

    if (webui->cam_threads == 1){
        /* Only Motion.conf file */
        if (webui->cnt[0]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_000');\">%s 1</a>\n"
                ,_("Cameras")
                ,_("Camera"));
            webu_write(webui, response);
        } else {
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_000');\">%s</a>\n"
                ,_("Cameras")
                ,webui->cnt[0]->conf.camera_name);
            webu_write(webui, response);
        }
    } else if (webui->cam_threads > 1){
        /* Motion.conf + separate camera.conf file */
        snprintf(response, sizeof (response),
            "    <div class=\"dropdown\">\n"
            "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
            "      <div id='cam_btn' class=\"dropdown-content\">\n"
            "        <a onclick=\"camera_click('cam_all');\">%s</a>\n"
            ,_("Cameras")
            ,_("All"));
        webu_write(webui, response);

        for (indx=1;indx <= webui->cam_count;indx++){
            if (webui->cnt[indx]->conf.camera_name == NULL){
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%03d');\">%s %d</a>\n"
                    , indx, _("Camera"), indx);
            } else {
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%03d');\">%s</a>\n",
                    indx, webui->cnt[indx]->conf.camera_name
                );
            }
            webu_write(webui, response);
        }
    }

    snprintf(response, sizeof (response),"%s",
        "      </div>\n"
        "    </div>\n");
    webu_write(webui, response);

}

static void webu_html_navbar_action(struct webui_ctx *webui) {
    /* Write out the options included in the actions dropdown*/
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "    <div class=\"dropdown\">\n"
        "      <button onclick='display_actions()' id=\"act_drop\" class=\"dropbtn\">%s</button>\n"
        "      <div id='act_btn' class=\"dropdown-content\">\n"
        "        <a onclick=\"action_click('/action/makemovie');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/snapshot');\">%s</a>\n"
        "        <a onclick=\"action_click('config');\">%s</a>\n"
        "        <a onclick=\"action_click('/config/write');\">%s</a>\n"
        "        <a onclick=\"action_click('track');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/pause');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/start');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/restart');\">%s</a>\n"
        "      </div>\n"
        "    </div>\n"
        ,_("Action")
        ,_("Make Movie")
        ,_("Snapshot")
        ,_("Change Configuration")
        ,_("Write Configuration")
        ,_("Tracking")
        ,_("Pause")
        ,_("Start")
        ,_("Restart"));
    webu_write(webui, response);
}

static void webu_html_navbar(struct webui_ctx *webui) {
    /* Write the navbar section*/
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "  <div class=\"navbar\">\n");
    webu_write(webui, response);

    webu_html_navbar_camera(webui);

    webu_html_navbar_action(webui);

    snprintf(response, sizeof (response),
        "    <a href=\"https://motion-project.github.io/motion_guide.html\" "
        " target=\"_blank\">%s</a>\n"
        "    <p class=\"header-right\">Motion "VERSION"</p>\n"
        "  </div>\n"
        ,_("Help"));
    webu_write(webui, response);

}

static void webu_html_config_notice(struct webui_ctx *webui) {

    char response[WEBUI_LEN_RESP];

    if (webui->cnt[0]->conf.webcontrol_parms == 0){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 0 (%s)</h4>\n"
            ,_("No Configuration Options"));
    } else if (webui->cnt[0]->conf.webcontrol_parms == 1){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 1 (%s)</h4>\n"
            ,_("Limited Configuration Options"));
    } else if (webui->cnt[0]->conf.webcontrol_parms == 2){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 2 (%s)</h4>\n"
            ,_("Advanced Configuration Options"));
    } else{
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 3 (%s)</h4>\n"
            ,_("Restricted Configuration Options"));
    }
    webu_write(webui, response);
}

static void webu_html_hdesc(struct webui_ctx *webui) {

    char response[WEBUI_LEN_RESP];

    if (webui->cam_threads == 1){
        snprintf(response, sizeof (response),
            "  <div id=\"id_header\">\n"
            "    <h3 id='h3_cam' data-cam=\"cam_all\" class='header-center'>%s (%s)</h3>\n"
            "  </div>\n"
            ,_("All Cameras")
            ,(!webui->cnt[0]->running)? _("Not running") :
                (webui->cnt[0]->lost_connection)? _("Lost connection"):
                (webui->cnt[0]->pause)? _("Paused"):_("Active")
            );
        webu_write(webui,response);
    } else {
        snprintf(response, sizeof (response),
            "  <div id=\"id_header\">\n"
            "    <h3 id='h3_cam' data-cam=\"cam_all\" class='header-center'>%s</h3>\n"
            "  </div>\n"
            ,_("All Cameras"));
        webu_write(webui,response);
    }
}

static void webu_html_config(struct webui_ctx *webui) {

    /* Write out the options to put into the config dropdown
     * We use html data attributes to store the values for the options
     * We always set a cam_all attribute and if the value if different for
     * any of our cameras, then we also add a cam_xxx which has the config
     * value for camera xxx  The javascript then decodes these to display
     */

    char response[WEBUI_LEN_RESP];
    int indx_parm, indx, diff_vals;
    const char *val_main, *val_thread;
    char *val_temp;


    snprintf(response, sizeof (response),"%s",
        "  <div id='cfg_form' style=\"display:none\">\n");
    webu_write(webui, response);

    webu_html_config_notice(webui);

    snprintf(response, sizeof (response),
        "    <form class=\"frm-input\">\n"
        "      <select id='cfg_parms' name='onames' "
        " autocomplete='off' onchange='config_change();'>\n"
        "        <option value='default' data-cam_all=\"\" >%s</option>\n"
        ,_("Select option"));
    webu_write(webui, response);

    /* The config_params[indx_parm].print reuses the buffer so create a
     * temporary variable for storing our parameter from main to compare
     * to the thread specific value
     */
    val_temp=malloc(PATH_MAX);
    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER)){
            indx_parm++;
            continue;
        }

        val_main = config_params[indx_parm].print(webui->cnt, NULL, indx_parm, 0);

        snprintf(response, sizeof (response),
            "        <option value='%s' data-cam_all=\""
            , config_params[indx_parm].param_name);
        webu_write(webui, response);

        memset(val_temp,'\0',PATH_MAX);
        if (val_main != NULL){
            snprintf(response, sizeof (response),"%s", val_main);
            webu_write(webui, response);
            snprintf(val_temp, PATH_MAX,"%s", val_main);
        }

        if (webui->cam_threads > 1){
            for (indx=1;indx <= webui->cam_count;indx++){
                val_thread=config_params[indx_parm].print(webui->cnt, NULL, indx_parm, indx);
                diff_vals = 0;
                if (((strlen(val_temp) == 0) && (val_thread == NULL)) ||
                    ((strlen(val_temp) != 0) && (val_thread == NULL))) {
                    diff_vals = 0;
                } else if (((strlen(val_temp) == 0) && (val_thread != NULL)) ) {
                    diff_vals = 1;
                } else {
                    if (strcasecmp(val_temp, val_thread)) diff_vals = 1;
                }
                if (diff_vals){
                    snprintf(response, sizeof (response),"%s","\" \\ \n");
                    webu_write(webui, response);

                    snprintf(response, sizeof (response),
                        "           data-cam_%03d=\"",indx);
                    webu_write(webui, response);
                    if (val_thread != NULL){
                        snprintf(response, sizeof (response),"%s%s", response, val_thread);
                        webu_write(webui, response);
                    }
                }
            }
        }
        /* Terminate the open quote and option.  For foreign language put hint in ()  */
        if (!strcasecmp(webui->lang,"en") ||
            !strcasecmp(config_params[indx_parm].param_name
                ,_(config_params[indx_parm].param_name))){
            snprintf(response, sizeof (response),"\" >%s</option>\n",
                config_params[indx_parm].param_name);
            webu_write(webui, response);
        } else {
            snprintf(response, sizeof (response),"\" >%s (%s)</option>\n",
                config_params[indx_parm].param_name
                ,_(config_params[indx_parm].param_name));
            webu_write(webui, response);
        }

        indx_parm++;
    }

    free(val_temp);

    snprintf(response, sizeof (response),
        "      </select>\n"
        "      <input type=\"text\"   id=\"cfg_value\" >\n"
        "      <input type='button' id='cfg_button' value='%s' onclick='config_click()'>\n"
        "    </form>\n"
        "  </div>\n"
        ,_("Save"));
    webu_write(webui, response);

}

static void webu_html_track(struct webui_ctx *webui) {

    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "  <div id='trk_form' style='display:none'>\n"
        "    <form class='frm-input'>\n"
        "      <select id='trk_option' name='trkopt'  autocomplete='off' "
        " style='width:20%%' onchange='track_change();'>\n"
        "        <option value='pan/tilt' data-trk='pan' >%s</option>\n"
        "        <option value='absolute' data-trk='abs' >%s</option>\n"
        "        <option value='center' data-trk='ctr' >%s</option>\n"
        "      </select>\n"
        "      <label id='trk_lblpan' style='color:white; display:inline' >%s</label>\n"
        "      <label id='trk_lblx'   style='color:white; display:none' >X</label>\n"
        "      <input type='text'   id='trk_panx' style='width:10%%' >\n"
        "      <label id='trk_lbltilt' style='color:white; display:inline' >%s</label>\n"
        "      <label id='trk_lbly'   style='color:white; display:none' >Y</label>\n"
        "      <input type='text'   id='trk_tilty' style='width:10%%' >\n"
        "      <input type='button' id='trk_button' value='%s' "
        " style='width:10%%' onclick='track_click()'>\n"
        "    </form>\n"
        "  </div>\n"
        ,_("Pan/Tilt")
        ,_("Absolute Change")
        ,_("Center")
        ,_("Pan")
        ,_("Tilt")
        ,_("Save"));
    webu_write(webui, response);

}

static void webu_html_preview(struct webui_ctx *webui) {

    /* Write the initial version of the preview section.  The javascript
     * will change this section when user selects a different camera */
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, strm_port;

    snprintf(response, sizeof (response),"%s",
        "  <div id=\"liveview\">\n"
        "    <section class=\"main-content\">\n"
        "      <br>\n"
        "      <p id=\"id_preview\">\n");
    webu_write(webui, response);

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (webui->cnt[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","      <br>\n");
            webu_write(webui, response);
        }
        strm_port = webui->cnt[indx]->conf.stream_port;
        if (webui->cnt[indx]->conf.substream_port) strm_port = webui->cnt[indx]->conf.substream_port;

        snprintf(response, sizeof (response),
            "      <a href=http://%s:%d> <img src=http://%s:%d/ border=0 width=%d%%></a>\n",
            webui->hostname, webui->cnt[indx]->conf.stream_port,webui->hostname,
            strm_port, webui->cnt[indx]->conf.stream_preview_scale);
        webu_write(webui, response);
    }

    snprintf(response, sizeof (response),"%s",
        "      </p>\n"
        "      <br>\n"
        "    </section>\n"
        "  </div>\n");
    webu_write(webui, response);

}

static void webu_html_script_action(struct webui_ctx *webui) {
    /* Write the javascript action_click() function.
     * We do not have a good notification section on the page so the successful
     * submission and response is currently a empty if block for the future
     * enhancement to somehow notify the user everything worked */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function event_reloadpage() {\n"
        "      window.location.reload();\n"
        "    }\n\n"
    );
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "    function action_click(actval) {\n"
        "      if (actval == \"config\"){\n"
        "        document.getElementById('trk_form').style.display=\"none\";\n"
        "        document.getElementById('cfg_form').style.display=\"inline\";\n"
        "      } else if (actval == \"track\"){\n"
        "        document.getElementById('cfg_form').style.display=\"none\";\n"
        "        document.getElementById('trk_form').style.display=\"inline\";\n"
        "      } else {\n"
        "        document.getElementById('cfg_form').style.display=\"none\";\n"
        "        document.getElementById('trk_form').style.display=\"none\";\n"
        "        var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "        var camnbr = camstr.substring(4,7);\n"
        "        var http = new XMLHttpRequest();\n"
        "        if ((actval == \"/detection/pause\") || (actval == \"/detection/start\")) {\n"
        "          http.addEventListener('load', event_reloadpage); \n"
        "        }\n"
    );
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "        var url = \"http://%s:%d/\"; \n",
        webui->hostname,webui->cnt[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "        if (camnbr == \"all\"){\n"
        "          url = url + \"0\";\n"
        "        } else {\n"
        "          url = url + camnbr;\n"
        "        }\n"
        "        url = url + actval;\n"
        "        http.open(\"GET\", url, true);\n"
        "        http.onreadystatechange = function() {\n"
        "          if(http.readyState == 4 && http.status == 200) {\n"

        "          }\n"
        "        }\n"
        "        http.send(null);\n"
        "      }\n"
        "      document.getElementById('act_btn').style.display=\"none\"; \n"
        "      document.getElementById('cfg_value').value = '';\n"
        "      document.getElementById('cfg_parms').value = 'default';\n"
        "    }\n\n");
    webu_write(webui, response);
}

static void webu_html_script_camera_thread(struct webui_ctx *webui) {
    /* Write the javascript thread IF conditions of camera_click() function */
    char response[WEBUI_LEN_RESP];
    int  strm_port;
    int indx, indx_st;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        snprintf(response, sizeof (response),
            "      if (camid == \"cam_%03d\"){\n",indx);
        webu_write(webui, response);

        strm_port = webui->cnt[indx]->conf.stream_port;
        if (webui->cnt[indx]->conf.substream_port) strm_port = webui->cnt[indx]->conf.substream_port;

        snprintf(response, sizeof (response),
            "        preview=\"<a href=http://%s:%d> "
            " <img src=http://%s:%d/ border=0></a>\"  \n",
            webui->hostname, webui->cnt[indx]->conf.stream_port, webui->hostname, strm_port);
        webu_write(webui, response);

        if (webui->cnt[indx]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s %d (%s)</h3>\"\n"
                ,_("Camera")
                , indx
                ,(!webui->cnt[indx]->running)? _("Not running") :
                 (webui->cnt[indx]->lost_connection)? _("Lost connection"):
                 (webui->cnt[indx]->pause)? _("Paused"):_("Active")
             );
        } else {
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s (%s)</h3>\"\n"
                , webui->cnt[indx]->conf.camera_name
                ,(!webui->cnt[indx]->running)? _("Not running") :
                 (webui->cnt[indx]->lost_connection)? _("Lost connection"):
                 (webui->cnt[indx]->pause)? _("Paused"):_("Active")
                );
        }
        webu_write(webui, response);

        snprintf(response, sizeof (response),"%s","      }\n");
        webu_write(webui, response);
    }

    return;
}

static void webu_html_script_camera_all(struct webui_ctx *webui) {
    /* Write the javascript "All" IF condition of camera_click() function */
    char response[WEBUI_LEN_RESP];
    int  strm_port;
    int indx, indx_st;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    snprintf(response, sizeof (response), "      if (camid == \"cam_all\"){\n");
    webu_write(webui, response);

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (indx == indx_st){
            snprintf(response, sizeof (response),"%s","        preview = \"\";\n");
            webu_write(webui, response);
        }
        strm_port = webui->cnt[indx]->conf.stream_port;
        if (webui->cnt[indx]->conf.substream_port) strm_port = webui->cnt[indx]->conf.substream_port;

        if (webui->cnt[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","    preview = preview + \"      <br>\";\n ");
            webu_write(webui, response);
        }
        snprintf(response, sizeof (response),
            "        preview = preview + \"<a href=http://%s:%d> "
            " <img src=http://%s:%d/ border=0 width=%d%%></a>\"; \n",
            webui->hostname, webui->cnt[indx]->conf.stream_port,
            webui->hostname, strm_port,webui->cnt[indx]->conf.stream_preview_scale);
        webu_write(webui, response);
    }

    snprintf(response, sizeof (response),
        "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
        " class='header-center' >%s</h3>\"\n"
        "      }\n"
        ,_("All Cameras"));
    webu_write(webui, response);

    return;
}

static void webu_html_script_camera(struct webui_ctx *webui) {
    /* Write the javascript camera_click() function */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function camera_click(camid) {\n"
        "      var preview = \"\";\n"
        "      var header = \"\";\n");
    webu_write(webui, response);

    webu_html_script_camera_thread(webui);

    webu_html_script_camera_all(webui);

    snprintf(response, sizeof (response),"%s",
        "      document.getElementById(\"id_preview\").innerHTML = preview; \n"
        "      document.getElementById(\"id_header\").innerHTML = header; \n"
        "      document.getElementById('cfg_form').style.display=\"none\"; \n"
        "      document.getElementById('trk_form').style.display=\"none\"; \n"
        "      document.getElementById('cam_btn').style.display=\"none\"; \n"
        "      document.getElementById('cfg_value').value = '';\n"
        "      document.getElementById('cfg_parms').value = 'default';\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script_menucam(struct webui_ctx *webui) {
    /* Write the javascript display_cameras() function */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function display_cameras() {\n"
        "      document.getElementById('act_btn').style.display = 'none';\n"
        "      if (document.getElementById('cam_btn').style.display == 'block'){\n"
        "        document.getElementById('cam_btn').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('cam_btn').style.display = 'block';\n"
        "      }\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script_menuact(struct webui_ctx *webui) {
    /* Write the javascript display_actions() function */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function display_actions() {\n"
        "      document.getElementById('cam_btn').style.display = 'none';\n"
        "      if (document.getElementById('act_btn').style.display == 'block'){\n"
        "        document.getElementById('act_btn').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('act_btn').style.display = 'block';\n"
        "      }\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script_evtclk(struct webui_ctx *webui) {
    /* Write the javascript 'click' EventListener */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    document.addEventListener('click', function(event) {\n"
        "      const dropCam = document.getElementById('cam_drop');\n"
        "      const dropAct = document.getElementById('act_drop');\n"
        "      if (!dropCam.contains(event.target) && !dropAct.contains(event.target)) {\n"
        "        document.getElementById('cam_btn').style.display = 'none';\n"
        "        document.getElementById('act_btn').style.display = 'none';\n"
        "      }\n"
        "    });\n\n");
    webu_write(webui, response);

}

static void webu_html_script_cfgclk(struct webui_ctx *webui) {
    /* Write the javascript config_click function
     * We do not have a good notification section on the page so the successful
     * submission and response is currently a empty if block for the future
     * enhancement to somehow notify the user everything worked */

    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function config_click() {\n"
        "      var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var camnbr = camstr.substring(4,7);\n"
        "      var opts = document.getElementById('cfg_parms');\n"
        "      var optsel = opts.options[opts.selectedIndex].value;\n"
        "      var baseval = document.getElementById('cfg_value').value;\n"
        "      var http = new XMLHttpRequest();\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      var url = \"http://%s:%d/\"; \n",
        webui->hostname,webui->cnt[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "      var optval=encodeURI(baseval);\n"
        "      if (camnbr == \"all\"){\n"
        "        url = url + \"0\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        "      url = url + \"/config/set?\" + optsel + \"=\" + optval;\n"
        "      http.open(\"GET\", url, true);\n"
        "      http.onreadystatechange = function() {\n"
        "        if(http.readyState == 4 && http.status == 200) {\n"
        "        }\n"
        "      }\n"
        "      http.send(null);\n"
        "      document.getElementById('cfg_value').value = \"\";\n"
        "      opts.options[opts.selectedIndex].setAttribute('data-'+camstr,baseval);\n"
        "      opts.value = 'default';\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script_cfgchg(struct webui_ctx *webui) {
    /* Write the javascript option_change function */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function config_change() {\n"
        "      var camSel = 'data-'+ document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var opts = document.getElementById('cfg_parms');\n"
        "      var optval = opts.options[opts.selectedIndex].getAttribute(camSel);\n"
        "      if (optval == null){\n"
        "        optval = opts.options[opts.selectedIndex].getAttribute('data-cam_all');\n"
        "      }\n"
        "      document.getElementById('cfg_value').value = optval;\n"
        "    }\n\n");
    webu_write(webui, response);
}

static void webu_html_script_trkchg(struct webui_ctx *webui) {
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s",
        "    function track_change() {\n"
        "      var opts = document.getElementById('trk_option');\n"
        "      var optval = opts.options[opts.selectedIndex].getAttribute('data-trk');\n"
        "      if (optval == 'pan'){\n"
        "        document.getElementById('trk_panx').disabled=false;\n"
        "        document.getElementById('trk_tilty').disabled = false;\n"
        "        document.getElementById('trk_lblx').style.display='none';\n"
        "        document.getElementById('trk_lbly').style.display='none';\n"
        "        document.getElementById('trk_lblpan').style.display='inline';\n"
        "        document.getElementById('trk_lbltilt').style.display='inline';\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "      } else if (optval =='abs'){\n"
        "        document.getElementById('trk_panx').disabled=false;\n"
        "        document.getElementById('trk_tilty').disabled = false;\n"
        "        document.getElementById('trk_lblx').value = 'X';\n"
        "        document.getElementById('trk_lbly').value = 'Y';\n"
        "        document.getElementById('trk_lblpan').style.display='none';\n"
        "        document.getElementById('trk_lbltilt').style.display='none';\n"
        "        document.getElementById('trk_lblx').style.display='inline';\n"
        "        document.getElementById('trk_lbly').style.display='inline';\n");
    webu_write(webui, response);

   snprintf(response, sizeof (response),"%s",
        "      } else {\n"
        "        document.getElementById('cfg_form').style.display='none';\n"
        "        document.getElementById('trk_panx').disabled=true;\n"
        "        document.getElementById('trk_tilty').disabled = true;\n"
        "      }\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script_trkclk(struct webui_ctx *webui) {
    char response[WEBUI_LEN_RESP];
    snprintf(response, sizeof (response),"%s",
        "    function track_click() {\n"
        "      var camstr = document.getElementById('h3_cam').getAttribute('data-cam');\n"
        "      var camnbr = camstr.substring(4,7);\n"
        "      var opts = document.getElementById('trk_option');\n"
        "      var optsel = opts.options[opts.selectedIndex].getAttribute('data-trk');\n"
        "      var optval1 = document.getElementById('trk_panx').value;\n"
        "      var optval2 = document.getElementById('trk_tilty').value;\n"
        "      var http = new XMLHttpRequest();\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      var url = \"http://%s:%d/\"; \n",
        webui->hostname,webui->cnt[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
        "      if (camnbr == \"all\"){\n"
        "        url = url + \"0\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        "      if (optsel == 'pan'){\n"
        "        url = url + '/track/set?pan=' + optval1 + '&tilt=' + optval2;\n"
        "      } else if (optsel == 'abs') {\n"
        "        url = url + '/track/set?x=' + optval1 + '&y=' + optval2;\n"
        "      } else {\n"
        "        url = url + '/track/center'\n"
        "      }\n"
        "      http.open(\"GET\", url, true);\n"
        "      http.onreadystatechange = function() {\n"
        "        if(http.readyState == 4 && http.status == 200) {\n"
        "         }\n"
        "      }\n"
        "      http.send(null);\n"
        "    }\n\n");
    webu_write(webui, response);

}

static void webu_html_script(struct webui_ctx *webui) {
    /* Write the javascripts */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s", "  <script>\n");
    webu_write(webui, response);

    webu_html_script_action(webui);

    webu_html_script_camera(webui);

    webu_html_script_cfgclk(webui);

    webu_html_script_cfgchg(webui);

    webu_html_script_trkclk(webui);

    webu_html_script_trkchg(webui);

    webu_html_script_menucam(webui);

    webu_html_script_menuact(webui);

    webu_html_script_evtclk(webui);

    snprintf(response, sizeof (response),"%s", "  </script>\n");
    webu_write(webui, response);

}

static void webu_html_body(struct webui_ctx *webui) {
    /* Write the body section of the form */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),"%s","<body class=\"body\">\n");
    webu_write(webui, response);

    webu_html_navbar(webui);

    webu_html_hdesc(webui);

    webu_html_config(webui);

    webu_html_track(webui);

    webu_html_preview(webui);

    webu_html_script(webui);

    snprintf(response, sizeof (response),"%s", "</body>\n");
    webu_write(webui, response);

}

static void webu_html_page(struct webui_ctx *webui) {
    /* Write the main page html */
    char response[WEBUI_LEN_RESP];

    snprintf(response, sizeof (response),
        "<!DOCTYPE html>\n"
        "<html lang=\"%s\">\n",webui->lang);
    webu_write(webui, response);

    webu_html_head(webui);

    webu_html_body(webui);

    snprintf(response, sizeof (response),"%s", "</html>\n");
    webu_write(webui, response);

}

static void webu_process_action(struct webui_ctx *webui) {

    int thread_nbr, indx;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx = 0;
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        if (thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cnt[++indx])
            webui->cnt[indx]->makemovie = 1;
        } else {
            webui->cnt[thread_nbr]->makemovie = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        if (thread_nbr == 0 && webui->cam_threads > 1) {
            while (webui->cnt[++indx])
            webui->cnt[indx]->snapshot = 1;
        } else {
            webui->cnt[thread_nbr]->snapshot = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"restart")){
        /* This is the legacy method...(we can do better than signals..).*/
        if (thread_nbr == 0) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "httpd is going to restart");
            kill(getpid(),SIGHUP);
            webui->cnt[0]->webcontrol_finish = TRUE;
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
                "httpd is going to restart thread %d",thread_nbr);
            if (webui->cnt[thread_nbr]->running) {
                webui->cnt[thread_nbr]->makemovie = 1;
                webui->cnt[thread_nbr]->finish = 1;
            }
            webui->cnt[thread_nbr]->restart = 1;
        }
    } else if (!strcmp(webui->uri_cmd2,"start")){
        if (thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cnt[indx]->pause = 0;
            } while (webui->cnt[++indx]);
        } else {
            webui->cnt[thread_nbr]->pause = 0;
        }
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        if (thread_nbr == 0 && webui->cam_threads > 1) {
            do {
                webui->cnt[indx]->pause = 1;
            } while (webui->cnt[++indx]);
        } else {
            webui->cnt[thread_nbr]->pause = 1;
        }
    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        conf_print(webui->cnt);
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            "Invalid action requested: %s",webui->uri_cmd2);
        return;
    }
}

static void webu_process_config(struct webui_ctx *webui) {

    int thread_nbr, indx;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }
    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (strcmp(webui->uri_cmd2, "set")) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid command request: %s",webui->uri_cmd2);
        return;
    }

    indx=0;
    while (config_params[indx].param_name != NULL) {
        if (((thread_nbr != 0) && (config_params[indx].main_thread)) ||
            (config_params[indx].webui_level > webui->cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx].webui_level == WEBUI_LEVEL_NEVER) ) {
            indx++;
            continue;
        }
        if (!strcmp(webui->uri_parm1, config_params[indx].param_name)) break;
        indx++;
    }
    if (config_params[indx].param_name != NULL){
        if (strlen(webui->uri_parm1) > 0){
            /* This is legacy assumption on the pointers being sequential*/
            conf_cmdparse(webui->cnt + thread_nbr, config_params[indx].param_name, webui->uri_value1);

            /*If we are updating vid parms, set the flag to update the device.*/
            if (!strcmp(config_params[indx].param_name, "vid_control_params") &&
                (webui->cnt[thread_nbr]->vdev != NULL)) webui->cnt[thread_nbr]->vdev->update_parms = TRUE;

            /* If changing language, do it now */
            if (!strcmp(config_params[indx].param_name, "native_language")){
                nls_enabled = webui->cnt[thread_nbr]->conf.native_language;
                if (nls_enabled){
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Native Language : on"));
                } else {
                    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,"Native Language : off");
                }
            }
        } else {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,"set the value to null/zero");
        }
    }

    return;

}

static void webu_process_track(struct webui_ctx *webui) {

    int thread_nbr;
    struct coord cent;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (!strcmp(webui->uri_cmd2, "center")) {
        webui->cnt[thread_nbr]->moved = track_center(webui->cnt[thread_nbr], 0, 1, 0, 0);
    } else if (!strcmp(webui->uri_cmd2, "set")) {
        if (!strcmp(webui->uri_parm1, "pan")) {
            cent.width = webui->cnt[thread_nbr]->imgs.width;
            cent.height = webui->cnt[thread_nbr]->imgs.height;
            cent.x = atoi(webui->uri_value1);
            cent.y = 0;
            webui->cnt[thread_nbr]->moved = track_move(webui->cnt[thread_nbr],
                                            webui->cnt[thread_nbr]->video_dev,
                                            &cent, &webui->cnt[thread_nbr]->imgs, 1);

            cent.width = webui->cnt[thread_nbr]->imgs.width;
            cent.height = webui->cnt[thread_nbr]->imgs.height;
            cent.x = 0;
            cent.y = atoi(webui->uri_value2);
            webui->cnt[thread_nbr]->moved = track_move(webui->cnt[thread_nbr],
                                            webui->cnt[thread_nbr]->video_dev,
                                            &cent, &webui->cnt[thread_nbr]->imgs, 1);

        } else if (!strcasecmp(webui->uri_parm1, "x")) {
            webui->cnt[thread_nbr]->moved = track_center(webui->cnt[thread_nbr]
                                                  , webui->cnt[thread_nbr]->video_dev, 1
                                                  , atoi(webui->uri_value1)
                                                  , atoi(webui->uri_value2));
        }
    }

    return;

}

static int webu_html_main(struct webui_ctx *webui) {

    /* Note some detection and config requested actions call the
     * action function.  This is because the legacy interface
     * put these into those pages.  We put them together here
     * based upon the structure of the new interface
     */
    int retcd;

    /*
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s<"
       " parm1:>%s< val1:>%s<"
       " parm2:>%s< val2:>%s<"
       ,webui->uri_thread
       ,webui->uri_cmd1, webui->uri_cmd2
       ,webui->uri_parm1, webui->uri_value1
       ,webui->uri_parm2, webui->uri_value2);
    */


    retcd = 0;

    if (strlen(webui->uri_thread) == 0){
        webu_html_page(webui);
    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"set"))) {
        webu_process_config(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"action")){
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"detection")){
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        webu_process_track(webui);

    } else{
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid action requested");
        retcd = -1;
    }

    return retcd;
}

static void webu_text_page(struct webui_ctx *webui) {
    /* Write the main page text */
    char response[WEBUI_LEN_RESP];
    int indx;

    snprintf(response, sizeof (response),
        "Motion "VERSION" Running [%d] Camera%s\n0\n",
        webui->cam_count, (webui->cam_count > 1 ? "s" : ""));
    webu_write(webui, response);

    if (webui->cam_threads > 1){
        for (indx = 1; indx < webui->cam_threads; indx++) {
            snprintf(response, sizeof (response), "%d\n", indx);
            webu_write(webui, response);
        }
    }
}

static void webu_text_list(struct webui_ctx *webui) {
    /* Write out the options and values */
    char response[WEBUI_LEN_RESP];
    int thread_nbr, indx_parm;
    const char *val_parm;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            ((thread_nbr != 0) && (config_params[indx_parm].main_thread != 0))){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(webui->cnt, NULL, indx_parm, thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(webui->cnt, NULL, indx_parm, 0);
        }
        snprintf(response, sizeof (response),"  %s = %s \n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        webu_write(webui, response);

        indx_parm++;
    }

}

static void webu_text_get(struct webui_ctx *webui) {
    /* Write out the option value for one parm */
    char response[WEBUI_LEN_RESP];
    int indx_parm, thread_nbr;
    const char *val_parm;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cnt[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER) ||
            strcmp(webui->uri_parm1,"query") ||
            strcmp(webui->uri_value1, config_params[indx_parm].param_name)){
            indx_parm++;
            continue;
        }

        val_parm = config_params[indx_parm].print(webui->cnt, NULL, indx_parm, thread_nbr);
        if (val_parm == NULL){
            val_parm = config_params[indx_parm].print(webui->cnt, NULL, indx_parm, 0);
        }

        snprintf(response, sizeof (response),"%s = %s \nDone\n"
            ,config_params[indx_parm].param_name
            ,val_parm);
        webu_write(webui, response);

        break;
    }

}

static void webu_text_quit(struct webui_ctx *webui) {
    /* Write out the option value for one parm */
    char response[WEBUI_LEN_RESP];
    int thread_nbr;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }

    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    /* This is the legacy method...(we can do better than signals..).*/
    if (thread_nbr == 0) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO, "httpd quits");
        kill(getpid(),SIGQUIT);
        webui->cnt[0]->webcontrol_finish = TRUE;
    } else {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
            "httpd quits thread %d",thread_nbr);
        webui->cnt[thread_nbr]->restart = 0;
        webui->cnt[thread_nbr]->makemovie = 1;
        webui->cnt[thread_nbr]->finish = 1;
    }

    snprintf(response,sizeof(response)
        ,"quit in progress ... bye \nDone\n");
    webu_write(webui, response);

}

static void webu_text_status(struct webui_ctx *webui) {
    /* Write out the pause/active status */

    char response[WEBUI_LEN_RESP];
    int indx, indx_st, thread_nbr;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }
    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (thread_nbr == 0){
        for (indx = indx_st; indx < webui->cam_threads; indx++) {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s\n"
            ,webui->cnt[indx]->conf.camera_id
            ,(!webui->cnt[indx]->running)? "NOT RUNNING":
            (webui->cnt[indx]->pause)? "PAUSE":"ACTIVE");
            webu_write(webui, response);
        }
    } else {
        snprintf(response, sizeof(response),
            "Camera %d Detection status %s\n"
            ,webui->cnt[thread_nbr]->conf.camera_id
            ,(!webui->cnt[thread_nbr]->running)? "NOT RUNNING":
            (webui->cnt[thread_nbr]->pause)? "PAUSE":"ACTIVE");
        webu_write(webui, response);
    }

}

static void webu_text_connection(struct webui_ctx *webui) {
    /* Write out the connection status */
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, thread_nbr;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    if (webui->uri_thread == NULL){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "NULL thread detected");
        return;
    }
    /* webui->cam_threads is a 1 based counter, thread_nbr is zero based */
    thread_nbr = atoi(webui->uri_thread);
    if (thread_nbr >= webui->cam_threads){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid thread specified");
        return;
    }

    if (thread_nbr == 0){
        for (indx = indx_st; indx < webui->cam_threads; indx++) {
            snprintf(response,sizeof(response)
                , "Camera %d%s%s %s\n"
                ,webui->cnt[indx]->conf.camera_id
                ,webui->cnt[indx]->conf.camera_name ? " -- " : ""
                ,webui->cnt[indx]->conf.camera_name ? webui->cnt[indx]->conf.camera_name : ""
                ,(!webui->cnt[indx]->running)? "NOT RUNNING" :
                (webui->cnt[indx]->lost_connection)? "Lost connection": "Connection OK");
            webu_write(webui, response);
        }
    } else {
        snprintf(response,sizeof(response)
            , "Camera %d%s%s %s\n"
            ,webui->cnt[thread_nbr]->conf.camera_id
            ,webui->cnt[thread_nbr]->conf.camera_name ? " -- " : ""
            ,webui->cnt[thread_nbr]->conf.camera_name ? webui->cnt[thread_nbr]->conf.camera_name : ""
            ,(!webui->cnt[thread_nbr]->running)? "NOT RUNNING" :
             (webui->cnt[thread_nbr]->lost_connection)? "Lost connection": "Connection OK");
        webu_write(webui, response);
    }

}

static void webu_text_set(struct webui_ctx *webui) {
    /* Write out the connection status */

    char response[WEBUI_LEN_RESP];

    webu_process_config(webui);

    snprintf(response,sizeof(response)
        , "%s = %s\nDone \n"
        ,webui->uri_parm1
        ,webui->uri_value1
    );
    webu_write(webui, response);

}

static void webu_text_action(struct webui_ctx *webui) {
    /* Call the start */
    char response[WEBUI_LEN_RESP];

    webu_process_action(webui);

    /* Send response message for action */
    if (!strcmp(webui->uri_cmd2,"makemovie")){
        snprintf(response,sizeof(response)
            ,"makemovie for thread %s \nDone\n"
            ,webui->uri_thread
        );
        webu_write(webui, response);
    } else if (!strcmp(webui->uri_cmd2,"snapshot")){
        snprintf(response,sizeof(response)
            ,"Snapshot for thread %s \nDone\n"
            ,webui->uri_thread
        );
        webu_write(webui, response);
    } else if (!strcmp(webui->uri_cmd2,"restart")){
        snprintf(response,sizeof(response)
            ,"Restart in progress ...\nDone\n");
        webu_write(webui, response);
    } else if (!strcmp(webui->uri_cmd2,"start")){
        snprintf(response,sizeof(response)
            ,"Camera %s Detection resumed\nDone \n"
            ,webui->uri_thread
        );
        webu_write(webui, response);
    } else if (!strcmp(webui->uri_cmd2,"pause")){
        snprintf(response,sizeof(response)
            ,"Camera %s Detection paused\nDone \n"
            ,webui->uri_thread
        );
        webu_write(webui, response);
    } else if ((!strcmp(webui->uri_cmd2,"write")) ||
               (!strcmp(webui->uri_cmd2,"writeyes"))){
        snprintf(response,sizeof(response)
            ,"Camera %s write\nDone \n"
            ,webui->uri_thread
        );
        webu_write(webui, response);
    } else {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            "Invalid action requested: %s",webui->uri_cmd2);
        return;
    }

}

static void webu_text_track(struct webui_ctx *webui) {
    /* Call the start */
    char response[WEBUI_LEN_RESP];

    webu_process_track(webui);
    snprintf(response,sizeof(response)
        ,"Camera %s \nTrack set %s\nDone \n"
        ,webui->uri_thread
        ,webui->uri_cmd2
    );
    webu_write(webui, response);

}

static int webu_text_main(struct webui_ctx *webui) {

    int retcd;

    /*
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO,
       "thread: >%s< cmd1: >%s< cmd2: >%s<"
       " parm1:>%s< val1:>%s<"
       " parm2:>%s< val2:>%s<"
       ,webui->uri_thread
       ,webui->uri_cmd1, webui->uri_cmd2
       ,webui->uri_parm1, webui->uri_value1
       ,webui->uri_parm2, webui->uri_value2);
    */

    retcd = 0;
    if (strlen(webui->uri_thread) == 0){
        webu_text_page(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"set"))) {
        webu_text_set(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_text_action(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"list"))) {
        webu_text_list(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"get"))) {
        webu_text_get(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"status"))) {
        webu_text_status(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"connection"))) {
        webu_text_connection(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"start"))) {
        webu_text_action(webui);

    } else if ((!strcmp(webui->uri_cmd1,"detection")) &&
               (!strcmp(webui->uri_cmd2,"pause"))) {
        webu_text_action(webui);

    } else if ((strcmp(webui->uri_cmd1,"action") == 0) &&
               (strcmp(webui->uri_cmd2,"quit") == 0)){
        webu_text_quit(webui);

    } else if (!strcmp(webui->uri_cmd1,"action")) {
        webu_text_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        webu_text_track(webui);

    } else{
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid action requested");
        retcd = -1;
    }

    return retcd;
}

static void webu_mhd_clientip(struct webui_ctx *webui) {

    const union MHD_ConnectionInfo *con_info;
    char client[WEBUI_LEN_URLI];
    const char *ipv6;
    struct sockaddr_in6 *con_socket;

    /* We are using MHD_DUAL_STACK which puts everything in IPV6 */
    con_info = MHD_get_connection_info(webui->connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    con_socket = (struct sockaddr_in6 *)con_info->client_addr;
    ipv6 = inet_ntop(AF_INET6, &con_socket->sin6_addr,client,WEBUI_LEN_URLI);
    if (ipv6 == NULL){
        snprintf(webui->clientip,WEBUI_LEN_URLI,"%s","Unknown");
    } else {
        if (strncmp(client,"::ffff:",7) == 0){
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client + 7);
        } else {
            snprintf(webui->clientip,WEBUI_LEN_URLI,"%s",client);
        }
    }

    MOTION_LOG(INF,TYPE_ALL, NO_ERRNO, "Connection from: %s",webui->clientip);

}

static void webu_mhd_hostname(struct webui_ctx *webui) {

    /* use the hostname the browser used to connect to us when
     * constructing links to the stream ports. If available
     * (which it is in all modern browsers) it is more likely to
     * work than the result of gethostname(), which is reliant on
     * the machine we're running on having it's hostname setup
     * correctly and corresponding DNS in place. */

    const char *hdr;
    char *en_pos;
    int host_len;

    hdr = MHD_lookup_connection_value (webui->connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_HOST);
    if (hdr != NULL){
        snprintf(webui->hostname, WEBUI_LEN_PARM, "%s", hdr);
        en_pos = strstr(webui->hostname, ":");
        if (en_pos != NULL){
            host_len = en_pos - webui->hostname + 1;
            snprintf(webui->hostname, host_len, "%s", hdr);
        }
    } else {
        gethostname(webui->hostname, WEBUI_LEN_PARM - 1);
    }

    return;
}

static int webu_mhd_digest(struct webui_ctx *webui) {
    int retcd,len_userpass;
    char *user, *pass, *col_pos;
    struct MHD_Response *response;
    const char *denied = "<html><head><title>Access denied</title></head><body>Access denied</body></html>";
    const char *opaque = "80fb23a1e3760a3d1c91cd060bd07f7a90877334";
    const char *realm = "Motion";

    if (webui->cnt[0]->conf.webcontrol_authentication == NULL){
        len_userpass = 0;
    } else {
        len_userpass = strlen(webui->cnt[0]->conf.webcontrol_authentication);
    }
    if (len_userpass == 0){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"No user:pass provided");
        snprintf(webui->userpass, webui->userpass_size,"%s","digest");
        return MHD_YES;
    }
    col_pos = strstr(webui->cnt[0]->conf.webcontrol_authentication,":");
    if (col_pos == NULL){
        pass = NULL;
        user = mymalloc(len_userpass+1);
        snprintf(user,len_userpass+1,"%s",webui->cnt[0]->conf.webcontrol_authentication);
    } else {
        user = mymalloc(len_userpass - strlen(col_pos) + 1);
        pass = mymalloc(strlen(col_pos));
        snprintf(user, len_userpass - strlen(col_pos) + 1, "%s"
            ,webui->cnt[0]->conf.webcontrol_authentication);
        snprintf(pass, strlen(col_pos), "%s", col_pos + 1);
    }

    user = MHD_digest_auth_get_username(webui->connection);
    if (user == NULL) {
        response = MHD_create_response_from_buffer(strlen(denied)
            ,(void *)denied, MHD_RESPMEM_PERSISTENT);
        retcd = MHD_queue_auth_fail_response(webui->connection, realm
            ,opaque, response, MHD_NO);
        MHD_destroy_response(response);
        free(user);
        free(pass);
        return retcd;
    }

    retcd = MHD_digest_auth_check(webui->connection, realm, user, pass, 300);
    free(user);
    free(pass);

    if (retcd == MHD_NO) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO ,"Failed authentication from %s", webui->clientip);
    }

    if ( (retcd == MHD_INVALID_NONCE) || (retcd == MHD_NO) )  {
        response = MHD_create_response_from_buffer(strlen (denied)
            ,(void *)denied, MHD_RESPMEM_PERSISTENT);
        if (response == NULL){
            return MHD_NO;
        }
        retcd = MHD_queue_auth_fail_response(webui->connection, realm
            ,opaque, response
            ,(retcd == MHD_INVALID_NONCE) ? MHD_YES : MHD_NO);
        MHD_destroy_response(response);
        return retcd;
    }

    snprintf(webui->userpass, webui->userpass_size,"%s","digest");

    return MHD_YES;

}

static int webu_mhd_basic(struct webui_ctx *webui) {
    /* Basic Authentication */
    int retcd;
    char *user, *pass;
    struct MHD_Response *response;
    const char *denied = "<html><head><title>Access denied</title></head><body>Access denied</body></html>";

    pass = NULL;
    user = NULL;
    retcd = MHD_YES;

    user = MHD_basic_auth_get_username_password (webui->connection, &pass);
    if ((user != NULL) && (pass != NULL)){
        if ((int)(strlen(pass) + strlen(user)+2) > webui->userpass_size){
            free(webui->userpass);
            webui->userpass_size = (strlen(pass) + strlen(user)+2);
            webui->userpass = malloc(webui->userpass_size);
        }
        snprintf(webui->userpass, webui->userpass_size,"%s:%s",user,pass);
    }

    if (strlen(webui->userpass) == 0){
        response = MHD_create_response_from_buffer (strlen (denied),
						  (void *) denied, MHD_RESPMEM_PERSISTENT);
        if (!response){
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid response");
            retcd = MHD_NO;
        } else {
            retcd = MHD_queue_basic_auth_fail_response (webui->connection,"Motion",response);
            MHD_destroy_response (response);
        }
    } else {
        if (strcmp(webui->userpass, webui->cnt[0]->conf.webcontrol_authentication) != 0){
            MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO ,"Failed authentication from %s",webui->clientip);
            response = MHD_create_response_from_buffer (strlen (denied),
						  (void *) denied, MHD_RESPMEM_PERSISTENT);
            if (!response){
                MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid response");
                retcd = MHD_NO;
            } else {
                retcd = MHD_queue_basic_auth_fail_response (webui->connection,"Motion",response);
                MHD_destroy_response (response);
            }
        } else {
            retcd = MHD_YES;
        }
    }

    if (pass != NULL) free(pass);
    if (user != NULL) free(user);

    if (retcd == MHD_NO){
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO, "Failed authentication from %s",webui->clientip);
    }

    return retcd;

}

static int webu_mhd_send(struct webui_ctx *webui) {

    int retcd;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer (strlen(webui->resp_page)
        ,(void *)webui->resp_page, MHD_RESPMEM_PERSISTENT);

    if (!response){
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, "Invalid response");
        return MHD_NO;
    }

    if (webui->cnt[0]->conf.webcontrol_cors_header != NULL){
        MHD_add_response_header (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN
            , webui->cnt[0]->conf.webcontrol_cors_header);
    }
    if (webui->cnt[0]->conf.webcontrol_interface == 1){
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain;");
    } else {
        MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
    }

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

static int webu_mhd_ans(void *cls
        , struct MHD_Connection *connection
        , const char *url
        , const char *method
        , const char *version
        , const char *upload_data
        , size_t     *upload_data_size
        , void **ptr) {

    int retcd;
    struct webui_ctx *webui = *ptr;

    /* Eliminate compiler warnings */
    (void)cls;
    (void)url;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;

    /* Per docs, this is called twice and we should process the second call */
    if (webui->mhd_first_connect) {
        webui->mhd_first_connect = FALSE;
        return MHD_YES;
    }

    if (strcmp (method, "GET") != 0){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"Invalid Method requested: %s",method);
        return MHD_NO;
    }

    webui->connection = connection;
    if (strlen(webui->clientip) == 0){
        webu_mhd_clientip(webui);
    }

    webu_mhd_hostname(webui);

    if ((webui->cnt[0]->conf.webcontrol_auth_method == 1) &&
        (strlen(webui->userpass) == 0)){
        retcd = webu_mhd_basic(webui);
        return retcd;
    } else if ((webui->cnt[0]->conf.webcontrol_auth_method == 2) &&
        (strlen(webui->userpass) == 0)){
        retcd = webu_mhd_digest(webui);
        return retcd;
    }

    retcd = 0;
    if (webui->cnt[0]->conf.webcontrol_interface == 1){
        if (retcd == 0) retcd = webu_text_main(webui);
        if (retcd <  0) webu_text_badreq(webui);
        retcd = webu_mhd_send(webui);
    } else {
        if (retcd == 0) retcd = webu_html_main(webui);
        if (retcd <  0) webu_html_badreq(webui);
        retcd = webu_mhd_send(webui);
    }

    if (retcd == MHD_NO){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"send page failed %d",retcd);
    }
    return retcd;

}

static void * webu_mhd_init(void *cls, const char *uri, struct MHD_Connection *connection) {

    struct context **cnt = cls;
    struct webui_ctx *webui;

    (void)connection;

    util_threadname_set("wu", 0,NULL);

    webui = malloc(sizeof(struct webui_ctx));

    webu_context_init(cnt, webui);
    webui->mhd_first_connect = TRUE;

    snprintf(webui->url,WEBUI_LEN_URLI,"%s",uri);

    webu_parseurl(webui);

    memset(webui->hostname,'\0',WEBUI_LEN_PARM);
    memset(webui->resp_page,'\0',webui->resp_size);
    memset(webui->userpass,'\0',webui->userpass_size);

    return webui;
}

static void webu_mhd_deinit(void *cls
    , struct MHD_Connection *connection
    , void **con_cls
    , enum MHD_RequestTerminationCode toe) {

    struct webui_ctx *webui = *con_cls;

    /* Eliminate compiler warnings */
    (void)connection;
    (void)cls;
    (void)toe;

    webu_context_free(webui);

    return;
}

static void webu_mhd_features(struct context **cnt){

    /* sample format: 0x01093001 = 1.9.30-1 , 0x00094400 ships with 16.04 */
#if MHD_VERSION < 0x00094400
    if (cnt[0]->conf.webcontrol_ssl){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"libmicrohttpd libary too old SSL disabled");
        cnt[0]->conf.webcontrol_ssl = 0;
    }
#else
    int retcd;
    retcd = MHD_is_feature_supported (MHD_FEATURE_BASIC_AUTH);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"Basic authentication: enabled");
    } else {
        if (cnt[0]->conf.webcontrol_auth_method == 1){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"Basic authentication: disabled");
            cnt[0]->conf.webcontrol_auth_method = 0;
        } else {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"Basic authentication: disabled");
        }
    }

    retcd = MHD_is_feature_supported (MHD_FEATURE_DIGEST_AUTH);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"Digest authentication: enabled");
    } else {
        if (cnt[0]->conf.webcontrol_auth_method == 2){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"Digest authentication: disabled");
            cnt[0]->conf.webcontrol_auth_method = 0;
        } else {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"Digest authentication: disabled");
        }
    }

    retcd = MHD_is_feature_supported (MHD_FEATURE_IPv6);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"IPV6: enabled");
    } else {
        if (cnt[0]->conf.ipv6_enabled){
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"IPV6: disabled");
            cnt[0]->conf.ipv6_enabled = FALSE;
        } else {
            MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"IPV6: disabled");
        }
    }

    retcd = MHD_is_feature_supported (MHD_FEATURE_SSL);
    if (retcd == MHD_YES){
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"SSL: enabled");
    } else {
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO ,"SSL: disabled");
        cnt[0]->conf.webcontrol_ssl = 0;
    }

#endif
}

static char * webui_mhd_loadfile(const char *fname){
    FILE *infile;
    size_t file_size, read_size;
    char * file_char;

    if (fname == NULL) {
        file_char = NULL;
    } else {
        infile = fopen(fname, "rb");
        fseek(infile, 0, SEEK_END);
        file_size = ftell(infile);
        if (file_size > 0 ){
            file_char = mymalloc(file_size +1);
            fseek(infile, 0, SEEK_SET);
            read_size = fread(file_char, file_size, 1, infile);
            if (read_size > 0 ){
                file_char[file_size] = 0;
            } else {
                free(file_char);
                file_char = NULL;
                MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
                    ,"Error reading file for SSL support.");
            }
        } else {
            file_char = NULL;
        }
        fclose(infile);
    }
    return file_char;
}

static void webu_mhd_checkssl(struct context **cnt, char *ssl_cert, char *ssl_key){

    if (cnt[0]->conf.webcontrol_ssl){
        if ((cnt[0]->conf.webcontrol_cert == NULL) || (ssl_cert == NULL)) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,"SSL requested but no cert file provided.  SSL disabled");
            cnt[0]->conf.webcontrol_ssl = 0;
        }
        if ((cnt[0]->conf.webcontrol_key == NULL) || (ssl_key == NULL)) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                ,"SSL requested but no key file provided.  SSL disabled");
            cnt[0]->conf.webcontrol_ssl = 0;
        }
    }

}

static void webu_mhd_setoptions(struct context **cnt
    , struct MHD_OptionItem *mhd_ops, char *ssl_cert, char *ssl_key){

    struct sockaddr_in loopback_addr;
    int mhd_opt_nbr;
    unsigned int randnbr;
    char randchar[8];

    webu_mhd_checkssl(cnt, ssl_cert, ssl_key);

    mhd_opt_nbr = 0;

    mhd_ops[mhd_opt_nbr].option = MHD_OPTION_NOTIFY_COMPLETED;
    mhd_ops[mhd_opt_nbr].value = (intptr_t)webu_mhd_deinit;
    mhd_ops[mhd_opt_nbr].ptr_value = NULL;
    mhd_opt_nbr++;

    mhd_ops[mhd_opt_nbr].option = MHD_OPTION_URI_LOG_CALLBACK;
    mhd_ops[mhd_opt_nbr].value = (intptr_t)webu_mhd_init;
    mhd_ops[mhd_opt_nbr].ptr_value = cnt;
    mhd_opt_nbr++;

    if (cnt[0]->conf.webcontrol_localhost){
        memset(&loopback_addr, 0, sizeof(loopback_addr));
        loopback_addr.sin_family = AF_INET;
        loopback_addr.sin_port = htons(cnt[0]->conf.webcontrol_port);
        loopback_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_SOCK_ADDR;
        mhd_ops[mhd_opt_nbr].value = 0;
        mhd_ops[mhd_opt_nbr].ptr_value = (struct sockaddr *)(&loopback_addr);
        mhd_opt_nbr++;
    }

    if (cnt[0]->conf.webcontrol_auth_method == 2){
        srand(time(NULL));
        randnbr = (unsigned int)(42000000.0 * rand() / (RAND_MAX + 1.0));
        snprintf(randchar,sizeof(randchar),"%d",randnbr);

        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_DIGEST_AUTH_RANDOM;
        mhd_ops[mhd_opt_nbr].value = sizeof(randchar);
        mhd_ops[mhd_opt_nbr].ptr_value = randchar;
        mhd_opt_nbr++;

        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_NONCE_NC_SIZE;
        mhd_ops[mhd_opt_nbr].value = 300;
        mhd_ops[mhd_opt_nbr].ptr_value = NULL;
        mhd_opt_nbr++;

        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_CONNECTION_TIMEOUT;
        mhd_ops[mhd_opt_nbr].value = (unsigned int) 120;
        mhd_ops[mhd_opt_nbr].ptr_value = NULL;
        mhd_opt_nbr++;
    }

    if (cnt[0]->conf.webcontrol_ssl ){
        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_CERT;
        mhd_ops[mhd_opt_nbr].value = 0;
        mhd_ops[mhd_opt_nbr].ptr_value = ssl_cert;
        mhd_opt_nbr++;

        mhd_ops[mhd_opt_nbr].option = MHD_OPTION_HTTPS_MEM_KEY;
        mhd_ops[mhd_opt_nbr].value = 0;
        mhd_ops[mhd_opt_nbr].ptr_value = ssl_key;
        mhd_opt_nbr++;
    }

    mhd_ops[mhd_opt_nbr].option = MHD_OPTION_END;
    mhd_ops[mhd_opt_nbr].value = 0;
    mhd_ops[mhd_opt_nbr].ptr_value = NULL;
    mhd_opt_nbr++;

}

void webu_stop(struct context **cnt) {

    if (cnt[0]->webcontrol_daemon != NULL){
        MHD_stop_daemon (cnt[0]->webcontrol_daemon);
    }

}

void webu_start(struct context **cnt) {

    struct MHD_OptionItem *mhd_ops;
    unsigned int mhd_flags;
    char *ssl_cert, *ssl_key;
    struct sigaction act;

    cnt[0]->webcontrol_daemon = NULL;
    if (cnt[0]->conf.webcontrol_port == 0 ) return;

    /* set signal handlers TO IGNORE */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    mhd_ops= malloc(sizeof(struct MHD_OptionItem)*10);


    ssl_cert = webui_mhd_loadfile(cnt[0]->conf.webcontrol_cert);
    ssl_key  = webui_mhd_loadfile(cnt[0]->conf.webcontrol_key);

    webu_mhd_features(cnt);

    webu_mhd_setoptions(cnt, mhd_ops, ssl_cert, ssl_key);

    if (cnt[0]->conf.webcontrol_ssl){
        mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                    MHD_USE_POLL|
                    MHD_USE_DUAL_STACK |
                    MHD_USE_SELECT_INTERNALLY|
                    MHD_USE_SSL;
    } else {
        mhd_flags = MHD_USE_THREAD_PER_CONNECTION |
                    MHD_USE_POLL|
                    MHD_USE_DUAL_STACK |
                    MHD_USE_SELECT_INTERNALLY;
    }

    cnt[0]->webcontrol_daemon = MHD_start_daemon (mhd_flags
        ,cnt[0]->conf.webcontrol_port
        ,NULL, NULL
        ,&webu_mhd_ans, cnt
        ,MHD_OPTION_ARRAY, mhd_ops
        ,MHD_OPTION_END);

    free(mhd_ops);
    if (ssl_cert != NULL) free(ssl_cert);
    if (ssl_key  != NULL) free(ssl_key);

    if (cnt[0]->webcontrol_daemon == NULL){
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO ,"Unable to start MHD");
    }
    return;

}


