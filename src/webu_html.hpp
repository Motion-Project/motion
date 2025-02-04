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

#ifndef _INCLUDE_WEBU_HTML_HPP_
#define _INCLUDE_WEBU_HTML_HPP_
    class cls_webu_html {
        public:
            cls_webu_html(cls_webu_ans *p_webua);
            ~cls_webu_html();
            void main();
        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;

            void style_navbar();
            void style_config();
            void style_base();
            void style();
            void head();
            void navbar();
            void divmain();
            void script_nav();
            void script_send_config();
            void script_send_action();
            void script_send_reload();
            void script_dropchange_cam();
            void script_config_hideall();
            void script_config_click();
            void script_assign_camid();
            void script_assign_version();
            void script_assign_cams();
            void script_assign_actions();
            void script_assign_vals();
            void script_assign_config_nav();
            void script_assign_config_item();
            void script_assign_config_cat();
            void script_assign_config();
            void script_initform();
            void script_display_cameras();
            void script_display_config();
            void script_display_movies();
            void script_display_actions();
            void script_camera_buttons_ptz();
            void script_image_picall();
            void script_image_pantilt();
            void script_cams_reset();
            void script_cams_one_click();
            void script_cams_all_click();
            void script_movies_page();
            void script_movies_click();
            void script_cams_scan_click();
            void script_cams_one_fnc();
            void script_cams_all_fnc();
            void script_cams_scan_fnc();
            void script_log_display();
            void script_log_get();
            void script_log_showhide();
            void script();
            void body();
            void default_page();
            void user_page();
    };

#endif /* _INCLUDE_WEBU_HTML_HPP_ */
