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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 */

#include <dirent.h>
#include <string>
#include "motionplus.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "conf.hpp"


enum PARM_ACT{
    PARM_ACT_DFLT
    , PARM_ACT_SET
    , PARM_ACT_GET
    , PARM_ACT_LIST
};

/* Forward Declares */
void conf_process(ctx_motapp *motapp, ctx_config *conf);

/*Configuration parameters */
ctx_parm config_parms[] = {
    {"daemon",                    PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"setup_mode",                PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"conf_filename",             PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"pid_file",                  PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"log_file",                  PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"log_level",                 PARM_TYP_LIST,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"log_type",                  PARM_TYP_LIST,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"native_language",           PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },

    {"device_name",               PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"device_id",                 PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"device_tmo",                PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"target_dir",                PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"watchdog_tmo",              PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"watchdog_kill",             PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"config_dir",                PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"camera",                    PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },

    {"v4l2_device",               PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"v4l2_params",               PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_url",                PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_params",             PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_high_url",           PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_high_params",        PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_userpass",           PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"libcam_name",               PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"libcam_params",             PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },

    {"width",                     PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"height",                    PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"framerate",                 PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"rotate",                    PARM_TYP_LIST,   PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"flip_axis",                 PARM_TYP_LIST,   PARM_CAT_03, WEBUI_LEVEL_LIMITED },

    {"locate_motion_mode",        PARM_TYP_LIST,   PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"locate_motion_style",       PARM_TYP_LIST,   PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"text_left",                 PARM_TYP_STRING, PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"text_right",                PARM_TYP_STRING, PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"text_changes",              PARM_TYP_BOOL,   PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"text_scale",                PARM_TYP_LIST,   PARM_CAT_04, WEBUI_LEVEL_LIMITED },
    {"text_event",                PARM_TYP_STRING, PARM_CAT_04, WEBUI_LEVEL_LIMITED },

    {"emulate_motion",            PARM_TYP_BOOL,   PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold",                 PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_maximum",         PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_sdevx",           PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_sdevy",           PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_sdevxy",          PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_ratio",           PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_ratio_change",    PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"threshold_tune",            PARM_TYP_BOOL,   PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"secondary_method",          PARM_TYP_LIST,   PARM_CAT_05, WEBUI_LEVEL_LIMITED },
    {"secondary_params",          PARM_TYP_STRING, PARM_CAT_05, WEBUI_LEVEL_LIMITED },

    {"noise_level",               PARM_TYP_INT,    PARM_CAT_06, WEBUI_LEVEL_LIMITED },
    {"noise_tune",                PARM_TYP_BOOL,   PARM_CAT_06, WEBUI_LEVEL_LIMITED },
    {"despeckle_filter",          PARM_TYP_STRING, PARM_CAT_06, WEBUI_LEVEL_LIMITED },
    {"area_detect",               PARM_TYP_STRING, PARM_CAT_06, WEBUI_LEVEL_LIMITED },
    {"mask_file",                 PARM_TYP_STRING, PARM_CAT_06, WEBUI_LEVEL_ADVANCED },
    {"mask_privacy",              PARM_TYP_STRING, PARM_CAT_06, WEBUI_LEVEL_ADVANCED },
    {"smart_mask_speed",          PARM_TYP_LIST,   PARM_CAT_06, WEBUI_LEVEL_LIMITED },

    {"lightswitch_percent",       PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"lightswitch_frames",        PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"minimum_motion_frames",     PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"static_object_time",        PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"event_gap",                 PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"pre_capture",               PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },
    {"post_capture",              PARM_TYP_INT,    PARM_CAT_07, WEBUI_LEVEL_LIMITED },

    {"on_event_start",            PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_event_end",              PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_picture_save",           PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_area_detected",          PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_motion_detected",        PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_movie_start",            PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_movie_end",              PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_camera_lost",            PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_camera_found",           PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_secondary_detect",       PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_action_user",            PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },
    {"on_sound_alert",            PARM_TYP_STRING, PARM_CAT_08, WEBUI_LEVEL_RESTRICTED },

    {"picture_output",            PARM_TYP_LIST,   PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"picture_output_motion",     PARM_TYP_LIST,   PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"picture_type",              PARM_TYP_LIST,   PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"picture_quality",           PARM_TYP_INT,    PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"picture_exif",              PARM_TYP_STRING, PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"picture_filename",          PARM_TYP_STRING, PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"snapshot_interval",         PARM_TYP_INT,    PARM_CAT_09, WEBUI_LEVEL_LIMITED },
    {"snapshot_filename",         PARM_TYP_STRING, PARM_CAT_09, WEBUI_LEVEL_LIMITED },

    {"movie_output",              PARM_TYP_BOOL,   PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_output_motion",       PARM_TYP_BOOL,   PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_max_time",            PARM_TYP_INT,    PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_bps",                 PARM_TYP_INT,    PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_quality",             PARM_TYP_INT,    PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_container",           PARM_TYP_STRING, PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_passthrough",         PARM_TYP_BOOL,   PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_filename",            PARM_TYP_STRING, PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_retain",              PARM_TYP_LIST,   PARM_CAT_10, WEBUI_LEVEL_LIMITED },
    {"movie_extpipe_use",         PARM_TYP_BOOL,   PARM_CAT_10, WEBUI_LEVEL_RESTRICTED },
    {"movie_extpipe",             PARM_TYP_STRING, PARM_CAT_10, WEBUI_LEVEL_RESTRICTED },

    {"timelapse_interval",        PARM_TYP_INT,    PARM_CAT_11, WEBUI_LEVEL_LIMITED },
    {"timelapse_mode",            PARM_TYP_LIST,   PARM_CAT_11, WEBUI_LEVEL_LIMITED },
    {"timelapse_fps",             PARM_TYP_INT,    PARM_CAT_11, WEBUI_LEVEL_LIMITED },
    {"timelapse_container",       PARM_TYP_LIST,   PARM_CAT_11, WEBUI_LEVEL_LIMITED },
    {"timelapse_filename",        PARM_TYP_STRING, PARM_CAT_11, WEBUI_LEVEL_LIMITED },

    {"video_pipe",                PARM_TYP_STRING, PARM_CAT_12, WEBUI_LEVEL_LIMITED },
    {"video_pipe_motion",         PARM_TYP_STRING, PARM_CAT_12, WEBUI_LEVEL_LIMITED },

    {"webcontrol_port",           PARM_TYP_INT,    PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_base_path",      PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_ipv6",           PARM_TYP_BOOL,   PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_localhost",      PARM_TYP_BOOL,   PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_parms",          PARM_TYP_LIST,   PARM_CAT_13, WEBUI_LEVEL_NEVER},
    {"webcontrol_interface",      PARM_TYP_LIST,   PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_auth_method",    PARM_TYP_LIST,   PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_authentication", PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_tls",            PARM_TYP_BOOL,   PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_cert",           PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_key",            PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_headers",        PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_html",           PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_actions",        PARM_TYP_STRING, PARM_CAT_13, WEBUI_LEVEL_RESTRICTED },
    {"webcontrol_lock_minutes",   PARM_TYP_INT,    PARM_CAT_13, WEBUI_LEVEL_ADVANCED },
    {"webcontrol_lock_attempts",  PARM_TYP_INT,    PARM_CAT_13, WEBUI_LEVEL_ADVANCED },

    {"stream_preview_scale",      PARM_TYP_INT,    PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_preview_newline",    PARM_TYP_BOOL,   PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_preview_method",     PARM_TYP_LIST,   PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_preview_ptz",        PARM_TYP_BOOL,   PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_quality",            PARM_TYP_INT,    PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_grey",               PARM_TYP_BOOL,   PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_motion",             PARM_TYP_BOOL,   PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_maxrate",            PARM_TYP_INT,    PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_scan_time",          PARM_TYP_INT,    PARM_CAT_14, WEBUI_LEVEL_LIMITED },
    {"stream_scan_scale",         PARM_TYP_INT,    PARM_CAT_14, WEBUI_LEVEL_LIMITED },

    {"database_type",             PARM_TYP_LIST,   PARM_CAT_15, WEBUI_LEVEL_ADVANCED },
    {"database_dbname",           PARM_TYP_STRING, PARM_CAT_15, WEBUI_LEVEL_ADVANCED },
    {"database_host",             PARM_TYP_STRING, PARM_CAT_15, WEBUI_LEVEL_ADVANCED },
    {"database_port",             PARM_TYP_INT,    PARM_CAT_15, WEBUI_LEVEL_ADVANCED },
    {"database_user",             PARM_TYP_STRING, PARM_CAT_15, WEBUI_LEVEL_RESTRICTED },
    {"database_password",         PARM_TYP_STRING, PARM_CAT_15, WEBUI_LEVEL_RESTRICTED },
    {"database_busy_timeout",     PARM_TYP_INT,    PARM_CAT_15, WEBUI_LEVEL_ADVANCED },

    {"sql_event_start",           PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_event_end",             PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_movie_start",           PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_movie_end",             PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_pic_save",              PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED},

    {"ptz_auto_track",            PARM_TYP_BOOL,   PARM_CAT_17, WEBUI_LEVEL_LIMITED },
    {"ptz_wait",                  PARM_TYP_INT,    PARM_CAT_17, WEBUI_LEVEL_LIMITED },
    {"ptz_move_track",            PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_pan_left",              PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_pan_right",             PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_tilt_up",               PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_tilt_down",             PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_zoom_in",               PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_zoom_out",              PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },

    {"snd_device",                PARM_TYP_STRING, PARM_CAT_18, WEBUI_LEVEL_ADVANCED },
    {"snd_params",                PARM_TYP_STRING, PARM_CAT_18, WEBUI_LEVEL_ADVANCED },
    {"snd_alerts",                PARM_TYP_ARRAY, PARM_CAT_18, WEBUI_LEVEL_ADVANCED },
    {"snd_window",                PARM_TYP_LIST, PARM_CAT_18, WEBUI_LEVEL_ADVANCED },
    {"snd_show",                  PARM_TYP_BOOL, PARM_CAT_18, WEBUI_LEVEL_ADVANCED },

    { "", (enum PARM_TYP)0, (enum PARM_CAT)0, (enum WEBUI_LEVEL)0 }
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
    {
    "thread",
    "3.4.1",
    "The \"thread\" option has been replaced by the \"camera\"",
    "camera"
    },
    {
    "ffmpeg_timelapse",
    "4.0.1",
    "\"ffmpeg_timelapse\" replaced with \"timelapse_interval\"",
    "timelapse_interval"
    },
    {
    "ffmpeg_timelapse_mode",
    "4.0.1",
    "\"ffmpeg_timelapse_mode\" replaced with \"timelapse_mode\"",
    "timelapse_mode"
    },
    {
    "brightness",
    "4.1.1",
    "\"brightness\" replaced with \"v4l2_params\"",
    "v4l2_params"
    },
    {
    "contrast",
    "4.1.1",
    "\"contrast\" replaced with \"v4l2_params\"",
    "v4l2_params"
    },
    {
    "saturation",
    "4.1.1",
    "\"saturation\" replaced with \"v4l2_params\"",
    "v4l2_params"
    },
    {
    "hue",
    "4.1.1",
    "\"hue\" replaced with \"v4l2_params\"",
    "v4l2_params"
    },
    {
    "power_line_frequency",
    "4.1.1",
    "\"power_line_frequency\" replaced with \"v4l2_params\"",
    "v4l2_params"
    },
    {
    "text_double",
    "4.1.1",
    "\"text_double\" replaced with \"text_scale\"",
    "text_scale"
    },
    {
    "webcontrol_html_output",
    "4.1.1",
    "\"webcontrol_html_output\" replaced with \"webcontrol_interface\"",
    "webcontrol_interface"
    },
    {
     "lightswitch",
    "4.1.1",
    "\"lightswitch\" replaced with \"lightswitch_percent\"",
    "lightswitch_percent"
    },
    {
    "ffmpeg_output_movies",
    "4.1.1",
    "\"ffmpeg_output_movies\" replaced with \"movie_output\"",
    "movie_output"
    },
    {
    "ffmpeg_output_debug_movies",
    "4.1.1",
    "\"ffmpeg_output_debug_movies\" replaced with \"movie_output_motion\"",
    "movie_output_motion"
    },
    {
    "max_movie_time",
    "4.1.1",
    "\"max_movie_time\" replaced with \"movie_max_time\"",
    "movie_max_time"
    },
    {
    "ffmpeg_bps",
    "4.1.1",
    "\"ffmpeg_bps\" replaced with \"movie_bps\"",
    "movie_bps"
    },
    {
    "ffmpeg_variable_bitrate",
    "4.1.1",
    "\"ffmpeg_variable_bitrate\" replaced with \"movie_quality\"",
    "movie_quality"
    },
    {
    "ffmpeg_video_codec",
    "4.1.1",
    "\"ffmpeg_video_codec\" replaced with \"movie_container\"",
    "movie_container"
    },
    {
    "ffmpeg_passthrough",
    "4.1.1",
    "\"ffmpeg_passthrough\" replaced with \"movie_passthrough\"",
    "movie_passthrough"
    },
    {
    "use_extpipe",
    "4.1.1",
    "\"use_extpipe\" replaced with \"movie_extpipe_use\"",
    "movie_extpipe_use"
    },
    {
    "extpipe",
    "4.1.1",
    "\"extpipe\" replaced with \"movie_extpipe\"",
    "movie_extpipe"
    },
    {
    "output_pictures",
    "4.1.1",
    "\"output_pictures\" replaced with \"picture_output\"",
    "picture_output"
    },
    {
    "output_debug_pictures",
    "4.1.1",
    "\"output_debug_pictures\" replaced with \"picture_output_motion\"",
    "picture_output_motion"
    },
    {
    "quality",
    "4.1.1",
    "\"quality\" replaced with \"picture_quality\"",
    "picture_quality"
    },
    {
    "exif_text",
    "4.1.1",
    "\"exif_text\" replaced with \"picture_exif\"",
    "picture_exif"
    },
    {
    "motion_video_pipe",
    "4.1.1",
    "\"motion_video_pipe\" replaced with \"video_pipe_motion\"",
    "video_pipe_motion"
    },
    {
    "ipv6_enabled",
    "4.1.1",
    "\"ipv6_enabled\" replaced with \"webcontrol_ipv6\"",
    "webcontrol_ipv6"
    },
    {
    "rtsp_uses_tcp",
    "4.1.1",
    "\"rtsp_uses_tcp\" replaced with \"netcam_use_tcp\"",
    "netcam_use_tcp"
    },
    {
    "switchfilter",
    "4.1.1",
    "\"switchfilter\" replaced with \"roundrobin_switchfilter\"",
    "roundrobin_switchfilter"
    },
    {
    "logfile",
    "4.1.1",
    "\"logfile\" replaced with \"log_file\"",
    "log_file"
    },
    {
    "process_id_file",
    "4.1.1",
    "\"process_id_file\" replaced with \"pid_file\"",
    "pid_file"
    },
    {
    "movie_codec",
    "0.0.1",
    "\"movie_codec\" replaced with \"movie_container\"",
    "movie_container"
    },
    {
    "camera_id",
    "0.0.1",
    "\"camera_id\" replaced with \"device_id\"",
    "device_id"
    },
    {
    "camera_name",
    "0.0.1",
    "\"camera_name\" replaced with \"device_name\"",
    "device_name"
    },

    { "","","",""}
};

static void conf_edit_set_bool(bool &parm_dest, std::string &parm_in)
{
    if ((parm_in == "1") || (parm_in == "yes") || (parm_in == "on") || (parm_in == "true") ) {
        parm_dest = true;
    } else {
        parm_dest = false;
    }
}

static void conf_edit_get_bool(std::string &parm_dest, bool &parm_in)
{
    if (parm_in == true) {
        parm_dest = "on";
    } else {
        parm_dest = "off";
    }
}

static void conf_edit_daemon(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->daemon = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->daemon, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->daemon);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
}

static void conf_edit_setup_mode(ctx_config *conf, std::string &parm, int pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->setup_mode = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->setup_mode, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->setup_mode);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","setup_mode",_("setup_mode"));
}

static void conf_edit_conf_filename(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->conf_filename = "";
    } else if (pact == PARM_ACT_SET) {
        conf->conf_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->conf_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}

static void conf_edit_pid_file(ctx_config *conf, std::string &parm, int pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->pid_file = "";
    } else if (pact == PARM_ACT_SET) {
        conf->pid_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->pid_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
}

static void conf_edit_log_file(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    char    lognm[4096];
    tm      *logtm;
    time_t  logt;

    if (pact == PARM_ACT_DFLT) {
        conf->log_file = "";
    } else if (pact == PARM_ACT_SET) {
        time(&logt);
        logtm = localtime(&logt);
        strftime(lognm, 4096, parm.c_str(), logtm);
        conf->log_file = lognm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->log_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}

static void conf_edit_log_level(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->log_level = 6;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %d"),parm_in);
        } else {
            conf->log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->log_level);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\"";
        parm = parm + "]";
    }

    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
}

static void conf_edit_log_type(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->log_type_str = "ALL";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "ALL") || (parm == "COR") ||
            (parm == "STR") || (parm == "ENC") ||
            (parm == "NET") || (parm == "DBL") ||
            (parm == "EVT") || (parm == "TRK") ||
            (parm == "VID") || (parm == "ALL")) {
            conf->log_type_str = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_type %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->log_type_str;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"ALL\",\"COR\",\"STR\",\"ENC\",\"NET\"";
        parm = parm + ",\"DBL\",\"EVT\",\"TRK\",\"VID\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
}

static void conf_edit_native_language(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->native_language = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->native_language, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->native_language);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
}

static void conf_edit_camera(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_SET) {
        conf->conf_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->conf_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera",_("camera"));
}

static void conf_edit_device_name(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->device_name= "";
    } else if (pact == PARM_ACT_SET) {
        conf->device_name = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->device_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_name",_("device_name"));
}

static void conf_edit_device_id(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;

    if (pact == PARM_ACT_DFLT) {
        conf->device_id = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_id %d"),parm_in);
        } else if (parm_in > 32000) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_id %d"),parm_in);
        } else {
            conf->device_id = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->device_id);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_id",_("device_id"));
}

static void conf_edit_device_tmo(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->device_tmo = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid device_tmo %d"),parm_in);
        } else {
            conf->device_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->device_tmo);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_tmo",_("device_tmo"));
}

static void conf_edit_config_dir(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->config_dir = "";
    } else if (pact == PARM_ACT_SET) {
        conf->config_dir = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->config_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","config_dir",_("config_dir"));
}

static void conf_edit_target_dir(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->target_dir = ".";
    } else if (pact == PARM_ACT_SET) {
        if (parm.find("%", 0) != std::string::npos) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid target_dir.  Conversion specifiers not permitted. %s")
                , parm.c_str());
        } else {
            conf->target_dir = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->target_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
}

static void conf_edit_watchdog_tmo(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->watchdog_tmo = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog timeout %d"),parm_in);
        } else {
            conf->watchdog_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->watchdog_tmo);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_tmo",_("watchdog_tmo"));
}

static void conf_edit_watchdog_kill(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->watchdog_kill = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog kill timeout %d"),parm_in);
        } else {
            conf->watchdog_kill = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->watchdog_kill);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_kill",_("watchdog_kill"));
}

static void conf_edit_v4l2_device(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->v4l2_device = "";
    } else if (pact == PARM_ACT_SET) {
        conf->v4l2_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->v4l2_device;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_device",_("v4l2_device"));
}

static void conf_edit_v4l2_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->v4l2_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->v4l2_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->v4l2_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_params",_("v4l2_params"));
}

static void conf_edit_netcam_url(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->netcam_url = "";
    } else if (pact == PARM_ACT_SET) {
        conf->netcam_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->netcam_url;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
}

static void conf_edit_netcam_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->netcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->netcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->netcam_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_params",_("netcam_params"));
}

static void conf_edit_netcam_high_url(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->netcam_high_url = "";
    } else if (pact == PARM_ACT_SET) {
        conf->netcam_high_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->netcam_high_url;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_url",_("netcam_high_url"));
}

static void conf_edit_netcam_high_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->netcam_high_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->netcam_high_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->netcam_high_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_params",_("netcam_high_params"));
}

static void conf_edit_netcam_userpass(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->netcam_userpass = "";
    } else if (pact == PARM_ACT_SET) {
        conf->netcam_userpass = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->netcam_userpass;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
}

static void conf_edit_libcam_name(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->libcam_name = "";
    } else if (pact == PARM_ACT_SET) {
        conf->libcam_name = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->libcam_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_name",_("libcam_name"));
}

static void conf_edit_libcam_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->libcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->libcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->libcam_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","libcam_params",_("libcam_params"));
}

static void conf_edit_width(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->width = 640;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid width %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image width (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting width to next higher multiple of 8 (%d)."), parm_in);
            conf->width = parm_in;
        } else {
            conf->width = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->width);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
}

static void conf_edit_height(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->height = 480;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 64) || (parm_in > 9999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid height %d"),parm_in);
        } else if (parm_in % 8) {
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Image height (%d) requested is not modulo 8."), parm_in);
            parm_in = parm_in - (parm_in % 8) + 8;
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Adjusting height to next higher multiple of 8 (%d)."), parm_in);
            conf->height = parm_in;
        } else {
            conf->height = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->height);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
}

static void conf_edit_framerate(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->framerate = 15;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid framerate %d"),parm_in);
        } else {
            conf->framerate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->framerate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
}

static void conf_edit_rotate(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->rotate = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in != 0) && (parm_in != 90) &&
            (parm_in != 180) && (parm_in != 270) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid rotate %d"),parm_in);
        } else {
            conf->rotate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->rotate);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"0\",\"90\",\"180\",\"270\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
}

static void conf_edit_flip_axis(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->flip_axis = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "vertical") || (parm == "horizontal")) {
            conf->flip_axis = parm;
        } else if (parm == "") {
            conf->flip_axis = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->flip_axis;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"vertical\",\"horizontal\"]";

    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
}

static void conf_edit_locate_motion_mode(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->locate_motion_mode = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "off") || (parm == "on") || (parm == "preview")) {
            conf->locate_motion_mode = parm;
        } else if (parm == "") {
            conf->locate_motion_mode = "off";
        } else {
          MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->locate_motion_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"off\",\"on\",\"preview\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
}

static void conf_edit_locate_motion_style(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->locate_motion_style = "box";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "box") || (parm == "redbox") ||
            (parm == "cross") || (parm == "redcross"))  {
            conf->locate_motion_style = parm;
        } else if (parm == "") {
            conf->locate_motion_style = "box";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->locate_motion_style;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"box\",\"redbox\",\"cross\",\"redcross\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
}

static void conf_edit_text_left(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->text_left = "";
    } else if (pact == PARM_ACT_SET) {
        conf->text_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->text_left;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
}

static void conf_edit_text_right(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->text_right = "%Y-%m-%d\\n%T";
    } else if (pact == PARM_ACT_SET) {
        conf->text_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->text_right;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
}

static void conf_edit_text_changes(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->text_changes = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->text_changes, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->text_changes);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
}

static void conf_edit_text_scale(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->text_scale = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid text_scale %d"),parm_in);
        } else {
            conf->text_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->text_scale);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
}

static void conf_edit_text_event(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->text_event = "%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        conf->text_event = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->text_event;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
}

static void conf_edit_emulate_motion(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->emulate_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->emulate_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->emulate_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
}

static void conf_edit_threshold(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold = 1500;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            conf->threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
}

static void conf_edit_threshold_maximum(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_maximum = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_maximum %d"),parm_in);
        } else {
            conf->threshold_maximum = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_maximum);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
}

static void conf_edit_threshold_sdevx(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_sdevx = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevx %d"),parm_in);
        } else {
            conf->threshold_sdevx = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_sdevx);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevx",_("threshold_sdevx"));
}

static void conf_edit_threshold_sdevy(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_sdevy = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevy %d"),parm_in);
        } else {
            conf->threshold_sdevy = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_sdevy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevy",_("threshold_sdevy"));
}

static void conf_edit_threshold_sdevxy(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_sdevxy = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevxy %d"),parm_in);
        } else {
            conf->threshold_sdevxy = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_sdevxy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevxy",_("threshold_sdevxy"));
}

static void conf_edit_threshold_ratio(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_ratio = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_ratio %d"),parm_in);
        } else {
            conf->threshold_ratio = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_ratio);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_ratio",_("threshold_ratio"));
}

static void conf_edit_threshold_ratio_change(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_ratio_change = 64;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 255) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_ratio_change %d"),parm_in);
        } else {
            conf->threshold_ratio_change = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->threshold_ratio_change);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_ratio_change",_("threshold_ratio_change"));
}

static void conf_edit_threshold_tune(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->threshold_tune = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->threshold_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->threshold_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
}

static void conf_edit_secondary_method(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->secondary_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "haar") ||
            (parm == "hog")  || (parm == "dnn"))  {
            conf->secondary_method = parm;
        } else if (parm == "") {
            conf->secondary_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->secondary_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"haar\",\"hog\",\"dnn\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method",_("secondary_method"));
}

static void conf_edit_secondary_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->secondary_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->secondary_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->secondary_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_params",_("secondary_params"));
}

static void conf_edit_noise_level(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->noise_level = 32;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 255)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid noise_level %d"),parm_in);
        } else {
            conf->noise_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->noise_level);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
}

static void conf_edit_noise_tune(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->noise_tune = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->noise_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->noise_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
}

static void conf_edit_despeckle_filter(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->despeckle_filter = "";
    } else if (pact == PARM_ACT_SET) {
        conf->despeckle_filter = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->despeckle_filter;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
}

static void conf_edit_area_detect(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->area_detect = "";
    } else if (pact == PARM_ACT_SET) {
        conf->area_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->area_detect;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
}

static void conf_edit_mask_file(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->mask_file = "";
    } else if (pact == PARM_ACT_SET) {
        conf->mask_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->mask_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
}

static void conf_edit_mask_privacy(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->mask_privacy = "";
    } else if (pact == PARM_ACT_SET) {
        conf->mask_privacy = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->mask_privacy;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
}

static void conf_edit_smart_mask_speed(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->smart_mask_speed = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid smart_mask_speed %d"),parm_in);
        } else {
            conf->smart_mask_speed = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->smart_mask_speed);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
}

static void conf_edit_lightswitch_percent(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->lightswitch_percent = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_percent %d"),parm_in);
        } else {
            conf->lightswitch_percent = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->lightswitch_percent);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
}

static void conf_edit_lightswitch_frames(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->lightswitch_frames = 5;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_frames %d"),parm_in);
        } else {
            conf->lightswitch_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->lightswitch_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
}

static void conf_edit_minimum_motion_frames(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->minimum_motion_frames = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_motion_frames %d"),parm_in);
        } else {
            conf->minimum_motion_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->minimum_motion_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
}

static void conf_edit_static_object_time(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->static_object_time = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid static_object_time %d"),parm_in);
        } else {
            conf->static_object_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->static_object_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","static_object_time",_("static_object_time"));
}

static void conf_edit_event_gap(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->event_gap = 60;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid event_gap %d"),parm_in);
        } else {
            conf->event_gap = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->event_gap);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
}

static void conf_edit_pre_capture(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->pre_capture = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pre_capture %d"),parm_in);
        } else {
            conf->pre_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->pre_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
}

static void conf_edit_post_capture(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->post_capture = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid post_capture %d"),parm_in);
        } else {
            conf->post_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->post_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
}

static void conf_edit_on_event_start(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_event_start = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_event_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_event_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
}

static void conf_edit_on_event_end(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_event_end = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_event_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_event_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
}

static void conf_edit_on_picture_save(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_picture_save = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_picture_save = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_picture_save;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
}

static void conf_edit_on_area_detected(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_area_detected = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_area_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_area_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
}

static void conf_edit_on_motion_detected(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_motion_detected = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_motion_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_motion_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
}

static void conf_edit_on_movie_start(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_movie_start = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_movie_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_movie_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
}

static void conf_edit_on_movie_end(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_movie_end = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_movie_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_movie_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
}

static void conf_edit_on_camera_lost(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_camera_lost = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_camera_lost = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_camera_lost;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
}

static void conf_edit_on_camera_found(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_camera_found = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_camera_found = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_camera_found;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
}

static void conf_edit_on_secondary_detect(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_secondary_detect = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_secondary_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_secondary_detect;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_secondary_detect",_("on_secondary_detect"));
}

static void conf_edit_on_action_user(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_action_user = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_action_user = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_action_user;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_action_user",_("on_action_user"));
}

static void conf_edit_on_sound_alert(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->on_sound_alert = "";
    } else if (pact == PARM_ACT_SET) {
        conf->on_sound_alert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->on_sound_alert;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_sound_alert",_("on_sound_alert"));
}

static void conf_edit_picture_output(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->picture_output = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "center") ||
            (parm == "first") || (parm == "best"))  {
            conf->picture_output = parm;
        } else if (parm == "") {
            conf->picture_output = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->picture_output;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"first\",\"best\",\"center\" ";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
}

static void conf_edit_picture_output_motion(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->picture_output_motion = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "roi"))  {
            conf->picture_output_motion = parm;
        } else if (parm == "") {
            conf->picture_output_motion = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output_motion %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->picture_output_motion;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"roi\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
}

static void conf_edit_picture_type(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->picture_type = "jpeg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "jpeg") || (parm == "webp") || (parm == "ppm"))  {
            conf->picture_type = parm;
        } else if (parm == "") {
            conf->picture_type = "jpeg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->picture_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"jpeg\",\"webp\",\"ppm\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
}

static void conf_edit_picture_quality(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->picture_quality = 75;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_quality %d"),parm_in);
        } else {
            conf->picture_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->picture_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
}

static void conf_edit_picture_exif(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->picture_exif = "";
    } else if (pact == PARM_ACT_SET) {
        conf->picture_exif = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->picture_exif;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
}

static void conf_edit_picture_filename(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->picture_filename = "%v-%Y%m%d%H%M%S-%q";
    } else if (pact == PARM_ACT_SET) {
        conf->picture_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->picture_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
}

static void conf_edit_snapshot_interval(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->snapshot_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snapshot_interval %d"),parm_in);
        } else {
            conf->snapshot_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->snapshot_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
}

static void conf_edit_snapshot_filename(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->snapshot_filename = "%v-%Y%m%d%H%M%S-snapshot";
    } else if (pact == PARM_ACT_SET) {
        conf->snapshot_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->snapshot_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
}

static void conf_edit_movie_output(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_output = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->movie_output, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->movie_output);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
}

static void conf_edit_movie_output_motion(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_output_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->movie_output_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->movie_output_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
}

static void conf_edit_movie_max_time(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->movie_max_time = 120;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_max_time %d"),parm_in);
        } else {
            conf->movie_max_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->movie_max_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
}

static void conf_edit_movie_bps(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->movie_bps = 400000;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 9999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_bps %d"),parm_in);
        } else {
            conf->movie_bps = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->movie_bps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
}

static void conf_edit_movie_quality(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->movie_quality = 60;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_quality %d"),parm_in);
        } else {
            conf->movie_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->movie_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
}

static void conf_edit_movie_container(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_container = "mkv";
    } else if (pact == PARM_ACT_SET) {
        conf->movie_container = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->movie_container;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_container",_("movie_container"));
}

static void conf_edit_movie_passthrough(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_passthrough = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->movie_passthrough, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->movie_passthrough);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
}

static void conf_edit_movie_filename(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_filename = "%v-%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        conf->movie_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->movie_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
}

static void conf_edit_movie_retain(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_retain = "all";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "all") || (parm == "secondary") )  {
            conf->movie_retain = parm;
        } else if (parm == "") {
            conf->movie_retain = "all";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_retain %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->movie_retain;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"all\",\"secondary\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_retain",_("movie_retain"));
}

static void conf_edit_movie_extpipe_use(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_extpipe_use = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->movie_extpipe_use, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->movie_extpipe_use);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
}

static void conf_edit_movie_extpipe(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->movie_extpipe = "";
    } else if (pact == PARM_ACT_SET) {
        conf->movie_extpipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->movie_extpipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
}

static void conf_edit_timelapse_interval(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->timelapse_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_interval %d"),parm_in);
        } else {
            conf->timelapse_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->timelapse_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
}

static void conf_edit_timelapse_mode(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->timelapse_mode = "daily";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "hourly") || (parm == "daily") ||
            (parm == "weekly-sunday") || (parm == "weekly-monday") ||
            (parm == "monthly") || (parm == "manual"))  {
            conf->timelapse_mode = parm;
        } else if (parm == "") {
            conf->timelapse_mode = "daily";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->timelapse_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"hourly\",\"daily\",\"weekly-sunday\"";
        parm = parm + ",\"weekly-monday\",\"monthly\",\"manual\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
}

static void conf_edit_timelapse_fps(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->timelapse_fps = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_fps %d"),parm_in);
        } else {
            conf->timelapse_fps = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->timelapse_fps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
}

static void conf_edit_timelapse_container(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->timelapse_container = "mpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mpg") || (parm == "mkv"))  {
            conf->timelapse_container = parm;
        } else if (parm == "") {
            conf->timelapse_container = "mpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_container %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->timelapse_container;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"mpg\",\"mkv\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_container",_("timelapse_container"));
}

static void conf_edit_timelapse_filename(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->timelapse_filename = "%Y%m%d-timelapse";
    } else if (pact == PARM_ACT_SET) {
        conf->timelapse_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->timelapse_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
}

static void conf_edit_video_pipe(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->video_pipe = "";
    } else if (pact == PARM_ACT_SET) {
        conf->video_pipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->video_pipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
}

static void conf_edit_video_pipe_motion(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->video_pipe_motion = "";
    } else if (pact == PARM_ACT_SET) {
        conf->video_pipe_motion = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->video_pipe_motion;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
}

static void conf_edit_webcontrol_port(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_port %d"),parm_in);
        } else {
            conf->webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
}

static void conf_edit_webcontrol_base_path(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_base_path = "";
    } else if (pact == PARM_ACT_SET) {
        if (parm == "/") {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Invalid webcontrol_base_path: Use blank instead of single / "));
            conf->webcontrol_base_path = "";
        } else if (parm.length() >= 1) {
            if (parm.substr(0, 1) != "/") {
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    , _("Invalid webcontrol_base_path:  Must start with a / "));
                conf->webcontrol_base_path = "/" + parm;
            } else {
                conf->webcontrol_base_path = parm;
            }
        } else {
            conf->webcontrol_base_path = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_base_path;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_base_path",_("webcontrol_base_path"));
}

static void conf_edit_webcontrol_ipv6(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_ipv6 = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_ipv6, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_ipv6);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
}

static void conf_edit_webcontrol_localhost(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_localhost = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_localhost, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_localhost);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
}

static void conf_edit_webcontrol_parms(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_parms = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_parms %d"),parm_in);
        } else {
            conf->webcontrol_parms = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_parms);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_parms",_("webcontrol_parms"));
}

static void conf_edit_webcontrol_interface(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_interface = "default";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "default") || (parm == "user"))  {
            conf->webcontrol_interface = parm;
        } else if (parm == "") {
            conf->webcontrol_interface = "default";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_interface %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_interface;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"default\",\"user\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
}

static void conf_edit_webcontrol_auth_method(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_auth_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "basic") || (parm == "digest"))  {
            conf->webcontrol_auth_method = parm;
        } else if (parm == "") {
            conf->webcontrol_auth_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_auth_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_auth_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"none\",\"basic\",\"digest\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
}

static void conf_edit_webcontrol_authentication(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_authentication = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_authentication = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_authentication;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_authentication",_("webcontrol_authentication"));
}

static void conf_edit_webcontrol_tls(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_tls = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->webcontrol_tls, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->webcontrol_tls);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
}

static void conf_edit_webcontrol_cert(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_cert = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_cert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_cert;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
}

static void conf_edit_webcontrol_key(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_key = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_key = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_key;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
}

static void conf_edit_webcontrol_headers(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_headers = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_headers = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_headers;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_headers",_("webcontrol_headers"));
}

static void conf_edit_webcontrol_html(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_html = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_html = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_html;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_html",_("webcontrol_html"));
}

static void conf_edit_webcontrol_actions(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_actions = "";
    } else if (pact == PARM_ACT_SET) {
        conf->webcontrol_actions = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->webcontrol_actions;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_actions",_("webcontrol_actions"));
}

static void conf_edit_webcontrol_lock_minutes(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_lock_minutes = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_lock_minutes %d"),parm_in);
        } else {
            conf->webcontrol_lock_minutes = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_lock_minutes);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_minutes",_("webcontrol_lock_minutes"));
}

static void conf_edit_webcontrol_lock_attempts(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->webcontrol_lock_attempts = 3;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_lock_attempts %d"),parm_in);
        } else {
            conf->webcontrol_lock_attempts = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->webcontrol_lock_attempts);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_lock_attempts",_("webcontrol_lock_attempts"));
}

static void conf_edit_stream_preview_scale(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->stream_preview_scale = 25;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_scale %d"),parm_in);
        } else {
            conf->stream_preview_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->stream_preview_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_scale",_("stream_preview_scale"));
}

static void conf_edit_stream_preview_newline(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->stream_preview_newline = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->stream_preview_newline, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->stream_preview_newline);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_newline",_("stream_preview_newline"));
}

static void conf_edit_stream_preview_method(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->stream_preview_method = "mjpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mjpg") || (parm == "static"))  {
            conf->stream_preview_method = parm;
        } else if (parm == "") {
            conf->stream_preview_method = "mjpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->stream_preview_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"mjpg\",\"static\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_method",_("stream_preview_method"));
}

static void conf_edit_stream_preview_ptz(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->stream_preview_ptz = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->stream_preview_ptz, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->stream_preview_ptz);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_ptz",_("stream_preview_ptz"));
}

static void conf_edit_stream_quality(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->stream_quality = 50;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_quality %d"),parm_in);
        } else {
            conf->stream_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->stream_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
}

static void conf_edit_stream_grey(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->stream_grey = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->stream_grey, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->stream_grey);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
}

static void conf_edit_stream_motion(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->stream_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->stream_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->stream_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
}

static void conf_edit_stream_maxrate(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->stream_maxrate = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_maxrate %d"),parm_in);
        } else {
            conf->stream_maxrate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->stream_maxrate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
}

static void conf_edit_stream_scan_time(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->stream_scan_time = 5;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 600)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_scan_time %d"),parm_in);
        } else {
            conf->stream_scan_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->stream_scan_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_scan_time",_("stream_scan_time"));
}

static void conf_edit_stream_scan_scale(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->stream_scan_scale = 25;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_scan_scale %d"),parm_in);
        } else {
            conf->stream_scan_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->stream_scan_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_scan_scale",_("stream_scan_scale"));
}

static void conf_edit_database_type(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->database_type = "";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mysql") || (parm == "mariadb") || (parm == "") ||
            (parm == "postgresql") || (parm == "sqlite3")) {
            conf->database_type = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->database_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"\",\"mysql\",\"mariadb\",\"postgresql\",\"sqlite3\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
}

static void conf_edit_database_dbname(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->database_dbname = "";
    } else if (pact == PARM_ACT_SET) {
        conf->database_dbname = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->database_dbname;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
}

static void conf_edit_database_host(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->database_host = "";
    } else if (pact == PARM_ACT_SET) {
        conf->database_host = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->database_host;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
}

static void conf_edit_database_port(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->database_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_port %d"),parm_in);
        } else {
            conf->database_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->database_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
}

static void conf_edit_database_user(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->database_user = "";
    } else if (pact == PARM_ACT_SET) {
        conf->database_user = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->database_user;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
}

static void conf_edit_database_password(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->database_password = "";
    } else if (pact == PARM_ACT_SET) {
        conf->database_password = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->database_password;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
}

static void conf_edit_database_busy_timeout(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->database_busy_timeout = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_busy_timeout %d"),parm_in);
        } else {
            conf->database_busy_timeout = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->database_busy_timeout);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
}

static void conf_edit_sql_event_start(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->sql_event_start = "";
    } else if (pact == PARM_ACT_SET) {
        conf->sql_event_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->sql_event_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_start",_("sql_event_start"));
}

static void conf_edit_sql_event_end(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->sql_event_end = "";
    } else if (pact == PARM_ACT_SET) {
        conf->sql_event_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->sql_event_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_event_end",_("sql_event_end"));
}

static void conf_edit_sql_movie_start(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->sql_movie_start = "";
    } else if (pact == PARM_ACT_SET) {
        conf->sql_movie_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->sql_movie_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_start",_("sql_movie_start"));
}

static void conf_edit_sql_movie_end(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->sql_movie_end = "";
    } else if (pact == PARM_ACT_SET) {
        conf->sql_movie_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->sql_movie_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_movie_end",_("sql_movie_end"));
}

static void conf_edit_sql_pic_save(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->sql_pic_save = "";
    } else if (pact == PARM_ACT_SET) {
        conf->sql_pic_save = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->sql_pic_save;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_pic_save",_("sql_pic_save"));
}

static void conf_edit_ptz_auto_track(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_auto_track = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->ptz_auto_track, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->ptz_auto_track);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_auto_track",_("ptz_auto_track"));
}

static void conf_edit_ptz_wait(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_wait = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid ptz_wait %d"),parm_in);
        } else {
            conf->ptz_wait = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(conf->ptz_wait);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_wait",_("ptz_wait"));
}

static void conf_edit_ptz_move_track(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_move_track = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_move_track = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_move_track;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_move_track",_("ptz_move_track"));
}

static void conf_edit_ptz_pan_left(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_pan_left = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_pan_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_pan_left;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_left",_("ptz_pan_left"));
}

static void conf_edit_ptz_pan_right(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_pan_right = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_pan_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_pan_right;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_right",_("ptz_pan_right"));
}

static void conf_edit_ptz_tilt_up(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_tilt_up = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_tilt_up = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_tilt_up;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_up",_("ptz_tilt_up"));
}

static void conf_edit_ptz_tilt_down(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_tilt_down = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_tilt_down = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_tilt_down;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_down",_("ptz_tilt_down"));
}

static void conf_edit_ptz_zoom_in(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_zoom_in = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_zoom_in = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_zoom_in;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_in",_("ptz_zoom_in"));
}

static void conf_edit_ptz_zoom_out(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->ptz_zoom_out = "";
    } else if (pact == PARM_ACT_SET) {
        conf->ptz_zoom_out = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->ptz_zoom_out;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_out",_("ptz_zoom_out"));
}

static void conf_edit_snd_device(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->snd_device = "";
    } else if (pact == PARM_ACT_SET) {
        conf->snd_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->snd_device;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_device",_("snd_device"));
}

static void conf_edit_snd_params(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->snd_params = "";
    } else if (pact == PARM_ACT_SET) {
        conf->snd_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->snd_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_params",_("snd_params"));
}

static void conf_edit_snd_alerts(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    std::list<std::string>::iterator   it;

    if (pact == PARM_ACT_DFLT) {
        conf->snd_alerts.clear();
        if (parm == "") {
            return;
        }
        conf->snd_alerts.push_back(parm);
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            return;
        }
        conf->snd_alerts.push_back(parm);   /* Add to the end of list*/
        for (it= conf->snd_alerts.begin(); it != conf->snd_alerts.end(); it++) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"%s:%s"
                ,"snd_alerts", it->c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        if (conf->snd_alerts.empty()) {
            parm = "";
        } else {
            parm = conf->snd_alerts.back();     /* Give last item*/
        }
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_alerts",_("snd_alerts"));
}

static void conf_edit_snd_alerts(ctx_config *conf, std::list<std::string> &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->snd_alerts.clear();
        conf->snd_alerts = parm;
    } else if (pact == PARM_ACT_SET) {
        conf->snd_alerts = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = conf->snd_alerts;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_alerts",_("snd_alerts"));
}

static void conf_edit_snd_window(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        conf->snd_window = "hamming";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "hamming") || (parm == "hann")) {
            conf->snd_window = parm;
        } else if (parm == "") {
            conf->snd_window = "hamming";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snd_window %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = conf->snd_window;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"hamming\",\"hann\"]";

    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_window",_("snd_window"));
}

static void conf_edit_snd_show(ctx_config *conf, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
       conf->snd_show = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(conf->snd_show, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, conf->snd_show);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_show",_("snd_show"));
}

/* Application level parameters */
static void conf_edit_cat00(ctx_config *conf, std::string cmd
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (cmd == "daemon") {                         conf_edit_daemon(conf, parm_val, pact);
    } else if (cmd == "conf_filename") {           conf_edit_conf_filename(conf, parm_val, pact);
    } else if (cmd == "setup_mode") {              conf_edit_setup_mode(conf, parm_val, pact);
    } else if (cmd == "pid_file") {                conf_edit_pid_file(conf, parm_val, pact);
    } else if (cmd == "log_file") {                conf_edit_log_file(conf, parm_val, pact);
    } else if (cmd == "log_level") {               conf_edit_log_level(conf, parm_val, pact);
    } else if (cmd == "log_type") {                conf_edit_log_type(conf, parm_val, pact);
    } else if (cmd == "native_language") {         conf_edit_native_language(conf, parm_val, pact);
    }

}

static void conf_edit_cat01(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "config_dir") {                   conf_edit_config_dir(conf, parm_val, pact);
    } else if (parm_nm == "camera") {                conf_edit_camera(conf, parm_val, pact);
    } else if (parm_nm == "device_name") {           conf_edit_device_name(conf, parm_val, pact);
    } else if (parm_nm == "device_id") {             conf_edit_device_id(conf, parm_val, pact);
    } else if (parm_nm == "device_tmo") {            conf_edit_device_tmo(conf, parm_val, pact);
    } else if (parm_nm == "target_dir") {            conf_edit_target_dir(conf, parm_val, pact);
    } else if (parm_nm == "watchdog_tmo") {          conf_edit_watchdog_tmo(conf, parm_val, pact);
    } else if (parm_nm == "watchdog_kill") {         conf_edit_watchdog_kill(conf, parm_val, pact);
    }

}

static void conf_edit_cat02(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{

    if (parm_nm == "v4l2_device") {                  conf_edit_v4l2_device(conf, parm_val, pact);
    } else if (parm_nm == "v4l2_params") {           conf_edit_v4l2_params(conf, parm_val, pact);
    } else if (parm_nm == "netcam_url") {            conf_edit_netcam_url(conf, parm_val, pact);
    } else if (parm_nm == "netcam_params") {         conf_edit_netcam_params(conf, parm_val, pact);
    } else if (parm_nm == "netcam_high_url") {       conf_edit_netcam_high_url(conf, parm_val, pact);
    } else if (parm_nm == "netcam_high_params") {    conf_edit_netcam_high_params(conf, parm_val, pact);
    } else if (parm_nm == "netcam_userpass") {       conf_edit_netcam_userpass(conf, parm_val, pact);
    } else if (parm_nm == "libcam_name") {           conf_edit_libcam_name(conf, parm_val, pact);
    } else if (parm_nm == "libcam_params") {         conf_edit_libcam_params(conf, parm_val, pact);
    }

}

static void conf_edit_cat03(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "width") {                          conf_edit_width(conf, parm_val, pact);
    } else if (parm_nm == "height") {                  conf_edit_height(conf, parm_val, pact);
    } else if (parm_nm == "framerate") {               conf_edit_framerate(conf, parm_val, pact);
    } else if (parm_nm == "rotate") {                  conf_edit_rotate(conf, parm_val, pact);
    } else if (parm_nm == "flip_axis") {               conf_edit_flip_axis(conf, parm_val, pact);
    }

}

static void conf_edit_cat04(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "locate_motion_mode") {             conf_edit_locate_motion_mode(conf, parm_val, pact);
    } else if (parm_nm == "locate_motion_style") {     conf_edit_locate_motion_style(conf, parm_val, pact);
    } else if (parm_nm == "text_left") {               conf_edit_text_left(conf, parm_val, pact);
    } else if (parm_nm == "text_right") {              conf_edit_text_right(conf, parm_val, pact);
    } else if (parm_nm == "text_changes") {            conf_edit_text_changes(conf, parm_val, pact);
    } else if (parm_nm == "text_scale") {              conf_edit_text_scale(conf, parm_val, pact);
    } else if (parm_nm == "text_event") {              conf_edit_text_event(conf, parm_val, pact);
    }

}

static void conf_edit_cat05(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "emulate_motion") {                 conf_edit_emulate_motion(conf, parm_val, pact);
    } else if (parm_nm == "threshold") {               conf_edit_threshold(conf, parm_val, pact);
    } else if (parm_nm == "threshold_maximum") {       conf_edit_threshold_maximum(conf, parm_val, pact);
    } else if (parm_nm == "threshold_sdevx") {         conf_edit_threshold_sdevx(conf, parm_val, pact);
    } else if (parm_nm == "threshold_sdevy") {         conf_edit_threshold_sdevy(conf, parm_val, pact);
    } else if (parm_nm == "threshold_sdevxy") {        conf_edit_threshold_sdevxy(conf, parm_val, pact);
    } else if (parm_nm == "threshold_ratio") {         conf_edit_threshold_ratio(conf, parm_val, pact);
    } else if (parm_nm == "threshold_ratio_change") {  conf_edit_threshold_ratio_change(conf, parm_val, pact);
    } else if (parm_nm == "threshold_tune") {          conf_edit_threshold_tune(conf, parm_val, pact);
    } else if (parm_nm == "secondary_method") {        conf_edit_secondary_method(conf, parm_val, pact);
    } else if (parm_nm == "secondary_params") {        conf_edit_secondary_params(conf, parm_val, pact);
    }

}

static void conf_edit_cat06(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "noise_level") {                    conf_edit_noise_level(conf, parm_val, pact);
    } else if (parm_nm == "noise_tune") {              conf_edit_noise_tune(conf, parm_val, pact);
    } else if (parm_nm == "despeckle_filter") {        conf_edit_despeckle_filter(conf, parm_val, pact);
    } else if (parm_nm == "area_detect") {             conf_edit_area_detect(conf, parm_val, pact);
    } else if (parm_nm == "mask_file") {               conf_edit_mask_file(conf, parm_val, pact);
    } else if (parm_nm == "mask_privacy") {            conf_edit_mask_privacy(conf, parm_val, pact);
    } else if (parm_nm == "smart_mask_speed") {        conf_edit_smart_mask_speed(conf, parm_val, pact);
    }

}

static void conf_edit_cat07(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "lightswitch_percent") {            conf_edit_lightswitch_percent(conf, parm_val, pact);
    } else if (parm_nm == "lightswitch_frames") {      conf_edit_lightswitch_frames(conf, parm_val, pact);
    } else if (parm_nm == "minimum_motion_frames") {   conf_edit_minimum_motion_frames(conf, parm_val, pact);
    } else if (parm_nm == "static_object_time") {      conf_edit_static_object_time(conf, parm_val, pact);
    } else if (parm_nm == "event_gap") {               conf_edit_event_gap(conf, parm_val, pact);
    } else if (parm_nm == "pre_capture") {             conf_edit_pre_capture(conf, parm_val, pact);
    } else if (parm_nm == "post_capture") {            conf_edit_post_capture(conf, parm_val, pact);
    }

}

static void conf_edit_cat08(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "on_event_start") {                 conf_edit_on_event_start(conf, parm_val, pact);
    } else if (parm_nm == "on_event_end") {            conf_edit_on_event_end(conf, parm_val, pact);
    } else if (parm_nm == "on_picture_save") {         conf_edit_on_picture_save(conf, parm_val, pact);
    } else if (parm_nm == "on_area_detected") {        conf_edit_on_area_detected(conf, parm_val, pact);
    } else if (parm_nm == "on_motion_detected") {      conf_edit_on_motion_detected(conf, parm_val, pact);
    } else if (parm_nm == "on_movie_start") {          conf_edit_on_movie_start(conf, parm_val, pact);
    } else if (parm_nm == "on_movie_end") {            conf_edit_on_movie_end(conf, parm_val, pact);
    } else if (parm_nm == "on_camera_lost") {          conf_edit_on_camera_lost(conf, parm_val, pact);
    } else if (parm_nm == "on_camera_found") {         conf_edit_on_camera_found(conf, parm_val, pact);
    } else if (parm_nm == "on_secondary_detect") {     conf_edit_on_secondary_detect(conf, parm_val, pact);
    } else if (parm_nm == "on_action_user") {          conf_edit_on_action_user(conf, parm_val, pact);
    } else if (parm_nm == "on_sound_alert") {          conf_edit_on_sound_alert(conf, parm_val, pact);
    }

}

static void conf_edit_cat09(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "picture_output") {                 conf_edit_picture_output(conf, parm_val, pact);
    } else if (parm_nm == "picture_output_motion") {   conf_edit_picture_output_motion(conf, parm_val, pact);
    } else if (parm_nm == "picture_type") {            conf_edit_picture_type(conf, parm_val, pact);
    } else if (parm_nm == "picture_quality") {         conf_edit_picture_quality(conf, parm_val, pact);
    } else if (parm_nm == "picture_exif") {            conf_edit_picture_exif(conf, parm_val, pact);
    } else if (parm_nm == "picture_filename") {        conf_edit_picture_filename(conf, parm_val, pact);
    } else if (parm_nm == "snapshot_interval") {       conf_edit_snapshot_interval(conf, parm_val, pact);
    } else if (parm_nm == "snapshot_filename") {       conf_edit_snapshot_filename(conf, parm_val, pact);
    }

}

static void conf_edit_cat10(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "movie_output") {                   conf_edit_movie_output(conf, parm_val, pact);
    } else if (parm_nm == "movie_output_motion") {     conf_edit_movie_output_motion(conf, parm_val, pact);
    } else if (parm_nm == "movie_max_time") {          conf_edit_movie_max_time(conf, parm_val, pact);
    } else if (parm_nm == "movie_bps") {               conf_edit_movie_bps(conf, parm_val, pact);
    } else if (parm_nm == "movie_quality") {           conf_edit_movie_quality(conf, parm_val, pact);
    } else if (parm_nm == "movie_container") {         conf_edit_movie_container(conf, parm_val, pact);
    } else if (parm_nm == "movie_passthrough") {       conf_edit_movie_passthrough(conf, parm_val, pact);
    } else if (parm_nm == "movie_filename") {          conf_edit_movie_filename(conf, parm_val, pact);
    } else if (parm_nm == "movie_retain") {            conf_edit_movie_retain(conf, parm_val, pact);
    } else if (parm_nm == "movie_extpipe_use") {       conf_edit_movie_extpipe_use(conf, parm_val, pact);
    } else if (parm_nm == "movie_extpipe") {           conf_edit_movie_extpipe(conf, parm_val, pact);
    }

}

static void conf_edit_cat11(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "timelapse_interval") {             conf_edit_timelapse_interval(conf, parm_val, pact);
    } else if (parm_nm == "timelapse_mode") {          conf_edit_timelapse_mode(conf, parm_val, pact);
    } else if (parm_nm == "timelapse_fps") {           conf_edit_timelapse_fps(conf, parm_val, pact);
    } else if (parm_nm == "timelapse_container") {     conf_edit_timelapse_container(conf, parm_val, pact);
    } else if (parm_nm == "timelapse_filename") {      conf_edit_timelapse_filename(conf, parm_val, pact);
    }

}

static void conf_edit_cat12(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "video_pipe") {                     conf_edit_video_pipe(conf, parm_val, pact);
    } else if (parm_nm == "video_pipe_motion") {       conf_edit_video_pipe_motion(conf, parm_val, pact);
    }

}

static void conf_edit_cat13(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "webcontrol_port") {                    conf_edit_webcontrol_port(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_base_path") {        conf_edit_webcontrol_base_path(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_ipv6") {             conf_edit_webcontrol_ipv6(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_localhost") {        conf_edit_webcontrol_localhost(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_parms") {            conf_edit_webcontrol_parms(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_interface") {        conf_edit_webcontrol_interface(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_method") {      conf_edit_webcontrol_auth_method(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_authentication") {   conf_edit_webcontrol_authentication(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_tls") {              conf_edit_webcontrol_tls(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_cert") {             conf_edit_webcontrol_cert(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_key") {              conf_edit_webcontrol_key(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_headers") {          conf_edit_webcontrol_headers(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_html") {             conf_edit_webcontrol_html(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_actions") {          conf_edit_webcontrol_actions(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_minutes") {     conf_edit_webcontrol_lock_minutes(conf, parm_val, pact);
    } else if (parm_nm == "webcontrol_lock_attempts") {    conf_edit_webcontrol_lock_attempts(conf, parm_val, pact);
    }

}

static void conf_edit_cat14(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "stream_preview_scale") {               conf_edit_stream_preview_scale(conf, parm_val, pact);
    } else if (parm_nm == "stream_preview_newline") {      conf_edit_stream_preview_newline(conf, parm_val, pact);
    } else if (parm_nm == "stream_preview_method") {       conf_edit_stream_preview_method(conf, parm_val, pact);
    } else if (parm_nm == "stream_preview_ptz") {          conf_edit_stream_preview_ptz(conf, parm_val, pact);
    } else if (parm_nm == "stream_quality") {              conf_edit_stream_quality(conf, parm_val, pact);
    } else if (parm_nm == "stream_grey") {                 conf_edit_stream_grey(conf, parm_val, pact);
    } else if (parm_nm == "stream_motion") {               conf_edit_stream_motion(conf, parm_val, pact);
    } else if (parm_nm == "stream_maxrate") {              conf_edit_stream_maxrate(conf, parm_val, pact);
    } else if (parm_nm == "stream_scan_time") {            conf_edit_stream_scan_time(conf, parm_val, pact);
    } else if (parm_nm == "stream_scan_scale") {           conf_edit_stream_scan_scale(conf, parm_val, pact);
    }

}

static void conf_edit_cat15(ctx_config *conf, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "database_type") {                 conf_edit_database_type(conf, parm_val, pact);
    } else if (parm_nm == "database_dbname") {        conf_edit_database_dbname(conf, parm_val, pact);
    } else if (parm_nm == "database_host") {          conf_edit_database_host(conf, parm_val, pact);
    } else if (parm_nm == "database_port") {          conf_edit_database_port(conf, parm_val, pact);
    } else if (parm_nm == "database_user") {          conf_edit_database_user(conf, parm_val, pact);
    } else if (parm_nm == "database_password") {      conf_edit_database_password(conf, parm_val, pact);
    } else if (parm_nm == "database_busy_timeout") {  conf_edit_database_busy_timeout(conf, parm_val, pact);
    }

}

static void conf_edit_cat16(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "sql_event_start") {         conf_edit_sql_event_start(conf, parm_val, pact);
    } else if (parm_nm == "sql_event_end") {    conf_edit_sql_event_end(conf, parm_val, pact);
    } else if (parm_nm == "sql_movie_start") {  conf_edit_sql_movie_start(conf, parm_val, pact);
    } else if (parm_nm == "sql_movie_end") {    conf_edit_sql_movie_end(conf, parm_val, pact);
    } else if (parm_nm == "sql_pic_save") {     conf_edit_sql_pic_save(conf, parm_val, pact);
    }

}

static void conf_edit_cat17(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "ptz_auto_track") {         conf_edit_ptz_auto_track(conf, parm_val, pact);
    } else if (parm_nm == "ptz_wait") {        conf_edit_ptz_wait(conf, parm_val, pact);
    } else if (parm_nm == "ptz_move_track") {  conf_edit_ptz_move_track(conf, parm_val, pact);
    } else if (parm_nm == "ptz_pan_left") {    conf_edit_ptz_pan_left(conf, parm_val, pact);
    } else if (parm_nm == "ptz_pan_right") {   conf_edit_ptz_pan_right(conf, parm_val, pact);
    } else if (parm_nm == "ptz_tilt_up") {     conf_edit_ptz_tilt_up(conf, parm_val, pact);
    } else if (parm_nm == "ptz_tilt_down") {   conf_edit_ptz_tilt_down(conf, parm_val, pact);
    } else if (parm_nm == "ptz_zoom_in") {     conf_edit_ptz_zoom_in(conf, parm_val, pact);
    } else if (parm_nm == "ptz_zoom_out") {    conf_edit_ptz_zoom_out(conf, parm_val, pact);
    }
}

static void conf_edit_cat18(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "snd_device") {          conf_edit_snd_device(conf, parm_val, pact);
    } else if (parm_nm == "snd_params") {   conf_edit_snd_params(conf, parm_val, pact);
    } else if (parm_nm == "snd_window") {   conf_edit_snd_window(conf, parm_val, pact);
    } else if (parm_nm == "snd_alerts") {   conf_edit_snd_alerts(conf, parm_val, pact);
    } else if (parm_nm == "snd_show") {     conf_edit_snd_show(conf, parm_val, pact);

    }
}

static void conf_edit_cat18(ctx_config *conf, std::string parm_nm
        ,std::list<std::string> &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "snd_alerts") {  conf_edit_snd_alerts(conf, parm_val, pact);
    }
}

static void conf_edit_cat(ctx_config *conf, std::string parm_nm
        ,std::list<std::string> &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_18) {
        conf_edit_cat18(conf, parm_nm, parm_val, pact);
    }
}

static void conf_edit_cat(ctx_config *conf, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{
    if (pcat == PARM_CAT_00) {          conf_edit_cat00(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_01) {   conf_edit_cat01(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_02) {   conf_edit_cat02(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_03) {   conf_edit_cat03(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_04) {   conf_edit_cat04(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_05) {   conf_edit_cat05(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_06) {   conf_edit_cat06(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_07) {   conf_edit_cat07(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_08) {   conf_edit_cat08(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_09) {   conf_edit_cat09(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_10) {   conf_edit_cat10(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_11) {   conf_edit_cat11(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_12) {   conf_edit_cat12(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_13) {   conf_edit_cat13(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_14) {   conf_edit_cat14(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_15) {   conf_edit_cat15(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_16) {   conf_edit_cat16(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_17) {   conf_edit_cat17(conf, parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_18) {   conf_edit_cat18(conf, parm_nm, parm_val, pact);
    }

}

static void conf_edit_dflt(ctx_config *conf)
{
    int indx;
    std::string dflt = "";

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        conf_edit_cat(conf, config_parms[indx].parm_name, dflt
            , PARM_ACT_DFLT, config_parms[indx].parm_cat);
        indx++;
    }

}

int conf_edit_set_active(ctx_config *conf
        , std::string parm_nm, std::string parm_val)
{
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (parm_nm ==  config_parms[indx].parm_name) {
            pcat = config_parms[indx].parm_cat;
            conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_SET, pcat);
            return 0;
        }
        indx++;
    }
    return -1;

}

static void conf_edit_depr_vid(ctx_config *conf
        , std::string parm_nm, std::string newname, std::string parm_val)
{
    std::string parm_curr, parm_new;

    conf_edit_v4l2_params(conf, parm_curr, PARM_ACT_GET);
    if (parm_curr == "") {
        if (parm_nm == "power_line_frequency") {
            parm_new = "\"power line frequency\"=" + parm_val;
        } else {
            parm_new = parm_nm + "=" + parm_val;
        }
    } else {
        if (parm_nm == "power_line_frequency") {
            parm_new = parm_curr + ", \"power line frequency\"=" + parm_val;
        } else {
            parm_new = parm_curr +", " + parm_nm + "=" + parm_val;
        }
    }
    conf_edit_set_active(conf, newname, parm_new);

}

static void conf_edit_depr_web(ctx_config *conf, std::string newname
        , std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "0";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(conf, newname, parm_new);
}

static void conf_edit_depr_tdbl(ctx_config *conf, std::string newname
        , std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "2";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(conf, newname, parm_new);
}

static int conf_edit_set_depr(ctx_config *conf, std::string &parm_nm
        , std::string &parm_val)
{
    int indx;

    indx = 0;
    while (config_parms_depr[indx].parm_name != "") {
        if (parm_nm ==  config_parms_depr[indx].parm_name) {
            MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, "%s after version %s"
                , config_parms_depr[indx].info.c_str()
                , config_parms_depr[indx].last_version.c_str());

            if ((config_parms_depr[indx].parm_name == "brightness") ||
                (config_parms_depr[indx].parm_name == "contrast") ||
                (config_parms_depr[indx].parm_name == "saturation") ||
                (config_parms_depr[indx].parm_name == "hue") ||
                (config_parms_depr[indx].parm_name == "power_line_frequency")) {
                conf_edit_depr_vid(conf, parm_nm, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "webcontrol_html_output")) {
                conf_edit_depr_web(conf, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "text_double")) {
                conf_edit_depr_tdbl(conf, config_parms_depr[indx].newname, parm_val);

            } else {
                conf_edit_set_active(conf, config_parms_depr[indx].newname, parm_val);
            }
            return 0;
        }
        indx++;
    }
    return -1;
}

void conf_edit_get(ctx_config *conf, std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

void conf_edit_get(ctx_config *conf, std::string parm_nm
    , std::list<std::string> &parm_val, enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
}

/* Assign the parameter value */
void conf_edit_set(ctx_config *conf, std::string parm_nm
        , std::string parm_val)
{
    if (conf_edit_set_active(conf, parm_nm, parm_val) == 0) {
        return;
    }

    if (conf_edit_set_depr(conf, parm_nm, parm_val) == 0) {
        return;
    }

    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
}

/* Get list of valid values for items only permitting a set*/
void conf_edit_list(ctx_config *conf, std::string parm_nm, std::string &parm_val
        , enum PARM_CAT parm_cat)
{
    conf_edit_cat(conf, parm_nm, parm_val, PARM_ACT_LIST, parm_cat);
}

std::string conf_type_desc(enum PARM_TYP ptype)
{
    if (ptype == PARM_TYP_BOOL) {           return "bool";
    } else if (ptype == PARM_TYP_INT) {     return "int";
    } else if (ptype == PARM_TYP_LIST) {    return "list";
    } else if (ptype == PARM_TYP_STRING) {  return "string";
    } else if (ptype == PARM_TYP_ARRAY) {   return "array";
    } else {                                return "error";
    }
}

/* Return a string describing the parameter category */
std::string conf_cat_desc(enum PARM_CAT pcat, bool shrt) {

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

/** Prints usage and options allowed from Command-line. */
static void usage(void)
{
    printf("MotionPlus version %s, Copyright 2020-2023\n",PACKAGE_VERSION);
    printf("\nusage:\tmotionplus [options]\n");
    printf("\n\n");
    printf("Possible options:\n\n");
    printf("-b\t\t\tRun in background (daemon) mode.\n");
    printf("-n\t\t\tRun in non-daemon mode.\n");
    printf("-s\t\t\tRun in setup mode.\n");
    printf("-c config\t\tFull path and filename of config file.\n");
    printf("-d level\t\tLog level (1-9) (EMG, ALR, CRT, ERR, WRN, NTC, INF, DBG, ALL). default: 6 / NTC.\n");
    printf("-k type\t\t\tType of log (COR, STR, ENC, NET, DBL, EVT, TRK, VID, ALL). default: ALL.\n");
    printf("-p process_id_file\tFull path and filename of process id file (pid file).\n");
    printf("-l log file \t\tFull path and filename of log file.\n");
    printf("-m\t\t\tDisable detection at startup.\n");
    printf("-h\t\t\tShow this screen.\n");
    printf("\n");
}

/** Process Command-line options specified */
static void conf_cmdline(ctx_motapp *motapp)
{
    int c;

    while ((c = getopt(motapp->argc, motapp->argv, "bc:d:hmns?p:k:l:")) != EOF)
        switch (c) {
        case 'c':
            conf_edit_set(motapp->conf, "conf_filename", optarg);
            break;
        case 'b':
            conf_edit_set(motapp->conf, "daemon", "on");
            break;
        case 'n':
            conf_edit_set(motapp->conf, "daemon", "off");
            break;
        case 's':
            conf_edit_set(motapp->conf, "setup_mode", "on");
            break;
        case 'd':
            conf_edit_set(motapp->conf, "log_level", optarg);
            break;
        case 'k':
            conf_edit_set(motapp->conf, "log_type", optarg);
            break;
        case 'p':
            conf_edit_set(motapp->conf, "pid_file", optarg);
            break;
        case 'l':
            conf_edit_set(motapp->conf, "log_file", optarg);
            break;
        case 'm':
            motapp->pause = true;
            break;
        case 'h':
        case '?':
        default:
             usage();
             exit(1);
        }

    optind = 1;
}

/* Add in a default filename for the last camera config if it wasn't provided. */
static void conf_camera_filenm(ctx_motapp *motapp)
{
    int indx_cam, indx;
    std::string dirnm, fullnm;
    struct stat statbuf;
    size_t lstpos;

    if (motapp->cam_list[motapp->cam_cnt-1]->conf->conf_filename != "") {
        return;
    }

    lstpos = motapp->conf->conf_filename.find_last_of("/");
    if (lstpos != std::string::npos) {
        lstpos++;
    }
    dirnm = motapp->conf->conf_filename.substr(0, lstpos);

    indx_cam = 1;
    fullnm = "";
    while (fullnm == "") {
        fullnm = dirnm + "camera" + std::to_string(indx_cam) + ".conf";
        for (indx=0;indx<motapp->cam_cnt;indx++) {
            if (fullnm == motapp->cam_list[indx_cam]->conf->conf_filename) {
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

    motapp->cam_list[motapp->cam_cnt-1]->conf->conf_filename = fullnm;

}

void conf_camera_add(ctx_motapp *motapp)
{
    int indx;
    std::string parm_val;

    motapp->cam_cnt++;
    motapp->cam_list = (ctx_dev **)myrealloc(
        motapp->cam_list, sizeof(ctx_dev *) * (motapp->cam_cnt + 1), "config_camera");

    motapp->cam_list[motapp->cam_cnt-1] = new ctx_dev;
    memset(motapp->cam_list[motapp->cam_cnt-1],0,sizeof(ctx_dev));
    motapp->cam_list[motapp->cam_cnt-1]->conf = new ctx_config;

    motapp->cam_list[motapp->cam_cnt] = NULL;
    motapp->cam_list[motapp->cam_cnt-1]->motapp = motapp;

    conf_edit_dflt(motapp->cam_list[motapp->cam_cnt-1]->conf);

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (mystrne(config_parms[indx].parm_name.c_str(),"device_id")) {
            conf_edit_get(motapp->conf, config_parms[indx].parm_name
                , parm_val, config_parms[indx].parm_cat);
            conf_edit_set(motapp->cam_list[motapp->cam_cnt-1]->conf
                , config_parms[indx].parm_name, parm_val);
        }
        indx++;
    }

    conf_camera_filenm(motapp);

}

static void conf_camera_parm(ctx_motapp *motapp, std::string filename)
{
    struct stat statbuf;

    if (stat(filename.c_str(), &statbuf) != 0) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera config file %s not found"), filename.c_str());
        return;
    }

    conf_camera_add(motapp);
    motapp->cam_list[motapp->cam_cnt-1]->conf->conf_filename = filename;
    conf_process(motapp, motapp->cam_list[motapp->cam_cnt-1]->conf);

}

/* Add in a default filename for the last sound config if it wasn't provided. */
static void conf_sound_filenm(ctx_motapp *motapp)
{
    int indx_snd, indx;
    std::string dirnm, fullnm;
    struct stat statbuf;
    size_t lstpos;

    if (motapp->snd_list[motapp->snd_cnt-1]->conf->conf_filename != "") {
        return;
    }

    lstpos = motapp->conf->conf_filename.find_last_of("/");
    if (lstpos != std::string::npos) {
        lstpos++;
    }
    dirnm = motapp->conf->conf_filename.substr(0, lstpos);

    indx_snd = 1;
    fullnm = "";
    while (fullnm == "") {
        fullnm = dirnm + "sound" + std::to_string(indx_snd) + ".conf";
        for (indx=0;indx<motapp->snd_cnt;indx++) {
            if (fullnm == motapp->snd_list[indx_snd]->conf->conf_filename) {
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

    motapp->snd_list[motapp->snd_cnt-1]->conf->conf_filename = fullnm;

}

void conf_sound_add(ctx_motapp *motapp)
{
    int indx;
    std::string parm_val;

    motapp->snd_cnt++;
    motapp->snd_list = (ctx_dev **)myrealloc(
        motapp->snd_list, sizeof(ctx_dev *) * (motapp->snd_cnt + 1), "config_sound");

    motapp->snd_list[motapp->snd_cnt-1] = new ctx_dev;
    memset(motapp->snd_list[motapp->snd_cnt-1],0,sizeof(ctx_dev));
    motapp->snd_list[motapp->snd_cnt-1]->conf = new ctx_config;

    motapp->snd_list[motapp->snd_cnt] = NULL;
    motapp->snd_list[motapp->snd_cnt-1]->motapp = motapp;

    conf_edit_dflt(motapp->snd_list[motapp->snd_cnt-1]->conf);

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (mystrne(config_parms[indx].parm_name.c_str(),"device_id")) {
            conf_edit_get(motapp->conf, config_parms[indx].parm_name
                , parm_val, config_parms[indx].parm_cat);
            conf_edit_set(motapp->snd_list[motapp->snd_cnt-1]->conf
                , config_parms[indx].parm_name, parm_val);
        }
        indx++;
    }

    conf_sound_filenm(motapp);

}

static void conf_sound_parm(ctx_motapp *motapp, std::string filename)
{
    struct stat statbuf;

    if (stat(filename.c_str(), &statbuf) != 0) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Sound config file %s not found"), filename.c_str());
        return;
    }

    conf_sound_add(motapp);
    motapp->snd_list[motapp->snd_cnt-1]->conf->conf_filename = filename;
    conf_process(motapp, motapp->snd_list[motapp->snd_cnt-1]->conf);

}

/** Process config_dir */
static void conf_parm_config_dir(ctx_motapp *motapp, std::string confdir)
{
    DIR *dp;
    dirent *ep;
    std::string conf_file;

    dp = opendir(confdir.c_str());
    if (dp != NULL) {
        while( (ep = readdir(dp)) ) {
            conf_file.assign(ep->d_name);
            if (conf_file.find_first_of(".conf") != std::string::npos) {
                conf_file = confdir + "/" + conf_file;
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Processing config file %s"), conf_file.c_str() );
                conf_camera_parm(motapp, conf_file);
                motapp->cam_list[motapp->cam_cnt-1]->conf->from_conf_dir = true;
            }
        }
    }
    closedir(dp);

    conf_edit_set(motapp->conf, "config_dir", confdir);

}

/** Process each line from the config file. */
void conf_process(ctx_motapp *motapp, ctx_config *conf)
{
    size_t stpos;
    std::string line, parm_nm, parm_vl;
    std::ifstream ifs;

    ifs.open(conf->conf_filename);
        if (ifs.is_open() == false) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("params_file not found: %s")
                , conf->conf_filename.c_str());
            return;
        }

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Processing config file %s")
            , conf->conf_filename.c_str());

        while (std::getline(ifs, line)) {
            mytrim(line);
            stpos = line.find(" ");
            if (stpos > line.find("=")) {
                stpos = line.find("=");
            }
            if ((stpos != std::string::npos) &&
                (stpos != line.length()-1) &&
                (stpos != 0) &&
                (line.substr(0, 1) != ";") &&
                (line.substr(0, 1) != "#")) {
                parm_nm = line.substr(0, stpos);
                parm_vl = line.substr(stpos+1, line.length()-stpos);
                myunquote(parm_nm);
                myunquote(parm_vl);
                if ((parm_nm == "camera") && (motapp->conf == conf)) {
                    conf_camera_parm(motapp, parm_vl);
                } else if ((parm_nm == "sound") && (motapp->conf == conf)) {
                    conf_sound_parm(motapp, parm_vl);
                } else if ((parm_nm == "config_dir") && (motapp->conf == conf)){
                    conf_parm_config_dir(motapp, parm_vl);
                } else if ((parm_nm != "camera") && (parm_nm != "sound") &&
                    (parm_nm != "config_dir")) {
                   conf_edit_set(conf, parm_nm, parm_vl);
                }
            } else if ((line != "") &&
                (line.substr(0, 1) != ";") &&
                (line.substr(0, 1) != "#") &&
                (stpos != std::string::npos) ) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Unable to parse line: %s"), line.c_str());
            }
        }
    ifs.close();

}

void conf_parms_log_parm(std::string parm_nm, std::string parm_vl)
{
    if ((parm_nm == "netcam_url") ||
        (parm_nm == "netcam_userpass") ||
        (parm_nm == "netcam_high_url") ||
        (parm_nm == "webcontrol_authentication") ||
        (parm_nm == "webcontrol_key") ||
        (parm_nm == "webcontrol_cert") ||
        (parm_nm == "database_user") ||
        (parm_nm == "database_password"))
    {
        motion_log(INF, TYPE_ALL, NO_ERRNO,0
            ,_("%-25s <redacted>"), parm_nm.c_str());
    } else {
        if ((parm_nm.compare(0,4,"text") == 0) ||
            (parm_vl.compare(0,1, " ") != 0)) {
            motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s %s"
                , parm_nm.c_str(), parm_vl.c_str());
        } else {
            motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s \"%s\""
                , parm_nm.c_str(), parm_vl.c_str());
        }
    }

}

/**  Write the configuration(s) to the log */
void conf_parms_log(ctx_motapp *motapp)
{
    int i, indx;
    std::string parm_vl, parm_main, parm_nm;
    std::list<std::string> parm_array;
    std::list<std::string>::iterator it;
    enum PARM_CAT parm_ct;
    enum PARM_TYP parm_typ;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("Logging configuration parameters from all files"));

    motion_log(INF, TYPE_ALL, NO_ERRNO,0, _("Config file: %s")
        , motapp->conf->conf_filename.c_str());

    i = 0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;

        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            conf_edit_get(motapp->conf, parm_nm,parm_vl, parm_ct);
            conf_parms_log_parm(parm_nm, parm_vl);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            conf_edit_get(motapp->conf, parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                conf_parms_log_parm(parm_nm, it->c_str());
            }
        }
        i++;
    }

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        motion_log(INF, TYPE_ALL, NO_ERRNO, 0
            , _("Camera %d - Config file: %s")
            , motapp->cam_list[indx]->conf->device_id
            , motapp->cam_list[indx]->conf->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            conf_edit_get(motapp->conf, parm_nm, parm_main, parm_ct);
            conf_edit_get(motapp->cam_list[indx]->conf, parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY) ) {
                conf_parms_log_parm(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                conf_edit_get(motapp->cam_list[indx]->conf, parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    conf_parms_log_parm(parm_nm, it->c_str());
                }
            }
            i++;
        }
    }

    for (indx=0; indx<motapp->snd_cnt; indx++) {
        motion_log(INF, TYPE_ALL, NO_ERRNO, 0
            , _("Sound %d - Config file: %s")
            , motapp->snd_list[indx]->conf->device_id
            , motapp->snd_list[indx]->conf->conf_filename.c_str());
        i = 0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            conf_edit_get(motapp->conf, parm_nm, parm_main, parm_ct);
            conf_edit_get(motapp->snd_list[indx]->conf, parm_nm, parm_vl, parm_ct);
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_main != parm_vl) && (parm_typ != PARM_TYP_ARRAY) ) {
                conf_parms_log_parm(parm_nm, parm_vl);
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                conf_edit_get(motapp->snd_list[indx]->conf, parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    conf_parms_log_parm(parm_nm, it->c_str());
                }
            }
            i++;
        }
    }

}

void conf_parms_write_parms(FILE *conffile, std::string parm_nm
    , std::string parm_vl, enum PARM_CAT parm_ct, bool reset)
{
    static enum PARM_CAT prev_ct;

    if (reset) {
        prev_ct = PARM_CAT_00;
        return;
    }

    if (parm_ct != prev_ct) {
        fprintf(conffile,"\n%s",";*************************************************\n");
        fprintf(conffile,"%s%s\n", ";*****   ", conf_cat_desc(parm_ct,false).c_str());
        fprintf(conffile,"%s",";*************************************************\n");
        prev_ct = parm_ct;
    }

    if (parm_vl.compare(0, 1, " ") == 0) {
        fprintf(conffile, "%s \"%s\"\n", parm_nm.c_str(), parm_vl.c_str());
    } else {
        fprintf(conffile, "%s %s\n", parm_nm.c_str(), parm_vl.c_str());
    }
}

void conf_parms_write_app(ctx_motapp *motapp)
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

    conffile = myfopen(motapp->conf->conf_filename.c_str(), "we");
    if (conffile == NULL) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Failed to write configuration to %s")
            , motapp->conf->conf_filename);
        return;
    }

    fprintf(conffile, "; %s\n", motapp->conf->conf_filename.c_str());
    fprintf(conffile, ";\n; This config file was generated by MotionPlus " VERSION "\n");
    fprintf(conffile, "; at %s\n", timestamp);
    fprintf(conffile, "\n\n");

    conf_parms_write_parms(conffile, "", "", PARM_CAT_00, true);

    i=0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            conf_edit_get(motapp->conf, parm_nm, parm_vl, parm_ct);
            conf_parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            conf_edit_get(motapp->conf, parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                conf_parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        }
        i++;
    }

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        if (motapp->cam_list[indx]->conf->from_conf_dir == false) {
            conf_parms_write_parms(conffile, "camera"
                , motapp->cam_list[indx]->conf->conf_filename
                , PARM_CAT_01, false);
        }
    }

    for (indx=0; indx<motapp->snd_cnt; indx++) {
        if (motapp->snd_list[indx]->conf->from_conf_dir == false) {
            conf_parms_write_parms(conffile, "sound"
                , motapp->snd_list[indx]->conf->conf_filename
                , PARM_CAT_01, false);
        }
    }

    fprintf(conffile, "\n");

    conf_edit_get(motapp->conf, "config_dir", parm_vl, PARM_CAT_01);
    conf_parms_write_parms(conffile, "config_dir", parm_vl, PARM_CAT_01, false);

    fprintf(conffile, "\n");
    myfclose(conffile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Configuration written to %s")
        , motapp->conf->conf_filename.c_str());

}

void conf_parms_write_cam(ctx_motapp *motapp)
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

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        conffile = myfopen(motapp->cam_list[indx]->conf->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to write configuration to %s")
                , motapp->cam_list[indx]->conf->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", motapp->cam_list[indx]->conf->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by MotionPlus " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        conf_parms_write_parms(conffile, "", "", PARM_CAT_00, true);

        i=0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY) ) {
                conf_edit_get(motapp->conf, parm_nm, parm_main, parm_ct);
                conf_edit_get(motapp->cam_list[indx]->conf, parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    conf_parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                conf_edit_get(motapp->conf, parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    conf_parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Configuration written to %s")
            , motapp->cam_list[indx]->conf->conf_filename.c_str());
    }

}

void conf_parms_write_snd(ctx_motapp *motapp)
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

    for (indx=0; indx<motapp->snd_cnt; indx++) {
        conffile = myfopen(motapp->snd_list[indx]->conf->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to write configuration to %s")
                , motapp->snd_list[indx]->conf->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", motapp->snd_list[indx]->conf->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by MotionPlus " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        conf_parms_write_parms(conffile, "", "", PARM_CAT_00, true);

        i=0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY)) {
                conf_edit_get(motapp->conf, parm_nm, parm_main, parm_ct);
                conf_edit_get(motapp->snd_list[indx]->conf, parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    conf_parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                conf_edit_get(motapp->conf, parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); ++it) {
                    conf_parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Configuration written to %s")
            , motapp->snd_list[indx]->conf->conf_filename.c_str());
    }
}

/**  Write the configuration(s) to file */
void conf_parms_write(ctx_motapp *motapp)
{
    conf_parms_write_app(motapp);
    conf_parms_write_cam(motapp);
    conf_parms_write_snd(motapp);

}

void conf_init(ctx_motapp *motapp)
{
    std::string filename;
    char path[PATH_MAX];
    struct stat statbuf;
    int indx;

    conf_edit_dflt(motapp->conf);

    conf_cmdline(motapp);

    filename = "";
    if (motapp->conf->conf_filename != "") {
        filename = motapp->conf->conf_filename;
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename="";
        }
    }

    if (filename == "") {
        if (getcwd(path, sizeof(path)) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error getcwd"));
            exit(-1);
        }
        filename = path + std::string("/motionplus.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string(getenv("HOME")) + std::string("/.motionplus/motionplus.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        filename = std::string( sysconfdir ) + std::string("/motionplus.conf");
        if (stat(filename.c_str(), &statbuf) != 0) {
            filename = "";
        }
    }

    if (filename == "") {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Could not open configuration file"));
        exit(-1);
    }

    conf_edit_set(motapp->conf, "conf_filename", filename);

    conf_process(motapp, motapp->conf);

    conf_cmdline(motapp);

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        motapp->cam_list[indx]->threadnr = indx;
    }

    for (indx=0; indx<motapp->snd_cnt; indx++) {
        motapp->snd_list[indx]->threadnr = indx + motapp->cam_cnt;
    }

}

void conf_deinit(ctx_motapp *motapp)
{
    int indx;

    for (indx=0; indx<motapp->cam_cnt; indx++) {
        delete motapp->cam_list[indx]->conf;
        delete motapp->cam_list[indx];
    }
    myfree(&motapp->cam_list);

    for (indx=0; indx<motapp->snd_cnt; indx++) {
        delete motapp->snd_list[indx]->conf;
        delete motapp->snd_list[indx];
    }
    myfree(&motapp->snd_list);

}

