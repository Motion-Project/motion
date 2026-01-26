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
 * conf.hpp - Configuration System Interface
 *
 * Header file defining configuration parameter structures, categories,
 * and the configuration management class for handling Motion's extensive
 * parameter system and runtime configuration updates.
 *
 */

#ifndef _INCLUDE_CONF_HPP_
#define _INCLUDE_CONF_HPP_

#include "parm_structs.hpp"

    /* Categories for he edits and display on web interface*/
    enum PARM_CAT{
        PARM_CAT_00     /* system */
        ,PARM_CAT_01    /* camera */
        ,PARM_CAT_02    /* source */
        ,PARM_CAT_03    /* image */
        ,PARM_CAT_04    /* overlay */
        ,PARM_CAT_05    /* method */
        ,PARM_CAT_06    /* masks */
        ,PARM_CAT_07    /* detect */
        ,PARM_CAT_08    /* scripts */
        ,PARM_CAT_09    /* picture */
        ,PARM_CAT_10    /* movies */
        ,PARM_CAT_11    /* timelapse */
        ,PARM_CAT_12    /* pipes */
        ,PARM_CAT_13    /* webcontrol */
        ,PARM_CAT_14    /* streams */
        ,PARM_CAT_15    /* database */
        ,PARM_CAT_16    /* sql */
        ,PARM_CAT_17    /* tracking */
        ,PARM_CAT_18    /* sound */
        ,PARM_CAT_MAX
    };
    enum PARM_TYP{
        PARM_TYP_STRING
        , PARM_TYP_INT
        , PARM_TYP_LIST
        , PARM_TYP_BOOL
        , PARM_TYP_ARRAY
        , PARM_TYP_PARAMS
    };
    enum PARM_LEVEL{
        PARM_LEVEL_ALWAYS      = 0
        ,PARM_LEVEL_LIMITED    = 1
        ,PARM_LEVEL_ADVANCED   = 2
        ,PARM_LEVEL_RESTRICTED = 3
        ,PARM_LEVEL_NEVER      = 99
    };

    /** Current parameters in the config file */
    struct ctx_parm {
        const std::string   parm_name;      /* name for this parameter                  */
        enum PARM_TYP       parm_type;      /* enum of parm_typ for bool,int or string. */
        enum PARM_CAT       parm_cat;       /* enum of parm_cat for grouping. */
        int                 webui_level;    /* Enum to display in webui: 0,1,2,3,99(always to never)*/
        bool                hot_reload;     /* true if param can be updated without restart */
    };

    enum PARM_ACT{
        PARM_ACT_DFLT
        , PARM_ACT_SET
        , PARM_ACT_GET
        , PARM_ACT_LIST
    };

    extern struct ctx_parm config_parms[];

    class cls_config {
        public:
            cls_config(cls_motapp *p_app);
            ~cls_config();

            /* ================================================================
             * Scoped Parameter Structures
             *
             * These hold the actual parameter values, grouped by scope:
             * - parm_app: Application-level (daemon, webcontrol, database)
             * - parm_cam: Camera devices (detection, capture, output)
             * - parm_snd: Sound devices (sound alerts)
             * ================================================================ */
            ctx_parm_app    parm_app;
            ctx_parm_cam    parm_cam;
            ctx_parm_snd    parm_snd;

            /* ================================================================
             * Configuration File Tracking (not in scoped structs)
             * ================================================================ */
            std::string     conf_filename;
            bool            from_conf_dir;

            /* ================================================================
             * Backward Compatibility Reference Aliases
             *
             * These reference aliases redirect the existing flat member access
             * pattern (->cfg->threshold) to the scoped structs. This preserves
             * binary compatibility with the 469 direct access sites in 28 files.
             *
             * DO NOT add new parameters here - add them to the scoped structs.
             * ================================================================ */

            /* Application parameters (-> parm_app) */
            bool&           daemon                  = parm_app.daemon;
            std::string&    pid_file                = parm_app.pid_file;
            std::string&    log_file                = parm_app.log_file;
            std::string&    log_type_str            = parm_app.log_type_str;
            int&            log_level               = parm_app.log_level;
            int&            log_fflevel             = parm_app.log_fflevel;
            int&            log_type                = parm_app.log_type;
            bool&           native_language         = parm_app.native_language;

            /* Camera device parameters (-> parm_cam) */
            std::string&    device_name             = parm_cam.device_name;
            int&            device_id               = parm_cam.device_id;
            std::string&    config_dir              = parm_cam.config_dir;
            std::string&    target_dir              = parm_cam.target_dir;
            int&            watchdog_tmo            = parm_cam.watchdog_tmo;
            int&            watchdog_kill           = parm_cam.watchdog_kill;
            int&            device_tmo              = parm_cam.device_tmo;
            std::string&    pause                   = parm_cam.pause;
            std::string&    schedule_params         = parm_cam.schedule_params;
            std::string&    picture_schedule_params = parm_cam.picture_schedule_params;
            std::string&    cleandir_params         = parm_cam.cleandir_params;

            /* Source parameters (-> parm_cam) */
            std::string&    v4l2_device             = parm_cam.v4l2_device;
            std::string&    v4l2_params             = parm_cam.v4l2_params;
            std::string&    netcam_url              = parm_cam.netcam_url;
            std::string&    netcam_params           = parm_cam.netcam_params;
            std::string&    netcam_high_url         = parm_cam.netcam_high_url;
            std::string&    netcam_high_params      = parm_cam.netcam_high_params;
            std::string&    netcam_userpass         = parm_cam.netcam_userpass;
            std::string&    libcam_device           = parm_cam.libcam_device;
            std::string&    libcam_params           = parm_cam.libcam_params;
            int&            libcam_buffer_count     = parm_cam.libcam_buffer_count;

            /* Image parameters (-> parm_cam) */
            int&            width                   = parm_cam.width;
            int&            height                  = parm_cam.height;
            int&            framerate               = parm_cam.framerate;
            int&            rotate                  = parm_cam.rotate;
            std::string&    flip_axis               = parm_cam.flip_axis;

            /* Overlay parameters (-> parm_cam) */
            std::string&    locate_motion_mode      = parm_cam.locate_motion_mode;
            std::string&    locate_motion_style     = parm_cam.locate_motion_style;
            std::string&    text_left               = parm_cam.text_left;
            std::string&    text_right              = parm_cam.text_right;
            bool&           text_changes            = parm_cam.text_changes;
            int&            text_scale              = parm_cam.text_scale;
            std::string&    text_event              = parm_cam.text_event;

            /* Detection method parameters (-> parm_cam) - HOT PATH */
            bool&           emulate_motion          = parm_cam.emulate_motion;
            int&            threshold               = parm_cam.threshold;
            int&            threshold_maximum       = parm_cam.threshold_maximum;
            int&            threshold_sdevx         = parm_cam.threshold_sdevx;
            int&            threshold_sdevy         = parm_cam.threshold_sdevy;
            int&            threshold_sdevxy        = parm_cam.threshold_sdevxy;
            int&            threshold_ratio         = parm_cam.threshold_ratio;
            int&            threshold_ratio_change  = parm_cam.threshold_ratio_change;
            bool&           threshold_tune          = parm_cam.threshold_tune;
            std::string&    secondary_method        = parm_cam.secondary_method;
            std::string&    secondary_params        = parm_cam.secondary_params;

            /* Mask parameters (-> parm_cam) */
            int&            noise_level             = parm_cam.noise_level;
            bool&           noise_tune              = parm_cam.noise_tune;
            std::string&    despeckle_filter        = parm_cam.despeckle_filter;
            std::string&    area_detect             = parm_cam.area_detect;
            std::string&    mask_file               = parm_cam.mask_file;
            std::string&    mask_privacy            = parm_cam.mask_privacy;
            int&            smart_mask_speed        = parm_cam.smart_mask_speed;

            /* Detect parameters (-> parm_cam) - HOT PATH */
            int&            lightswitch_percent     = parm_cam.lightswitch_percent;
            int&            lightswitch_frames      = parm_cam.lightswitch_frames;
            int&            minimum_motion_frames   = parm_cam.minimum_motion_frames;
            int&            event_gap               = parm_cam.event_gap;
            int&            static_object_time      = parm_cam.static_object_time;
            int&            post_capture            = parm_cam.post_capture;
            int&            pre_capture             = parm_cam.pre_capture;

            /* Script parameters (-> parm_cam) */
            std::string&    on_event_start          = parm_cam.on_event_start;
            std::string&    on_event_end            = parm_cam.on_event_end;
            std::string&    on_picture_save         = parm_cam.on_picture_save;
            std::string&    on_area_detected        = parm_cam.on_area_detected;
            std::string&    on_motion_detected      = parm_cam.on_motion_detected;
            std::string&    on_movie_start          = parm_cam.on_movie_start;
            std::string&    on_movie_end            = parm_cam.on_movie_end;
            std::string&    on_camera_lost          = parm_cam.on_camera_lost;
            std::string&    on_camera_found         = parm_cam.on_camera_found;
            std::string&    on_secondary_detect     = parm_cam.on_secondary_detect;
            std::string&    on_action_user          = parm_cam.on_action_user;
            std::string&    on_sound_alert          = parm_cam.on_sound_alert;

            /* Picture output parameters (-> parm_cam) */
            std::string&    picture_output          = parm_cam.picture_output;
            std::string&    picture_output_motion   = parm_cam.picture_output_motion;
            std::string&    picture_type            = parm_cam.picture_type;
            int&            picture_quality         = parm_cam.picture_quality;
            std::string&    picture_exif            = parm_cam.picture_exif;
            std::string&    picture_filename        = parm_cam.picture_filename;

            /* Snapshot parameters (-> parm_cam) */
            int&            snapshot_interval       = parm_cam.snapshot_interval;
            std::string&    snapshot_filename       = parm_cam.snapshot_filename;

            /* Picture limits (-> parm_cam) */
            int&            picture_max_per_event   = parm_cam.picture_max_per_event;
            int&            picture_min_interval    = parm_cam.picture_min_interval;

            /* Movie output parameters (-> parm_cam) */
            bool&           movie_output            = parm_cam.movie_output;
            bool&           movie_output_motion     = parm_cam.movie_output_motion;
            int&            movie_max_time          = parm_cam.movie_max_time;
            int&            movie_bps               = parm_cam.movie_bps;
            int&            movie_quality           = parm_cam.movie_quality;
            std::string&    movie_encoder_preset    = parm_cam.movie_encoder_preset;
            std::string&    movie_container         = parm_cam.movie_container;
            bool&           movie_passthrough       = parm_cam.movie_passthrough;
            std::string&    movie_filename          = parm_cam.movie_filename;
            std::string&    movie_retain            = parm_cam.movie_retain;
            bool&           movie_all_frames        = parm_cam.movie_all_frames;
            bool&           movie_extpipe_use       = parm_cam.movie_extpipe_use;
            std::string&    movie_extpipe           = parm_cam.movie_extpipe;

            /* Timelapse parameters (-> parm_cam) */
            int&            timelapse_interval      = parm_cam.timelapse_interval;
            std::string&    timelapse_mode          = parm_cam.timelapse_mode;
            int&            timelapse_fps           = parm_cam.timelapse_fps;
            std::string&    timelapse_container     = parm_cam.timelapse_container;
            std::string&    timelapse_filename      = parm_cam.timelapse_filename;

            /* Pipe parameters (-> parm_cam) */
            std::string&    video_pipe              = parm_cam.video_pipe;
            std::string&    video_pipe_motion       = parm_cam.video_pipe_motion;

            /* Webcontrol parameters (-> parm_app) */
            int&            webcontrol_port         = parm_app.webcontrol_port;
            int&            webcontrol_port2        = parm_app.webcontrol_port2;
            std::string&    webcontrol_base_path    = parm_app.webcontrol_base_path;
            bool&           webcontrol_ipv6         = parm_app.webcontrol_ipv6;
            bool&           webcontrol_localhost    = parm_app.webcontrol_localhost;
            int&            webcontrol_parms        = parm_app.webcontrol_parms;
            std::string&    webcontrol_interface    = parm_app.webcontrol_interface;
            std::string&    webcontrol_auth_method  = parm_app.webcontrol_auth_method;
            std::string&    webcontrol_authentication = parm_app.webcontrol_authentication;
            std::string&    webcontrol_user_authentication = parm_app.webcontrol_user_authentication;
            bool&           webcontrol_tls          = parm_app.webcontrol_tls;
            std::string&    webcontrol_cert         = parm_app.webcontrol_cert;
            std::string&    webcontrol_key          = parm_app.webcontrol_key;
            std::string&    webcontrol_headers      = parm_app.webcontrol_headers;
            std::string&    webcontrol_html         = parm_app.webcontrol_html;
            std::string&    webcontrol_actions      = parm_app.webcontrol_actions;
            int&            webcontrol_lock_minutes = parm_app.webcontrol_lock_minutes;
            int&            webcontrol_lock_attempts = parm_app.webcontrol_lock_attempts;
            std::string&    webcontrol_lock_script  = parm_app.webcontrol_lock_script;
            std::string&    webcontrol_trusted_proxies = parm_app.webcontrol_trusted_proxies;
            int&            webcontrol_session_timeout = parm_app.webcontrol_session_timeout;
            std::string&    webcontrol_html_path    = parm_app.webcontrol_html_path;
            bool&           webcontrol_spa_mode     = parm_app.webcontrol_spa_mode;

            /* Stream parameters (-> parm_cam) */
            int&            stream_preview_scale    = parm_cam.stream_preview_scale;
            bool&           stream_preview_newline  = parm_cam.stream_preview_newline;
            std::string&    stream_preview_params   = parm_cam.stream_preview_params;
            std::string&    stream_preview_method   = parm_cam.stream_preview_method;
            bool&           stream_preview_ptz      = parm_cam.stream_preview_ptz;
            int&            stream_quality          = parm_cam.stream_quality;
            bool&           stream_grey             = parm_cam.stream_grey;
            bool&           stream_motion           = parm_cam.stream_motion;
            int&            stream_maxrate          = parm_cam.stream_maxrate;
            int&            stream_scan_time        = parm_cam.stream_scan_time;
            int&            stream_scan_scale       = parm_cam.stream_scan_scale;

            /* Database parameters (-> parm_app) */
            std::string&    database_type           = parm_app.database_type;
            std::string&    database_dbname         = parm_app.database_dbname;
            std::string&    database_host           = parm_app.database_host;
            int&            database_port           = parm_app.database_port;
            std::string&    database_user           = parm_app.database_user;
            std::string&    database_password       = parm_app.database_password;
            int&            database_busy_timeout   = parm_app.database_busy_timeout;

            /* SQL parameters (-> parm_app) */
            std::string&    sql_event_start         = parm_app.sql_event_start;
            std::string&    sql_event_end           = parm_app.sql_event_end;
            std::string&    sql_movie_start         = parm_app.sql_movie_start;
            std::string&    sql_movie_end           = parm_app.sql_movie_end;
            std::string&    sql_pic_save            = parm_app.sql_pic_save;

            /* Tracking/PTZ parameters (-> parm_cam) */
            bool&           ptz_auto_track          = parm_cam.ptz_auto_track;
            int&            ptz_wait                = parm_cam.ptz_wait;
            std::string&    ptz_move_track          = parm_cam.ptz_move_track;
            std::string&    ptz_pan_left            = parm_cam.ptz_pan_left;
            std::string&    ptz_pan_right           = parm_cam.ptz_pan_right;
            std::string&    ptz_tilt_up             = parm_cam.ptz_tilt_up;
            std::string&    ptz_tilt_down           = parm_cam.ptz_tilt_down;
            std::string&    ptz_zoom_in             = parm_cam.ptz_zoom_in;
            std::string&    ptz_zoom_out            = parm_cam.ptz_zoom_out;

            /* Sound parameters (-> parm_snd) */
            std::string&             snd_device     = parm_snd.snd_device;
            std::string&             snd_params     = parm_snd.snd_params;
            std::list<std::string>&  snd_alerts     = parm_snd.snd_alerts;
            std::string&             snd_window     = parm_snd.snd_window;
            bool&                    snd_show       = parm_snd.snd_show;

            void camera_add(std::string fname, bool srcdir);
            void camera_add_from_detection(const ctx_detected_cam &detected);
            void sound_add(std::string fname, bool srcdir);
            void camera_filenm();
            void sound_filenm();
            void process();

            void edit_set(std::string parm_nm, std::string parm_val);
            void edit_get(std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat);
            void edit_get(std::string parm_nm, std::list<std::string> &parm_val, enum PARM_CAT parm_cat);
            void edit_list(std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat);

            std::string type_desc(enum PARM_TYP ptype);
            std::string cat_desc(enum PARM_CAT pcat, bool shrt);
            void usage();
            void init();
            void parms_log();
            void parms_write();
            void parms_copy(cls_config *src);
            void parms_copy(cls_config *src, PARM_CAT p_cat);

            /* Scoped copy operations - O(1) direct struct copy */
            void copy_app(const cls_config *src);
            void copy_cam(const cls_config *src);
            void copy_snd(const cls_config *src);

        private:
            cls_motapp *app;
            void cmdline();
            void defaults();
            int get_next_devid();
            void config_dir_parm(std::string confdir);

            void parms_log_parm(std::string parm_nm, std::string parm_vl);
            void parms_write_app();
            void parms_write_cam();
            void parms_write_parms(FILE *conffile, std::string parm_nm, std::string parm_vl, enum PARM_CAT parm_ct, bool reset);
            void parms_write_snd();

            int edit_set_active(std::string parm_nm, std::string parm_val);
            void edit_get_bool(std::string &parm_dest, bool &parm_in);
            void edit_set_bool(bool &parm_dest, std::string &parm_in);

            /* Generic handlers for type-based parameter editing */
            void edit_generic_bool(bool& target, std::string& parm, enum PARM_ACT pact, bool default_val);
            void edit_generic_int(int& target, std::string& parm, enum PARM_ACT pact, int default_val, int min_val, int max_val);
            void edit_generic_float(float& target, std::string& parm, enum PARM_ACT pact, float default_val, float min_val, float max_val);
            void edit_generic_string(std::string& target, std::string& parm, enum PARM_ACT pact, const std::string& default_val);
            void edit_generic_list(std::string& target, std::string& parm, enum PARM_ACT pact, const std::string& default_val, const std::vector<std::string>& valid_values);

            /* Centralized parameter dispatch function */
            void dispatch_edit(const std::string& name, std::string& parm, enum PARM_ACT pact);

            void edit_cat(std::string parm_nm, std::list<std::string> &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat);
            void edit_cat(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat);
            void edit_cat00(std::string cmd, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat01(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat02(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat03(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat04(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat05(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat06(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat07(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat08(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat09(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat10(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat11(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat12(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat13(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat14(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat15(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat16(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat17(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat18(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact);
            void edit_cat18(std::string parm_nm,std::list<std::string> &parm_val, enum PARM_ACT pact);

            void edit_log_file(std::string &parm, enum PARM_ACT pact);

            void edit_device_id(std::string &parm, enum PARM_ACT pact);
            void edit_pause(std::string &parm, enum PARM_ACT pact);
            void edit_target_dir(std::string &parm, enum PARM_ACT pact);
            void edit_webcontrol_html_path(std::string &parm, enum PARM_ACT pact);



            void edit_text_changes(std::string &parm, enum PARM_ACT pact);





            void edit_picture_filename(std::string &parm, enum PARM_ACT pact);
            void edit_snapshot_filename(std::string &parm, enum PARM_ACT pact);

            void edit_movie_filename(std::string &parm, enum PARM_ACT pact);

            void edit_timelapse_filename(std::string &parm, enum PARM_ACT pact);
            void edit_snd_alerts(std::list<std::string> &parm, enum PARM_ACT pact);








    };

#endif /* _INCLUDE_CONF_HPP_ */
