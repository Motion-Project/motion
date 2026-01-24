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
 * webu_json.hpp - JSON REST API Interface
 *
 * Header file defining the JSON REST API class for configuration
 * management, camera control, status queries, and profile operations
 * between the React frontend and Motion backend.
 *
 */

#ifndef _INCLUDE_WEBU_JSON_HPP_
#define _INCLUDE_WEBU_JSON_HPP_
    class cls_webu_json {
        public:
            cls_webu_json(cls_webu_ans *p_webua);
            ~cls_webu_json();
            void main();

            /* React UI API endpoints */
            void api_auth_me();
            void api_auth_login();
            void api_auth_logout();
            void api_auth_status();
            void api_media_pictures();
            void api_media_movies();
            void api_media_dates();
            void api_media_folders();          /* GET /{camId}/api/media/folders */
            void api_delete_picture();
            void api_delete_movie();
            void api_delete_folder_files();    /* DELETE /{camId}/api/media/folders/files */
            void api_system_temperature();
            void api_system_status();
            void api_system_reboot();     /* POST /0/api/system/reboot */
            void api_system_shutdown();   /* POST /0/api/system/shutdown */
            void api_system_service_restart(); /* POST /0/api/system/service-restart */
            void api_cameras();
            void api_config();
            void api_config_patch();  /* Batch config update via PATCH */
            void api_mask_get();      /* GET /{camId}/api/mask/{type} */
            void api_mask_post();     /* POST /{camId}/api/mask/{type} */
            void api_mask_delete();   /* DELETE /{camId}/api/mask/{type} */

            /* Configuration Profile API endpoints */
            void api_profiles_list();    /* GET /0/api/profiles?camera_id=X */
            void api_profiles_get();     /* GET /0/api/profiles/{id} */
            void api_profiles_create();  /* POST /0/api/profiles */
            void api_profiles_update();  /* PATCH /0/api/profiles/{id} */
            void api_profiles_delete();  /* DELETE /0/api/profiles/{id} */
            void api_profiles_apply();   /* POST /0/api/profiles/{id}/apply */
            void api_profiles_set_default(); /* POST /0/api/profiles/{id}/default */

            /* Camera action API endpoints (JSON replacements for legacy POST) */
            void api_config_write();         /* POST /0/api/config/write */
            void api_camera_restart();       /* POST /{camId}/api/camera/restart */
            void api_camera_snapshot();      /* POST /{camId}/api/camera/snapshot */
            void api_camera_pause();         /* POST /{camId}/api/camera/pause */
            void api_camera_stop();          /* POST /{camId}/api/camera/stop */
            void api_camera_event_start();   /* POST /{camId}/api/camera/event/start */
            void api_camera_event_end();     /* POST /{camId}/api/camera/event/end */
            void api_camera_ptz();           /* POST /{camId}/api/camera/ptz */

        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;
            void parms_item(cls_config *conf, int indx_parm);
            void parms_one(cls_config *conf);
            void parms_all();
            void cameras_list();
            void categories_list();
            void config();
            void movies_list();
            void movies();
            void status_vars(int indx_cam);
            void status();
            void loghistory();
            std::string escstr(std::string invar);
            void parms_item_detail(cls_config *conf, std::string pNm);

            /* Hot reload helpers */
            bool validate_hot_reload(const std::string &parm_name, int &parm_index);
            void apply_hot_reload_to_camera(cls_camera *cam,
                const std::string &parm_name, const std::string &parm_val);
            void apply_hot_reload(int parm_index, const std::string &parm_val);
            void build_response(bool success, const std::string &parm_name,
                               const std::string &old_val, const std::string &new_val,
                               bool hot_reload);

            /* CSRF validation helper for POST endpoints */
            bool validate_csrf();
            /* webcontrol_actions permission check helper */
            bool check_action_permission(const std::string &action_name);
    };

#endif /* _INCLUDE_WEBU_JSON_HPP_ */
