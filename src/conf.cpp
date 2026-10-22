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

#include "motion.hpp"
#include "util.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "sound.hpp"
#include "conf.hpp"
/* TODO/MUSTDO  change variable names to use either params or parms.  This is nuts how I named them*/
/*Configuration parameters */
ctx_parm config_parms[] = {
    {"daemon",                    PARM_TYP_BOOL,   PARM_CAT_00, PARM_LVL_02, PARM_CHG_RESTART },
    {"conf_filename",             PARM_TYP_STRING, PARM_CAT_00, PARM_LVL_02, PARM_CHG_RESTART },
    {"pid_file",                  PARM_TYP_STRING, PARM_CAT_00, PARM_LVL_02, PARM_CHG_RESTART },
    {"log_file",                  PARM_TYP_STRING, PARM_CAT_00, PARM_LVL_02, PARM_CHG_RESTART },
    {"log_level",                 PARM_TYP_LIST,   PARM_CAT_00, PARM_LVL_01, PARM_CHG_COPY },
    {"log_fflevel",               PARM_TYP_LIST,   PARM_CAT_00, PARM_LVL_01, PARM_CHG_COPY },
    {"log_type",                  PARM_TYP_LIST,   PARM_CAT_00, PARM_LVL_01, PARM_CHG_COPY },
    {"native_language",           PARM_TYP_BOOL,   PARM_CAT_00, PARM_LVL_01, PARM_CHG_CODE },
    {"native_language_code",      PARM_TYP_LIST,   PARM_CAT_00, PARM_LVL_01, PARM_CHG_CODE },

    {"device_name",               PARM_TYP_STRING, PARM_CAT_01, PARM_LVL_01, PARM_CHG_COPY },
    {"device_id",                 PARM_TYP_INT,    PARM_CAT_01, PARM_LVL_01, PARM_CHG_RESTART },
    {"device_tmo",                PARM_TYP_INT,    PARM_CAT_01, PARM_LVL_01, PARM_CHG_COPY },
    {"pause",                     PARM_TYP_LIST,   PARM_CAT_01, PARM_LVL_01, PARM_CHG_COPY },
    {"schedule_params",           PARM_TYP_PARAMS, PARM_CAT_01, PARM_LVL_01, PARM_CHG_RESTART },
    {"cleandir_params",           PARM_TYP_PARAMS, PARM_CAT_01, PARM_LVL_01, PARM_CHG_RESTART },
    {"target_dir",                PARM_TYP_STRING, PARM_CAT_01, PARM_LVL_02, PARM_CHG_COPY },
    {"watchdog_tmo",              PARM_TYP_INT,    PARM_CAT_01, PARM_LVL_01, PARM_CHG_CODE },
    {"watchdog_kill",             PARM_TYP_INT,    PARM_CAT_01, PARM_LVL_01, PARM_CHG_COPY },
    {"config_dir",                PARM_TYP_STRING, PARM_CAT_01, PARM_LVL_02, PARM_CHG_RESTART },
    {"camera",                    PARM_TYP_STRING, PARM_CAT_01, PARM_LVL_02, PARM_CHG_RESTART },

    {"v4l2_device",               PARM_TYP_STRING, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"v4l2_params",               PARM_TYP_PARAMS, PARM_CAT_02, PARM_LVL_02, PARM_CHG_CODE },
    {"netcam_url",                PARM_TYP_STRING, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"netcam_params",             PARM_TYP_PARAMS, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"netcam_high_url",           PARM_TYP_STRING, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"netcam_high_params",        PARM_TYP_PARAMS, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"netcam_userpass",           PARM_TYP_STRING, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"libcam_device",             PARM_TYP_STRING, PARM_CAT_02, PARM_LVL_02, PARM_CHG_RESTART },
    {"libcam_params",             PARM_TYP_PARAMS, PARM_CAT_02, PARM_LVL_02, PARM_CHG_CODE },

    {"width",                     PARM_TYP_INT,    PARM_CAT_03, PARM_LVL_01, PARM_CHG_RESTART },
    {"height",                    PARM_TYP_INT,    PARM_CAT_03, PARM_LVL_01, PARM_CHG_RESTART },
    {"framerate",                 PARM_TYP_INT,    PARM_CAT_03, PARM_LVL_01, PARM_CHG_COPY},
    {"rotate",                    PARM_TYP_LIST,   PARM_CAT_03, PARM_LVL_01, PARM_CHG_RESTART },
    {"flip_axis",                 PARM_TYP_LIST,   PARM_CAT_03, PARM_LVL_01, PARM_CHG_RESTART },

    {"locate_motion_mode",        PARM_TYP_LIST,   PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },
    {"locate_motion_style",       PARM_TYP_LIST,   PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },
    {"text_left",                 PARM_TYP_STRING, PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },
    {"text_right",                PARM_TYP_STRING, PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },
    {"text_changes",              PARM_TYP_BOOL,   PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },
    {"text_scale",                PARM_TYP_LIST,   PARM_CAT_04, PARM_LVL_01, PARM_CHG_CODE },
    {"text_event",                PARM_TYP_STRING, PARM_CAT_04, PARM_LVL_01, PARM_CHG_COPY },

    {"emulate_motion",            PARM_TYP_BOOL,   PARM_CAT_05, PARM_LVL_01, PARM_CHG_COPY },
    {"threshold",                 PARM_TYP_INT,    PARM_CAT_05, PARM_LVL_01, PARM_CHG_CODE },
    {"threshold_maximum",         PARM_TYP_INT,    PARM_CAT_05, PARM_LVL_01, PARM_CHG_CODE },
    {"threshold_tune",            PARM_TYP_BOOL,   PARM_CAT_05, PARM_LVL_01, PARM_CHG_COPY },
    {"secondary_method",          PARM_TYP_LIST,   PARM_CAT_05, PARM_LVL_01, PARM_CHG_RESTART },
    {"secondary_params",          PARM_TYP_PARAMS, PARM_CAT_05, PARM_LVL_01, PARM_CHG_RESTART },

    {"noise_level",               PARM_TYP_INT,    PARM_CAT_06, PARM_LVL_01, PARM_CHG_CODE },
    {"noise_tune",                PARM_TYP_BOOL,   PARM_CAT_06, PARM_LVL_01, PARM_CHG_COPY },
    {"despeckle_filter",          PARM_TYP_STRING, PARM_CAT_06, PARM_LVL_01, PARM_CHG_RESTART },
    {"area_detect",               PARM_TYP_STRING, PARM_CAT_06, PARM_LVL_01, PARM_CHG_RESTART },
    {"mask_file",                 PARM_TYP_STRING, PARM_CAT_06, PARM_LVL_02, PARM_CHG_RESTART },
    {"mask_privacy",              PARM_TYP_STRING, PARM_CAT_06, PARM_LVL_02, PARM_CHG_RESTART },
    {"smart_mask_speed",          PARM_TYP_LIST,   PARM_CAT_06, PARM_LVL_01, PARM_CHG_RESTART },

    {"lightswitch_percent",       PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },
    {"lightswitch_frames",        PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },
    {"minimum_motion_frames",     PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },
    {"static_object_time",        PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },
    {"event_gap",                 PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },
    {"pre_capture",               PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_RESTART },
    {"post_capture",              PARM_TYP_INT,    PARM_CAT_07, PARM_LVL_01, PARM_CHG_COPY },

    {"on_event_start",            PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_event_end",              PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_picture_save",           PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_area_detected",          PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_motion_detected",        PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_movie_start",            PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_movie_end",              PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_camera_lost",            PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_camera_found",           PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_secondary_detect",       PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_action_user",            PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },
    {"on_sound_alert",            PARM_TYP_STRING, PARM_CAT_08, PARM_LVL_03, PARM_CHG_COPY },

    {"picture_output",            PARM_TYP_LIST,   PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"picture_output_motion",     PARM_TYP_LIST,   PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"picture_type",              PARM_TYP_LIST,   PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"picture_quality",           PARM_TYP_INT,    PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"picture_exif",              PARM_TYP_STRING, PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"picture_filename",          PARM_TYP_STRING, PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"snapshot_interval",         PARM_TYP_INT,    PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },
    {"snapshot_filename",         PARM_TYP_STRING, PARM_CAT_09, PARM_LVL_01, PARM_CHG_COPY },

    {"movie_output",              PARM_TYP_BOOL,   PARM_CAT_10, PARM_LVL_01, PARM_CHG_COPY },
    {"movie_output_motion",       PARM_TYP_BOOL,   PARM_CAT_10, PARM_LVL_01, PARM_CHG_COPY },
    {"movie_max_time",            PARM_TYP_INT,    PARM_CAT_10, PARM_LVL_01, PARM_CHG_COPY },
    {"movie_container",           PARM_TYP_STRING, PARM_CAT_10, PARM_LVL_01, PARM_CHG_COPY },
    {"movie_passthrough",         PARM_TYP_BOOL,   PARM_CAT_10, PARM_LVL_01, PARM_CHG_CODE },
    {"movie_filename",            PARM_TYP_STRING, PARM_CAT_10, PARM_LVL_01, PARM_CHG_COPY },
    {"movie_extpipe_use",         PARM_TYP_BOOL,   PARM_CAT_10, PARM_LVL_04, PARM_CHG_COPY },
    {"movie_extpipe",             PARM_TYP_STRING, PARM_CAT_10, PARM_LVL_04, PARM_CHG_COPY },
    {"movie_params",              PARM_TYP_PARAMS, PARM_CAT_10, PARM_LVL_01, PARM_CHG_CODE },

    {"timelapse_interval",        PARM_TYP_INT,    PARM_CAT_11, PARM_LVL_01, PARM_CHG_COPY },
    {"timelapse_mode",            PARM_TYP_LIST,   PARM_CAT_11, PARM_LVL_01, PARM_CHG_COPY },
    {"timelapse_fps",             PARM_TYP_INT,    PARM_CAT_11, PARM_LVL_01, PARM_CHG_COPY },
    {"timelapse_container",       PARM_TYP_LIST,   PARM_CAT_11, PARM_LVL_01, PARM_CHG_COPY },
    {"timelapse_filename",        PARM_TYP_STRING, PARM_CAT_11, PARM_LVL_01, PARM_CHG_COPY },

    {"video_pipe",                PARM_TYP_STRING, PARM_CAT_12, PARM_LVL_01, PARM_CHG_RESTART },
    {"video_pipe_motion",         PARM_TYP_STRING, PARM_CAT_12, PARM_LVL_01, PARM_CHG_RESTART },

    {"webcontrol_port",           PARM_TYP_INT,    PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_base_path",      PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_ipv6",           PARM_TYP_BOOL,   PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_localhost",      PARM_TYP_BOOL,   PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_access",         PARM_TYP_LIST,   PARM_CAT_13, PARM_LVL_99, PARM_CHG_RESTART},
    {"webcontrol_interface",      PARM_TYP_LIST,   PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_auth_method",    PARM_TYP_LIST,   PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_auth_admin",     PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_auth_user",      PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_tls",            PARM_TYP_BOOL,   PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_cert",           PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_key",            PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_04, PARM_CHG_RESTART },
    {"webcontrol_headers",        PARM_TYP_PARAMS, PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_html",           PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_actions",        PARM_TYP_PARAMS, PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_lock_minutes",   PARM_TYP_INT,    PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_lock_attempts",  PARM_TYP_INT,    PARM_CAT_13, PARM_LVL_02, PARM_CHG_RESTART },
    {"webcontrol_lock_script",    PARM_TYP_STRING, PARM_CAT_13, PARM_LVL_03, PARM_CHG_RESTART },

    {"stream_allcam_params",      PARM_TYP_PARAMS, PARM_CAT_14, PARM_LVL_01, PARM_CHG_RESTART },
    {"stream_quality",            PARM_TYP_INT,    PARM_CAT_14, PARM_LVL_01, PARM_CHG_RESTART },
    {"stream_grey",               PARM_TYP_BOOL,   PARM_CAT_14, PARM_LVL_01, PARM_CHG_RESTART },
    {"stream_motion",             PARM_TYP_BOOL,   PARM_CAT_14, PARM_LVL_01, PARM_CHG_RESTART },
    {"stream_maxrate",            PARM_TYP_INT,    PARM_CAT_14, PARM_LVL_01, PARM_CHG_RESTART },

    {"database_type",             PARM_TYP_LIST,   PARM_CAT_15, PARM_LVL_02, PARM_CHG_RESTART },
    {"database_dbname",           PARM_TYP_STRING, PARM_CAT_15, PARM_LVL_02, PARM_CHG_RESTART },
    {"database_host",             PARM_TYP_STRING, PARM_CAT_15, PARM_LVL_02, PARM_CHG_RESTART },
    {"database_port",             PARM_TYP_INT,    PARM_CAT_15, PARM_LVL_02, PARM_CHG_RESTART },
    {"database_user",             PARM_TYP_STRING, PARM_CAT_15, PARM_LVL_04, PARM_CHG_RESTART },
    {"database_password",         PARM_TYP_STRING, PARM_CAT_15, PARM_LVL_04, PARM_CHG_RESTART },
    {"database_busy_timeout",     PARM_TYP_INT,    PARM_CAT_15, PARM_LVL_02, PARM_CHG_RESTART },

    {"sql_event_start",           PARM_TYP_STRING, PARM_CAT_16, PARM_LVL_02, PARM_CHG_COPY },
    {"sql_event_end",             PARM_TYP_STRING, PARM_CAT_16, PARM_LVL_02, PARM_CHG_COPY },
    {"sql_movie_start",           PARM_TYP_STRING, PARM_CAT_16, PARM_LVL_02, PARM_CHG_COPY },
    {"sql_movie_end",             PARM_TYP_STRING, PARM_CAT_16, PARM_LVL_02, PARM_CHG_COPY },
    {"sql_pic_save",              PARM_TYP_STRING, PARM_CAT_16, PARM_LVL_02, PARM_CHG_COPY},

    {"ptz_auto_track",            PARM_TYP_BOOL,   PARM_CAT_17, PARM_LVL_01, PARM_CHG_COPY },
    {"ptz_wait",                  PARM_TYP_INT,    PARM_CAT_17, PARM_LVL_01, PARM_CHG_COPY },
    {"ptz_move_track",            PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_pan_left",              PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_pan_right",             PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_tilt_up",               PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_tilt_down",             PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_zoom_in",               PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },
    {"ptz_zoom_out",              PARM_TYP_STRING, PARM_CAT_17, PARM_LVL_03, PARM_CHG_COPY },

    {"snd_device",                PARM_TYP_STRING, PARM_CAT_18, PARM_LVL_02, PARM_CHG_RESTART },
    {"snd_params",                PARM_TYP_PARAMS, PARM_CAT_18, PARM_LVL_02, PARM_CHG_RESTART },
    {"snd_alerts",                PARM_TYP_ARRAY, PARM_CAT_18, PARM_LVL_02, PARM_CHG_RESTART },
    {"snd_window",                PARM_TYP_LIST, PARM_CAT_18, PARM_LVL_02, PARM_CHG_RESTART },
    {"snd_show",                  PARM_TYP_BOOL, PARM_CAT_18, PARM_LVL_02, PARM_CHG_RESTART },

    { "", (enum PARM_TYP)0, (enum PARM_CAT)0, (enum PARM_LVL)0, (enum PARM_CHG)0 }
};

/*
 * Array of deprecated config options:
 * When deprecating an option, remove it from above (config_parms array)
 * and create an entry in this array of name, last version, info,
 * and (if applicable) a replacement conf value and copy funcion.
 * Upon reading a deprecated config option, a warning will be logged
 * with the given information and last version it was used in.
 * If set, the given value will be copied into the conf value
 * for backwards compatibility.
 */

/* Array of deprecated config options */
ctx_parm_depr config_parms_depr[] = {
    {"movie_codec","5.0.0","movie_container"
        ,"\"movie_codec\" replaced with \"movie_container\""}
    ,{"camera_id","5.0.0","device_id"
        ,"\"camera_id\" replaced with \"device_id\""}
    ,{"camera_name","5.0.0","device_name"
        ,"\"camera_name\" replaced with \"device_name\""}
    ,{"camera_tmo","5.0.0","device_tmo"
        ,"\"camera_tmo\" replaced with \"device_tmo\""}
    ,{"libcam_name","5.0.0","libcam_device"
        ,"\"libcam_name\" replaced with \"libcam_device\""}
    ,{"video_device","5.0.0","v4l2_device"
        ,"\"video_device\" replaced with \"v4l2_device\""}
    ,{"video_params","5.0.0","v4l2_params"
        ,"\"video_params\" replaced with \"v4l2_params\""}
    ,{"timelapse_codec","5.0.0","timelapse_container"
        ,"\"timelapse_codec\" replaced with \"timelapse_container\""}
    ,{"stream_port","5.0.0","webcontrol_port"
        ,"\"stream_port\" replaced with \"webcontrol_port\""}
    ,{"stream_localhost", "5.0.0","webcontrol_localhost"
        ,"\"stream_localhost\" replaced with \"webcontrol_localhost\""}
    ,{"stream_auth_method", "5.0.0","webcontrol_auth_method"
        ,"\"stream_auth_method\" replaced with \"webcontrol_auth_method\""}
    ,{"stream_authentication", "5.0.0","webcontrol_auth_user"
        ,"\"stream_authentication\" replaced with \"webcontrol_auth_user\""}
    ,{"stream_tls", "5.0.0","webcontrol_tls"
        ,"\"stream_tls\" replaced with \"webcontrol_tls\""}
    ,{"stream_header_params", "5.0.0","webcontrol_header_params"
        ,"\"stream_header_params\" replaced with \"webcontrol_header_params\""}
    ,{"stream_tls", "5.0.0","webcontrol_tls"
        ,"\"stream_tls\" replaced with \"webcontrol_tls\""}
    ,{"stream_preview_params", "5.0.0","stream_allcam_params"
        ,"\"stream_preview_params\" replaced with \"stream_allcam_params\""}
    ,{"webcontrol_authentication", "5.0.0","webcontrol_auth_admin"
        ,"\"webcontrol_authentication\" replaced with \"webcontrol_auth_admin\""}
    ,{"webcontrol_parms", "5.0.0","webcontrol_access"
        ,"\"webcontrol_parms\" replaced with \"webcontrol_access\""}
    ,{"movie_quality", "5.0.0","movie_params option"
        ,"\"movie_quality\" replaced with \"movie_params\" parameter \"quality\""}
    ,{"movie_bps", "5.0.0","movie_params option"
        ,"\"movie_bps\" replaced with \"movie_params\" parameter \"bit_rate\""}
    ,{"movie_all_frames", "5.0.0","movie_params option"
        ,"\"movie_all_frames\" replaced with \"movie_params\" parameter \"all_frames\""}
    ,{"movie_retain", "5.0.0","movie_params option"
        ,"\"movie_retain\" replaced with \"movie_params\" parameter \"secdetect_only\""}

    ,{ "","","",""}
};

void cls_config::edit_set_bool(bool &parm_dest, std::string &parm_in)
{
    if ((parm_in == "1") || (parm_in == "yes") || (parm_in == "on") || (parm_in == "true") ) {
        parm_dest = true;
    } else {
        parm_dest = false;
    }
}

void cls_config::edit_get_bool(std::string &parm_dest, bool &parm_in)
{
    if (parm_in == true) {
        parm_dest = "on";
    } else {
        parm_dest = "off";
    }
}

void cls_config::edit_daemon(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        daemon = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(daemon, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, daemon);
    }
}

void cls_config::edit_conf_filename(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf_filename = "";
    } else if (pact == PARM_ACT_SET) {
        conf_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf_filename;
    }
}

void cls_config::edit_pid_file(std::string &parm, int pact)
{
    if (pact == PARM_ACT_DFLT) {
        pid_file = "";
    } else if (pact == PARM_ACT_SET) {
        pid_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = pid_file;
    }
}

void cls_config::edit_log_file(std::string &parm, enum PARM_ACT pact)
{
    char    lognm[4096];
    tm      *logtm;
    time_t  logt;

    if (pact == PARM_ACT_DFLT) {
        log_file = "";
    } else if (pact == PARM_ACT_SET) {
        time(&logt);
        logtm = localtime(&logt);
        strftime(lognm, 4096, parm.c_str(), logtm);
        log_file = lognm;
    } else if (pact == PARM_ACT_GET) {
        parm = log_file;
    }
}

void cls_config::edit_log_level(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        log_level = 6;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %d"),parm_in);
        } else {
            log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(log_level);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\"";
        parm = parm + "]";
    }
}

void cls_config::edit_log_fflevel(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        log_fflevel = 3;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_fflevel %d"),parm_in);
        } else {
            log_fflevel = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(log_fflevel);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\"";
        parm = parm + "]";
    }
}

void cls_config::edit_log_type(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        log_type_str = "ALL";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "ALL") || (parm == "COR") ||
            (parm == "STR") || (parm == "ENC") ||
            (parm == "NET") || (parm == "DBL") ||
            (parm == "EVT") || (parm == "TRK") ||
            (parm == "VID") || (parm == "ALL")) {
            log_type_str = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_type %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = log_type_str;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"ALL\",\"COR\",\"STR\",\"ENC\",\"NET\"";
        parm = parm + ",\"DBL\",\"EVT\",\"TRK\",\"VID\"";
        parm = parm + "]";
    }
}

void cls_config::edit_native_language(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        native_language = true;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(native_language, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, native_language);
    }
}

void cls_config::edit_native_language_code(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        native_language_code = "";
    } else if (pact == PARM_ACT_SET) {
        mytrim(parm);
        if ((parm == "")   || (parm == "da") || (parm == "de") || (parm == "es") ||
            (parm == "en") || (parm == "fi") || (parm == "fr") || (parm == "it") ||
            (parm == "ja") || (parm == "ko") || (parm == "nl") || (parm == "no") ||
            (parm == "pt") || (parm == "sv") || (parm == "zh")) {
            native_language_code = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid native_language_code %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = native_language_code;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"\",\"da\",\"de\",\"es\",\"en\"";
        parm = parm + ",\"fi\",\"fr\",\"it\",\"ja\",\"ko\"";
        parm = parm + ",\"nl\",\"no\",\"pt\",\"sv\",\"zh\"";
        parm = parm + "]";
    }
}

void cls_config::edit_device_name(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        device_name= "";
    } else if (pact == PARM_ACT_SET) {
        device_name = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = device_name;
    }
}

void cls_config::edit_device_id(std::string &parm, enum PARM_ACT pact)
{
    int parm_in, indx;

    if (pact == PARM_ACT_DFLT) {
        device_id = 0;
    } else if (pact == PARM_ACT_SET) {
        if ((this == app->cfg) || (this == app->conf_src)) {
            device_id = 0;
        } else {
            parm_in = atoi(parm.c_str());
            if (device_id == parm_in) {
                return;
            }
            if (parm_in < 1) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_id %d"),parm_in);
            } else if (parm_in > 32000) {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_id %d"),parm_in);
            } else {
                for (indx=0;indx<app->cam_list.size();indx++){
                    if ((app->cam_list[indx]->conf_src->device_id == parm_in) &&
                        (app->cam_list[indx]->cfg != this)) {
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , _("Duplicate device_id %d not permitted"),parm_in);
                        return;
                    }
                }
                for (indx=0;indx<app->snd_list.size();indx++){
                    if ((app->snd_list[indx]->conf_src->device_id == parm_in) &&
                        (app->snd_list[indx]->cfg != this)) {
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , _("Duplicate device_id %d not permitted"),parm_in);
                        return;
                    }
                }
                device_id = parm_in;
            }
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(device_id);
    }
}

void cls_config::edit_device_tmo(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        device_tmo = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_tmo %d"),parm_in);
        } else {
            device_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(device_tmo);
    }
}

void cls_config::edit_pause(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        pause = "schedule";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "schedule") ||
            (parm == "1")   || (parm == "yes") ||
            (parm == "on")  || (parm == "true") ||
            (parm == "0")   || (parm == "no") ||
            (parm == "off") || (parm == "false")) {
            if ((parm == "schedule") || (parm == "on") || (parm == "off")) {
                pause = parm;
            } else if ((parm == "1") || (parm == "yes") || (parm == "true")) {
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    , _("Old type specified for pause %s. Use 'on' instead")
                    ,parm.c_str());
                pause = "on";
            } else if ((parm == "0") || (parm == "no") || (parm == "false")) {
                MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                    , _("Old type specified for pause %s.  Use 'off' instead")
                    ,parm.c_str());
                pause = "off";
            }
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pause %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = pause;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"schedule\",\"on\",\"off\"";
        parm = parm + "]";
    }
}

void cls_config::edit_schedule_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        schedule_params = "";
    } else if (pact == PARM_ACT_SET) {
        schedule_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = schedule_params;
    }
}

void cls_config::edit_cleandir_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cleandir_params = "";
    } else if (pact == PARM_ACT_SET) {
        cleandir_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cleandir_params;
    }
}

void cls_config::edit_config_dir(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        config_dir = "";
    } else if (pact == PARM_ACT_SET) {
        config_dir = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = config_dir;
    }
}

void cls_config::edit_target_dir(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        target_dir = std::string( configdir ) + std::string("/media");
    } else if (pact == PARM_ACT_SET) {
        if (parm.find("%", 0) != std::string::npos) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid target_dir.  Conversion specifiers not permitted. %s")
                , parm.c_str());
        } else if (parm == "") {
            target_dir = ".";
        } else if (parm.substr(parm.length()-1,1) == "/") {
            target_dir = parm.substr(0, parm.length()-1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing trailing '/' from target_dir");
        } else {
            target_dir = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = target_dir;
    }
}

void cls_config::edit_watchdog_tmo(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        watchdog_tmo = 90;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog timeout %d"),parm_in);
        } else {
            watchdog_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(watchdog_tmo);
    }
}

void cls_config::edit_watchdog_kill(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        watchdog_kill = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog kill timeout %d"),parm_in);
        } else {
            watchdog_kill = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(watchdog_kill);
    }
}

void cls_config::edit_v4l2_device(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        v4l2_device = "";
    } else if (pact == PARM_ACT_SET) {
        v4l2_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = v4l2_device;
    }
}

void cls_config::edit_v4l2_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        v4l2_params = "";
    } else if (pact == PARM_ACT_SET) {
        v4l2_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = v4l2_params;
    }
}

void cls_config::edit_netcam_url(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        netcam_url = "";
    } else if (pact == PARM_ACT_SET) {
        netcam_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = netcam_url;
    }
}

void cls_config::edit_netcam_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        netcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        netcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = netcam_params;
    }
}

void cls_config::edit_netcam_high_url(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        netcam_high_url = "";
    } else if (pact == PARM_ACT_SET) {
        netcam_high_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = netcam_high_url;
    }
}

void cls_config::edit_netcam_high_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        netcam_high_params = "";
    } else if (pact == PARM_ACT_SET) {
        netcam_high_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = netcam_high_params;
    }
}

void cls_config::edit_netcam_userpass(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        netcam_userpass = "";
    } else if (pact == PARM_ACT_SET) {
        netcam_userpass = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = netcam_userpass;
    }
}

void cls_config::edit_libcam_device(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        libcam_device = "";
    } else if (pact == PARM_ACT_SET) {
        libcam_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = libcam_device;
    }
}

void cls_config::edit_libcam_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        libcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        libcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = libcam_params;
    }
}

void cls_config::edit_width(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        width = 640;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid width %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("Image width (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting width to next higher multiple of 8 (%d)."), parm_in);
            width = parm_in;
        } else {
            width = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(width);
    }
}

void cls_config::edit_height(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        height = 480;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid height %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("Image height (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting height to next higher multiple of 8 (%d)."), parm_in);
            height = parm_in;
        } else {
            height = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(height);
    }
}

void cls_config::edit_framerate(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        framerate = 15;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid framerate %d"),parm_in);
        } else {
            framerate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(framerate);
    }
}

void cls_config::edit_rotate(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        rotate = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in != 0) && (parm_in != 90) &&
            (parm_in != 180) && (parm_in != 270) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid rotate %d"),parm_in);
        } else {
            rotate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(rotate);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"0\",\"90\",\"180\",\"270\"]";
    }
}

void cls_config::edit_flip_axis(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        flip_axis = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "vertical") || (parm == "horizontal")) {
            flip_axis = parm;
        } else if (parm == "") {
            flip_axis = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = flip_axis;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"vertical\",\"horizontal\"]";
    }
}

void cls_config::edit_locate_motion_mode(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        locate_motion_mode = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "off") || (parm == "on") || (parm == "preview")) {
            locate_motion_mode = parm;
        } else if (parm == "") {
            locate_motion_mode = "off";
        } else {
          MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = locate_motion_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"off\",\"on\",\"preview\"]";
    }
}

void cls_config::edit_locate_motion_style(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        locate_motion_style = "box";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "box") || (parm == "redbox") ||
            (parm == "cross") || (parm == "redcross"))  {
            locate_motion_style = parm;
        } else if (parm == "") {
            locate_motion_style = "box";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = locate_motion_style;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"box\",\"redbox\",\"cross\",\"redcross\"]";
    }
}

void cls_config::edit_text_left(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        text_left = "";
    } else if (pact == PARM_ACT_SET) {
        text_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = text_left;
    }
}

void cls_config::edit_text_right(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        text_right = "%Y-%m-%d\\n%T";
    } else if (pact == PARM_ACT_SET) {
        text_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = text_right;
    }
}

void cls_config::edit_text_changes(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        text_changes = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(text_changes, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, text_changes);
    }
}

void cls_config::edit_text_scale(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        text_scale = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid text_scale %d"),parm_in);
        } else {
            text_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(text_scale);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
}

void cls_config::edit_text_event(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        text_event = "%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        text_event = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = text_event;
    }
}

void cls_config::edit_emulate_motion(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        emulate_motion = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(emulate_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, emulate_motion);
    }
}

void cls_config::edit_threshold(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        threshold = 1500;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(threshold);
    }
}

void cls_config::edit_threshold_maximum(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        threshold_maximum = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_maximum %d"),parm_in);
        } else {
            threshold_maximum = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(threshold_maximum);
    }
}

void cls_config::edit_threshold_tune(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        threshold_tune = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(threshold_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, threshold_tune);
    }
}

void cls_config::edit_secondary_method(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        secondary_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "haar") ||
            (parm == "hog")  || (parm == "dnn"))  {
            secondary_method = parm;
        } else if (parm == "") {
            secondary_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = secondary_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"haar\",\"hog\",\"dnn\"]";
    }
}

void cls_config::edit_secondary_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        secondary_params = "";
    } else if (pact == PARM_ACT_SET) {
        secondary_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = secondary_params;
    }
}

void cls_config::edit_noise_level(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        noise_level = 32;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 255)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid noise_level %d"),parm_in);
        } else {
            noise_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(noise_level);
    }
}

void cls_config::edit_noise_tune(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        noise_tune = true;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(noise_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, noise_tune);
    }
}

void cls_config::edit_despeckle_filter(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        despeckle_filter = "EedDl";
    } else if (pact == PARM_ACT_SET) {
        despeckle_filter = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = despeckle_filter;
    }
}

void cls_config::edit_area_detect(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        area_detect = "";
    } else if (pact == PARM_ACT_SET) {
        area_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = area_detect;
    }
}

void cls_config::edit_mask_file(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        mask_file = "";
    } else if (pact == PARM_ACT_SET) {
        mask_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = mask_file;
    }
}

void cls_config::edit_mask_privacy(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        mask_privacy = "";
    } else if (pact == PARM_ACT_SET) {
        mask_privacy = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = mask_privacy;
    }
}

void cls_config::edit_smart_mask_speed(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        smart_mask_speed = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid smart_mask_speed %d"),parm_in);
        } else {
            smart_mask_speed = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(smart_mask_speed);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
}

void cls_config::edit_lightswitch_percent(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        lightswitch_percent = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_percent %d"),parm_in);
        } else {
            lightswitch_percent = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(lightswitch_percent);
    }
}

void cls_config::edit_lightswitch_frames(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        lightswitch_frames = 5;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_frames %d"),parm_in);
        } else {
            lightswitch_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(lightswitch_frames);
    }
}

void cls_config::edit_minimum_motion_frames(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        minimum_motion_frames = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_motion_frames %d"),parm_in);
        } else {
            minimum_motion_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(minimum_motion_frames);
    }
}

void cls_config::edit_static_object_time(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        static_object_time = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid static_object_time %d"),parm_in);
        } else {
            static_object_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(static_object_time);
    }
}

void cls_config::edit_event_gap(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        event_gap = 60;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid event_gap %d"),parm_in);
        } else {
            event_gap = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(event_gap);
    }
}

void cls_config::edit_pre_capture(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        pre_capture = 3;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pre_capture %d"),parm_in);
        } else {
            pre_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(pre_capture);
    }
}

void cls_config::edit_post_capture(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        post_capture = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid post_capture %d"),parm_in);
        } else {
            post_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(post_capture);
    }
}

void cls_config::edit_on_event_start(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_event_start = "";
    } else if (pact == PARM_ACT_SET) {
        on_event_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_event_start;
    }
}

void cls_config::edit_on_event_end(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_event_end = "";
    } else if (pact == PARM_ACT_SET) {
        on_event_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_event_end;
    }
}

void cls_config::edit_on_picture_save(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_picture_save = "";
    } else if (pact == PARM_ACT_SET) {
        on_picture_save = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_picture_save;
    }
}

void cls_config::edit_on_area_detected(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_area_detected = "";
    } else if (pact == PARM_ACT_SET) {
        on_area_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_area_detected;
    }
}

void cls_config::edit_on_motion_detected(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_motion_detected = "";
    } else if (pact == PARM_ACT_SET) {
        on_motion_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_motion_detected;
    }
}

void cls_config::edit_on_movie_start(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_movie_start = "";
    } else if (pact == PARM_ACT_SET) {
        on_movie_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_movie_start;
    }
}

void cls_config::edit_on_movie_end(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_movie_end = "";
    } else if (pact == PARM_ACT_SET) {
        on_movie_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_movie_end;
    }
}

void cls_config::edit_on_camera_lost(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_camera_lost = "";
    } else if (pact == PARM_ACT_SET) {
        on_camera_lost = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_camera_lost;
    }
}

void cls_config::edit_on_camera_found(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_camera_found = "";
    } else if (pact == PARM_ACT_SET) {
        on_camera_found = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_camera_found;
    }
}

void cls_config::edit_on_secondary_detect(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_secondary_detect = "";
    } else if (pact == PARM_ACT_SET) {
        on_secondary_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_secondary_detect;
    }
}

void cls_config::edit_on_action_user(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_action_user = "";
    } else if (pact == PARM_ACT_SET) {
        on_action_user = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_action_user;
    }
}

void cls_config::edit_on_sound_alert(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        on_sound_alert = "";
    } else if (pact == PARM_ACT_SET) {
        on_sound_alert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = on_sound_alert;
    }
}

void cls_config::edit_picture_output(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        picture_output = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "center") ||
            (parm == "first") || (parm == "best"))  {
            picture_output = parm;
        } else if (parm == "") {
            picture_output = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = picture_output;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"first\",\"best\",\"center\" ";
        parm = parm + "]";
    }
}

void cls_config::edit_picture_output_motion(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        picture_output_motion = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "roi"))  {
            picture_output_motion = parm;
        } else if (parm == "") {
            picture_output_motion = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output_motion %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = picture_output_motion;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"roi\"";
        parm = parm + "]";
    }
}

void cls_config::edit_picture_type(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        picture_type = "jpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "jpg") || (parm == "webp") || (parm == "ppm"))  {
            picture_type = parm;
        } else if ((parm == "") ||(parm == "jpeg"))  {
            picture_type = "jpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = picture_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"jpg\",\"webp\",\"ppm\"";
        parm = parm + "]";
    }
}

void cls_config::edit_picture_quality(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        picture_quality = 75;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_quality %d"),parm_in);
        } else {
            picture_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(picture_quality);
    }
}

void cls_config::edit_picture_exif(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        picture_exif = "";
    } else if (pact == PARM_ACT_SET) {
        picture_exif = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = picture_exif;
    }
}

void cls_config::edit_picture_filename(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        picture_filename = "%v-%Y%m%d%H%M%S-%q";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            picture_filename = "";
        } else if (parm.substr(0,1) == "/") {
            picture_filename = parm.substr(1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing leading '/' from filename");
        } else {
            picture_filename = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = picture_filename;
    }
}

void cls_config::edit_snapshot_interval(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        snapshot_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snapshot_interval %d"),parm_in);
        } else {
            snapshot_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(snapshot_interval);
    }
}

void cls_config::edit_snapshot_filename(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        snapshot_filename = "%v-%Y%m%d%H%M%S-snapshot";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            snapshot_filename = "";
        } else if (parm.substr(0,1) == "/") {
            snapshot_filename = parm.substr(1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing leading '/' from filename");
        } else {
            snapshot_filename = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = snapshot_filename;
    }
}

void cls_config::edit_movie_output(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_output = true;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(movie_output, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, movie_output);
    }
}

void cls_config::edit_movie_output_motion(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_output_motion = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(movie_output_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, movie_output_motion);
    }
}

void cls_config::edit_movie_max_time(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        movie_max_time = 120;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_max_time %d"),parm_in);
        } else {
            movie_max_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(movie_max_time);
    }
}

void cls_config::edit_movie_container(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_container = "mkv";
    } else if (pact == PARM_ACT_SET) {
        movie_container = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = movie_container;
    }
}

void cls_config::edit_movie_passthrough(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_passthrough = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(movie_passthrough, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, movie_passthrough);
    }
}

void cls_config::edit_movie_filename(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_filename = "%v-%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            movie_filename = "";
        } else if (parm.substr(0,1) == "/") {
            movie_filename = parm.substr(1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing leading '/' from filename");
        } else {
            movie_filename = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = movie_filename;
    }
}

void cls_config::edit_movie_extpipe_use(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_extpipe_use = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(movie_extpipe_use, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, movie_extpipe_use);
    }
}

void cls_config::edit_movie_extpipe(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_extpipe = "";
    } else if (pact == PARM_ACT_SET) {
        movie_extpipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = movie_extpipe;
    }
}

void cls_config::edit_movie_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        movie_params = "";
    } else if (pact == PARM_ACT_SET) {
        movie_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = movie_params;
    }
}

void cls_config::edit_timelapse_interval(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        timelapse_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_interval %d"),parm_in);
        } else {
            timelapse_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(timelapse_interval);
    }
}

void cls_config::edit_timelapse_mode(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        timelapse_mode = "daily";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "hourly") || (parm == "daily") ||
            (parm == "weekly-sunday") || (parm == "weekly-monday") ||
            (parm == "monthly") || (parm == "manual"))  {
            timelapse_mode = parm;
        } else if (parm == "") {
            timelapse_mode = "daily";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = timelapse_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"hourly\",\"daily\",\"weekly-sunday\"";
        parm = parm + ",\"weekly-monday\",\"monthly\",\"manual\"";
        parm = parm + "]";
    }
}

void cls_config::edit_timelapse_fps(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        timelapse_fps = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_fps %d"),parm_in);
        } else {
            timelapse_fps = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(timelapse_fps);
    }
}

void cls_config::edit_timelapse_container(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        timelapse_container = "mpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mpg") || (parm == "mkv"))  {
            timelapse_container = parm;
        } else if (parm == "") {
            timelapse_container = "mpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_container %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = timelapse_container;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"mpg\",\"mkv\"";
        parm = parm + "]";
    }
}

void cls_config::edit_timelapse_filename(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        timelapse_filename = "%Y%m%d-timelapse";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            timelapse_filename = "";
        } else if (parm.substr(0,1) == "/") {
            timelapse_filename = parm.substr(1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing leading '/' from filename");
        } else {
            timelapse_filename = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = timelapse_filename;
    }
}

void cls_config::edit_video_pipe(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        video_pipe = "";
    } else if (pact == PARM_ACT_SET) {
        video_pipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = video_pipe;
    }
}

void cls_config::edit_video_pipe_motion(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        video_pipe_motion = "";
    } else if (pact == PARM_ACT_SET) {
        video_pipe_motion = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = video_pipe_motion;
    }
}

void cls_config::edit_webcontrol_port(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        webcontrol_port = 8080;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_port %d"),parm_in);
        } else {
            webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(webcontrol_port);
    }
}

void cls_config::edit_webcontrol_base_path(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_base_path = "";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "/") {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_base_path: Use blank instead of single / "));
            webcontrol_base_path = "";
        } else if (parm.length() >= 1) {
            if (parm.substr(0, 1) != "/") {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    , _("Invalid webcontrol_base_path:  Must start with a / "));
                webcontrol_base_path = "/" + parm;
            } else {
                webcontrol_base_path = parm;
            }
        } else {
            webcontrol_base_path = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_base_path;
    }
}

void cls_config::edit_webcontrol_ipv6(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_ipv6 = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(webcontrol_ipv6, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, webcontrol_ipv6);
    }
}

void cls_config::edit_webcontrol_localhost(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_localhost = true;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(webcontrol_localhost, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, webcontrol_localhost);
    }
}

void cls_config::edit_webcontrol_access(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        webcontrol_access = PARM_LVL_02;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < PARM_LVL_00) || (parm_in > PARM_LVL_04)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_access %d"),parm_in);
        } else {
            webcontrol_access = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(webcontrol_access);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\",\"4\"";
        parm = parm + "]";
    }
}

void cls_config::edit_webcontrol_interface(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        if ((this == app->cfg) || (this == app->conf_src)) {
            webcontrol_interface = "default";
        } else {
            webcontrol_interface = "stream";
        }
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "default") || (parm == "stream"))  {
            webcontrol_interface = parm;
        } else if (parm == "") {
            webcontrol_interface = "default";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_interface %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_interface;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"default\",\"stream\"";
        parm = parm + "]";
    }
}

void cls_config::edit_webcontrol_auth_method(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_auth_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "basic") || (parm == "digest"))  {
            webcontrol_auth_method = parm;
        } else if (parm == "") {
            webcontrol_auth_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_auth_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_auth_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"none\",\"basic\",\"digest\"";
        parm = parm + "]";
    }
}

void cls_config::edit_webcontrol_auth_admin(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_auth_admin = "";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            webcontrol_auth_admin = parm;
        } else if (parm.find(":",0) == std::string::npos) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_admin.  No colon separator found"));
        } else if (parm.find(":",0) == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_admin.  No username found"));
        } else if (parm.find(":",0) == parm.length()-1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_admin.  No password found"));
        } else {
            webcontrol_auth_admin = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_auth_admin;
    }
}

void cls_config::edit_webcontrol_auth_user(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_auth_user = "";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            webcontrol_auth_user = parm;
        } else if (parm.find(":",0) == std::string::npos) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_user.  No colon separator found"));
        } else if (parm.find(":",0) == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_user.  No username found"));
        } else if (parm.find(":",0) == parm.length()-1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_auth_user.  No password found"));
        } else {
            webcontrol_auth_user = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_auth_user;
    }
}

void cls_config::edit_webcontrol_tls(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_tls = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(webcontrol_tls, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, webcontrol_tls);
    }
}

void cls_config::edit_webcontrol_cert(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_cert = "";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_cert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_cert;
    }
}

void cls_config::edit_webcontrol_key(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_key = "";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_key = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_key;
    }
}

void cls_config::edit_webcontrol_headers(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_headers = "";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_headers = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_headers;
    }
}

void cls_config::edit_webcontrol_html(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_html = "samplepage.html";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_html = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_html;
    }
}

void cls_config::edit_webcontrol_actions(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_actions = "";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_actions = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_actions;
    }
}

void cls_config::edit_webcontrol_lock_minutes(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        webcontrol_lock_minutes = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_lock_minutes %d"),parm_in);
        } else {
            webcontrol_lock_minutes = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(webcontrol_lock_minutes);
    }
}

void cls_config::edit_webcontrol_lock_attempts(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        webcontrol_lock_attempts = 3;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_lock_attempts %d"),parm_in);
        } else {
            webcontrol_lock_attempts = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(webcontrol_lock_attempts);
    }
}

void cls_config::edit_webcontrol_lock_script(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_lock_script = "";
    } else if (pact == PARM_ACT_SET) {
        webcontrol_lock_script = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_lock_script;
    }
}

void cls_config::edit_stream_allcam_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        stream_allcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        stream_allcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = stream_allcam_params;
    }
}

void cls_config::edit_stream_quality(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        stream_quality = 50;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_quality %d"),parm_in);
        } else {
            stream_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(stream_quality);
    }
}

void cls_config::edit_stream_grey(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        stream_grey = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(stream_grey, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, stream_grey);
    }
}

void cls_config::edit_stream_motion(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        stream_motion = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(stream_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, stream_motion);
    }
}

void cls_config::edit_stream_maxrate(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        stream_maxrate = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_maxrate %d"),parm_in);
        } else {
            stream_maxrate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(stream_maxrate);
    }
}

void cls_config::edit_database_type(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        database_type = "sqlite3";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mariadb") || (parm == "") ||
            (parm == "postgresql") || (parm == "sqlite3")) {
            database_type = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = database_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"\",\"mariadb\",\"postgresql\",\"sqlite3\"";
        parm = parm + "]";
    }
}

void cls_config::edit_database_dbname(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        database_dbname = std::string( configdir ) + std::string("/motion.db");
    } else if (pact == PARM_ACT_SET) {
        database_dbname = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = database_dbname;
    }
}

void cls_config::edit_database_host(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        database_host = "";
    } else if (pact == PARM_ACT_SET) {
        database_host = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = database_host;
    }
}

void cls_config::edit_database_port(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        database_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_port %d"),parm_in);
        } else {
            database_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(database_port);
    }
}

void cls_config::edit_database_user(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        database_user = "";
    } else if (pact == PARM_ACT_SET) {
        database_user = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = database_user;
    }
}

void cls_config::edit_database_password(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        database_password = "";
    } else if (pact == PARM_ACT_SET) {
        database_password = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = database_password;
    }
}

void cls_config::edit_database_busy_timeout(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        database_busy_timeout = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_busy_timeout %d"),parm_in);
        } else {
            database_busy_timeout = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(database_busy_timeout);
    }
}

void cls_config::edit_sql_event_start(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        sql_event_start = "";
    } else if (pact == PARM_ACT_SET) {
        sql_event_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = sql_event_start;
    }
}

void cls_config::edit_sql_event_end(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        sql_event_end = "";
    } else if (pact == PARM_ACT_SET) {
        sql_event_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = sql_event_end;
    }
}

void cls_config::edit_sql_movie_start(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        sql_movie_start = "";
    } else if (pact == PARM_ACT_SET) {
        sql_movie_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = sql_movie_start;
    }
}

void cls_config::edit_sql_movie_end(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        sql_movie_end = "";
    } else if (pact == PARM_ACT_SET) {
        sql_movie_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = sql_movie_end;
    }
}

void cls_config::edit_sql_pic_save(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        sql_pic_save = "";
    } else if (pact == PARM_ACT_SET) {
        sql_pic_save = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = sql_pic_save;
    }
}

void cls_config::edit_ptz_auto_track(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_auto_track = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(ptz_auto_track, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, ptz_auto_track);
    }
}

void cls_config::edit_ptz_wait(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        ptz_wait = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid ptz_wait %d"),parm_in);
        } else {
            ptz_wait = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(ptz_wait);
    }
}

void cls_config::edit_ptz_move_track(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_move_track = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_move_track = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_move_track;
    }
}

void cls_config::edit_ptz_pan_left(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_pan_left = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_pan_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_pan_left;
    }
}

void cls_config::edit_ptz_pan_right(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_pan_right = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_pan_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_pan_right;
    }
}

void cls_config::edit_ptz_tilt_up(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_tilt_up = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_tilt_up = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_tilt_up;
    }
}

void cls_config::edit_ptz_tilt_down(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_tilt_down = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_tilt_down = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_tilt_down;
    }
}

void cls_config::edit_ptz_zoom_in(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_zoom_in = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_zoom_in = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_zoom_in;
    }
}

void cls_config::edit_ptz_zoom_out(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        ptz_zoom_out = "";
    } else if (pact == PARM_ACT_SET) {
        ptz_zoom_out = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = ptz_zoom_out;
    }
}

void cls_config::edit_snd_device(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        snd_device = "";
    } else if (pact == PARM_ACT_SET) {
        snd_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = snd_device;
    }
}

void cls_config::edit_snd_params(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        snd_params = "";
    } else if (pact == PARM_ACT_SET) {
        snd_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = snd_params;
    }
}

void cls_config::edit_snd_alerts(std::string &parm, enum PARM_ACT pact)
{
    std::list<std::string>::iterator   it;

    if (pact == PARM_ACT_DFLT) {
        snd_alerts.clear();
        if (parm == "") {
            return;
        }
        snd_alerts.push_back(parm);
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            return;
        }
        snd_alerts.push_back(parm);   /* Add to the end of list*/
    } else if (pact == PARM_ACT_GET) {
        if (snd_alerts.empty()) {
            parm = "";
        } else {
            parm = snd_alerts.back();     /* Give last item*/
        }
    }
}

void cls_config::edit_snd_alerts(std::list<std::string> &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        snd_alerts.clear();
        snd_alerts = parm;
    } else if (pact == PARM_ACT_SET) {
        snd_alerts = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = snd_alerts;
    }
}

void cls_config::edit_snd_window(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        snd_window = "hamming";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "hamming") || (parm == "hann")) {
            snd_window = parm;
        } else if (parm == "") {
            snd_window = "hamming";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snd_window %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = snd_window;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"hamming\",\"hann\"]";
    }
}

void cls_config::edit_snd_show(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
       snd_show = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(snd_show, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, snd_show);
    }
}

void cls_config::edit_cat00(std::string cmd, std::string &parm_val, enum PARM_ACT pact)
{
    if (cmd == "daemon") {                         edit_daemon(parm_val, pact);
    } else if (cmd == "conf_filename") {           edit_conf_filename(parm_val, pact);
    } else if (cmd == "pid_file") {                edit_pid_file(parm_val, pact);
    } else if (cmd == "log_file") {                edit_log_file(parm_val, pact);
    } else if (cmd == "log_level") {               edit_log_level(parm_val, pact);
    } else if (cmd == "log_fflevel") {             edit_log_fflevel(parm_val, pact);
    } else if (cmd == "log_type") {                edit_log_type(parm_val, pact);
    } else if (cmd == "native_language") {         edit_native_language(parm_val, pact);
    } else if (cmd == "native_language_code") {    edit_native_language_code(parm_val, pact);
    }

}

void cls_config::edit_cat01(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "config_dir") {                   edit_config_dir(parm_val, pact);
    } else if (parm_nm == "device_name") {           edit_device_name(parm_val, pact);
    } else if (parm_nm == "device_id") {             edit_device_id(parm_val, pact);
    } else if (parm_nm == "device_tmo") {            edit_device_tmo(parm_val, pact);
    } else if (parm_nm == "pause") {                 edit_pause(parm_val, pact);
    } else if (parm_nm == "schedule_params") {       edit_schedule_params(parm_val, pact);
    } else if (parm_nm == "cleandir_params") {       edit_cleandir_params(parm_val, pact);
    } else if (parm_nm == "target_dir") {            edit_target_dir(parm_val, pact);
    } else if (parm_nm == "watchdog_tmo") {          edit_watchdog_tmo(parm_val, pact);
    } else if (parm_nm == "watchdog_kill") {         edit_watchdog_kill(parm_val, pact);
    }

}

void cls_config::edit_cat02(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{

    if (parm_nm == "v4l2_device") {                  edit_v4l2_device(parm_val, pact);
    } else if (parm_nm == "v4l2_params") {           edit_v4l2_params(parm_val, pact);
    } else if (parm_nm == "netcam_url") {            edit_netcam_url(parm_val, pact);
    } else if (parm_nm == "netcam_params") {         edit_netcam_params(parm_val, pact);
    } else if (parm_nm == "netcam_high_url") {       edit_netcam_high_url(parm_val, pact);
    } else if (parm_nm == "netcam_high_params") {    edit_netcam_high_params(parm_val, pact);
    } else if (parm_nm == "netcam_userpass") {       edit_netcam_userpass(parm_val, pact);
    } else if (parm_nm == "libcam_device") {         edit_libcam_device(parm_val, pact);
    } else if (parm_nm == "libcam_params") {         edit_libcam_params(parm_val, pact);
    }

}

void cls_config::edit_cat03(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "width") {                          edit_width(parm_val, pact);
    } else if (parm_nm == "height") {                  edit_height(parm_val, pact);
    } else if (parm_nm == "framerate") {               edit_framerate(parm_val, pact);
    } else if (parm_nm == "rotate") {                  edit_rotate(parm_val, pact);
    } else if (parm_nm == "flip_axis") {               edit_flip_axis(parm_val, pact);
    }

}

void cls_config::edit_cat04(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "locate_motion_mode") {             edit_locate_motion_mode(parm_val, pact);
    } else if (parm_nm == "locate_motion_style") {     edit_locate_motion_style(parm_val, pact);
    } else if (parm_nm == "text_left") {               edit_text_left(parm_val, pact);
    } else if (parm_nm == "text_right") {              edit_text_right(parm_val, pact);
    } else if (parm_nm == "text_changes") {            edit_text_changes(parm_val, pact);
    } else if (parm_nm == "text_scale") {              edit_text_scale(parm_val, pact);
    } else if (parm_nm == "text_event") {              edit_text_event(parm_val, pact);
    }

}

void cls_config::edit_cat05(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "emulate_motion") {                 edit_emulate_motion(parm_val, pact);
    } else if (parm_nm == "threshold") {               edit_threshold(parm_val, pact);
    } else if (parm_nm == "threshold_maximum") {       edit_threshold_maximum(parm_val, pact);
    } else if (parm_nm == "threshold_tune") {          edit_threshold_tune(parm_val, pact);
    } else if (parm_nm == "secondary_method") {        edit_secondary_method(parm_val, pact);
    } else if (parm_nm == "secondary_params") {        edit_secondary_params(parm_val, pact);
    }

}

void cls_config::edit_cat06(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "noise_level") {                    edit_noise_level(parm_val, pact);
    } else if (parm_nm == "noise_tune") {              edit_noise_tune(parm_val, pact);
    } else if (parm_nm == "despeckle_filter") {        edit_despeckle_filter(parm_val, pact);
    } else if (parm_nm == "area_detect") {             edit_area_detect(parm_val, pact);
    } else if (parm_nm == "mask_file") {               edit_mask_file(parm_val, pact);
    } else if (parm_nm == "mask_privacy") {            edit_mask_privacy(parm_val, pact);
    } else if (parm_nm == "smart_mask_speed") {        edit_smart_mask_speed(parm_val, pact);
    }

}

void cls_config::edit_cat07(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "lightswitch_percent") {            edit_lightswitch_percent(parm_val, pact);
    } else if (parm_nm == "lightswitch_frames") {      edit_lightswitch_frames(parm_val, pact);
    } else if (parm_nm == "minimum_motion_frames") {   edit_minimum_motion_frames(parm_val, pact);
    } else if (parm_nm == "static_object_time") {      edit_static_object_time(parm_val, pact);
    } else if (parm_nm == "event_gap") {               edit_event_gap(parm_val, pact);
    } else if (parm_nm == "pre_capture") {             edit_pre_capture(parm_val, pact);
    } else if (parm_nm == "post_capture") {            edit_post_capture(parm_val, pact);
    }

}

void cls_config::edit_cat08(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "on_event_start") {                 edit_on_event_start(parm_val, pact);
    } else if (parm_nm == "on_event_end") {            edit_on_event_end(parm_val, pact);
    } else if (parm_nm == "on_picture_save") {         edit_on_picture_save(parm_val, pact);
    } else if (parm_nm == "on_area_detected") {        edit_on_area_detected(parm_val, pact);
    } else if (parm_nm == "on_motion_detected") {      edit_on_motion_detected(parm_val, pact);
    } else if (parm_nm == "on_movie_start") {          edit_on_movie_start(parm_val, pact);
    } else if (parm_nm == "on_movie_end") {            edit_on_movie_end(parm_val, pact);
    } else if (parm_nm == "on_camera_lost") {          edit_on_camera_lost(parm_val, pact);
    } else if (parm_nm == "on_camera_found") {         edit_on_camera_found(parm_val, pact);
    } else if (parm_nm == "on_secondary_detect") {     edit_on_secondary_detect(parm_val, pact);
    } else if (parm_nm == "on_action_user") {          edit_on_action_user(parm_val, pact);
    } else if (parm_nm == "on_sound_alert") {          edit_on_sound_alert(parm_val, pact);
    }

}

void cls_config::edit_cat09(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "picture_output") {                 edit_picture_output(parm_val, pact);
    } else if (parm_nm == "picture_output_motion") {   edit_picture_output_motion(parm_val, pact);
    } else if (parm_nm == "picture_type") {            edit_picture_type(parm_val, pact);
    } else if (parm_nm == "picture_quality") {         edit_picture_quality(parm_val, pact);
    } else if (parm_nm == "picture_exif") {            edit_picture_exif(parm_val, pact);
    } else if (parm_nm == "picture_filename") {        edit_picture_filename(parm_val, pact);
    } else if (parm_nm == "snapshot_interval") {       edit_snapshot_interval(parm_val, pact);
    } else if (parm_nm == "snapshot_filename") {       edit_snapshot_filename(parm_val, pact);
    }

}

void cls_config::edit_cat10(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "movie_output") {                   edit_movie_output(parm_val, pact);
    } else if (parm_nm == "movie_output_motion") {     edit_movie_output_motion(parm_val, pact);
    } else if (parm_nm == "movie_max_time") {          edit_movie_max_time(parm_val, pact);
    } else if (parm_nm == "movie_container") {         edit_movie_container(parm_val, pact);
    } else if (parm_nm == "movie_passthrough") {       edit_movie_passthrough(parm_val, pact);
    } else if (parm_nm == "movie_filename") {          edit_movie_filename(parm_val, pact);
    } else if (parm_nm == "movie_extpipe_use") {       edit_movie_extpipe_use(parm_val, pact);
    } else if (parm_nm == "movie_extpipe") {           edit_movie_extpipe(parm_val, pact);
    } else if (parm_nm == "movie_params") {            edit_movie_params(parm_val, pact);
    }

}

void cls_config::edit_cat11(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "timelapse_interval") {             edit_timelapse_interval(parm_val, pact);
    } else if (parm_nm == "timelapse_mode") {          edit_timelapse_mode(parm_val, pact);
    } else if (parm_nm == "timelapse_fps") {           edit_timelapse_fps(parm_val, pact);
    } else if (parm_nm == "timelapse_container") {     edit_timelapse_container(parm_val, pact);
    } else if (parm_nm == "timelapse_filename") {      edit_timelapse_filename(parm_val, pact);
    }

}

void cls_config::edit_cat12(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "video_pipe") {                     edit_video_pipe(parm_val, pact);
    } else if (parm_nm == "video_pipe_motion") {       edit_video_pipe_motion(parm_val, pact);
    }

}

void cls_config::edit_cat13(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "webcontrol_port") {                    edit_webcontrol_port(parm_val, pact);
    } else if (parm_nm == "webcontrol_base_path") {        edit_webcontrol_base_path(parm_val, pact);
    } else if (parm_nm == "webcontrol_ipv6") {             edit_webcontrol_ipv6(parm_val, pact);
    } else if (parm_nm == "webcontrol_localhost") {        edit_webcontrol_localhost(parm_val, pact);
    } else if (parm_nm == "webcontrol_access") {           edit_webcontrol_access(parm_val, pact);
    } else if (parm_nm == "webcontrol_interface") {        edit_webcontrol_interface(parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_method") {      edit_webcontrol_auth_method(parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_admin") {       edit_webcontrol_auth_admin(parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_user") {        edit_webcontrol_auth_user(parm_val, pact);
    } else if (parm_nm == "webcontrol_tls") {              edit_webcontrol_tls(parm_val, pact);
    } else if (parm_nm == "webcontrol_cert") {             edit_webcontrol_cert(parm_val, pact);
    } else if (parm_nm == "webcontrol_key") {              edit_webcontrol_key(parm_val, pact);
    } else if (parm_nm == "webcontrol_headers") {          edit_webcontrol_headers(parm_val, pact);
    } else if (parm_nm == "webcontrol_html") {             edit_webcontrol_html(parm_val, pact);
    } else if (parm_nm == "webcontrol_actions") {          edit_webcontrol_actions(parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_minutes") {     edit_webcontrol_lock_minutes(parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_attempts") {    edit_webcontrol_lock_attempts(parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_script") {      edit_webcontrol_lock_script(parm_val, pact);
    }

}

void cls_config::edit_cat14(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "stream_allcam_params") {    edit_stream_allcam_params(parm_val, pact);
    } else if (parm_nm == "stream_quality") {   edit_stream_quality(parm_val, pact);
    } else if (parm_nm == "stream_grey") {      edit_stream_grey(parm_val, pact);
    } else if (parm_nm == "stream_motion") {    edit_stream_motion(parm_val, pact);
    } else if (parm_nm == "stream_maxrate") {   edit_stream_maxrate(parm_val, pact);
    }

}

void cls_config::edit_cat15(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "database_type") {                 edit_database_type(parm_val, pact);
    } else if (parm_nm == "database_dbname") {        edit_database_dbname(parm_val, pact);
    } else if (parm_nm == "database_host") {          edit_database_host(parm_val, pact);
    } else if (parm_nm == "database_port") {          edit_database_port(parm_val, pact);
    } else if (parm_nm == "database_user") {          edit_database_user(parm_val, pact);
    } else if (parm_nm == "database_password") {      edit_database_password(parm_val, pact);
    } else if (parm_nm == "database_busy_timeout") {  edit_database_busy_timeout(parm_val, pact);
    }

}

void cls_config::edit_cat16(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "sql_event_start") {         edit_sql_event_start(parm_val, pact);
    } else if (parm_nm == "sql_event_end") {    edit_sql_event_end(parm_val, pact);
    } else if (parm_nm == "sql_movie_start") {  edit_sql_movie_start(parm_val, pact);
    } else if (parm_nm == "sql_movie_end") {    edit_sql_movie_end(parm_val, pact);
    } else if (parm_nm == "sql_pic_save") {     edit_sql_pic_save(parm_val, pact);
    }

}

void cls_config::edit_cat17(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "ptz_auto_track") {         edit_ptz_auto_track(parm_val, pact);
    } else if (parm_nm == "ptz_wait") {        edit_ptz_wait(parm_val, pact);
    } else if (parm_nm == "ptz_move_track") {  edit_ptz_move_track(parm_val, pact);
    } else if (parm_nm == "ptz_pan_left") {    edit_ptz_pan_left(parm_val, pact);
    } else if (parm_nm == "ptz_pan_right") {   edit_ptz_pan_right(parm_val, pact);
    } else if (parm_nm == "ptz_tilt_up") {     edit_ptz_tilt_up(parm_val, pact);
    } else if (parm_nm == "ptz_tilt_down") {   edit_ptz_tilt_down(parm_val, pact);
    } else if (parm_nm == "ptz_zoom_in") {     edit_ptz_zoom_in(parm_val, pact);
    } else if (parm_nm == "ptz_zoom_out") {    edit_ptz_zoom_out(parm_val, pact);
    }
}

void cls_config::edit_cat18(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "snd_device") {          edit_snd_device(parm_val, pact);
    } else if (parm_nm == "snd_params") {   edit_snd_params(parm_val, pact);
    } else if (parm_nm == "snd_window") {   edit_snd_window(parm_val, pact);
    } else if (parm_nm == "snd_alerts") {   edit_snd_alerts(parm_val, pact);
    } else if (parm_nm == "snd_show") {     edit_snd_show(parm_val, pact);

    }
}

void cls_config::edit_cat18(std::string parm_nm,std::list<std::string> &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "snd_alerts") {  edit_snd_alerts(parm_val, pact);
    }
}

void cls_config::edit_cat(std::string parm_nm, std::list<std::string> &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_18) {
        edit_cat18(parm_nm, parm_val, pact);
    }
}

void cls_config::edit_cat(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_00) {          edit_cat00(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_01) {   edit_cat01(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_02) {   edit_cat02(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_03) {   edit_cat03(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_04) {   edit_cat04(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_05) {   edit_cat05(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_06) {   edit_cat06(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_07) {   edit_cat07(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_08) {   edit_cat08(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_09) {   edit_cat09(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_10) {   edit_cat10(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_11) {   edit_cat11(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_12) {   edit_cat12(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_13) {   edit_cat13(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_14) {   edit_cat14(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_15) {   edit_cat15(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_16) {   edit_cat16(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_17) {   edit_cat17(parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_18) {   edit_cat18(parm_nm, parm_val, pact);
    }

}

void cls_config::params_load_schedule()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "schedule_params";
    prm->params_pcat = PARM_PCAT_SCH;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"detection","",PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Detection on/off(checked/unchecked)"});
    prm->params_array.push_back(
        {"action","",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"pause","stop"},""});

    prm->params_array.push_back(
        {"sun","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Sunday"});
    prm->params_array.push_back(
        {"mon","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Monday"});
    prm->params_array.push_back(
        {"tue","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Tuesday"});
    prm->params_array.push_back(
        {"wed","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Wednesday"});
    prm->params_array.push_back(
        {"thu","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Thursday"});
    prm->params_array.push_back(
        {"fri","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Friday"});
    prm->params_array.push_back(
        {"sat","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Saturday"});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"schedule_params",schedule_params,PARM_PCAT_SCH);
    params.push_back(prm);
}

void cls_config::params_load_cleandir()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "cleandir_params";
    prm->params_pcat = PARM_PCAT_CLN;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"action","",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"delete","script"},""});
    prm->params_array.push_back(
        {"freq","",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"hourly","daily","weekly"},""});
    prm->params_array.push_back(
        {"runtime","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"dur_unit","",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"m","h","d","w"},"duration units(minutes,hours,days,weeks)"});
    prm->params_array.push_back(
        {"dur_val","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"duration value (expressed in duration units)"});
    prm->params_array.push_back(
        {"removedir","",PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Remove directory"});
    prm->params_array.push_back(
        {"script","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_03, PARM_CHG_CODE
        ,{""},"script to execute"});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"cleandir_params",cleandir_params,PARM_PCAT_CLN);

    params.push_back(prm);

}

void cls_config::params_load_v4l2()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "v4l2_params";
    prm->params_pcat = PARM_PCAT_V4L2;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"input","-1",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"palette","17",PARM_SRC_SYS
        , PARM_TYP_INTLIST , PARM_LVL_01, PARM_CHG_CODE
        ,{"00-S910", "01-BYR2", "02-BA81", "03-S561", "04-GBRG"
        , "05-GRBG", "06-P207", "07-PJPG", "08-MJPG", "09-JPEG"
        , "10-RGB3", "11-S501", "12-S505", "13-S508", "14-UYVY"
        , "15-YUYV", "16-422P", "17-YU12", "18-Y10" , "19-Y12"
        , "20-GREY"},""});
    prm->params_array.push_back(
        {"norm","0",PARM_SRC_SYS
        , PARM_TYP_INTLIST , PARM_LVL_01, PARM_CHG_CODE
        ,{"0-PAL", "1-NTSC", "2-SECCAM", "3-PALBW"},""});
    prm->params_array.push_back(
        {"frequency","0",PARM_SRC_SYS
        , PARM_TYP_INT , PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"v4l2_params",v4l2_params,PARM_PCAT_V4L2);

    params.push_back(prm);

}

void cls_config::params_load_netcam()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "netcam_params";
    prm->params_pcat = PARM_PCAT_NET;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"decoder","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"capture_rate","",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"interrupt","",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"netcam_params",netcam_params,PARM_PCAT_NET);

    params.push_back(prm);

}

void cls_config::params_load_netcam_high()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "netcam_high_params";
    prm->params_pcat = PARM_PCAT_NETH;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"decoder","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"capture_rate","",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"interrupt","",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"netcam_high_params",netcam_high_params,PARM_PCAT_NETH);

    params.push_back(prm);

}

void cls_config::params_load_libcamera()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "libcam_params";
    prm->params_pcat = PARM_PCAT_LIB;
    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"pixelformat","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_array.push_back(
        {"Orientation","", PARM_SRC_SYS
        ,PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"Rotate0", "Rotate0Mirror", "Rotate180", "Rotate180Mirror"
        ,"Rotate90", "Rotate90Mirror", "Rotate270", "Rotate270Mirror"}
        ,""});
    prm->params_array.push_back(
        {"AeConstraintMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Normal", "1-Highlight", "2-Shadows", "3-Custom"}
        ,""});
    prm->params_array.push_back(
        {"AeEnable","",PARM_SRC_SYS
        ,PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"AeExposureMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Normal", "1-Short", "2-Long", "3-Custom"},""});
    prm->params_array.push_back(
        {"AeFlickerMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Off", "1-Manual", "2-Auto"},""});
    prm->params_array.push_back(
        {"AeFlickerPeriod","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"AeMeteringMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-CentreWeighted", "1-Spot", "2-Matrix", "3-Custom"},""});
    prm->params_array.push_back(
        {"AeState","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Idle", "1-Searching", "2-Converged"},""});
    prm->params_array.push_back(
        {"AfMetering","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Auto", "1-Windows"},""});
    prm->params_array.push_back(
        {"AfMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Manual", "1-Auto", "2-Continuous"},""});
    prm->params_array.push_back(
        {"AfPause","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Immediate", "1-Deferred", "2-Resume"},""});
    prm->params_array.push_back(
        {"AfPauseState","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Running", "1-Pausing", "2-Paused"},""});
    prm->params_array.push_back(
        {"AfRange","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Normal", "1-Macro", "2-Full"},""});
    prm->params_array.push_back(
        {"AfSpeed","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Normal", "1-Fast"},""});
    prm->params_array.push_back(
        {"AfState","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Idle", "1-Scanning", "2-Focused", "3-Failed"},""});
    prm->params_array.push_back(
        {"AfTrigger","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Start", "1-Cancel"},""});
    prm->params_array.push_back(
        {"AfWindows","",PARM_SRC_SYS
        ,PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as x|y|h|w"});
    prm->params_array.push_back(
        {"AnalogueGain","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"AnalogueGainMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Auto", "1-Manual"},""});
    prm->params_array.push_back(
        {"AwbEnable","",PARM_SRC_SYS
        ,PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"AwbLocked","",PARM_SRC_SYS
        ,PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"AwbMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Auto", "1-Incandescent", "2-Tungsten", "3-Fluorescent"
        ,"4-Indoor", "5-Daylight", "6-Cloudy", "7-Custom"},""});
    prm->params_array.push_back(
        {"Brightness","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"ColourCorrectionMatrix","",PARM_SRC_SYS
        ,PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as var1|var2|...|var8|var9"});
    prm->params_array.push_back(
        {"ColourGains","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as Red|Blue)"});
    prm->params_array.push_back(
        {"ColourTemperature","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"Contrast","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"DigitalGain","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"ExposureTime","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"ExposureTimeMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Auto", "1-Manual"},""});
    prm->params_array.push_back(
        {"ExposureValue","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"FocusFoM","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"FrameDuration","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"FrameDurationLimits","",PARM_SRC_SYS
        ,PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as min|max"});
    prm->params_array.push_back(
        {"Gamma","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"HdrChannel","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-None", "1-Short", "2-Medium", "3-Long"},""});
    prm->params_array.push_back(
        {"HdrMode","",PARM_SRC_SYS
        ,PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Off", "1-MultiExposureUnmerged", "2-MultiExposure"
        ,"3-SingleExposure", "4-Night"},""});
    prm->params_array.push_back(
        {"Hue","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"LensPosition","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"Lux","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"Saturation","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"ScalerCrop","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as x|y|h|w"});
    prm->params_array.push_back(
        {"SensorBlackLevels","",PARM_SRC_SYS
        ,PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},"Pipe delimited as var1|var2|var3|var4"});
    prm->params_array.push_back(
        {"SensorTemperature","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"SensorTimestamp","",PARM_SRC_SYS
        ,PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"Sharpness","",PARM_SRC_SYS
        ,PARM_TYP_FLOAT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"WdrMode","",PARM_SRC_SYS
        , PARM_TYP_INTLIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"0-Off", "1-Linear", "2-Power"
        ,"3-Exponential", "4-HistogramEqualization"},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"libcam_params",libcam_params,PARM_PCAT_LIB);
    params.push_back(prm);
}

void cls_config::params_load_secondary()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "secondary_params";
    prm->params_pcat = PARM_PCAT_SEC;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"frame_interval","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"model_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"image_type","",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"full","grey","roi"},""});
    prm->params_array.push_back(
        {"rotate","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"threshold","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"scalefactor","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"minsize","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"maxsize","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"padding","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"threshold_model","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"winstride","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"flags","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"minneighbors","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"config","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"classes_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"framework","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"backend","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"target","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"scale","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"width","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"height","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"secondary_params",secondary_params,PARM_PCAT_SEC);

    params.push_back(prm);

}

void cls_config::params_load_allcam()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "stream_allcam_params";
    prm->params_pcat = PARM_PCAT_ACAM;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"scale","100",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"row","1",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"col","1",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"offset_row","0",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"offset_col","0",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"stream_allcam_params",stream_allcam_params,PARM_PCAT_ACAM);

    params.push_back(prm);

}

void cls_config::params_load_actions()
{
    ctx_params *prm;
    std::string parm_vl;

    if (webcontrol_access == 0) {
        parm_vl = "off";
    } else {
        parm_vl = "on";
    }

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "webcontrol_actions";
    prm->params_pcat = PARM_PCAT_ACTS;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"pause",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"event",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"snapshot",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"camera_add",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"camera_delete",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"ptz",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"stop",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"restart",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"action_user",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"config_write",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"config",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"movies",parm_vl,PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});

    prm->params_cnt = (int)prm->params_array.size();
    util_parms_parse(prm,"webcontrol_actions", webcontrol_actions, PARM_PCAT_ACTS);

    params.push_back(prm);

}

void cls_config::params_load_headers()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "webcontrol_headers";
    prm->params_pcat = PARM_PCAT_HDR;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"webcontrol_headers",webcontrol_headers,PARM_PCAT_HDR);

    params.push_back(prm);

}

void cls_config::params_load_sound()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "snd_params";
    prm->params_pcat = PARM_PCAT_SND;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_cnt = (int)prm->params_array.size();

    util_parms_parse(prm,"snd_params",snd_params,PARM_PCAT_SND);

    params.push_back(prm);

}

void cls_config::params_load_movie()
{
    ctx_params *prm;

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "movie_params";
    prm->params_pcat = PARM_PCAT_MOVIE;

    /*param_name,param_value,param_source*/
    /*param_type,param_lvl,param_chg,param_lst,desc*/
    prm->params_array.push_back(
        {"params_file","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"preferred_codec","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"quality","60",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"profile","main",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"baseline","main","high"},""});
    prm->params_array.push_back(
        {"bit_rate","0",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"dict_preset","superfast",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"ultrafast","superfast","veryfast","faster"
        ,"fast","medium","slow","slower","veryslow"},""});
    prm->params_array.push_back(
        {"dict_tune","zerolatency",PARM_SRC_SYS
        , PARM_TYP_LIST, PARM_LVL_01, PARM_CHG_CODE
        ,{"film","animation","grain","stillimage"
        ,"fastdecode","zerolatency"},""});
    prm->params_array.push_back(
        {"opt_profile","main",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{"baseline","main","high"},""});
    prm->params_array.push_back(
        {"opt_crf","18",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"opt_preset","superfast",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{"ultrafast","superfast","veryfast","faster"
        ,"fast","medium","slow","slower","veryslow"},""});
    prm->params_array.push_back(
        {"global_quality","75",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"flags","1<<22|1<<1",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"flags2","",PARM_SRC_SYS
        , PARM_TYP_STRING, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"gop_size","0",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"max_b_frames","0",PARM_SRC_SYS
        , PARM_TYP_INT, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"secdetect_only","false",PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});
    prm->params_array.push_back(
        {"all_frames","false",PARM_SRC_SYS
        , PARM_TYP_BOOL, PARM_LVL_01, PARM_CHG_CODE
        ,{""},""});


    prm->params_cnt = (int)prm->params_array.size();

    params_load_dpritm(app->conf_src, prm, PARM_PCAT_MOVIE);

    util_parms_parse(prm,"movie_params",movie_params,PARM_PCAT_MOVIE);

    params.push_back(prm);

}

void cls_config::params_load_dpritm(cls_config *cfgs, ctx_params *prmd, PARM_PCAT pcat)
{
    int indx;
    ctx_params *dprm;

    dprm = nullptr;
    for (indx=0;indx<cfgs->depr_params.size();indx++){
        if (cfgs->depr_params[indx]->params_pcat == pcat) {
            dprm =cfgs->depr_params[indx];
        }
    }
    if (dprm != nullptr) {
        for (indx=0;indx<dprm->params_array.size();indx++) {
            util_parms_add(prmd
                , dprm->params_array[indx].param_name
                , dprm->params_array[indx].param_value, false);
        }
    }
}

void cls_config::params_load_listitm(cls_config *cfgs, ctx_params *prma,ctx_params *prmd)
{
    int ipa, ipd;
    ctx_params_item *itm_a, *itm_d, itm_n;
    bool chknew;
    std::string ptmp;

    if (prmd->params_pcat == PARM_PCAT_SCH) {           ptmp=cfgs->schedule_params;
    } else if (prmd->params_pcat == PARM_PCAT_CLN) {    ptmp=cfgs->cleandir_params;
    } else if (prmd->params_pcat == PARM_PCAT_V4L2){    ptmp=cfgs->v4l2_params;
    } else if (prmd->params_pcat == PARM_PCAT_NET) {    ptmp=cfgs->netcam_params;
    } else if (prmd->params_pcat == PARM_PCAT_NETH) {   ptmp=cfgs->netcam_high_params;
    } else if (prmd->params_pcat == PARM_PCAT_LIB) {    ptmp=cfgs->libcam_params;
    } else if (prmd->params_pcat == PARM_PCAT_SEC) {    ptmp=cfgs->secondary_params;
    } else if (prmd->params_pcat == PARM_PCAT_ACAM) {   ptmp=cfgs->stream_allcam_params;
    } else if (prmd->params_pcat == PARM_PCAT_ACTS) {   ptmp=cfgs->webcontrol_actions;
    } else if (prmd->params_pcat == PARM_PCAT_HDR) {    ptmp=cfgs->webcontrol_headers;
    } else if (prmd->params_pcat == PARM_PCAT_SND) {    ptmp=cfgs->snd_params;
    } else if (prmd->params_pcat == PARM_PCAT_MOVIE) {  ptmp=cfgs->movie_params;
    } else {  return;
    }
    params_load_dpritm(cfgs, prmd, prmd->params_pcat);
    util_parms_parse(prmd, prmd->params_desc, ptmp, prmd->params_pcat);

    for (ipa=0;ipa<prma->params_array.size();ipa++) {
        itm_a = &prma->params_array[ipa];
        chknew = true;
        for (ipd=0;ipd<prmd->params_array.size();ipd++) {
            itm_d = &prmd->params_array[ipd];
            if (itm_a->param_name == itm_d->param_name) {
                chknew = false;
            }
        }
        if (chknew == true) {
            itm_n = prma->params_array[ipa];
            prmd->params_array.push_back(itm_n);
        }
    }
    prmd->params_cnt = (int)prmd->params_array.size();

}

void cls_config::params_load_listcam()
{
    int ica, icd, idev;
    ctx_params *prm_a, *prm_d;
    cls_camera *dev;

    for (ica=0;ica<app->conf_src->params.size();ica++) {
        prm_a = app->conf_src->params[ica];
        for (idev=0; idev<app->cam_cnt; idev++) {
            dev = app->cam_list[idev];
            prm_d = nullptr;
            for (icd=0;icd<dev->conf_src->params.size();icd++) {
                if (prm_a->params_pcat ==dev->conf_src->params[icd]->params_pcat) {
                    prm_d = dev->conf_src->params[icd];
                    break;
                }
            }
            if (prm_d == nullptr) {
                prm_d = new ctx_params;
                prm_d->app = app;
                prm_d->params_desc = prm_a->params_desc;
                prm_d->params_pcat = prm_a->params_pcat;
                prm_d->params_changed = prm_a->params_changed;
                prm_d->params_cnt = 0;
                prm_d->params_array.clear();
                dev->conf_src->params.push_back(prm_d);
            }
            params_load_listitm(dev->conf_src, prm_a, prm_d);
        }
    }
}
/* ToDo:  Optimize code by consolidating with params_load_camlist*/
void cls_config::params_load_listsnd()
{
    int ica, icd, idev;
    ctx_params *prm_a, *prm_d;
    cls_sound *dev;

    for (ica=0;ica<app->conf_src->params.size();ica++) {
        prm_a = app->conf_src->params[ica];
        for (idev=0; idev<app->snd_cnt; idev++) {
            dev =app->snd_list[idev];
            prm_d = nullptr;
            for (icd=0;icd<dev->conf_src->params.size();icd++) {
                if (prm_a->params_pcat == prm_d->params_pcat) {
                    prm_d = dev->conf_src->params[icd];
                    break;
                }
            }
            if (prm_d == nullptr) {
                prm_d = new ctx_params;
                prm_d->app = app;
                prm_d->params_desc = prm_a->params_desc;
                prm_d->params_pcat = prm_a->params_pcat;
                prm_d->params_changed = prm_a->params_changed;
                prm_d->params_cnt = 0;
                prm_d->params_array.clear();
                dev->conf_src->params.push_back(prm_d);
            }
            params_load_listitm(dev->conf_src, prm_a, prm_d);
        }
    }
}

void cls_config::params_load()
{
    params_load_schedule();
    params_load_cleandir();
    params_load_v4l2();
    params_load_netcam();
    params_load_netcam_high();
    params_load_libcamera();
    params_load_secondary();
    params_load_allcam();
    params_load_actions();
    params_load_headers();
    params_load_sound();
    params_load_movie();
}

std::string cls_config::params_prefix(PARM_PCAT pcat) {
    /* To make the webu_json/post processing easier, these
      need to be exactly 5 characters long ending in _
      Also make sure these do not match the start of the names
      of any of the primary configuration parameters */
    if (pcat == PARM_PCAT_SCH)          { return "psch_";
    } else if (pcat == PARM_PCAT_CLN)   { return "pcln_";
    } else if (pcat == PARM_PCAT_V4L2)  { return "pv4l_";
    } else if (pcat == PARM_PCAT_NET)   { return "pnet_";
    } else if (pcat == PARM_PCAT_NETH)  { return "pnth_";
    } else if (pcat == PARM_PCAT_LIB)   { return "plib_";
    } else if (pcat == PARM_PCAT_SEC)   { return "psec_";
    } else if (pcat == PARM_PCAT_ACAM)  { return "pacm_";
    } else if (pcat == PARM_PCAT_ACTS)  { return "pact_";
    } else if (pcat == PARM_PCAT_HDR)   { return "phdr_";
    } else if (pcat == PARM_PCAT_SND)   { return "psnd_";
    } else if (pcat == PARM_PCAT_MOVIE) { return "pmov_";
    } else { return "pudf_";
    }
}

void cls_config::defaults()
{
    int indx;
    std::string dflt = "";

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        edit_cat(config_parms[indx].parm_name, dflt
            , PARM_ACT_DFLT, config_parms[indx].parm_cat);
        indx++;
    }
    params.clear();
    defaults_depr();
}

void cls_config::defaults_depr()
{
    ctx_params *prm;

    depr_params.clear();

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "movies_params";
    prm->params_pcat = PARM_PCAT_MOVIE;
    prm->params_cnt = 0;
    depr_params.push_back(prm);

    prm = new ctx_params;
    prm->app = app;
    prm->params_desc = "v4l2_params";
    prm->params_pcat = PARM_PCAT_V4L2;
    prm->params_cnt = 0;
    depr_params.push_back(prm);

}

int cls_config::edit_set_active(std::string parm_nm, std::string parm_val)
{
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (parm_nm ==  config_parms[indx].parm_name) {
            pcat = config_parms[indx].parm_cat;
            edit_cat(parm_nm, parm_val, PARM_ACT_SET, pcat);
            return 0;
        }
        indx++;
    }
    return -1;

}

void cls_config::edit_depr_vid(std::string parm_nm, std::string parm_val)
{
    std::string pnew_nm, pnew_val;
    int indx;
    ctx_params *prm;

    prm = nullptr;
    for (indx=0;indx<depr_params.size();indx++) {
        if (depr_params[indx]->params_pcat == PARM_PCAT_V4L2) {
            prm = depr_params[indx];
        }
    }
    if (prm == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Bad programming"));
        return;
    }

    util_parms_add(prm, parm_nm, parm_val, false);
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("%s %s changed to v4l2_params option %s=%s")
        ,parm_nm.c_str(),parm_val.c_str()
        ,parm_nm.c_str(),parm_val.c_str());

}

void cls_config::edit_depr_web(std::string newname, std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "0";
    } else {
        parm_new = "1";
    }
    edit_set_active(newname, parm_new);
}

void cls_config::edit_depr_tdbl(std::string newname, std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "2";
    } else {
        parm_new = "1";
    }
    edit_set_active(newname, parm_new);
}

void cls_config::edit_depr_movie(std::string parm_nm, std::string parm_val)
{
    std::string pnew_nm, pnew_val;
    int indx;
    ctx_params *prm;

    prm = nullptr;
    for (indx=0;indx<depr_params.size();indx++) {
        if (depr_params[indx]->params_pcat == PARM_PCAT_MOVIE) {
            prm = depr_params[indx];
        }
    }
    if (prm == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Bad programming"));
        return;
    }

    if (parm_nm == "movie_quality") {
        pnew_nm = "quality";
        pnew_val = parm_val;
    } else if (parm_nm == "movie_bps") {
        pnew_nm = "bit_rate";
        pnew_val = parm_val;
    } else if (parm_nm == "movie_retain") {
        pnew_nm="secdetect_only";
        if (parm_val == "all") {
            pnew_val = "false";
        } else if (parm_val == "secondary") {
            pnew_val = "true";
        } else {
            pnew_val = "false";
        }
    } else if (parm_nm == "movie_all_frames") {
        pnew_nm="all_frames";
        pnew_val = parm_val;
    } else {
        return;
    }
    util_parms_add(prm, pnew_nm, pnew_val, false);
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("%s %s changed to movie_params option %s=%s")
        ,parm_nm.c_str(),parm_val.c_str()
        ,pnew_nm.c_str(),pnew_val.c_str());

}

int cls_config::edit_set_depr(std::string &parm_nm, std::string &parm_val)
{
    int indx;

    indx = 0;
    while (config_parms_depr[indx].parm_name != "") {
        if (parm_nm ==  config_parms_depr[indx].parm_name) {
            MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, "%s after version %s"
                , config_parms_depr[indx].info.c_str()
                , config_parms_depr[indx].last_version.c_str());

            if ((parm_nm == "brightness") || (parm_nm == "contrast") ||
                (parm_nm == "saturation") || (parm_nm == "hue") ||
                (parm_nm == "power_line_frequency")) {
                edit_depr_vid(parm_nm, parm_val);

            } else if ((parm_nm == "webcontrol_html_output")) {
                edit_depr_web(config_parms_depr[indx].newname, parm_val);

            } else if ((parm_nm == "text_double")) {
                edit_depr_tdbl(config_parms_depr[indx].newname, parm_val);

            } else if ((parm_nm == "movie_quality") || (parm_nm == "movie_bps") ||
                (parm_nm == "movie_retain") || (parm_nm == "movie_all_frames")) {
                edit_depr_movie(parm_nm, parm_val);

            } else {
                edit_set_active(config_parms_depr[indx].newname, parm_val);
            }
            return 0;
        }
        indx++;
    }
    return -1;
}

void cls_config::edit_get(std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat)
{
    edit_cat(parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

void cls_config::edit_get(std::string parm_nm, std::list<std::string> &parm_val, enum PARM_CAT parm_cat)
{
    edit_cat(parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

void cls_config::edit_set(std::string parm_nm, std::string parm_val)
{
    if (edit_set_active(parm_nm, parm_val) == 0) {
        return;
    }

    if (edit_set_depr(parm_nm, parm_val) == 0) {
        return;
    }

    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
}

void cls_config::edit_list(std::string parm_nm, std::string &parm_val, enum PARM_PCAT pcat)
{
    int indx1, indx2;
    ctx_params_item *itm;
    ctx_params  *prm;

    prm = nullptr;
    for (indx1=0;indx1<params.size();indx1++) {
        if (pcat == params[indx1]->params_pcat) {
            prm = params[indx1];
            break;
        }
    }
    if (prm == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("coding error %d"), pcat);
        parm_val = "[\"\"]";
        return;
    }
    for (indx1=0;indx1<prm->params_cnt;indx1++) {
        itm = &prm->params_array[indx1];
        if (itm->param_name == parm_nm) {
            for (indx2=0;indx2<itm->param_lst.size();indx2++) {
                if (indx2 == 0) {
                    parm_val = "[";
                } else {
                    parm_val += ",";
                }
                parm_val += "\"" + itm->param_lst[indx2] + "\"";
            }
            if (itm->param_lst.size() > 0) {
                parm_val += "]";
            }
        }
    }
    if (parm_val == "") {
        parm_val = "[\"\"]";
    }
}

void cls_config::edit_list(std::string parm_nm, std::string &parm_val, enum PARM_CAT pcat)
{
    edit_cat(parm_nm, parm_val, PARM_ACT_LIST, pcat);

    if (parm_val == "") {
        parm_val = "[\"\"]";
    }
}

std::string cls_config::type_desc(enum PARM_TYP ptype)
{
    if (ptype == PARM_TYP_BOOL) {           return "bool";
    } else if (ptype == PARM_TYP_INT) {     return "int";
    } else if (ptype == PARM_TYP_FLOAT) {   return "float";
    } else if (ptype == PARM_TYP_LIST) {    return "list";
    } else if (ptype == PARM_TYP_INTLIST) { return "intlist";
    } else if (ptype == PARM_TYP_STRING) {  return "string";
    } else if (ptype == PARM_TYP_ARRAY) {   return "array";
    } else if (ptype == PARM_TYP_PARAMS) {  return "params";
    } else {                                return "string";
    }
}

std::string cls_config::cat_desc(enum PARM_CAT pcat, bool shrt) {

    if (shrt) {
        if (pcat == PARM_CAT_00)        { return "system";
        } else if (pcat == PARM_CAT_01) { return "camera";
        } else if (pcat == PARM_CAT_02) { return "source";
        } else if (pcat == PARM_CAT_03) { return "image";
        } else if (pcat == PARM_CAT_04) { return "overlay";
        } else if (pcat == PARM_CAT_05) { return "method";
        } else if (pcat == PARM_CAT_06) { return "masks";
        } else if (pcat == PARM_CAT_07) { return "detect";
        } else if (pcat == PARM_CAT_08) { return "scripts";
        } else if (pcat == PARM_CAT_09) { return "picture";
        } else if (pcat == PARM_CAT_10) { return "movie";
        } else if (pcat == PARM_CAT_11) { return "timelapse";
        } else if (pcat == PARM_CAT_12) { return "pipes";
        } else if (pcat == PARM_CAT_13) { return "webcontrol";
        } else if (pcat == PARM_CAT_14) { return "streams";
        } else if (pcat == PARM_CAT_15) { return "database";
        } else if (pcat == PARM_CAT_16) { return "sql";
        } else if (pcat == PARM_CAT_17) { return "track";
        } else if (pcat == PARM_CAT_18) { return "sound";
        } else { return "unk";
        }
    } else {
        if (pcat == PARM_CAT_00)        { return "System";
        } else if (pcat == PARM_CAT_01) { return "Camera";
        } else if (pcat == PARM_CAT_02) { return "Source";
        } else if (pcat == PARM_CAT_03) { return "Image";
        } else if (pcat == PARM_CAT_04) { return "Overlays";
        } else if (pcat == PARM_CAT_05) { return "Method";
        } else if (pcat == PARM_CAT_06) { return "Masks";
        } else if (pcat == PARM_CAT_07) { return "Detection";
        } else if (pcat == PARM_CAT_08) { return "Scripts";
        } else if (pcat == PARM_CAT_09) { return "Picture";
        } else if (pcat == PARM_CAT_10) { return "Movie";
        } else if (pcat == PARM_CAT_11) { return "Timelapse";
        } else if (pcat == PARM_CAT_12) { return "Pipes";
        } else if (pcat == PARM_CAT_13) { return "Web Control";
        } else if (pcat == PARM_CAT_14) { return "Web Stream";
        } else if (pcat == PARM_CAT_15) { return "Database";
        } else if (pcat == PARM_CAT_16) { return "SQL";
        } else if (pcat == PARM_CAT_17) { return "Tracking";
        } else if (pcat == PARM_CAT_18) { return "Sound";
        } else { return "Other";
        }
    }
}

void cls_config::usage(void)
{
    printf("Motion version %s\n",PACKAGE_VERSION);
    printf("Copyright 2014-2026 Motion-Project maintainers\n");
    printf("Copyright 2000-2014 Jeroen Vreeken/Folkert van Heusden/Kenneth Lavrsen\n");
    printf("\nusage:\tmotion [options]\n");
    printf("\n\n");
    printf("Possible options:\n\n");
    printf("-b\t\t\tRun in background (daemon) mode.\n");
    printf("-n\t\t\tRun in non-daemon mode.\n");
    printf("-c config\t\tFull path and filename of config file.\n");
    printf("-d level\t\tLog level (1-9) (EMG, ALR, CRT, ERR, WRN, NTC, INF, DBG, ALL). default: 6 / NTC.\n");
    printf("-k type\t\t\tType of log (COR, STR, ENC, NET, DBL, EVT, TRK, VID, ALL). default: ALL.\n");
    printf("-p process_id_file\tFull path and filename of process id file (pid file).\n");
    printf("-l log file \t\tFull path and filename of log file.\n");
    printf("-m\t\t\tDisable detection at startup.\n");
    printf("-h\t\t\tShow this screen.\n");
    printf("\n");
}

void cls_config::cmdline()
{
    int c;

    while ((c = getopt(app->argc, app->argv, "bc:d:hmn?p:k:l:")) != EOF)
        switch (c) {
        case 'c':
            edit_set("conf_filename", optarg);
            break;
        case 'b':
            edit_set("daemon", "on");
            break;
        case 'n':
            edit_set("daemon", "off");
            break;
        case 'd':
            edit_set("log_level", optarg);
            break;
        case 'k':
            edit_set("log_type", optarg);
            break;
        case 'p':
            edit_set("pid_file", optarg);
            break;
        case 'l':
            edit_set("log_file", optarg);
            break;
        case 'm':
            app->user_pause = "on";
            break;
        case 'h':
        case '?':
        default:
             usage();
             exit(1);
        }

    optind = 1;
}

void cls_config::camera_filenm()
{
    int indx_cam, indx;
    std::string dirnm, fullnm;
    struct stat statbuf;
    size_t lstpos;

    lstpos = app->conf_src->conf_filename.find_last_of("/");
    if (lstpos != std::string::npos) {
        lstpos++;
    }
    dirnm = app->conf_src->conf_filename.substr(0, lstpos);

    indx_cam = 1;
    fullnm = "";
    while (fullnm == "") {
        fullnm = dirnm + "camera" + std::to_string(indx_cam) + ".conf";
        for (indx = 0; indx<app->cam_cnt; indx++) {
            if (fullnm == app->cam_list[indx]->conf_src->conf_filename) {
                fullnm = "";
            }
        }
        if (fullnm == "") {
            indx_cam++;
        } else {
            if (stat(fullnm.c_str(), &statbuf) == 0) {
                fullnm = "";
                indx_cam++;
            }
        }
    }

    conf_filename = fullnm;
}

int cls_config::get_next_devid()
{
    int indx, dev_id;
    bool  chkid;

    dev_id = 0;
    chkid = true;
    while (chkid) {
        dev_id++;
        chkid = false;
        for (indx = 0; indx<app->cam_cnt;indx++) {
            if (app->cam_list[indx]->conf_src->device_id == dev_id) {
                chkid = true;
            }
        }
        for (indx = 0; indx<app->snd_cnt;indx++) {
            if (app->snd_list[indx]->conf_src->device_id == dev_id) {
                chkid = true;
            }
        }
    }
    return dev_id;
}

void cls_config::camera_add(std::string fname, bool srcdir)
{
    struct stat statbuf;
    int indx;
    std::string parm_val, parm_nm;
    cls_camera *cam_cls;

    cam_cls = new cls_camera(app);
    cam_cls->conf_src = new cls_config(app);

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        parm_nm =config_parms[indx].parm_name;
        if ((parm_nm != "device_id") &&
            (parm_nm != "webcontrol_interface")) {
            app->conf_src->edit_get(parm_nm, parm_val, config_parms[indx].parm_cat);
            cam_cls->conf_src->edit_set(parm_nm, parm_val);
        }
        indx++;
    }

    cam_cls->conf_src->from_conf_dir = srcdir;
    cam_cls->conf_src->conf_filename = fname;
    cam_cls->conf_src->device_id = get_next_devid();

    if (fname == "") {
        cam_cls->conf_src->camera_filenm();
    } else if (stat(fname.c_str(), &statbuf) != 0) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera config file %s not found"), fname.c_str());
    } else {
        cam_cls->conf_src->process();
    }

    cam_cls->cfg = new cls_config(app);
    cam_cls->cfg->config_copy(cam_cls->conf_src);

    app->cam_list.push_back(cam_cls);
    app->cam_cnt = (int)app->cam_list.size();
}

/* Create default configuration file name*/
void cls_config::sound_filenm()
{
    int indx_snd, indx;
    std::string dirnm, fullnm;
    struct stat statbuf;
    size_t lstpos;

    lstpos = app->conf_src->conf_filename.find_last_of("/");
    if (lstpos != std::string::npos) {
        lstpos++;
    }
    dirnm = app->conf_src->conf_filename.substr(0, lstpos);

    indx_snd = 1;
    fullnm = "";
    while (fullnm == "") {
        fullnm = dirnm + "sound" + std::to_string(indx_snd) + ".conf";
        for (indx = 0; indx<app->snd_cnt; indx++) {
            if (fullnm == app->snd_list[indx]->conf_src->conf_filename) {
                fullnm = "";
            }
        }
        if (fullnm == "") {
            indx_snd++;
        } else {
            if (stat(fullnm.c_str(), &statbuf) == 0) {
                fullnm = "";
                indx_snd++;
            }
        }
    }

    conf_filename = fullnm;
}

void cls_config::sound_add(std::string fname, bool srcdir)
{
    struct stat statbuf;
    int indx;
    std::string parm_val, parm_nm;
    cls_sound *snd_cls;

    snd_cls = new cls_sound(app);
    snd_cls->conf_src = new cls_config(app);

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        parm_nm =config_parms[indx].parm_name;
        if (parm_nm != "device_id") {
            app->conf_src->edit_get(parm_nm, parm_val, config_parms[indx].parm_cat);
            snd_cls->conf_src->edit_set(parm_nm, parm_val);
        }
        indx++;
    }

    snd_cls->conf_src->from_conf_dir = srcdir;
    snd_cls->conf_src->conf_filename = fname;
    snd_cls->conf_src->device_id = get_next_devid();

    if (fname == "") {
        snd_cls->conf_src->sound_filenm();
    } else if (stat(fname.c_str(), &statbuf) != 0) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Sound config file %s not found"), fname.c_str());
    } else {
        snd_cls->conf_src->process();
    }

    snd_cls->cfg = new cls_config(app);
    snd_cls->cfg->config_copy(snd_cls->conf_src);

    app->snd_list.push_back(snd_cls);
    app->snd_cnt = (int)app->snd_list.size();
}

void cls_config::config_dir_item(std::string confdir)
{
    DIR *dp;
    dirent *ep;
    std::string file;

    dp = opendir(confdir.c_str());
    if (dp != NULL) {
        while( (ep = readdir(dp)) ) {
            file.assign(ep->d_name);
            if (file.length() >= 5) {
                if (file.substr(file.length()-5,5) == ".conf") {
                    if (file.find("sound") == std::string::npos) {
                        file = confdir + "/" + file;
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            ,_("Processing as camera config file %s")
                            , file.c_str() );
                        camera_add(file, true);
                    } else {
                        file = confdir + "/" + file;
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            ,_("Processing as sound config file %s")
                            , file.c_str() );
                        sound_add(file, true);
                    }
                }
            }
        }
    }
    closedir(dp);

    edit_set("config_dir", confdir);

}

void cls_config::process()
{
    size_t stpos;
    std::string line, parm_nm, parm_vl;
    std::ifstream ifs;

    ifs.open(conf_filename);
        if (ifs.is_open() == false) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Unable to open configuration file: %s")
                , conf_filename.c_str());
            return;
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Processing config file %s")
            , conf_filename.c_str());

        while (std::getline(ifs, line)) {
            mytrim(line);
            if ((line == "") ||
                (line.substr(0, 1) == ";") ||
                (line.substr(0, 1) == "#")) {
                continue;
            }
            stpos = line.find(" ");
            if (line.find('\t') != std::string::npos) {
                if (line.find('\t') < stpos) {
                    stpos =line.find('\t');
                    MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                        , _("Tabs should not be used in configuration file.:%s")
                        , line.c_str());
                }
            }
            if (stpos > line.find("=")) {
                stpos = line.find("=");
            }
            if (stpos == std::string::npos) {
                parm_nm = line;
                parm_vl = "";
            } else if (stpos != 0) {
                parm_nm = line.substr(0, stpos);
                while ((stpos < line.length()) &&
                    ((line.substr(stpos,1) == "=") ||
                     (line.substr(stpos,1) == " "))) {
                    stpos++;
                }
                if (stpos >= line.length()) {
                    parm_vl = "";
                } else {
                    parm_vl = line.substr(stpos, line.length()-stpos);
                }

            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Unable to parse line: %s"), line.c_str());
                continue;
            }
            myunquote(parm_nm);
            myunquote(parm_vl);
            if ((parm_nm == "camera") && (app->conf_src == this)) {
                camera_add(parm_vl, false);
            } else if ((parm_nm == "sound") && (app->conf_src == this)) {
                sound_add(parm_vl, false);
            } else if ((parm_nm == "config_dir") && (app->conf_src == this)){
                config_dir_item(parm_vl);
            } else if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir")) {
                edit_set(parm_nm, parm_vl);
            }
        }
    ifs.close();

}

void cls_config::config_log_item(std::string parm_nm, std::string parm_vl)
{
    if ((parm_nm == "netcam_url") ||
        (parm_nm == "netcam_userpass") ||
        (parm_nm == "netcam_high_url") ||
        (parm_nm == "webcontrol_auth_admin") ||
        (parm_nm == "webcontrol_auth_user") ||
        (parm_nm == "webcontrol_key") ||
        (parm_nm == "webcontrol_cert") ||
        (parm_nm == "database_user") ||
        (parm_nm == "database_password"))
    {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
            ,_("%-25s <redacted>"), parm_nm.c_str());
    } else {
        if ((parm_nm.compare(0,4,"text") == 0) ||
            (parm_vl.compare(0,1, " ") != 0)) {
            MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
                , "%-25s %s", parm_nm.c_str(), parm_vl.c_str());
        } else {
            MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
                , "%-25s \"%s\"", parm_nm.c_str(), parm_vl.c_str());
        }
    }

}

void cls_config::config_log()
{
    int i, indx, indx2, indx3;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    ctx_params *prm_a, *prm_c;
    ctx_params_item *itma, *itmc;
    bool chk, chklbl;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("Logging configuration parameters from all files"));

    MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
        , _("Config file: %s"), app->conf_src->conf_filename.c_str());

    i = 0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            app->conf_src->edit_get(parm_nm,parm_vl, parm_ct);
            config_log_item(parm_nm, parm_vl);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                config_log_item(parm_nm, it->c_str());
            }
        } else if (parm_typ == PARM_TYP_PARAMS) {
            prm_a=nullptr;
            for (indx=0;indx<app->conf_src->params.size();indx++){
                if (parm_nm == app->conf_src->params[indx]->params_desc) {
                    prm_a=app->conf_src->params[indx];
                }
            }
            if (prm_a != nullptr) {
                for (indx=0;indx<prm_a->params_array.size();indx++){
                    itma = &prm_a->params_array[indx];
                    config_log_item("  "+itma->param_name, itma->param_value);
                }
            }
        }
        i++;
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
            , _("Camera config file: %s")
            , app->cam_list[indx]->conf_src->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            chklbl = false;
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            app->conf_src->edit_get(parm_nm, parm_main, parm_ct);

            app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY) ) {
                chklbl = true;
                config_log_item(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    config_log_item(parm_nm, it->c_str());
                }
            } else if (parm_typ == PARM_TYP_PARAMS) {
                prm_a=nullptr;
                for (indx2=0;indx2<app->conf_src->params.size();indx2++){
                    if (parm_nm == app->conf_src->params[indx2]->params_desc) {
                        prm_a=app->conf_src->params[indx2];
                    }
                }
                prm_c=nullptr;
                for (indx2=0;indx2<app->cam_list[indx]->conf_src->params.size();indx2++){
                    if (parm_nm == app->cam_list[indx]->conf_src->params[indx2]->params_desc) {
                        prm_c=app->cam_list[indx]->conf_src->params[indx2];
                    }
                }
                if ((prm_a != nullptr) && (prm_c != nullptr)) {
                    for (indx2=0;indx2<prm_c->params_array.size();indx2++) {
                        itmc = &prm_c->params_array[indx2];
                        chk = false;
                        for (indx3=0;indx3<prm_a->params_array.size();indx3++) {
                            itma = &prm_a->params_array[indx3];
                            if ((itma->param_name == itmc->param_name) &&
                                (itma->param_value == itmc->param_value)) {
                                chk=true;
                            }
                        }
                        if (chk == false) {
                            if (chklbl == false) {
                                MOTION_SHT(INF, TYPE_ALL, NO_ERRNO, "%-25s ", parm_nm.c_str());
                                chklbl = true;
                            }
                            config_log_item("  "+itmc->param_name,itmc->param_value);
                        }
                    }
                }
            }

            i++;
        }
    }

    for (indx=0; indx<app->snd_cnt; indx++) {
        MOTION_SHT(INF, TYPE_ALL, NO_ERRNO
            , _("Sound config file: %s")
            , app->snd_list[indx]->conf_src->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
            app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY) ) {
                config_log_item(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    config_log_item(parm_nm, it->c_str());
                }
            }
            i++;
        }
    }

}

void cls_config::config_write_params(FILE *conffile, cls_config *cfg
    , std::string &parm_nm , std::string &parm_vl)
{
    int indx;
    std::string fnm_chk, mdir;
    ctx_params *prm;
    ctx_params_item *itm;
    char timestamp[32];
    FILE *prmfile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    prm=nullptr;
    for (indx=0;indx<cfg->params.size();indx++) {
        if (parm_nm == cfg->params[indx]->params_desc) {
            prm= cfg->params[indx];
        }
    }
    if (prm == nullptr) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Coding error with %s"), parm_nm.c_str());
        return;
    }

    fnm_chk = "";
    for (indx=0;indx<prm->params_array.size();indx++) {
        itm = &prm->params_array[indx];
        if (itm->param_name == "params_file") {
            fnm_chk = itm->param_value;
        }
    }

    if (fnm_chk == "") {
        util_parms_update(prm, parm_vl);
    } else {
        /* set new val for parm and write everything out to the params_file*/
        if (fnm_chk.find(" ") != std::string::npos) {
            parm_vl="params_file=\""+fnm_chk+"\"";
        } else {
            parm_vl="params_file=" + fnm_chk;
        }
        if (fnm_chk.find("/") == std::string::npos) {
            mdir = app->conf_src->conf_filename;
            mdir = mdir.substr(0,mdir.find_last_of("/"));
            fnm_chk = mdir+"/"+fnm_chk;
        }
        prmfile = myfopen(fnm_chk.c_str(), "we");
        if (prmfile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to open for writing %s params_file %s")
                , parm_nm.c_str(), fnm_chk.c_str());
            return;
        }
        fprintf(prmfile, ";\n; This %s params_file was generated by Motion " VERSION "\n",parm_nm.c_str());
        fprintf(prmfile, "; at %s\n", timestamp);
        fprintf(prmfile, "\n\n");
        for (indx=0;indx<prm->params_array.size();indx++) {
            itm = &prm->params_array[indx];
            if ((itm->param_name != "params_file") &&
                (itm->param_source == PARM_SRC_USER)) {
                if (itm->param_value.find(" ") != std::string::npos) {
                    fprintf(prmfile, "%s=\"%s\"\n"
                        , itm->param_name.c_str()
                        , itm->param_value.c_str());
                } else {
                    fprintf(prmfile, "%s=%s\n"
                        , itm->param_name.c_str()
                        , itm->param_value.c_str());
                }
            }
        }
        fprintf(prmfile, "\n");
        myfclose(prmfile);
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("%s values written to params_file: %s")
            , parm_nm.c_str(), fnm_chk.c_str());

    }

    if (parm_vl.compare(0, 1, " ") == 0) {
        fprintf(conffile, "%s \"%s\"\n", parm_nm.c_str(), parm_vl.c_str());
    } else {
        fprintf(conffile, "%s %s\n", parm_nm.c_str(), parm_vl.c_str());
    }
}

void cls_config::config_write_item(FILE *conffile, std::string parm_nm
    , std::string parm_vl, enum PARM_CAT parm_ct, bool reset)
{
    static enum PARM_CAT prev_ct;

    if (reset) {
        prev_ct = PARM_CAT_00;
        return;
    }

    if (parm_ct != prev_ct) {
        fprintf(conffile,"\n%s",";*************************************************\n");
        fprintf(conffile,"%s%s\n", ";*****   ", cat_desc(parm_ct,false).c_str());
        fprintf(conffile,"%s",";*************************************************\n");
        prev_ct = parm_ct;
    }

    if (parm_vl.compare(0, 1, " ") == 0) {
        fprintf(conffile, "%s \"%s\"\n", parm_nm.c_str(), parm_vl.c_str());
    } else {
        fprintf(conffile, "%s %s\n", parm_nm.c_str(), parm_vl.c_str());
    }
}

void cls_config::config_write_app()
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    conffile = myfopen(app->conf_src->conf_filename.c_str(), "we");
    if (conffile == NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Failed to write configuration to %s")
            , app->conf_src->conf_filename.c_str());
        return;
    }

    fprintf(conffile, "; %s\n", app->conf_src->conf_filename.c_str());
    fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
    fprintf(conffile, "; at %s\n", timestamp);
    fprintf(conffile, "\n\n");

    config_write_item(conffile, "", "", PARM_CAT_00, true);

    i=0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY) && (parm_typ != PARM_TYP_PARAMS)) {
            app->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            config_write_item(conffile, parm_nm, parm_vl, parm_ct, false);
        }
        if (parm_typ == PARM_TYP_PARAMS) {
            config_write_params(conffile,app->conf_src,parm_nm,parm_vl);
            app->conf_src->edit_set(parm_nm, parm_vl);
        } else if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                config_write_item(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        }
        i++;
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->conf_src->from_conf_dir == false) {
            config_write_item(conffile, "camera"
                , app->cam_list[indx]->conf_src->conf_filename
                , PARM_CAT_01, false);
        }
    }

    for (indx=0; indx<app->snd_cnt; indx++) {
        if (app->snd_list[indx]->conf_src->from_conf_dir == false) {
            config_write_item(conffile, "sound"
                , app->snd_list[indx]->conf_src->conf_filename
                , PARM_CAT_01, false);
        }
    }

    fprintf(conffile, "\n");

    app->conf_src->edit_get("config_dir", parm_vl, PARM_CAT_01);
    config_write_item(conffile, "config_dir", parm_vl, PARM_CAT_01, false);

    fprintf(conffile, "\n");
    myfclose(conffile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Configuration written to %s")
        , app->conf_src->conf_filename.c_str());

}

void cls_config::config_write_cam(int camindx)
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;
    cls_config *cfgm, *cfgc;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    cfgm = app->conf_src;

    cfgc = app->cam_list[camindx]->conf_src;
    conffile = myfopen(cfgc->conf_filename.c_str(), "we");
    if (conffile == NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Failed to write configuration to %s")
            , cfgc->conf_filename.c_str());
        return;
    }
    fprintf(conffile, "; %s\n", cfgc->conf_filename.c_str());
    fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
    fprintf(conffile, "; at %s\n", timestamp);
    fprintf(conffile, "\n\n");
    config_write_item(conffile, "", "", PARM_CAT_00, true);

    i=0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY) && (parm_typ != PARM_TYP_PARAMS)) {
            cfgm->edit_get(parm_nm, parm_main, parm_ct);
            cfgc->edit_get(parm_nm, parm_vl, parm_ct);
            if (parm_main != parm_vl) {
                config_write_item(conffile, parm_nm, parm_vl, parm_ct, false);
            }
        }
        if (parm_typ == PARM_TYP_PARAMS) {
            parm_main="";
            parm_vl = "";
            for (indx=0;indx<cfgm->params.size();indx++) {
                if (cfgm->params[indx]->params_desc == parm_nm) {
                    util_parms_update(cfgm->params[indx],parm_main);
                }
            }
            for (indx=0;indx<cfgc->params.size();indx++) {
                if (cfgc->params[indx]->params_desc == parm_nm) {
                    util_parms_update(cfgc->params[indx],parm_vl);
                }
            }
            if (parm_main != parm_vl) {
                config_write_params(conffile,cfgc,parm_nm,parm_vl);
                cfgc->edit_set(parm_nm, parm_vl);
            }
        } else if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                config_write_item(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        }
        i++;
    }
    fprintf(conffile, "\n");
    myfclose(conffile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Configuration written to %s")
        , cfgc->conf_filename.c_str());

}

void cls_config::config_write_snd()
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;
    char timestamp[32];
    FILE *conffile;

    time_t now = time(0);
    strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

    for (indx=0; indx<app->snd_cnt; indx++) {
        conffile = myfopen(app->snd_list[indx]->conf_src->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to write configuration to %s")
                , app->snd_list[indx]->conf_src->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", app->snd_list[indx]->conf_src->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        config_write_item(conffile, "", "", PARM_CAT_00, true);

        i=0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY)) {
                app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
                app->snd_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    config_write_item(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); ++it) {
                    config_write_item(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Configuration written to %s")
            , app->snd_list[indx]->conf_src->conf_filename.c_str());
    }
}

void cls_config::config_write(int camindx)
{
    int indx;
    if (camindx == -1) {
        config_write_app();
        for (indx=0;indx<app->cam_cnt;indx++) {
            config_write_cam(indx);
        }
        config_write_snd();
    } else {
        config_write_cam(camindx);
    }
}

void cls_config::config_copy(cls_config *src)
{
    int indx1, indx2;
    std::string parm_nm, parm_val;
    std::list<std::string> lst_val;
    std::list<std::string>::iterator  it_a;
    ctx_params_item itm;
    ctx_params *prm, *prmsrc;

    indx1 = 0;
    while (config_parms[indx1].parm_name != "") {
        parm_nm = config_parms[indx1].parm_name;
        if (parm_nm == "snd_alerts") {
            snd_alerts.clear();
            src->edit_get(parm_nm, lst_val, config_parms[indx1].parm_cat);
            for (it_a=lst_val.begin(); it_a!=lst_val.end(); it_a++) {
                edit_set(parm_nm, it_a->c_str());
            }
        } else {
            src->edit_get(parm_nm, parm_val, config_parms[indx1].parm_cat);
            edit_set(parm_nm, parm_val);
        }
        indx1++;
    }

    for (indx1=0;indx1<params.size();indx1++) {
        mydelete (params[indx1]);
    }
    params.clear();
    for (indx1=0;indx1<src->params.size();indx1++) {
        prmsrc = src->params[indx1];
        prm = new ctx_params;
        prm->params_pcat = prmsrc->params_pcat;
        prm->params_desc = prmsrc->params_desc;
        for (indx2=0;indx2<prmsrc->params_array.size();indx2++) {
            prm->params_array.push_back(prmsrc->params_array[indx2]);
        }
        prm->params_cnt = (int)prm->params_array.size();
        params.push_back(prm);
    }

}

void cls_config::config_copy(cls_config *src, PARM_CAT p_cat)
{
    int indx;
    std::string parm_nm, parm_val;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (config_parms[indx].parm_cat == p_cat) {
            parm_nm =config_parms[indx].parm_name;
            src->edit_get(parm_nm, parm_val, p_cat);
            edit_set(parm_nm, parm_val);
        }
        indx++;
    }

}

void cls_config::init()
{
    std::string filename;
    char path[PATH_MAX];
    struct stat statbuf;
    int indx;

    defaults();

    cmdline();

    filename = "";
    if (app->conf_src->conf_filename != "") {
        filename = app->conf_src->conf_filename;
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename="";
        }
    }

    if (filename == "") {
        if (getcwd(path, sizeof(path)) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error getcwd"));
            exit(-1);
        }
        filename = path + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string(getenv("HOME")) + std::string("/.motion/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string( configdir ) + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string( sysconfdir ) + std::string("/motion.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
        if (filename != "") {
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO
                ,_("The configuration file location '%s' is deprecated.")
                , sysconfdir );
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO
                ,_("The new default configuration file location is '%s'")
                , configdir );
        }
    }

    if (filename == "") {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Could not open configuration file"));
        exit(-1);
    }

    edit_set("conf_filename", filename);

    app->conf_src->process();

    if ((app->cam_cnt == 0) && (app->snd_cnt == 0)) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("No camera or sound configuration files specified."));
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Adding a camera configuration file."));
        app->conf_src->camera_add("", false);
    }


    cmdline();
    app->conf_src->params_load();
    app->conf_src->params_load_listcam();
    app->conf_src->params_load_listsnd();

    for (indx=0; indx<app->cam_cnt; indx++) {
        app->cam_list[indx]->threadnr = indx;
    }

    for (indx=0; indx<app->snd_cnt; indx++) {
        app->snd_list[indx]->threadnr = (indx + app->cam_cnt);
    }

}

cls_config::cls_config(cls_motapp *p_app)
{
    app = p_app;

    defaults();
}

cls_config::~cls_config()
{
    int indx;

    for (indx=0;indx<params.size();indx++) {
        mydelete (params[indx]);
    }
}
