/*
 *    webu_html.c
 *
 *    Create the html for the webcontrol page
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 *    Functional naming scheme
 *        webu_html* - Functions that create the display webcontrol page.
 *        webu_html_main        - Entry point from webu_ans_ctrl(in webu.c)
 *        webu_html_page        - Create the web page
 *          webu_html_head      - Header section of the page
 *            webu_html_style*  - The style section of the web page
 *          webu_html_body      - Calls all the functions to create the body
 *            webu_html_navbar* - The navbar section of the web page
 *            webu_html_config* - config parms of page
 *            webu_html_track*  - Tracking functions
 *            webu_html_script* - The javascripts of the web page
 *
 *    To debug, run code, open page, view source and make copy of html
 *    into a local file to revise changes then determine applicable section(s)
 *    in this code to modify to match modified version.
 *    Known HTML Issues:
 *      Single and double quotes are not consistently used.
 *      HTML ids do not follow any naming convention.
 *      After clicking restart, do something...Try to connect again?
 *
 *    Additional functionality considerations:
 *      Notification to user of items that require restart when changed.
 *      Notification to user that item successfully implemented (config change/tracking)
 *      List motion parms somewhere so they can be found by xgettext
 *
 */

#include "motion.h"
#include "webu.h"
#include "webu_html.h"
#include "translate.h"

/* struct to save information regarding the links to include in html page */
struct strminfo_ctx {
    struct context  **cntlst;
    char            lnk_ref[WEBUI_LEN_LNK];
    char            lnk_src[WEBUI_LEN_LNK];
    char            lnk_camid[WEBUI_LEN_LNK];
    char            proto[WEBUI_LEN_LNK];
    int             port;
    int             motion_images;
};

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
        "    }\n"
        "    .border {\n"
        "      border-width: 2px;\n"
        "      border-color: white;\n"
        "      border-style: solid;\n"
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
        if (webui->cntlst[0]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_%05d');\">%s 1</a>\n"
                ,_("Cameras")
                ,webui->cntlst[0]->camera_id
                ,_("Camera"));
            webu_write(webui, response);
        } else {
            snprintf(response, sizeof (response),
                "    <div class=\"dropdown\">\n"
                "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
                "      <div id='cam_btn' class=\"dropdown-content\">\n"
                "        <a onclick=\"camera_click('cam_%05d');\">%s</a>\n"
                ,_("Cameras")
                ,webui->cntlst[0]->camera_id
                ,webui->cntlst[0]->conf.camera_name);
            webu_write(webui, response);
        }
    } else if (webui->cam_threads > 1){
        /* Motion.conf + separate camera.conf file */
        snprintf(response, sizeof (response),
            "    <div class=\"dropdown\">\n"
            "      <button onclick='display_cameras()' id=\"cam_drop\" class=\"dropbtn\">%s</button>\n"
            "      <div id='cam_btn' class=\"dropdown-content\">\n"
            "        <a onclick=\"camera_click('cam_all00');\">%s</a>\n"
            ,_("Cameras")
            ,_("All"));
        webu_write(webui, response);

        for (indx=1;indx <= webui->cam_count;indx++){
            if (webui->cntlst[indx]->conf.camera_name == NULL){
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%05d');\">%s %d</a>\n"
                    ,webui->cntlst[indx]->camera_id
                    , _("Camera"), webui->cntlst[indx]->camera_id);
            } else {
                snprintf(response, sizeof (response),
                    "        <a onclick=\"camera_click('cam_%05d');\">%s</a>\n"
                    ,webui->cntlst[indx]->camera_id
                    ,webui->cntlst[indx]->conf.camera_name
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
        "        <a onclick=\"action_click('/action/eventstart');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/eventend');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/snapshot');\">%s</a>\n"
        "        <a onclick=\"action_click('config');\">%s</a>\n"
        "        <a onclick=\"action_click('/config/write');\">%s</a>\n"
        "        <a onclick=\"action_click('track');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/pause');\">%s</a>\n"
        "        <a onclick=\"action_click('/detection/start');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/restart');\">%s</a>\n"
        "        <a onclick=\"action_click('/action/quit');\">%s</a>\n"
        "      </div>\n"
        "    </div>\n"
        ,_("Action")
        ,_("Start Event")
        ,_("End Event")
        ,_("Snapshot")
        ,_("Change Configuration")
        ,_("Write Configuration")
        ,_("Tracking")
        ,_("Pause")
        ,_("Start")
        ,_("Restart")
        ,_("Quit"));
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
    /* Print out the header description of which parameters are included based upon the
     * webcontrol_parms that was specified
     */
    char response[WEBUI_LEN_RESP];

    if (webui->cntlst[0]->conf.webcontrol_parms == 0){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 0 (%s)</h4>\n"
            ,_("No Configuration Options"));
    } else if (webui->cntlst[0]->conf.webcontrol_parms == 1){
        snprintf(response, sizeof (response),
            "    <h4 id='h4_parm' class='header-center'>webcontrol_parms = 1 (%s)</h4>\n"
            ,_("Limited Configuration Options"));
    } else if (webui->cntlst[0]->conf.webcontrol_parms == 2){
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

static void webu_html_h3desc(struct webui_ctx *webui) {
    /* Write out the status description for the camera */
    char response[WEBUI_LEN_RESP];

    if (webui->cam_threads == 1){
        snprintf(response, sizeof (response),
            "  <div id=\"id_header\">\n"
            "    <h3 id='h3_cam' data-cam=\"cam_all00\" class='header-center'>%s (%s)</h3>\n"
            "  </div>\n"
            ,_("All Cameras")
            ,(!webui->cntlst[0]->running)? _("Not running") :
                (webui->cntlst[0]->lost_connection)? _("Lost connection"):
                (webui->cntlst[0]->pause)? _("Paused"):_("Active")
            );
        webu_write(webui,response);
    } else {
        snprintf(response, sizeof (response),
            "  <div id=\"id_header\">\n"
            "    <h3 id='h3_cam' data-cam=\"cam_all00\" class='header-center'>%s</h3>\n"
            "  </div>\n"
            ,_("All Cameras"));
        webu_write(webui,response);
    }
}

static void webu_html_config(struct webui_ctx *webui) {

    /* Write out the options to put into the config dropdown
     * We use html data attributes to store the values for the options
     * We always set a cam_all00 attribute and if the value if different for
     * any of our cameras, then we also add a cam_xxxxx which has the config
     * value for camera xxxxx  The javascript then decodes these to display
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
        "        <option value='default' data-cam_all00=\"\" >%s</option>\n"
        ,_("Select option"));
    webu_write(webui, response);

    /* The config_params[indx_parm].print reuses the buffer so create a
     * temporary variable for storing our parameter from main to compare
     * to the thread specific value
     */
    val_temp=malloc(PATH_MAX);
    indx_parm = 0;
    while (config_params[indx_parm].param_name != NULL){

        if ((config_params[indx_parm].webui_level > webui->cntlst[0]->conf.webcontrol_parms) ||
            (config_params[indx_parm].webui_level == WEBUI_LEVEL_NEVER)){
            indx_parm++;
            continue;
        }

        val_main = config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, 0);

        snprintf(response, sizeof (response),
            "        <option value='%s' data-cam_all00=\""
            , config_params[indx_parm].param_name);
        webu_write(webui, response);

        memset(val_temp,'\0',PATH_MAX);
        if (val_main != NULL){
            snprintf(response, sizeof (response),"%s", val_main);
            webu_write(webui, response);
            snprintf(val_temp, PATH_MAX,"%s", val_main);
        }

        /* Loop through all the treads and see if any have a different value from motion.conf */
        if (webui->cam_threads > 1){
            for (indx=1;indx <= webui->cam_count;indx++){
                val_thread=config_params[indx_parm].print(webui->cntlst, NULL, indx_parm, indx);
                diff_vals = FALSE;
                if (((strlen(val_temp) == 0) && (val_thread == NULL)) ||
                    ((strlen(val_temp) != 0) && (val_thread == NULL))) {
                    diff_vals = FALSE;
                } else if (((strlen(val_temp) == 0) && (val_thread != NULL)) ) {
                    diff_vals = TRUE;
                } else {
                    if (strcasecmp(val_temp, val_thread)) diff_vals = TRUE;
                }
                if (diff_vals){
                    snprintf(response, sizeof (response),"%s","\" \\ \n");
                    webu_write(webui, response);

                    snprintf(response, sizeof (response),
                        "           data-cam_%05d=\""
                        ,webui->cntlst[indx]->camera_id);
                    webu_write(webui, response);
                    if (val_thread != NULL){
                        snprintf(response, sizeof (response),"%s", val_thread);
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
    /* Write the section for handling the tracking function */
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

static void webu_html_strminfo(struct strminfo_ctx *strm_info, int indx) {
    /* This determines all the items we need to know to specify links and
     * stream sources for the page.  The options are 0-3 as of this writing
     * where 0 = full streams, 1 = substreams, 2 = static images and 3 is
     * the legacy code for creating streams.  So we need to assign not only
     * what images are to be sent but also whether we have tls/ssl.
     * There are WAY too many options for this.
    */
    if (strm_info->cntlst[indx]->conf.stream_preview_method == 99){
        snprintf(strm_info->proto,WEBUI_LEN_LNK,"%s","http");
        snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","");
        snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","");
        snprintf(strm_info->lnk_camid,WEBUI_LEN_LNK,"%s","");
        strm_info->port = strm_info->cntlst[indx]->conf.stream_port;
    } else {
        /* If using the main port,we need to insert a thread number into url*/
        if (strm_info->cntlst[0]->conf.stream_port != 0) {
            snprintf(strm_info->lnk_camid,WEBUI_LEN_LNK,"/%d"
                ,strm_info->cntlst[indx]->camera_id);
            strm_info->port = strm_info->cntlst[0]->conf.stream_port;
            if (strm_info->cntlst[0]->conf.stream_tls) {
                snprintf(strm_info->proto,WEBUI_LEN_LNK,"%s","https");
            } else {
                snprintf(strm_info->proto,WEBUI_LEN_LNK,"%s","http");
            }
        } else {
            snprintf(strm_info->lnk_camid,WEBUI_LEN_LNK,"%s","");
            strm_info->port = strm_info->cntlst[indx]->conf.stream_port;
            if (strm_info->cntlst[indx]->conf.stream_tls) {
                snprintf(strm_info->proto,WEBUI_LEN_LNK,"%s","https");
            } else {
                snprintf(strm_info->proto,WEBUI_LEN_LNK,"%s","http");
            }
        }
        if (strm_info->motion_images){
            snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","/motion");
            snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","/motion");
        } else {
            /* Assign what images and links we want */
            if (strm_info->cntlst[indx]->conf.stream_preview_method == 1){
                /* Substream for preview */
                snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","/stream");
                snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","/substream");
            } else if (strm_info->cntlst[indx]->conf.stream_preview_method == 2){
                /* Static image for preview */
                snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","/stream");
                snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","/current");
            } else if (strm_info->cntlst[indx]->conf.stream_preview_method == 4){
                /* Source image for preview */
                snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","/source");
                snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","/source");
            } else {
                /* Full stream for preview (method 0 or 3)*/
                snprintf(strm_info->lnk_ref,WEBUI_LEN_LNK,"%s","/stream");
                snprintf(strm_info->lnk_src,WEBUI_LEN_LNK,"%s","/stream");
            }
        }
    }

}

static void webu_html_preview(struct webui_ctx *webui) {

    /* Write the initial version of the preview section.  The javascript
     * will change this section when user selects a different camera */
    char response[WEBUI_LEN_RESP];
    int indx, indx_st, preview_scale;
    struct strminfo_ctx strm_info;

    strm_info.cntlst = webui->cntlst;

    snprintf(response, sizeof (response),"%s",
        "  <div id=\"liveview\">\n"
        "    <section class=\"main-content\">\n"
        "      <br>\n"
        "      <p id=\"id_preview\">\n");
    webu_write(webui, response);

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (webui->cntlst[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","      <br>\n");
            webu_write(webui, response);
        }

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            preview_scale = 45;
        } else {
            preview_scale = webui->cntlst[indx]->conf.stream_preview_scale;
        }

        strm_info.motion_images = FALSE;
        webu_html_strminfo(&strm_info,indx);
        snprintf(response, sizeof (response),
            "      <a href=%s://%s:%d%s%s> "
            " <img src=%s://%s:%d%s%s border=0 width=%d%%></a>\n"
            ,strm_info.proto, webui->hostname, strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_ref
            ,strm_info.proto, webui->hostname, strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_src
            ,preview_scale);
        webu_write(webui, response);

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            strm_info.motion_images = TRUE;
            webu_html_strminfo(&strm_info,indx);
            snprintf(response, sizeof (response),
                "      <a href=%s://%s:%d%s%s> "
                " <img src=%s://%s:%d%s%s class=border width=%d%%></a>\n"
                ,strm_info.proto, webui->hostname, strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_ref
                ,strm_info.proto, webui->hostname, strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_src
                ,preview_scale);
            webu_write(webui, response);
        }

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
        "        var camnbr = camstr.substring(4,9);\n"
        "        var http = new XMLHttpRequest();\n"
        "        if ((actval == \"/detection/pause\") || (actval == \"/detection/start\")) {\n"
        "          http.addEventListener('load', event_reloadpage); \n"
        "        }\n"
    );
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "        var url = \"%s://%s:%d/\"; \n"
        ,webui->hostproto, webui->hostname
        ,webui->cntlst[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "        if (camnbr == \"all00\"){\n"
        "          url = url + \"%05d\";\n"
        "        } else {\n"
        "          url = url + camnbr;\n"
        "        }\n"
        "        url = url + actval;\n"
       ,webui->cntlst[0]->camera_id);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
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
    int indx, indx_st, preview_scale;
    struct strminfo_ctx strm_info;

    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    strm_info.cntlst = webui->cntlst;

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        snprintf(response, sizeof (response),
            "      if (camid == \"cam_%05d\"){\n"
            ,webui->cntlst[indx]->camera_id);
        webu_write(webui, response);

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            preview_scale = 45;
        } else {
            preview_scale = 95;
        }

        strm_info.motion_images = FALSE;
        webu_html_strminfo(&strm_info, indx);
        snprintf(response, sizeof (response),
            "        preview=\"<a href=%s://%s:%d%s%s> "
            " <img src=%s://%s:%d%s%s/ border=0 width=%d%%></a>\"  \n"
            ,strm_info.proto, webui->hostname, strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_ref
            ,strm_info.proto, webui->hostname,strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_src, preview_scale);
        webu_write(webui, response);

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            strm_info.motion_images = TRUE;
            webu_html_strminfo(&strm_info, indx);
            snprintf(response, sizeof (response),
                "        preview=preview + \"<a href=%s://%s:%d%s%s> "
                " <img src=%s://%s:%d%s%s/ class=border width=%d%%></a>\"  \n"
                ,strm_info.proto, webui->hostname, strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_ref
                ,strm_info.proto, webui->hostname,strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_src, preview_scale);
            webu_write(webui, response);
        }

        if (webui->cntlst[indx]->conf.camera_name == NULL){
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s %d (%s)</h3>\"\n"
                ,_("Camera")
                , webui->cntlst[indx]->camera_id
                ,(!webui->cntlst[indx]->running)? _("Not running") :
                 (webui->cntlst[indx]->lost_connection)? _("Lost connection"):
                 (webui->cntlst[indx]->pause)? _("Paused"):_("Active")
             );
        } else {
            snprintf(response, sizeof (response),
                "        header=\"<h3 id='h3_cam' data-cam='\" + camid + \"' "
                " class='header-center' >%s (%s)</h3>\"\n"
                , webui->cntlst[indx]->conf.camera_name
                ,(!webui->cntlst[indx]->running)? _("Not running") :
                 (webui->cntlst[indx]->lost_connection)? _("Lost connection"):
                 (webui->cntlst[indx]->pause)? _("Paused"):_("Active")
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
    int indx, indx_st, preview_scale;
    struct strminfo_ctx strm_info;


    indx_st = 1;
    if (webui->cam_threads == 1) indx_st = 0;

    strm_info.cntlst = webui->cntlst;

    snprintf(response, sizeof (response), "      if (camid == \"cam_all00\"){\n");
    webu_write(webui, response);

    for (indx = indx_st; indx<webui->cam_threads; indx++){
        if (indx == indx_st){
            snprintf(response, sizeof (response),"%s","        preview = \"\";\n");
            webu_write(webui, response);
        }

        if (webui->cntlst[indx]->conf.stream_preview_newline){
            snprintf(response, sizeof (response),"%s","        preview = preview + \"      <br>\";\n");
            webu_write(webui, response);
        }

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            preview_scale = 45;
        } else {
            preview_scale = webui->cntlst[indx]->conf.stream_preview_scale;
        }

        strm_info.motion_images = FALSE;
        webu_html_strminfo(&strm_info, indx);
        snprintf(response, sizeof (response),
            "        preview = preview + \"<a href=%s://%s:%d%s%s> "
            " <img src=%s://%s:%d%s%s border=0 width=%d%%></a>\"; \n"
            ,strm_info.proto, webui->hostname, strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_ref
            ,strm_info.proto, webui->hostname, strm_info.port
            ,strm_info.lnk_camid, strm_info.lnk_src
            ,preview_scale);
        webu_write(webui, response);

        if (webui->cntlst[indx]->conf.stream_preview_method == 3){
            strm_info.motion_images = TRUE;
            webu_html_strminfo(&strm_info, indx);
            snprintf(response, sizeof (response),
                "        preview = preview + \"<a href=%s://%s:%d%s%s> "
                " <img src=%s://%s:%d%s%s class=border width=%d%%></a>\"; \n"
                ,strm_info.proto, webui->hostname, strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_ref
                ,strm_info.proto, webui->hostname, strm_info.port
                ,strm_info.lnk_camid, strm_info.lnk_src
                ,preview_scale);
            webu_write(webui, response);
        }
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
        "      var camnbr = camstr.substring(4,9);\n"
        "      var opts = document.getElementById('cfg_parms');\n"
        "      var optsel = opts.options[opts.selectedIndex].value;\n"
        "      var baseval = document.getElementById('cfg_value').value;\n"
        "      var http = new XMLHttpRequest();\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      var url = \"%s://%s:%d/\"; \n"
        ,webui->hostproto, webui->hostname
        ,webui->cntlst[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      var optval=encodeURI(baseval);\n"
        "      if (camnbr == \"all00\"){\n"
        "        url = url + \"%05d\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        ,webui->cntlst[0]->camera_id);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",
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
        "        optval = opts.options[opts.selectedIndex].getAttribute('data-cam_all00');\n"
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
        "      var camnbr = camstr.substring(4,9);\n"
        "      var opts = document.getElementById('trk_option');\n"
        "      var optsel = opts.options[opts.selectedIndex].getAttribute('data-trk');\n"
        "      var optval1 = document.getElementById('trk_panx').value;\n"
        "      var optval2 = document.getElementById('trk_tilty').value;\n"
        "      var http = new XMLHttpRequest();\n");
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      var url = \"%s://%s:%d/\"; \n"
        ,webui->hostproto, webui->hostname
        ,webui->cntlst[0]->conf.webcontrol_port);
    webu_write(webui, response);

    snprintf(response, sizeof (response),
        "      if (camnbr == \"all00\"){\n"
        "        url = url + \"%05d\";\n"
        "      } else {\n"
        "        url = url + camnbr;\n"
        "      }\n"
        ,webui->cntlst[0]->camera_id);
    webu_write(webui, response);

    snprintf(response, sizeof (response),"%s",

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

    webu_html_h3desc(webui);

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

void webu_html_badreq(struct webui_ctx *webui) {

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

void webu_html_main(struct webui_ctx *webui) {

    /* Note some detection and config requested actions call the
     * action function.  This is because the legacy interface
     * put these into those pages.  We put them together here
     * based upon the structure of the new interface
     */

    int retcd;

    retcd = 0;
    if (strlen(webui->uri_camid) == 0){
        webu_html_page(webui);

    } else if ((!strcmp(webui->uri_cmd1,"config")) &&
               (!strcmp(webui->uri_cmd2,"write"))) {
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"config")) {
        retcd = webu_process_config(webui);

    } else if (!strcmp(webui->uri_cmd1,"action")){
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"detection")){
        webu_process_action(webui);

    } else if (!strcmp(webui->uri_cmd1,"track")){
        retcd = webu_process_track(webui);

    } else{
        MOTION_LOG(INF, TYPE_STREAM, NO_ERRNO,
            _("Invalid action requested: >%s< >%s< >%s<")
            ,webui->uri_camid, webui->uri_cmd1, webui->uri_cmd2);
        retcd = -1;
    }

    if (retcd < 0) webu_html_badreq(webui);

    return;
}
