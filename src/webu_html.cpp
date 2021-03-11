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
 *    Copyright 2020 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "webu.hpp"
#include "webu_html.hpp"


/* Create the CSS styles used in the navigation bar/side of the page */
static void webu_html_style_navbar(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    .sidenav {\n"
        "      height: 100%;\n"
        "      width: 10rem;\n"
        "      position: fixed;\n"
        "      z-index: 1;\n"
        "      top: 0;\n"
        "      left: 0;\n"
        "      background-color: lightgrey;\n"
        "      overflow-x: hidden;\n"
        "      overflow: auto;\n"
        "    }\n"
        "    .sidenav a, .dropbtn {\n"
        "      padding: 0.5rem 0rem 0.5em 1rem;\n"
        "      text-decoration: none;\n"
        "      font-size: 1rem;\n"
        "      display: block;\n"
        "      border: none;\n"
        "      background: none;\n"
        "      width:100%;\n"
        "      text-align: left;\n"
        "      cursor: pointer;\n"
        "      outline: none;\n"
        "      color: black;\n"
        "      background-color: lightgray;\n"
        "    }\n"
        "    .sidenav a:hover, .dropbtn:hover {\n"
        "      background-color: #555;\n"
        "      color: white;\n"
        "    }\n"
        "    .dropdown-content {\n"
        "      display: none;\n"
        "      background-color:lightgray;\n"
        "      padding-left: 1rem;\n"
        "    }\n"
        "    .actionbtn {\n"
        "      padding: 0.25rem;\n"
        "      text-decoration: none;\n"
        "      font-size: 0.5rem;\n"
        "      display: block;\n"
        "      border: none;\n"
        "      background: none;\n"
        "      width: 3rem;\n"
        "      text-align: center;\n"
        "      cursor: pointer;\n"
        "      outline: none;\n"
        "      color: black;\n"
        "      background-color: lightgray;\n"
        "    }\n";

}

/* Create the css styles used in the config sections */
static void webu_html_style_config(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    .cls_config {\n"
        "      background-color: #000000;\n"
        "      color: #fff;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "      font-size: 0.90rem;\n"
        "    }\n"
        "   .cls_config table {\n"
        "      display: table;\n"
        "      border-spacing: 1rem;\n"
        "      margin: auto;\n"
        "    }\n"
        "   .cls_config label {\n"
        "      padding: 0rem;\n"
        "      text-align: right;\n"
        "      width: 10rem;\n"
        "      height: 2.5rem;\n"
        "    }\n"
        "   .cls_config textarea {\n"
        "      margin: auto;\n"
        "      text-align: center;\n"
        "      width: 15.5rem;\n"
        "      height: 2.5rem;\n"
        "    }\n"
        "    .cls_drop {\n"
        "      padding: 0rem;\n"
        "      text-align: right;\n"
        "      width: 10rem;\n"
        "      height: 2.25rem;\n"
        "    }\n"
        "    .cls_text {\n"
        "      padding: 0rem;\n"
        "      width: 10em;\n"
        "      text-align: right;\n"
        "    }\n"
        "    .cls_text_nbr {\n"
        "      padding: 0rem;\n"
        "      width: 10rem;\n"
        "      text-align: right;\n"
        "    }\n"
        "    .cls_text_wide {\n"
        "      padding: 0rem;\n"
        "      height: 3rem;\n"
        "      width: 20rem;\n"
        "      text-align: right;\n"
        "    }\n"
        "    .cls_camdrop {\n"
        "      /* Only used to identify all the cam drops on page */\n"
        "    }\n"
        "    .arrow {\n"
        "      border: solid black;\n"
        "      border-width: 0 1rem 1rem 0;\n"
        "      border: double black;\n"
        "      border-width: 0 0.75rem 0.75rem 0;\n"
        "      display: inline-block;\n"
        "      padding: 1rem;\n"
        "      font-size: 0.5rem;\n"
        "    }\n"
        "    .right {\n"
        "      transform: rotate(-45deg);\n"
        "      -webkit-transform: rotate(-45deg);\n"
        "    }\n"
        "    .left {\n"
        "      transform: rotate(135deg);\n"
        "      -webkit-transform: rotate(135deg);\n"
        "    }\n"
        "    .up {\n"
        "      transform: rotate(-135deg);\n"
        "      -webkit-transform: rotate(-135deg);\n"
        "    }\n"
        "    .down {\n"
        "      transform: rotate(45deg);\n"
        "      -webkit-transform: rotate(45deg);\n"
        "    }\n"
        "    .zoombtn {\n"
        "      font-size:1.25rem;\n"
        "      width: 3rem;\n"
        "      height: 1.5rem;\n"
        "      margin: 0;\n"
        "    }\n";

}

/* Write out the starting style section of the web page */
static void webu_html_style_base(struct webui_ctx *webui)
{

    webui->resp_page +=
        "    * {\n"
        "      margin: 0;\n"
        "      padding: 0;\n"
        "    }\n"
        "    body {\n"
        "      padding: 0;\n"
        "      margin: 0;\n"
        "      font-family: Arial, Helvetica, sans-serif;\n"
        "      font-size: 1rem;\n"
        "      line-height: 1;\n"
        "      color: #606c71;\n"
        "      background-color: #159957;\n"
        "      background-image: linear-gradient(120deg, #155799, #159957);\n"
        "      margin-left:0.5% ;\n"
        "      margin-right:0.5% ;\n"
        "      width: device-width ;\n"
        "    }\n"
        "    .page-header {\n"
        "      color: #fff;\n"
        "      text-align: center;\n"
        "      margin-top: 0rem;\n"
        "      margin-bottom: 0rem;\n"
        "      font-weight: normal;\n"
        "    }\n"
        "    .page-header h3 {\n"
        "      height: 2rem;\n"
        "      padding: 0;\n"
        "      margin: 1rem;\n"
        "      border: 0;\n"
        "    }\n"
        "    h3 {\n"
        "      margin-left: 10rem;\n"
        "    }\n"
        "    .header-right{\n"
        "      float: right;\n"
        "      color: white;\n"
        "    }\n"
        "    .header-center {\n"
        "      text-align: center;\n"
        "      color: white;\n"
        "      margin-top: 1rem;\n"
        "      margin-bottom: 1rem;\n"
        "    }\n"
        "    .border {\n"
        "      border-width: 1rem;\n"
        "      border-color: white;\n"
        "      border-style: solid;\n"
        "    }\n";

}

/* Write out the style section of the web page */
static void webu_html_style(struct webui_ctx *webui)
{
    webui->resp_page += "  <style>\n";

    webu_html_style_base(webui);

    webu_html_style_navbar(webui);

    webu_html_style_config(webui);

    webui->resp_page += "  </style>\n";

}

/* Create the header section of the page */
static void webu_html_head(struct webui_ctx *webui)
{

    webui->resp_page += "<head> \n"
        "<meta charset='UTF-8'> \n"
        "<title>MotionPlus</title> \n"
        "<meta name='viewport' content='width=device-width, initial-scale=1'> \n";

    webu_html_style(webui);

    webui->resp_page += "</head>\n\n";

}

/* Create the navigation bar section of the page */
static void webu_html_navbar(struct webui_ctx *webui)
{
    webui->resp_page +=
        "  <div id=\"divnav_main\" class=\"sidenav\">\n"
        "    <div id=\"divnav_version\">\n"
        "      <a>MotionPlus 0.0.1</a>\n"
        "    </div>\n"
        "    <button onclick='display_cameras()' "
            " id='cam_btn' class='dropbtn'>Cameras</button>\n"
        "    <div id='divnav_cam' class='dropdown-content'>\n"
        "      <!-- Filled in by script -->\n"
        "    </div>\n"
        "    <button\n"
        "      onclick='display_config()' id='cfg_btn' class='dropbtn'>\n"
        "      Configuration\n"
        "    </button>\n"
        "    <div id='divnav_config' class='dropdown-content'>\n"
        "      <!-- Filled in by script -->\n"
        "    </div>\n"
        "  </div>\n\n";

}

/* Create the javascript function dropchange_cam */
static void webu_html_script_dropchange_cam(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    /*Cascade camera change in one dropdown to all the others*/\n"
        "    function dropchange_cam(camobj) {\n"
        "      assign_vals(camobj.value);\n\n"
        "      var sect = document.getElementsByName('camdrop');\n"
        "      for (var indx = 0; indx < sect.length; indx++) {\n"
        "        sect.item(indx).selectedIndex =camobj.selectedIndex;\n"
        "      }\n"
        "    }\n\n";
}

/* Create the javascript function submit_config */
static void webu_html_script_submit_config(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function submit_config(category) {\n"
        "      var formData = new FormData();\n"
        "      var camid = document.getElementsByName('camdrop')[0].value;\n"
        "      var pCfg = pData['configuration']['cam'+camid];\n\n"
        "      formData.append('command', 'config');\n"
        "      formData.append('camid', camid);\n\n"
        "      for (jkey in pCfg) {\n"
        "        if (document.getElementsByName(jkey)[0] != null) {\n"
        "          if (pCfg[jkey].category == category) {\n"
        "            if (document.getElementsByName(jkey)[0].type == 'checkbox') {\n"
        "              formData.append(jkey, document.getElementsByName(jkey)[0].checked);\n"
        "            } else {\n"
        "              formData.append(jkey, document.getElementsByName(jkey)[0].value);\n"
        "            }\n"
        "          }\n"
        "        }\n"
        "      }\n"
        "      var request = new XMLHttpRequest();\n"
        "      request.open('POST', '" + webui->hostfull + "');\n"
        "      request.send(formData);\n\n"
        "    }\n\n";
}

/* Create the javascript function config_hideall */
static void webu_html_script_config_hideall(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function config_hideall() {\n"
        "      var sect = document.getElementsByClassName('cls_config');\n"
        "      for (var i = 0; i < sect.length; i++) {\n"
        "        sect.item(i).style.display='none';\n"
        "      }\n"
        "      return;\n"
        "    }\n\n";
}

/* Create the javascript function config_click */
static void webu_html_script_config_click(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function config_click(actval) {\n"
        "      config_hideall();\n"
        "      document.getElementById('div_cam').style.display='none';\n"
        "      document.getElementById('div_config').style.display='inline';\n"
        "	   document.getElementById('div_' + actval).style.display='inline';\n"
        "    }\n\n";
}

/* Create the javascript function assign_version */
static void webu_html_script_assign_version(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function assign_version() {\n"
        "      var verstr ='<a>MotionPlus \\n'+pData['version'] +'</a>';\n"
        "      document.getElementById('divnav_version').innerHTML = verstr;\n"
        "    }\n\n";

}

/* Create the javascript function assign_cams */
static void webu_html_script_assign_cams(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function assign_cams() {\n"
        "      var camcnt = pData['cameras']['count'];\n"
        "      var html_drop = \"\\n\";\n"
        "      var html_nav = \"\\n\";\n\n"
        "      html_drop += \" <select class='cls_drop' \";\n"
        "      html_drop += \" onchange='dropchange_cam(this)' \";\n"
        "      html_drop += \" name='camdrop'>\\n\";\n\n"
        "      for (var indx = 0; indx <= camcnt; indx++) {\n"
        "        if (indx == 0) {\n"
        "          html_nav += \"<a onclick='camera_click(\" + indx +\");'>\";\n"
        "          html_nav += \"All Cameras</a>\\n\";\n"
        "        } else {\n"
        "          html_nav += \"<a onclick='camera_click(\" + indx + \");'>\";\n"
        "          html_nav += pData[\"cameras\"][indx][\"name\"] + \"</a>\\n\";\n"
        "        }\n\n"
        "        html_drop += \"<option \";\n"
        "        html_drop += \" value='\"+pData[\"cameras\"][indx][\"id\"]+\"'>\";\n"
        "        html_drop += pData[\"cameras\"][indx][\"name\"];\n"
        "        html_drop += \"</option>\\n\";\n"
        "      }\n"
        "      html_drop += \" </select>\\n\";\n\n"
        "      var sect = document.getElementsByClassName(\"cls_camdrop\");\n"
        "      for (indx = 0; indx < sect.length; indx++) {\n"
        "        sect.item(indx).innerHTML = html_drop;\n"
        "      }\n\n"
        "      document.getElementById(\"divnav_cam\").innerHTML = html_nav;\n\n"
        "      return;\n"
        "    }\n\n";
}

/* Create the javascript function assign_vals */
static void webu_html_script_assign_vals(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function assign_vals(camid) {\n"
        "      var pCfg = pData[\"configuration\"][\"cam\"+camid];\n\n"
        "      for (jkey in pCfg) {\n"
        "        if (document.getElementsByName(jkey)[0] != null) {\n"
        "          if (pCfg[jkey].enabled) {\n"
        "            document.getElementsByName(jkey)[0].disabled = false;\n"
        "            if (document.getElementsByName(jkey)[0].type == \"checkbox\") {\n"
        "              document.getElementsByName(jkey)[0].checked = pCfg[jkey].value;\n"
        "            } else {\n"
        "              document.getElementsByName(jkey)[0].value = pCfg[jkey].value;\n"
        "            }\n"
        "          } else {\n"
        "            document.getElementsByName(jkey)[0].disabled = true;\n"
        "            document.getElementsByName(jkey)[0].value = '';\n"
        "          }\n"
        "        } else {\n"
        "          console.log('Uncoded ' + jkey + ' : ' + pCfg[jkey].value);\n"
        "        }\n"
        "      }\n"
        "    }\n\n";
}

/* Create the javascript function assign_config */
static void webu_html_script_assign_config(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function assign_config() {\n"
        "      var pCfg = pData['configuration']['cam0'];\n"
        "      var pCat = pData['categories'];\n"
        "      var html_cfg = \"\";\n"
        "      var html_nav = \"\\n\";\n"
        "      var indx_lst = 0;\n\n"
        "      for (jcat in pCat) {\n"
        "        html_nav += \"<a onclick=\\\"config_click('\";\n"
        "        html_nav += pCat[jcat][\"name\"]+\"');\\\">\";\n"
        "        html_nav += pCat[jcat][\"display\"]+\"</a>\\n\";\n\n"
        "        html_cfg += \"<div id='div_\";\n"
        "        html_cfg += pCat[jcat][\"name\"];\n"
        "        html_cfg += \"' style='display:none' class='cls_config'>\\n\";\n"
        "        html_cfg += \"<h3>\";\n"
        "        html_cfg += pCat[jcat][\"display\"];\n"
        "        html_cfg += \" Parameters</h3>\\n\";\n"
        "        html_cfg += \"<table><tr> <td><label for 'camdrop'>camera</label></td>\\n\";\n"
        "        html_cfg += \"<td class='cls_camdrop'>\";\n"
        "        html_cfg += \"<select class='cls_drop' \";\n"
        "        html_cfg += \"onchange='dropchange_cam.call(this)' \";\n"
        "        html_cfg += \"name='camdrop'>\\n\";\n"
        "        html_cfg += \"<option value='0000'>default</option>\\n\";\n"
        "        html_cfg += \"</select></td></tr>\\n\";\n\n"
        "        for (jkey in pCfg) {\n"
        "          if (pCfg[jkey][\"category\"] == jcat) {\n"
        "            html_cfg += \"<tr><td><label for='\";\n"
        "            html_cfg += jkey + \"'>\"+jkey+\"</label></td>\\n\";\n\n"
        "            if (pCfg[jkey][\"type\"] == \"string\") {\n"
        "              html_cfg += \"<td><textarea name='\";\n"
        "              html_cfg += jkey+\"'></textarea></td>\";\n\n"
        "            } else if (pCfg[jkey][\"type\"] == \"bool\") {\n"
        "              html_cfg += \"<td><input class='cfg_check' \";\n"
        "              html_cfg += \" type='checkbox' name='\";\n"
        "              html_cfg += jkey+\"'></td>\";\n\n"
        "            } else if (pCfg[jkey][\"type\"] == \"int\") {\n"
        "              html_cfg += \"<td><input class='cls_text_nbr' \";\n"
        "              html_cfg += \"type='text' name='\";\n"
        "              html_cfg += jkey+\"'></td>\";\n\n"
        "            } else if (pCfg[jkey][\"type\"] == \"list\") {\n"
        "              html_cfg += \"<td><select class='cls_drop' \";\n"
        "              html_cfg += \" name='\"+jkey+\"'  autocomplete='off'>\";\n\n"
        "              for (indx_lst=0; indx_lst < pCfg[jkey][\"list\"].length; indx_lst++) {\n"
        "                html_cfg += \"<option value='\";\n"
        "                html_cfg += pCfg[jkey][\"list\"][indx_lst] + \"'>\";\n"
        "                html_cfg += pCfg[jkey][\"list\"][indx_lst] + \"</option>\\n\";\n"
        "              }\n"
        "              html_cfg += \"</select></td>\";\n"
        "            }\n"
        "            html_cfg += \"</tr>\\n\";\n"
        "          }\n"
        "        }\n"
        "        html_cfg += \"<tr><td><input type='hidden' name='trailer' value='null'></td>\\n\";\n"
        "        html_cfg += \"<td> <button onclick='submit_config(\";\n"
        "        html_cfg += jcat + \")'>Submit</button></td></tr>\\n\";\n"
        "        html_cfg += \"</table></div>\\n\";\n"
        "      }\n"
        "      document.getElementById(\"div_config\").innerHTML = html_cfg;\n"
        "      document.getElementById(\"divnav_config\").innerHTML = html_nav;\n\n"
        "    }\n\n";
}

/* Create the javascript function init_form */
static void webu_html_script_initform(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function initform() {\n"
        "      var xmlhttp = new XMLHttpRequest();\n"
        "      xmlhttp.onreadystatechange = function() {\n"
        "        if (this.readyState == 4 && this.status == 200) {\n"
        "          pData = JSON.parse(this.responseText);\n"
        "          assign_config();\n"
        "          assign_version();\n"
        "          assign_vals(0);\n"
        "          assign_cams();\n"
        "        }\n"
        "      };\n"
        "      xmlhttp.open('GET', '" + webui->hostfull + "/config.json', true);\n"
        "      xmlhttp.send();\n"
        "    }\n\n";
}

/* Create the javascript function display_cameras */
static void webu_html_script_display_cameras(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function display_cameras() {\n"
        "      document.getElementById('divnav_config').style.display = 'none';\n"
        "      if (document.getElementById('divnav_cam').style.display == 'block'){\n"
        "        document.getElementById('divnav_cam').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('divnav_cam').style.display = 'block';\n"
        "      }\n"
        "    }\n\n";
}

/* Create the javascript function display_config */
static void webu_html_script_display_config(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function display_config() {\n"
        "      document.getElementById('divnav_cam').style.display = 'none';\n"
        "      if (document.getElementById('divnav_config').style.display == 'block') {\n"
        "        document.getElementById('divnav_config').style.display = 'none';\n"
        "      } else {\n"
        "        document.getElementById('divnav_config').style.display = 'block';\n"
        "      }\n"
        "    }\n\n";
}

/* Create the action_click javascript function */
static void webu_html_script_action_click(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function action_click(actval) {\n\n"
        "      config_hideall();\n\n"
        "      var formData = new FormData();\n"
        "      var camid = pData['cameras'][gIndexCam]['id'];\n\n"
        "      formData.append('command', actval);\n"
        "      formData.append('camid', camid);\n\n"
        "      var request = new XMLHttpRequest();\n"
        "      request.open('POST', '" + webui->hostfull + "');\n"
        "      request.send(formData);\n\n"
        "      return;\n"
        "    }\n\n";
}

/* Create the camera_click javascript function */
static void webu_html_script_camera_click(struct webui_ctx *webui)
{
    webui->resp_page +=
        "   function camera_click(index_cam) {\n\n"
        "      var html_preview = \"\";\n"
        "      var camid;\n\n"
        "      config_hideall();\n\n"
        "      gIndexCam = index_cam;\n\n"
        "      html_preview += \"<table style='float: left' >\";\n"
        "      html_preview += \"<tr></tr><tr>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('eventstart');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Start Event</button></td> \\n\";\n"

        "      html_preview += \"<td>&nbsp;&nbsp;</td>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('eventend');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">End Event</button></td> \\n\";\n"

        "      html_preview += \"</tr><tr></tr><tr>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('pause');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Pause</button></td> \\n\";\n"

        "      html_preview += \"<td>&nbsp;&nbsp;</td>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('unpause');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Unpause</button></td> \\n\";\n"

        "      html_preview += \"</tr><tr></tr><tr>\\n\";\n"
        "      html_preview += \"<td>&nbsp;&nbsp;</td>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('snapshot');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Snapshot</button></td> \\n\";\n"

        "      html_preview += \"</tr><tr></tr><tr>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('stop');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Stop</button></td> \\n\";\n"

        "      html_preview += \"<td>&nbsp;&nbsp;</td>\\n\";\n"

        "      html_preview += \"<td><button \\n\";\n"
        "      html_preview += \"onclick=\\\"action_click('restart');\\\" \\n\";\n"
        "      html_preview += \"class=\\\"actionbtn\\\" \\n\";\n"
        "      html_preview += \">Restart</button></td> \\n\";\n"

        "      html_preview += \"</tr>\\n\";\n"


        "      if (gIndexCam > 0) {\n"
        "        camid = pData['cameras'][index_cam].id;\n"
        "        html_preview += \"<tr><td>&nbsp&nbsp</td><td>&nbsp&nbsp</td></tr>\\n\";\n"
        "        html_preview += \"<tr><td></td><td><button class='arrow up'></button></td></tr>\\n\";\n"
        "        html_preview += \"<tr><td><button class='arrow left'></button></td><td></td>\\n\";\n"
        "        html_preview += \"<td><button class='arrow right'></button></td><td>&nbsp&nbsp</td><tr>\\n\";\n"
        "        html_preview += \"<tr><td></td><td><button class='arrow down'></button></td></tr>\\n\";\n"
        "        html_preview += \"<tr><td>&nbsp&nbsp</td><td>&nbsp&nbsp</td></tr>\\n\";\n"
        "        html_preview += \"<tr><td></td><td><button class='zoombtn'>+</button></td></tr>\\n\";\n"
        "        html_preview += \"<tr><td></td><td><button class='zoombtn'>-</button></td></tr>\\n\";\n"
        "        html_preview += \"<tr><td>&nbsp&nbsp</td><td>&nbsp&nbsp</td></tr>\\n\";\n"

        "        html_preview += \"</table>\";\n"
        "        if (pData['configuration']['cam'+camid].stream_preview_method.value == 1) {\n"
        "          html_preview += \"<a><img id='pic\" + gIndexCam + \"' src=\";\n"
        "          html_preview += pData['cameras'][gIndexCam]['url'];\n"
        "          html_preview += \"static/stream/t\" + new Date().getTime();\n"
        "          html_preview += \" border=0 width=55%></a>\\n\";\n"
        "        } else { \n"
        "          html_preview += \"<a><img id='pic\" + gIndexCam + \"' src=\";\n"
        "          html_preview += pData['cameras'][gIndexCam]['url'];\n"
        "          html_preview += \"mjpg/stream\" ;\n"
        "          html_preview += \" border=0 width=55%></a>\\n\";\n"
        "        }\n"
        "        document.getElementById('div_config').style.display='none';\n"
        "        document.getElementById('div_cam').style.display='block';\n"
        "        document.getElementById('div_cam').innerHTML = html_preview;\n\n"
        "      } else if (gIndexCam == 0) {\n"
        "        var camcnt = pData['cameras']['count'];\n"
        "        html_preview += \"</table>\";\n"
        "        for (var indx = 1; indx <= camcnt; indx++) {\n"
        "          camid = pData['cameras'][indx].id;\n"
        "          if (pData['configuration']['cam'+camid].stream_preview_method.value == 1) {\n"
        "            html_preview += \"<a><img id='pic\" + indx + \"' src=\"\n"
        "            html_preview += pData['cameras'][indx]['url'];\n"
        "            html_preview += \"static/stream/t\" + new Date().getTime();\n"
        "            html_preview += \" border=0 width=\"\n"
        "            html_preview += pData['configuration']['cam'+camid].stream_preview_scale.value;\n"
        "            html_preview += \"%></a>\\n\";\n"
        "            if (pData['configuration']['cam'+camid].stream_preview_newline.value == true) {\n"
        "              html_preview += \"<br>\\n\";\n"
        "            }\n"
        "          } else { \n"
        "            html_preview += \"<a><img id='pic\" + indx + \"' src=\"\n"
        "            html_preview += pData['cameras'][indx]['url'];\n"
        "            html_preview += \"mjpg/stream\" ;\n"
        "            html_preview += \" border=0 width=\"\n"
        "            html_preview += pData['configuration']['cam'+camid].stream_preview_scale.value;\n"
        "            html_preview += \"%></a>\\n\";\n"
        "            if (pData['configuration']['cam'+camid].stream_preview_newline.value == true) {\n"
        "              html_preview += \"<br>\\n\";\n"
        "            }\n"
        "          } \n"
        "        }\n"
        "        document.getElementById('div_config').style.display='none';\n"
        "        document.getElementById('div_cam').style.display='block';\n"
        "        document.getElementById('div_cam').innerHTML = html_preview;\n"
        "      }\n\n"
        "      timer.start();\n\n"
        "    }\n\n";
}

/* Create the timer_function javascript function */
static void webu_html_script_timer_function(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    function Timer(fn, t) {\n"
        "      var timerObj = setInterval(fn, t);\n"
        "      this.stop = function() {\n"
        "        if (timerObj) {\n"
        "            clearInterval(timerObj);\n"
        "            timerObj = null;\n"
        "        }\n"
        "        return this;\n"
        "      }\n"
        "      this.start = function() {\n"
        "        if (!timerObj) {\n"
        "            this.stop();\n"
        "            timerObj = setInterval(fn, t);\n"
        "        }\n"
        "        return this;\n"
        "      }\n"
        "    }\n\n";

}

/* Create the pictimer_function javascript function */
static void webu_html_script_timer_pic(struct webui_ctx *webui)
{
    webui->resp_page +=
        "    var timer = new Timer(function() {\n"
        "      var picurl = \"\";\n"
        "      var img = new Image();\n\n"
        "      var camid;\n\n"
        "      if (gIndexCam > 0) {\n"
        "        camid = pData['cameras'][gIndexCam]['id'];\n\n"
        "        if (pData['configuration']['cam'+camid].stream_preview_method.value == 1) {\n"
        "          picurl = pData['cameras'][gIndexCam]['url'] + \"static/stream/t\" + new Date().getTime();\n"
        "          img.src = picurl;\n"
        "          document.getElementById('pic'+gIndexCam).src = picurl;\n"
        "         }\n "
        "      } else if (gIndexCam == 0) {\n"
        "        var camcnt = pData['cameras']['count'];\n"
        "        for (var indx = 1; indx <= camcnt; indx++) {\n"
        "          camid = pData['cameras'][indx]['id'];\n\n"
        "          if (pData['configuration']['cam'+camid].stream_preview_method.value == 1) {\n"
        "            picurl = pData['cameras'][indx]['url'] + \"static/stream/t\" + new Date().getTime();\n"
        "            img.src = picurl;\n"
        "            document.getElementById('pic'+indx).src = picurl;\n"
        "          }\n"
        "        }\n"
        "      }\n"
        "    }, 1000);\n\n";

}

/* Call all the functions to create the java scripts of page*/
static void webu_html_script(struct webui_ctx *webui)
{
    webui->resp_page += "  <script>\n"
        "    var pData;\n"
        "    var gIndexCam;\n\n";

    webu_html_script_submit_config(webui);
    webu_html_script_dropchange_cam(webui);
    webu_html_script_config_hideall(webui);
    webu_html_script_config_click(webui);
    webu_html_script_assign_version(webui);
    webu_html_script_assign_cams(webui);
    webu_html_script_assign_vals(webui);
    webu_html_script_assign_config(webui);
    webu_html_script_initform(webui);
    webu_html_script_display_cameras(webui);
    webu_html_script_display_config(webui);
    webu_html_script_action_click(webui);
    webu_html_script_camera_click(webui);
    webu_html_script_timer_function(webui);
    webu_html_script_timer_pic(webui);

    webui->resp_page += "  </script>\n\n";

}

/* Create the body section of the web page */
static void webu_html_body(struct webui_ctx *webui)
{
    webui->resp_page += "<body class='body' onload='initform()'>\n";

    webu_html_navbar(webui);

    webui->resp_page +=
        "  <div id='div_cam' style='margin-left:11rem' >\n"
        "    <!-- Filled in by script -->\n"
        "  </div>\n\n"
        "  <div id='div_config'>\n"
        "    <!-- Filled in by script -->\n"
        "  </div>\n\n";

    webu_html_script(webui);

    webui->resp_page += "</body>\n";

}

/* Create the default motionplus page */
void webu_html_page(struct webui_ctx *webui)
{
    webui->resp_page += "<!DOCTYPE html>\n"
        "<html lang='" + webui->lang + "'>\n";

    webu_html_head(webui);

    webu_html_body(webui);

    webui->resp_page += "</html>\n";
}

/*Create the bad request page*/
void webu_html_badreq(struct webui_ctx *webui)
{
    webui->resp_page =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "<p>Bad Request</p>\n"
        "<p>The server did not understand your request.</p>\n"
        "</body>\n"
        "</html>\n";

}

/* Load a user provided html page */
void webu_html_user(struct webui_ctx *webui)
{
    char response[PATH_MAX];
    FILE *fp = NULL;

    fp = fopen(webui->motapp->cam_list[0]->conf->webcontrol_html.c_str(), "r");

    if (fp == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Invalid user html file: %s")
            , webui->motapp->cam_list[0]->conf->webcontrol_html.c_str());

        webu_html_badreq(webui);

        return;
    }

    webui->resp_page = "";
    while (fgets(response, PATH_MAX-1, fp)) {
        webui->resp_page += response;
    }

    myfclose(fp);

}


