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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
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
void conf_process(struct ctx_motapp *motapp, bool ismotapp, FILE *fp, int threadnbr);

/*Configuration parameters */
struct ctx_parm config_parms[] = {
    {"daemon",                    PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"setup_mode",                PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"conf_filename",             PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"pid_file",                  PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"log_file",                  PARM_TYP_STRING, PARM_CAT_00, WEBUI_LEVEL_ADVANCED },
    {"log_level",                 PARM_TYP_LIST,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"log_type",                  PARM_TYP_LIST,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },
    {"native_language",           PARM_TYP_BOOL,   PARM_CAT_00, WEBUI_LEVEL_LIMITED },

    {"camera_name",               PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"camera_id",                 PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"camera_tmo",                PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"target_dir",                PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"watchdog_tmo",              PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"watchdog_kill",             PARM_TYP_INT,    PARM_CAT_01, WEBUI_LEVEL_LIMITED },
    {"camera_dir",                PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },
    {"camera",                    PARM_TYP_STRING, PARM_CAT_01, WEBUI_LEVEL_ADVANCED },

    {"v4l2_device",               PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"v4l2_params",               PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_url",                PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_params",             PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_high_url",           PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_high_params",        PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"netcam_userpass",           PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"mmalcam_name",              PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },
    {"mmalcam_params",            PARM_TYP_STRING, PARM_CAT_02, WEBUI_LEVEL_ADVANCED },

    {"width",                     PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"height",                    PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"framerate",                 PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
    {"minimum_frame_time",        PARM_TYP_INT,    PARM_CAT_03, WEBUI_LEVEL_LIMITED },
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
    {"secondary_interval",        PARM_TYP_INT,    PARM_CAT_05, WEBUI_LEVEL_LIMITED },
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
    {"movie_codec",               PARM_TYP_STRING, PARM_CAT_10, WEBUI_LEVEL_LIMITED },
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

    {"sql_log_picture",           PARM_TYP_BOOL,   PARM_CAT_16, WEBUI_LEVEL_LIMITED },
    {"sql_log_snapshot",          PARM_TYP_BOOL,   PARM_CAT_16, WEBUI_LEVEL_LIMITED},
    {"sql_log_movie",             PARM_TYP_BOOL,   PARM_CAT_16, WEBUI_LEVEL_LIMITED },
    {"sql_log_timelapse",         PARM_TYP_BOOL,   PARM_CAT_16, WEBUI_LEVEL_LIMITED},
    {"sql_query_start",           PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_query_stop",            PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED },
    {"sql_query",                 PARM_TYP_STRING, PARM_CAT_16, WEBUI_LEVEL_ADVANCED},

    {"ptz_auto_track",            PARM_TYP_BOOL,   PARM_CAT_17, WEBUI_LEVEL_LIMITED },
    {"ptz_wait",                  PARM_TYP_INT,    PARM_CAT_17, WEBUI_LEVEL_LIMITED },
    {"ptz_move_track",            PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_pan_left",              PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_pan_right",             PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_tilt_up",               PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_tilt_down",             PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_zoom_in",               PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },
    {"ptz_zoom_out",              PARM_TYP_STRING, PARM_CAT_17, WEBUI_LEVEL_RESTRICTED },

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
struct ctx_parm_depr config_parms_depr[] = {
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
    "\"ffmpeg_video_codec\" replaced with \"movie_codec\"",
    "movie_codec"
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

static void conf_edit_daemon(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->daemon = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(motapp->daemon, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, motapp->daemon);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
}

static void conf_edit_setup_mode(struct ctx_motapp *motapp, std::string &parm, int pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->setup_mode = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(motapp->setup_mode, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, motapp->setup_mode);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","setup_mode",_("setup_mode"));
}

static void conf_edit_conf_filename(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->conf_filename = "";
    } else if (pact == PARM_ACT_SET) {
        motapp->conf_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = motapp->conf_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}

static void conf_edit_pid_file(struct ctx_motapp *motapp, std::string &parm, int pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->pid_file = "";
    } else if (pact == PARM_ACT_SET) {
        motapp->pid_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = motapp->pid_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
}

static void conf_edit_log_file(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->log_file = "";
    } else if (pact == PARM_ACT_SET) {
        motapp->log_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = motapp->log_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
}

static void conf_edit_log_level(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        motapp->log_level = 6;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 9)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_level %d"),parm_in);
        } else {
            motapp->log_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(motapp->log_level);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\"";
        parm = parm + "]";
    }

    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
}

static void conf_edit_log_type(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->log_type_str = "ALL";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "ALL") || (parm == "COR") ||
            (parm == "STR") || (parm == "ENC") ||
            (parm == "NET") || (parm == "DBL") ||
            (parm == "EVT") || (parm == "TRK") ||
            (parm == "VID") || (parm == "ALL")) {
            motapp->log_type_str = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_type %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = motapp->log_type_str;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"ALL\",\"COR\",\"STR\",\"ENC\",\"NET\"";
        parm = parm + ",\"DBL\",\"EVT\",\"TRK\",\"VID\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
}

static void conf_edit_native_language(struct ctx_motapp *motapp, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        motapp->native_language = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(motapp->native_language, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, motapp->native_language);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

static void conf_edit_camera(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    /* This edit routine is special for the camera.
     * We are dealing with the file name that is still a char
     * the file name back on request so that it shows on web interface
     */
    int retcd;
    if (pact == PARM_ACT_SET) {
        retcd = snprintf(cam->conf_filename, PATH_MAX, "%s", parm.c_str());
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO,"Error using %s", parm.c_str());
            cam->conf_filename[0] = '\0';
        };
    } else if (pact == PARM_ACT_GET) {
        parm.assign(cam->conf_filename);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera",_("camera"));
}

static void conf_edit_camera_name(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_name= "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->camera_name = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->camera_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_name",_("camera_name"));
}

static void conf_edit_camera_id(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in, minval;

    if (cam->motapp->cam_list[0] == cam) {
        minval = 0;
    } else {
        minval = 1;
    }

    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_id = minval;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < minval) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid camera_id %d"),parm_in);
        } else {
            cam->conf->camera_id = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->camera_id);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_id",_("camera_id"));
}

static void conf_edit_camera_tmo(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_tmo = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid camera_tmo %d"),parm_in);
        } else {
            cam->conf->camera_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->camera_tmo);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_tmo",_("camera_tmo"));
}

static void conf_edit_camera_dir(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->camera_dir = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->camera_dir = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->camera_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_dir",_("camera_dir"));
}

static void conf_edit_target_dir(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->target_dir = ".";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->target_dir = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->target_dir;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
}

static void conf_edit_watchdog_tmo(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->watchdog_tmo = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog timeout %d"),parm_in);
        } else {
            cam->conf->watchdog_tmo = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->watchdog_tmo);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_tmo",_("watchdog_tmo"));
}

static void conf_edit_watchdog_kill(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->watchdog_kill = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid watchdog kill timeout %d"),parm_in);
        } else {
            cam->conf->watchdog_kill = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->watchdog_kill);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_kill",_("watchdog_kill"));
}

static void conf_edit_v4l2_device(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->v4l2_device = "/dev/video0";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->v4l2_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->v4l2_device;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_device",_("v4l2_device"));
}

static void conf_edit_v4l2_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->v4l2_params = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->v4l2_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->v4l2_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","v4l2_params",_("v4l2_params"));
}

static void conf_edit_netcam_url(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_url = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->netcam_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->netcam_url;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
}

static void conf_edit_netcam_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->netcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->netcam_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_params",_("netcam_params"));
}

static void conf_edit_netcam_high_url(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_high_url = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->netcam_high_url = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->netcam_high_url;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_url",_("netcam_high_url"));
}

static void conf_edit_netcam_high_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_high_params = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->netcam_high_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->netcam_high_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_params",_("netcam_high_params"));
}

static void conf_edit_netcam_userpass(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->netcam_userpass = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->netcam_userpass = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->netcam_userpass;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
}

static void conf_edit_mmalcam_name(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mmalcam_name = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->mmalcam_name = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->mmalcam_name;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_name",_("mmalcam_name"));
}

static void conf_edit_mmalcam_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mmalcam_params = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->mmalcam_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->mmalcam_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_params",_("mmalcam_params"));
}

static void conf_edit_width(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->width = 640;
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
            cam->conf->width = parm_in;
        } else {
            cam->conf->width = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->width);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
}

static void conf_edit_height(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->height = 480;
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
            cam->conf->height = parm_in;
        } else {
            cam->conf->height = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->height);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
}

static void conf_edit_framerate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->framerate = 15;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid framerate %d"),parm_in);
        } else {
            cam->conf->framerate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->framerate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
}

static void conf_edit_minimum_frame_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->minimum_frame_time = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_frame_time %d"),parm_in);
        } else {
            cam->conf->minimum_frame_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->minimum_frame_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_frame_time",_("minimum_frame_time"));
}

static void conf_edit_rotate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->rotate = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in != 0) && (parm_in != 90) &&
            (parm_in != 180) && (parm_in != 270) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid rotate %d"),parm_in);
        } else {
            cam->conf->rotate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->rotate);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"0\",\"90\",\"180\",\"270\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
}

static void conf_edit_flip_axis(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->flip_axis = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "vertical") || (parm == "horizontal")) {
            cam->conf->flip_axis = parm;
        } else if (parm == "") {
            cam->conf->flip_axis = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid flip_axis %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->flip_axis;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"vertical\",\"horizontal\"]";

    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
}

static void conf_edit_locate_motion_mode(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->locate_motion_mode = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "off") || (parm == "on") || (parm == "preview")) {
            cam->conf->locate_motion_mode = parm;
        } else if (parm == "") {
            cam->conf->locate_motion_mode = "off";
        } else {
          MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->locate_motion_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"off\",\"on\",\"preview\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
}

static void conf_edit_locate_motion_style(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->locate_motion_style = "box";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "box") || (parm == "redbox") ||
            (parm == "cross") || (parm == "redcross"))  {
            cam->conf->locate_motion_style = parm;
        } else if (parm == "") {
            cam->conf->locate_motion_style = "box";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid locate_motion_style %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->locate_motion_style;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"box\",\"redbox\",\"cross\",\"redcross\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
}

static void conf_edit_text_left(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_left = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->text_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->text_left;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
}

static void conf_edit_text_right(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_right = "%Y-%m-%d\\n%T";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->text_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->text_right;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
}

static void conf_edit_text_changes(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_changes = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->text_changes, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->text_changes);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
}

static void conf_edit_text_scale(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_scale = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid text_scale %d"),parm_in);
        } else {
            cam->conf->text_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->text_scale);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm + "\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
}

static void conf_edit_text_event(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->text_event = "%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->text_event = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->text_event;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
}

static void conf_edit_emulate_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->emulate_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->emulate_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->emulate_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
}

static void conf_edit_threshold(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold = 1500;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            cam->conf->threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
}

static void conf_edit_threshold_maximum(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_maximum = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_maximum %d"),parm_in);
        } else {
            cam->conf->threshold_maximum = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_maximum);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
}

static void conf_edit_threshold_sdevx(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_sdevx = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevx %d"),parm_in);
        } else {
            cam->conf->threshold_sdevx = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_sdevx);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevx",_("threshold_sdevx"));
}

static void conf_edit_threshold_sdevy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_sdevy = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevy %d"),parm_in);
        } else {
            cam->conf->threshold_sdevy = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_sdevy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevy",_("threshold_sdevy"));
}

static void conf_edit_threshold_sdevxy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_sdevxy = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_sdevxy %d"),parm_in);
        } else {
            cam->conf->threshold_sdevxy = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_sdevxy);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_sdevxy",_("threshold_sdevxy"));
}

static void conf_edit_threshold_ratio(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_ratio = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_ratio %d"),parm_in);
        } else {
            cam->conf->threshold_ratio = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_ratio);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_ratio",_("threshold_ratio"));
}

static void conf_edit_threshold_ratio_change(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_ratio_change = 64;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 255) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold_ratio_change %d"),parm_in);
        } else {
            cam->conf->threshold_ratio_change = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->threshold_ratio_change);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_ratio_change",_("threshold_ratio_change"));
}

static void conf_edit_threshold_tune(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->threshold_tune = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->threshold_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->threshold_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
}

static void conf_edit_secondary_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) ) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_interval %d"),parm_in);
        } else {
            cam->conf->secondary_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->secondary_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_interval",_("secondary_interval"));
}

static void conf_edit_secondary_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "haar") ||
            (parm == "hog"))  {
            cam->conf->secondary_method = parm;
        } else if (parm == "") {
            cam->conf->secondary_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid secondary_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->secondary_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"none\",\"haar\",\"hog\"]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_method",_("secondary_method"));
}

static void conf_edit_secondary_params(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->secondary_params = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->secondary_params = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->secondary_params;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","secondary_params",_("secondary_params"));
}

static void conf_edit_noise_level(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->noise_level = 32;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 255)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid noise_level %d"),parm_in);
        } else {
            cam->conf->noise_level = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->noise_level);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
}

static void conf_edit_noise_tune(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->noise_tune = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->noise_tune, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->noise_tune);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
}

static void conf_edit_despeckle_filter(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->despeckle_filter = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->despeckle_filter = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->despeckle_filter;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
}

static void conf_edit_area_detect(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->area_detect = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->area_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->area_detect;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
}

static void conf_edit_mask_file(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mask_file = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->mask_file = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->mask_file;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
}

static void conf_edit_mask_privacy(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->mask_privacy = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->mask_privacy = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->mask_privacy;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
}

static void conf_edit_smart_mask_speed(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->smart_mask_speed = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid smart_mask_speed %d"),parm_in);
        } else {
            cam->conf->smart_mask_speed = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->smart_mask_speed);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\",\"4\",\"5\"";
        parm = parm + ",\"6\",\"7\",\"8\",\"9\",\"10\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
}

static void conf_edit_lightswitch_percent(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->lightswitch_percent = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_percent %d"),parm_in);
        } else {
            cam->conf->lightswitch_percent = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->lightswitch_percent);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
}

static void conf_edit_lightswitch_frames(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->lightswitch_frames = 5;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid lightswitch_frames %d"),parm_in);
        } else {
            cam->conf->lightswitch_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->lightswitch_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
}

static void conf_edit_minimum_motion_frames(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->minimum_motion_frames = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid minimum_motion_frames %d"),parm_in);
        } else {
            cam->conf->minimum_motion_frames = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->minimum_motion_frames);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
}

static void conf_edit_static_object_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->static_object_time = 10;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if (parm_in < 1) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid static_object_time %d"),parm_in);
        } else {
            cam->conf->static_object_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->static_object_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","static_object_time",_("static_object_time"));
}

static void conf_edit_event_gap(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->event_gap = 60;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid event_gap %d"),parm_in);
        } else {
            cam->conf->event_gap = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->event_gap);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
}

static void conf_edit_pre_capture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->pre_capture = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid pre_capture %d"),parm_in);
        } else {
            cam->conf->pre_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->pre_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
}

static void conf_edit_post_capture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->post_capture = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid post_capture %d"),parm_in);
        } else {
            cam->conf->post_capture = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->post_capture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
}

static void conf_edit_on_event_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_event_start = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_event_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_event_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
}

static void conf_edit_on_event_end(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_event_end = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_event_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_event_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
}

static void conf_edit_on_picture_save(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_picture_save = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_picture_save = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_picture_save;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
}

static void conf_edit_on_area_detected(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_area_detected = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_area_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_area_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
}

static void conf_edit_on_motion_detected(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_motion_detected = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_motion_detected = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_motion_detected;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
}

static void conf_edit_on_movie_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_movie_start = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_movie_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_movie_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
}

static void conf_edit_on_movie_end(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_movie_end = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_movie_end = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_movie_end;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
}

static void conf_edit_on_camera_lost(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_camera_lost = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_camera_lost = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_camera_lost;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
}

static void conf_edit_on_camera_found(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_camera_found = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_camera_found = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_camera_found;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
}

static void conf_edit_on_secondary_detect(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->on_secondary_detect = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->on_secondary_detect = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->on_secondary_detect;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_secondary_detect",_("on_secondary_detect"));
}

static void conf_edit_picture_output(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_output = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "center") ||
            (parm == "first") || (parm == "best"))  {
            cam->conf->picture_output = parm;
        } else if (parm == "") {
            cam->conf->picture_output = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->picture_output;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"first\",\"best\",\"center\" ";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
}

static void conf_edit_picture_output_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_output_motion = "off";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "on") || (parm == "off") || (parm == "roi"))  {
            cam->conf->picture_output_motion = parm;
        } else if (parm == "") {
            cam->conf->picture_output_motion = "off";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_output_motion %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->picture_output;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"on\",\"off\",\"roi\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
}

static void conf_edit_picture_type(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_type = "jpeg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "jpeg") || (parm == "webp") || (parm == "ppm"))  {
            cam->conf->picture_type = parm;
        } else if (parm == "") {
            cam->conf->picture_type = "jpeg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->picture_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"jpeg\",\"webp\",\"ppm\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
}

static void conf_edit_picture_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_quality = 75;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid picture_quality %d"),parm_in);
        } else {
            cam->conf->picture_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->picture_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
}

static void conf_edit_picture_exif(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_exif = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->picture_exif = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->picture_exif;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
}

static void conf_edit_picture_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->picture_filename = "%v-%Y%m%d%H%M%S-%q";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->picture_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->picture_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
}

static void conf_edit_snapshot_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->snapshot_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid snapshot_interval %d"),parm_in);
        } else {
            cam->conf->snapshot_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->snapshot_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
}

static void conf_edit_snapshot_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->snapshot_filename = "%v-%Y%m%d%H%M%S-snapshot";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->snapshot_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->snapshot_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
}

static void conf_edit_movie_output(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_output = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->movie_output, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->movie_output);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
}

static void conf_edit_movie_output_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_output_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->movie_output_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->movie_output_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
}

static void conf_edit_movie_max_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_max_time = 120;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_max_time %d"),parm_in);
        } else {
            cam->conf->movie_max_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->movie_max_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
}

static void conf_edit_movie_bps(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_bps = 400000;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 9999999)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_bps %d"),parm_in);
        } else {
            cam->conf->movie_bps = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->movie_bps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
}

static void conf_edit_movie_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_quality = 60;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_quality %d"),parm_in);
        } else {
            cam->conf->movie_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->movie_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
}

static void conf_edit_movie_codec(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_codec = "mkv";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->movie_codec = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->movie_codec;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_codec",_("movie_codec"));
}

static void conf_edit_movie_passthrough(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_passthrough = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->movie_passthrough, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->movie_passthrough);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
}

static void conf_edit_movie_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_filename = "%v-%Y%m%d%H%M%S";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->movie_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->movie_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
}

static void conf_edit_movie_retain(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_retain = "all";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "all") || (parm == "secondary") )  {
            cam->conf->movie_retain = parm;
        } else if (parm == "") {
            cam->conf->movie_retain = "all";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid movie_retain %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->movie_retain;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"all\",\"secondary\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_retain",_("movie_retain"));
}

static void conf_edit_movie_extpipe_use(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_extpipe_use = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->movie_extpipe_use, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->movie_extpipe_use);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
}

static void conf_edit_movie_extpipe(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->movie_extpipe = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->movie_extpipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->movie_extpipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
}

static void conf_edit_timelapse_interval(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_interval = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_interval %d"),parm_in);
        } else {
            cam->conf->timelapse_interval = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->timelapse_interval);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
}

static void conf_edit_timelapse_mode(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_mode = "daily";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "hourly") || (parm == "daily") ||
            (parm == "weekly-sunday") || (parm == "weekly-monday") ||
            (parm == "monthly") || (parm == "manual"))  {
            cam->conf->timelapse_mode = parm;
        } else if (parm == "") {
            cam->conf->timelapse_mode = "daily";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_mode %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->timelapse_mode;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"hourly\",\"daily\",\"weekly-sunday\"";
        parm = parm + ",\"weekly-monday\",\"monthly\",\"manual\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
}

static void conf_edit_timelapse_fps(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_fps = 30;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 2) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_fps %d"),parm_in);
        } else {
            cam->conf->timelapse_fps = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->timelapse_fps);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
}

static void conf_edit_timelapse_container(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_container = "mpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mpg") || (parm == "mkv"))  {
            cam->conf->timelapse_container = parm;
        } else if (parm == "") {
            cam->conf->timelapse_container = "mpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid timelapse_container %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->timelapse_container;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"mpg\",\"mkv\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_container",_("timelapse_container"));
}

static void conf_edit_timelapse_filename(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->timelapse_filename = "%Y%m%d-timelapse";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->timelapse_filename = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->timelapse_filename;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
}

static void conf_edit_video_pipe(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->video_pipe = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->video_pipe = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->video_pipe;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
}

static void conf_edit_video_pipe_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->video_pipe_motion = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->video_pipe_motion = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->video_pipe_motion;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
}

static void conf_edit_webcontrol_port(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_port %d"),parm_in);
        } else {
            cam->conf->webcontrol_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->webcontrol_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
}

static void conf_edit_webcontrol_ipv6(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_ipv6 = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->webcontrol_ipv6, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->webcontrol_ipv6);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
}

static void conf_edit_webcontrol_localhost(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_localhost = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->webcontrol_localhost, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->webcontrol_localhost);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
}

static void conf_edit_webcontrol_parms(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_parms = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 3)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_parms %d"),parm_in);
        } else {
            cam->conf->webcontrol_parms = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->webcontrol_parms);
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"0\",\"1\",\"2\",\"3\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_parms",_("webcontrol_parms"));
}

static void conf_edit_webcontrol_interface(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_interface = "default";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "default") || (parm == "user"))  {
            cam->conf->webcontrol_interface = parm;
        } else if (parm == "") {
            cam->conf->webcontrol_interface = "default";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_interface %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_interface;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"default\",\"user\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
}

static void conf_edit_webcontrol_auth_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_auth_method = "none";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "none") || (parm == "basic") || (parm == "digest"))  {
            cam->conf->webcontrol_auth_method = parm;
        } else if (parm == "") {
            cam->conf->webcontrol_auth_method = "none";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid webcontrol_auth_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_auth_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"none\",\"basic\",\"digest\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
}

static void conf_edit_webcontrol_authentication(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_authentication = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->webcontrol_authentication = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_authentication;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_authentication",_("webcontrol_authentication"));
}

static void conf_edit_webcontrol_tls(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_tls = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->webcontrol_tls, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->webcontrol_tls);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
}

static void conf_edit_webcontrol_cert(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_cert = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->webcontrol_cert = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_cert;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
}

static void conf_edit_webcontrol_key(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_key = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->webcontrol_key = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_key;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
}

static void conf_edit_webcontrol_headers(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_headers = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->webcontrol_headers = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_headers;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_headers",_("webcontrol_headers"));
}

static void conf_edit_webcontrol_html(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->webcontrol_html = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->webcontrol_html = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->webcontrol_html;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_html",_("webcontrol_html"));
}

static void conf_edit_stream_preview_scale(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_preview_scale = 25;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_scale %d"),parm_in);
        } else {
            cam->conf->stream_preview_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->stream_preview_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_scale",_("stream_preview_scale"));
}

static void conf_edit_stream_preview_newline(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_preview_newline = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->stream_preview_newline, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->stream_preview_newline);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_newline",_("stream_preview_newline"));
}

static void conf_edit_stream_preview_method(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_preview_method = "mjpg";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mjpg") || (parm == "static"))  {
            cam->conf->stream_preview_method = parm;
        } else if (parm == "") {
            cam->conf->stream_preview_method = "mjpg";
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_preview_method %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->stream_preview_method;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"mjpg\",\"static\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_method",_("stream_preview_method"));
}

static void conf_edit_stream_preview_ptz(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_preview_ptz = true;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->stream_preview_ptz, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->stream_preview_ptz);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_ptz",_("stream_preview_ptz"));
}

static void conf_edit_stream_quality(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_quality = 50;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_quality %d"),parm_in);
        } else {
            cam->conf->stream_quality = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->stream_quality);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
}

static void conf_edit_stream_grey(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_grey = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->stream_grey, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->stream_grey);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
}

static void conf_edit_stream_motion(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_motion = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->stream_motion, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->stream_motion);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
}

static void conf_edit_stream_maxrate(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_maxrate = 1;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 100)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_maxrate %d"),parm_in);
        } else {
            cam->conf->stream_maxrate = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->stream_maxrate);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
}

static void conf_edit_stream_scan_time(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_scan_time = 5;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 600)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_scan_time %d"),parm_in);
        } else {
            cam->conf->stream_scan_time = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->stream_scan_time);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_scan_time",_("stream_scan_time"));
}

static void conf_edit_stream_scan_scale(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->stream_scan_scale = 25;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 1000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid stream_scan_scale %d"),parm_in);
        } else {
            cam->conf->stream_scan_scale = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->stream_scan_scale);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_scan_scale",_("stream_scan_scale"));
}

static void conf_edit_database_type(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_type = "";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "mysql") || (parm == "mariadb") || (parm == "") ||
            (parm == "postgresql") || (parm == "sqlite3")) {
            cam->conf->database_type = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_type %s"), parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->database_type;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        parm = parm +  "\"\",\"mysql\",\"mariadb\",\"postgresql\",\"sqlite3\"";
        parm = parm + "]";
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
}

static void conf_edit_database_dbname(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_dbname = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->database_dbname = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->database_dbname;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
}

static void conf_edit_database_host(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_host = "localhost";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->database_host = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->database_host;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
}

static void conf_edit_database_port(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_port = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 65535)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_port %d"),parm_in);
        } else {
            cam->conf->database_port = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->database_port);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
}

static void conf_edit_database_user(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_user = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->database_user = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->database_user;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
}

static void conf_edit_database_password(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_password = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->database_password = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->database_password;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
}

static void conf_edit_database_busy_timeout(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->database_busy_timeout = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 10000)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid database_busy_timeout %d"),parm_in);
        } else {
            cam->conf->database_busy_timeout = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->database_busy_timeout);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
}

static void conf_edit_sql_log_picture(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_log_picture = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->sql_log_picture, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->sql_log_picture);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_picture",_("sql_log_picture"));
}

static void conf_edit_sql_log_snapshot(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_log_snapshot = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->sql_log_snapshot, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->sql_log_snapshot);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_snapshot",_("sql_log_snapshot"));
}

static void conf_edit_sql_log_movie(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_log_movie = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->sql_log_movie, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->sql_log_movie);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_movie",_("sql_log_movie"));
}

static void conf_edit_sql_log_timelapse(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_log_timelapse = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->sql_log_timelapse, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->sql_log_timelapse);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_timelapse",_("sql_log_timelapse"));
}

static void conf_edit_sql_query_start(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query_start = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->sql_query_start = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->sql_query_start;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_start",_("sql_query_start"));
}

static void conf_edit_sql_query_stop(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query_stop = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->sql_query_stop = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->sql_query_stop;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_stop",_("sql_query_stop"));
}

static void conf_edit_sql_query(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->sql_query = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->sql_query = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->sql_query;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query",_("sql_query"));
}

static void conf_edit_ptz_auto_track(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_auto_track = false;
    } else if (pact == PARM_ACT_SET) {
        conf_edit_set_bool(cam->conf->ptz_auto_track, parm);
    } else if (pact == PARM_ACT_GET) {
        conf_edit_get_bool(parm, cam->conf->ptz_auto_track);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_auto_track",_("ptz_auto_track"));
}

static void conf_edit_ptz_wait(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_wait = 0;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 0) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid ptz_wait %d"),parm_in);
        } else {
            cam->conf->ptz_wait = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(cam->conf->ptz_wait);
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_wait",_("ptz_wait"));
}

static void conf_edit_ptz_move_track(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_move_track = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_move_track = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_move_track;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_move_track",_("ptz_move_track"));
}

static void conf_edit_ptz_pan_left(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_pan_left = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_pan_left = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_pan_left;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_left",_("ptz_pan_left"));
}

static void conf_edit_ptz_pan_right(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_pan_right = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_pan_right = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_pan_right;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_pan_right",_("ptz_pan_right"));
}

static void conf_edit_ptz_tilt_up(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_tilt_up = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_tilt_up = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_tilt_up;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_up",_("ptz_tilt_up"));
}

static void conf_edit_ptz_tilt_down(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_tilt_down = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_tilt_down = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_tilt_down;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_tilt_down",_("ptz_tilt_down"));
}

static void conf_edit_ptz_zoom_in(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_zoom_in = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_zoom_in = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_zoom_in;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_in",_("ptz_zoom_in"));
}

static void conf_edit_ptz_zoom_out(struct ctx_cam *cam, std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        cam->conf->ptz_zoom_out = "";
    } else if (pact == PARM_ACT_SET) {
        cam->conf->ptz_zoom_out = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = cam->conf->ptz_zoom_out;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","ptz_zoom_out",_("ptz_zoom_out"));
}

/* Application level parameters */
static void conf_edit_cat00(struct ctx_motapp *motapp, std::string cmd
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (cmd == "daemon") {                         conf_edit_daemon(motapp, parm_val, pact);
    } else if (cmd == "conf_filename") {           conf_edit_conf_filename(motapp, parm_val, pact);
    } else if (cmd == "setup_mode") {              conf_edit_setup_mode(motapp, parm_val, pact);
    } else if (cmd == "pid_file") {                conf_edit_pid_file(motapp, parm_val, pact);
    } else if (cmd == "log_file") {                conf_edit_log_file(motapp, parm_val, pact);
    } else if (cmd == "log_level") {               conf_edit_log_level(motapp, parm_val, pact);
    } else if (cmd == "log_type") {                conf_edit_log_type(motapp, parm_val, pact);
    } else if (cmd == "native_language") {         conf_edit_native_language(motapp, parm_val, pact);
    }

}

static void conf_edit_cat01(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "camera_dir") {                   conf_edit_camera_dir(cam, parm_val, pact);
    } else if (parm_nm == "camera") {                conf_edit_camera(cam, parm_val, pact);
    } else if (parm_nm == "camera_name") {           conf_edit_camera_name(cam, parm_val, pact);
    } else if (parm_nm == "camera_id") {             conf_edit_camera_id(cam, parm_val, pact);
    } else if (parm_nm == "camera_tmo") {            conf_edit_camera_tmo(cam, parm_val, pact);
    } else if (parm_nm == "target_dir") {            conf_edit_target_dir(cam, parm_val, pact);
    } else if (parm_nm == "watchdog_tmo") {          conf_edit_watchdog_tmo(cam, parm_val, pact);
    } else if (parm_nm == "watchdog_kill") {         conf_edit_watchdog_kill(cam, parm_val, pact);
    }

}

static void conf_edit_cat02(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{

    if (parm_nm == "v4l2_device") {                  conf_edit_v4l2_device(cam, parm_val, pact);
    } else if (parm_nm == "v4l2_params") {           conf_edit_v4l2_params(cam, parm_val, pact);
    } else if (parm_nm == "netcam_url") {            conf_edit_netcam_url(cam, parm_val, pact);
    } else if (parm_nm == "netcam_params") {         conf_edit_netcam_params(cam, parm_val, pact);
    } else if (parm_nm == "netcam_high_url") {       conf_edit_netcam_high_url(cam, parm_val, pact);
    } else if (parm_nm == "netcam_high_params") {    conf_edit_netcam_high_params(cam, parm_val, pact);
    } else if (parm_nm == "netcam_userpass") {       conf_edit_netcam_userpass(cam, parm_val, pact);
    } else if (parm_nm == "mmalcam_name") {          conf_edit_mmalcam_name(cam, parm_val, pact);
    } else if (parm_nm == "mmalcam_params") {        conf_edit_mmalcam_params(cam, parm_val, pact);
    }

}

static void conf_edit_cat03(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "width") {                          conf_edit_width(cam, parm_val, pact);
    } else if (parm_nm == "height") {                  conf_edit_height(cam, parm_val, pact);
    } else if (parm_nm == "framerate") {               conf_edit_framerate(cam, parm_val, pact);
    } else if (parm_nm == "minimum_frame_time") {      conf_edit_minimum_frame_time(cam, parm_val, pact);
    } else if (parm_nm == "rotate") {                  conf_edit_rotate(cam, parm_val, pact);
    } else if (parm_nm == "flip_axis") {               conf_edit_flip_axis(cam, parm_val, pact);
    }

}

static void conf_edit_cat04(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "locate_motion_mode") {             conf_edit_locate_motion_mode(cam, parm_val, pact);
    } else if (parm_nm == "locate_motion_style") {     conf_edit_locate_motion_style(cam, parm_val, pact);
    } else if (parm_nm == "text_left") {               conf_edit_text_left(cam, parm_val, pact);
    } else if (parm_nm == "text_right") {              conf_edit_text_right(cam, parm_val, pact);
    } else if (parm_nm == "text_changes") {            conf_edit_text_changes(cam, parm_val, pact);
    } else if (parm_nm == "text_scale") {              conf_edit_text_scale(cam, parm_val, pact);
    } else if (parm_nm == "text_event") {              conf_edit_text_event(cam, parm_val, pact);
    }

}

static void conf_edit_cat05(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "emulate_motion") {                 conf_edit_emulate_motion(cam, parm_val, pact);
    } else if (parm_nm == "threshold") {               conf_edit_threshold(cam, parm_val, pact);
    } else if (parm_nm == "threshold_maximum") {       conf_edit_threshold_maximum(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevx") {         conf_edit_threshold_sdevx(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevy") {         conf_edit_threshold_sdevy(cam, parm_val, pact);
    } else if (parm_nm == "threshold_sdevxy") {        conf_edit_threshold_sdevxy(cam, parm_val, pact);
    } else if (parm_nm == "threshold_ratio") {         conf_edit_threshold_ratio(cam, parm_val, pact);
    } else if (parm_nm == "threshold_ratio_change") {  conf_edit_threshold_ratio_change(cam, parm_val, pact);
    } else if (parm_nm == "threshold_tune") {          conf_edit_threshold_tune(cam, parm_val, pact);
    } else if (parm_nm == "secondary_interval") {      conf_edit_secondary_interval(cam, parm_val, pact);
    } else if (parm_nm == "secondary_method") {        conf_edit_secondary_method(cam, parm_val, pact);
    } else if (parm_nm == "secondary_params") {        conf_edit_secondary_params(cam, parm_val, pact);
    }

}

static void conf_edit_cat06(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "noise_level") {                    conf_edit_noise_level(cam, parm_val, pact);
    } else if (parm_nm == "noise_tune") {              conf_edit_noise_tune(cam, parm_val, pact);
    } else if (parm_nm == "despeckle_filter") {        conf_edit_despeckle_filter(cam, parm_val, pact);
    } else if (parm_nm == "area_detect") {             conf_edit_area_detect(cam, parm_val, pact);
    } else if (parm_nm == "mask_file") {               conf_edit_mask_file(cam, parm_val, pact);
    } else if (parm_nm == "mask_privacy") {            conf_edit_mask_privacy(cam, parm_val, pact);
    } else if (parm_nm == "smart_mask_speed") {        conf_edit_smart_mask_speed(cam, parm_val, pact);
    }

}

static void conf_edit_cat07(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "lightswitch_percent") {            conf_edit_lightswitch_percent(cam, parm_val, pact);
    } else if (parm_nm == "lightswitch_frames") {      conf_edit_lightswitch_frames(cam, parm_val, pact);
    } else if (parm_nm == "minimum_motion_frames") {   conf_edit_minimum_motion_frames(cam, parm_val, pact);
    } else if (parm_nm == "static_object_time") {      conf_edit_static_object_time(cam, parm_val, pact);
    } else if (parm_nm == "event_gap") {               conf_edit_event_gap(cam, parm_val, pact);
    } else if (parm_nm == "pre_capture") {             conf_edit_pre_capture(cam, parm_val, pact);
    } else if (parm_nm == "post_capture") {            conf_edit_post_capture(cam, parm_val, pact);
    }

}

static void conf_edit_cat08(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "on_event_start") {                 conf_edit_on_event_start(cam, parm_val, pact);
    } else if (parm_nm == "on_event_end") {            conf_edit_on_event_end(cam, parm_val, pact);
    } else if (parm_nm == "on_picture_save") {         conf_edit_on_picture_save(cam, parm_val, pact);
    } else if (parm_nm == "on_area_detected") {        conf_edit_on_area_detected(cam, parm_val, pact);
    } else if (parm_nm == "on_motion_detected") {      conf_edit_on_motion_detected(cam, parm_val, pact);
    } else if (parm_nm == "on_movie_start") {          conf_edit_on_movie_start(cam, parm_val, pact);
    } else if (parm_nm == "on_movie_end") {            conf_edit_on_movie_end(cam, parm_val, pact);
    } else if (parm_nm == "on_camera_lost") {          conf_edit_on_camera_lost(cam, parm_val, pact);
    } else if (parm_nm == "on_camera_found") {         conf_edit_on_camera_found(cam, parm_val, pact);
    } else if (parm_nm == "on_secondary_detect") {     conf_edit_on_secondary_detect(cam, parm_val, pact);
    }

}

static void conf_edit_cat09(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "picture_output") {                 conf_edit_picture_output(cam, parm_val, pact);
    } else if (parm_nm == "picture_output_motion") {   conf_edit_picture_output_motion(cam, parm_val, pact);
    } else if (parm_nm == "picture_type") {            conf_edit_picture_type(cam, parm_val, pact);
    } else if (parm_nm == "picture_quality") {         conf_edit_picture_quality(cam, parm_val, pact);
    } else if (parm_nm == "picture_exif") {            conf_edit_picture_exif(cam, parm_val, pact);
    } else if (parm_nm == "picture_filename") {        conf_edit_picture_filename(cam, parm_val, pact);
    } else if (parm_nm == "snapshot_interval") {       conf_edit_snapshot_interval(cam, parm_val, pact);
    } else if (parm_nm == "snapshot_filename") {       conf_edit_snapshot_filename(cam, parm_val, pact);
    }

}

static void conf_edit_cat10(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "movie_output") {                   conf_edit_movie_output(cam, parm_val, pact);
    } else if (parm_nm == "movie_output_motion") {     conf_edit_movie_output_motion(cam, parm_val, pact);
    } else if (parm_nm == "movie_max_time") {          conf_edit_movie_max_time(cam, parm_val, pact);
    } else if (parm_nm == "movie_bps") {               conf_edit_movie_bps(cam, parm_val, pact);
    } else if (parm_nm == "movie_quality") {           conf_edit_movie_quality(cam, parm_val, pact);
    } else if (parm_nm == "movie_codec") {             conf_edit_movie_codec(cam, parm_val, pact);
    } else if (parm_nm == "movie_passthrough") {       conf_edit_movie_passthrough(cam, parm_val, pact);
    } else if (parm_nm == "movie_filename") {          conf_edit_movie_filename(cam, parm_val, pact);
    } else if (parm_nm == "movie_retain") {            conf_edit_movie_retain(cam, parm_val, pact);
    } else if (parm_nm == "movie_extpipe_use") {       conf_edit_movie_extpipe_use(cam, parm_val, pact);
    } else if (parm_nm == "movie_extpipe") {           conf_edit_movie_extpipe(cam, parm_val, pact);
    }

}

static void conf_edit_cat11(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "timelapse_interval") {             conf_edit_timelapse_interval(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_mode") {          conf_edit_timelapse_mode(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_fps") {           conf_edit_timelapse_fps(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_container") {     conf_edit_timelapse_container(cam, parm_val, pact);
    } else if (parm_nm == "timelapse_filename") {      conf_edit_timelapse_filename(cam, parm_val, pact);
    }

}

static void conf_edit_cat12(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "video_pipe") {                     conf_edit_video_pipe(cam, parm_val, pact);
    } else if (parm_nm == "video_pipe_motion") {       conf_edit_video_pipe_motion(cam, parm_val, pact);
    }

}

static void conf_edit_cat13(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "webcontrol_port") {                    conf_edit_webcontrol_port(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_ipv6") {             conf_edit_webcontrol_ipv6(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_localhost") {        conf_edit_webcontrol_localhost(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_parms") {            conf_edit_webcontrol_parms(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_interface") {        conf_edit_webcontrol_interface(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_auth_method") {      conf_edit_webcontrol_auth_method(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_authentication") {   conf_edit_webcontrol_authentication(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_tls") {              conf_edit_webcontrol_tls(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_cert") {             conf_edit_webcontrol_cert(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_key") {              conf_edit_webcontrol_key(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_headers") {          conf_edit_webcontrol_headers(cam, parm_val, pact);
    } else if (parm_nm == "webcontrol_html") {             conf_edit_webcontrol_html(cam, parm_val, pact);
    }

}

static void conf_edit_cat14(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "stream_preview_scale") {               conf_edit_stream_preview_scale(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_newline") {      conf_edit_stream_preview_newline(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_method") {       conf_edit_stream_preview_method(cam, parm_val, pact);
    } else if (parm_nm == "stream_preview_ptz") {          conf_edit_stream_preview_ptz(cam, parm_val, pact);
    } else if (parm_nm == "stream_quality") {              conf_edit_stream_quality(cam, parm_val, pact);
    } else if (parm_nm == "stream_grey") {                 conf_edit_stream_grey(cam, parm_val, pact);
    } else if (parm_nm == "stream_motion") {               conf_edit_stream_motion(cam, parm_val, pact);
    } else if (parm_nm == "stream_maxrate") {              conf_edit_stream_maxrate(cam, parm_val, pact);
    } else if (parm_nm == "stream_scan_time") {            conf_edit_stream_scan_time(cam, parm_val, pact);
    } else if (parm_nm == "stream_scan_scale") {           conf_edit_stream_scan_scale(cam, parm_val, pact);
    }

}

static void conf_edit_cat15(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "database_type") {                  conf_edit_database_type(cam, parm_val, pact);
    } else if (parm_nm == "database_dbname") {         conf_edit_database_dbname(cam, parm_val, pact);
    } else if (parm_nm == "database_host") {           conf_edit_database_host(cam, parm_val, pact);
    } else if (parm_nm == "database_port") {           conf_edit_database_port(cam, parm_val, pact);
    } else if (parm_nm == "database_user") {           conf_edit_database_user(cam, parm_val, pact);
    } else if (parm_nm == "database_password") {       conf_edit_database_password(cam, parm_val, pact);
    } else if (parm_nm == "database_busy_timeout") {   conf_edit_database_busy_timeout(cam, parm_val, pact);
    }

}

static void conf_edit_cat16(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "sql_log_picture") {                conf_edit_sql_log_picture(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_snapshot") {        conf_edit_sql_log_snapshot(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_movie") {           conf_edit_sql_log_movie(cam, parm_val, pact);
    } else if (parm_nm == "sql_log_timelapse") {       conf_edit_sql_log_timelapse(cam, parm_val, pact);
    } else if (parm_nm == "sql_query_start") {         conf_edit_sql_query_start(cam, parm_val, pact);
    } else if (parm_nm == "sql_query_stop") {          conf_edit_sql_query_stop(cam, parm_val, pact);
    } else if (parm_nm == "sql_query") {               conf_edit_sql_query(cam, parm_val, pact);
    }

}

static void conf_edit_cat17(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact)
{
    if (parm_nm == "ptz_auto_track") {           conf_edit_ptz_auto_track(cam, parm_val, pact);
    } else if (parm_nm == "ptz_wait") {          conf_edit_ptz_wait(cam, parm_val, pact);
    } else if (parm_nm == "ptz_move_track") {    conf_edit_ptz_move_track(cam, parm_val, pact);
    } else if (parm_nm == "ptz_pan_left") {      conf_edit_ptz_pan_left(cam, parm_val, pact);
    } else if (parm_nm == "ptz_pan_right") {     conf_edit_ptz_pan_right(cam, parm_val, pact);
    } else if (parm_nm == "ptz_tilt_up") {       conf_edit_ptz_tilt_up(cam, parm_val, pact);
    } else if (parm_nm == "ptz_tilt_down") {     conf_edit_ptz_tilt_down(cam, parm_val, pact);
    } else if (parm_nm == "ptz_zoom_in") {       conf_edit_ptz_zoom_in(cam, parm_val, pact);
    } else if (parm_nm == "ptz_zoom_out") {      conf_edit_ptz_zoom_out(cam, parm_val, pact);
    }

}

static void conf_edit_cat(struct ctx_cam *cam, std::string parm_nm
        , std::string &parm_val, enum PARM_ACT pact, enum PARM_CAT pcat)
{

    if (pcat == PARM_CAT_01) {          conf_edit_cat01(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_02) {   conf_edit_cat02(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_03) {   conf_edit_cat03(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_04) {   conf_edit_cat04(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_05) {   conf_edit_cat05(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_06) {   conf_edit_cat06(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_07) {   conf_edit_cat07(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_08) {   conf_edit_cat08(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_09) {   conf_edit_cat09(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_10) {   conf_edit_cat10(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_11) {   conf_edit_cat11(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_12) {   conf_edit_cat12(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_13) {   conf_edit_cat13(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_14) {   conf_edit_cat14(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_15) {   conf_edit_cat15(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_16) {   conf_edit_cat16(cam,parm_nm, parm_val, pact);
    } else if (pcat == PARM_CAT_17) {   conf_edit_cat17(cam,parm_nm, parm_val, pact);

    }

}


static void conf_edit_dflt_app(struct ctx_motapp *motapp)
{
    std::string dflt = "";

    motapp->parms_changed = false;
    conf_edit_conf_filename(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_type(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_pid_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_daemon(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_setup_mode(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_pid_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_file(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_level(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_log_type(motapp, dflt, PARM_ACT_DFLT);
    conf_edit_native_language(motapp, dflt, PARM_ACT_DFLT);

}

static void conf_edit_dflt_cam(struct ctx_cam *cam)
{
    int indx;
    enum PARM_CAT pcat;
    std::string dflt = "";

    cam->parms_changed = false;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        pcat = config_parms[indx].parm_cat;
        if ((config_parms[indx].parm_cat != PARM_CAT_00)) {
            conf_edit_cat(cam, config_parms[indx].parm_name, dflt, PARM_ACT_DFLT, pcat);
        }
        indx++;
    }

}

int conf_edit_set_active(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        , std::string parm_nm, std::string parm_val)
{
    int indx;
    enum PARM_CAT pcat;

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (parm_nm ==  config_parms[indx].parm_name) {
            pcat = config_parms[indx].parm_cat;
            if ((ismotapp == true) && (pcat == PARM_CAT_00)) {
                motapp->parms_changed = true;
                conf_edit_cat00(motapp, parm_nm, parm_val, PARM_ACT_SET);

            } else if ((ismotapp == false) && (pcat != PARM_CAT_00)) {
                motapp->cam_list[threadnbr]->parms_changed = true;
                conf_edit_cat(motapp->cam_list[threadnbr], parm_nm
                    , parm_val, PARM_ACT_SET, pcat);

            }
            return 0;
        }
        indx++;
    }
    return -1;

}

static void conf_edit_depr_vid(struct ctx_motapp *motapp, int threadnbr
        , std::string parm_nm, std::string newname, std::string parm_val)
{
    std::string parm_curr, parm_new;

    conf_edit_v4l2_params(motapp->cam_list[threadnbr], parm_curr, PARM_ACT_GET);
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
    conf_edit_set_active(motapp, false, threadnbr, newname, parm_new);

}

static void conf_edit_depr_web(struct ctx_motapp *motapp, int threadnbr
        , std::string newname, std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "0";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(motapp, false, threadnbr, newname, parm_new);
}

static void conf_edit_depr_tdbl(struct ctx_motapp *motapp, int threadnbr
        , std::string newname, std::string &parm_val)
{
    std::string parm_new;

    if ((parm_val == "1") || (parm_val == "yes") || (parm_val == "on")) {
        parm_new = "2";
    } else {
        parm_new = "1";
    }
    conf_edit_set_active(motapp, false, threadnbr, newname, parm_new);
}


static int conf_edit_set_depr(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        ,std::string &parm_nm, std::string &parm_val)
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
                conf_edit_depr_vid(motapp, threadnbr, parm_nm, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "webcontrol_html_output")) {
                conf_edit_depr_web(motapp, threadnbr, config_parms_depr[indx].newname, parm_val);

            } else if ((config_parms_depr[indx].parm_name == "text_double")) {
                conf_edit_depr_tdbl(motapp, threadnbr, config_parms_depr[indx].newname, parm_val);

            } else {
                conf_edit_set_active(motapp, ismotapp, threadnbr, config_parms_depr[indx].newname, parm_val);
            }
            return 0;
        }
        indx++;
    }
    return -1;
}

void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat)
{
    if (parm_cat == PARM_CAT_00) {
        conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_GET);
    } else {
        conf_edit_cat(cam, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
    }

}

/* Interim overload until webu goes to c++ with std::string */
void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, char *parm_chr, enum PARM_CAT parm_cat)
{
    std::string parm_val(parm_chr);
    int retcd;

    if (parm_cat == PARM_CAT_00) {
        conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_GET);
    } else {
        conf_edit_cat(cam, parm_nm, parm_val, PARM_ACT_GET, parm_cat);
    }

    retcd = snprintf(parm_chr, PATH_MAX, "%s", parm_val.c_str());
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error getting value for \"%s\""), parm_nm.c_str());
        parm_chr[0] = '\0';
    }

}

/* parm_name and parm_val as string */
void conf_edit_set(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        ,std::string parm_nm, std::string parm_val)
{
    if (conf_edit_set_active(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (conf_edit_set_depr(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (ismotapp == false) {
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

/* parm_name as char pointer and parm_val as string */
void conf_edit_set(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        ,const char *parm_nm_chr, std::string parm_val)
{
    std::string parm_nm(parm_nm_chr);

    if (conf_edit_set_active(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (conf_edit_set_depr(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (ismotapp == false) {
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

/* parm_name as string and parm_val as char pointer */
void conf_edit_set(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        ,std::string parm_nm, const char *parm_val_chr)
{
    std::string parm_val(parm_val_chr);

    if (conf_edit_set_active(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (conf_edit_set_depr(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (ismotapp == false) {
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

/* parm_name and parm_val as char pointers */
void conf_edit_set(struct ctx_motapp *motapp, bool ismotapp, int threadnbr
        ,const char *parm_nm_chr, const char *parm_val_chr)
{
    std::string parm_val(parm_val_chr);
    std::string parm_nm(parm_nm_chr);

    if (conf_edit_set_active(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (conf_edit_set_depr(motapp, ismotapp, threadnbr, parm_nm, parm_val) == 0) {
        return;
    }

    if (ismotapp == false) {
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
    }

}

void conf_edit_list(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat)
{
    if (parm_cat == PARM_CAT_00) {
        conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_LIST);
    } else {
        conf_edit_cat(cam, parm_nm, parm_val, PARM_ACT_LIST, parm_cat);
    }

}

/* Interim overload until webu goes to c++ with std::string */
void conf_edit_list(struct ctx_cam *cam, std::string parm_nm, char *parm_chr, enum PARM_CAT parm_cat)
{
    std::string parm_val(parm_chr);
    int retcd;

    if (parm_cat == PARM_CAT_00) {
        conf_edit_cat00(cam->motapp, parm_nm, parm_val, PARM_ACT_LIST);
    } else {
        conf_edit_cat(cam, parm_nm, parm_val, PARM_ACT_LIST, parm_cat);
    }

    retcd = snprintf(parm_chr, PATH_MAX, "%s", parm_val.c_str());
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error getting value for \"%s\""), parm_nm.c_str());
        parm_chr[0] = '\0';
    }

}

std::string conf_type_desc(enum PARM_TYP ptype)
{
    if (ptype == PARM_TYP_BOOL) {           return "bool";
    } else if (ptype == PARM_TYP_INT) {     return "int";
    } else if (ptype == PARM_TYP_LIST) {    return "list";
    } else if (ptype == PARM_TYP_STRING) {  return "string";
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
        } else { return "Other";
        }
    }
}

/** Prints usage and options allowed from Command-line. */
static void usage(void)
{
    printf("MotionPlus version %s, Copyright 2020\n",PACKAGE_VERSION);
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
static void conf_cmdline(struct ctx_motapp *motapp)
{
    int c;

    while ((c = getopt(motapp->argc, motapp->argv, "bc:d:hmns?p:k:l:")) != EOF)
        switch (c) {
        case 'c':
            conf_edit_set(motapp, true, 0, "conf_filename", optarg);
            break;
        case 'b':
            conf_edit_set(motapp, true, 0, "daemon", "on");
            break;
        case 'n':
            conf_edit_set(motapp, true, 0, "daemon", "off");
            break;
        case 's':
            conf_edit_set(motapp, true, 0, "setup_mode", "on");
            break;
        case 'd':
            conf_edit_set(motapp, true, 0, "log_level", optarg);
            break;
        case 'k':
            conf_edit_set(motapp, true, 0, "log_type", optarg);
            break;
        case 'p':
            conf_edit_set(motapp, true, 0, "pid_file", optarg);
            break;
        case 'l':
            conf_edit_set(motapp, true, 0, "log_file", optarg);
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
static void conf_camera_filenm(struct ctx_motapp *motapp)
{
    int indx_cam, indx, retcd;
    std::string src_nm, fullnm;
    FILE *fp;

    src_nm = motapp->conf_filename.substr(0, motapp->conf_filename.find_last_of("/") + 1);

    indx = 1;
    while (true) {
        fullnm = src_nm + "camera" + std::to_string(indx) + ".conf";
        /*
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, _("Trying %s"), fullnm.c_str());
        */

        indx_cam = 0;
        while (motapp->cam_list[indx_cam] != NULL) {
            if (fullnm == motapp->cam_list[indx_cam]->conf_filename) {
                break;
            }
            indx_cam++;
        }
        if (motapp->cam_list[indx_cam] == NULL) {
            fp = fopen(fullnm.c_str(), "r");
            if (fp) {
                myfclose(fp);
            } else {
                break;
            }
        }
        indx++;
    }
    /* Name does not match others nor is an existing file */
    indx_cam = 0;
    while (motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }

    retcd = snprintf(motapp->cam_list[indx_cam-1]->conf_filename, PATH_MAX, "%s", fullnm.c_str());
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Error with config file name %s"), fullnm.c_str());
        motapp->cam_list[indx_cam-1]->conf_filename[0] = '\0';
    }

}

void conf_camera_add(struct ctx_motapp *motapp)
{
    int indx_cams, indx;
    std::string parm_val;

    /* Find the number of cameras defined. */
    indx_cams = 0;
    while (motapp->cam_list[indx_cams] != NULL) {
        indx_cams++;
    };

    /* Index starts at zero (+1) plus another for our new camera(+2)*/
    motapp->cam_list = (struct ctx_cam **)myrealloc(
        motapp->cam_list, sizeof(struct ctx_cam *) * (indx_cams + 2), "config_camera");

    motapp->cam_list[indx_cams] = new ctx_cam;
    memset(motapp->cam_list[indx_cams],0,sizeof(struct ctx_cam));

    motapp->cam_list[indx_cams]->conf = new ctx_config;

    motapp->cam_list[indx_cams + 1] = NULL;

    motapp->cam_list[indx_cams]->motapp = motapp;

    conf_edit_dflt_cam(motapp->cam_list[indx_cams]);

    indx = 0;
    while (config_parms[indx].parm_name != "") {
        if (mystrne(config_parms[indx].parm_name.c_str(),"camera_id")) {
            conf_edit_get(motapp->cam_list[0], config_parms[indx].parm_name
                , parm_val, config_parms[indx].parm_cat);

            conf_edit_set(motapp, false, indx_cams, config_parms[indx].parm_name, parm_val);
        }
        indx++;
    }

    conf_camera_filenm(motapp);

}

static void conf_parm_camera(struct ctx_motapp *motapp, std::string filename)
{
    int indx_cam, retcd;
    FILE *fp;

    fp = fopen(filename.c_str(), "r");
    if (!fp) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera config file %s not found"), filename.c_str());
        return;
    }

    conf_camera_add(motapp);

    /* Find the number of cameras defined. */
    indx_cam = 0;
    while (motapp->cam_list[indx_cam] != NULL) {
        indx_cam++;
    }
    indx_cam--;

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Processing camera config file %s"), filename.c_str());

    retcd = snprintf(motapp->cam_list[indx_cam]->conf_filename, PATH_MAX, "%s", filename.c_str());
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            ,_("Error using config file  name %s"), filename.c_str());
        motapp->cam_list[indx_cam]->conf_filename[0] = '\0';
    };

    conf_process(motapp, false, fp, indx_cam);

    myfclose(fp);

    return;
}

/** Process camera_dir */
static void conf_parm_camera_dir(struct ctx_motapp *motapp, std::string str)
{
    DIR *dp;
    struct dirent *ep;
    size_t name_len;
    int i, retcd;

    char conf_file[PATH_MAX];

    dp = opendir(str.c_str());
    if (dp != NULL) {
        while( (ep = readdir(dp)) ) {
            name_len = strlen(ep->d_name);
            if ((name_len > strlen(".conf")) &&
                (mystreq(".conf", ep->d_name + name_len - strlen(".conf")))) {

                memset(conf_file, '\0', sizeof(conf_file));
                retcd = snprintf(conf_file, sizeof(conf_file) - 1, "%s/%s",
                            str.c_str(), ep->d_name);
                if ((retcd < 0) || (retcd >= PATH_MAX)) {
                    conf_file[0] = '\0';
                    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                        ,_("Error processing config dir %s"), ep->d_name);
                } else {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        ,_("Processing config file %s"), conf_file );
                    conf_parm_camera(motapp, conf_file);
                    /* The last ctx_cam thread would be ours,
                    * set it as created from conf directory.
                    */
                    i = 0;
                    while (motapp->cam_list[++i]);
                    motapp->cam_list[i-1]->from_conf_dir = true;
                }
            }
        }
        closedir(dp);
    } else {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera directory config %s not found"), str.c_str());
    }

    /* Store the given config value to allow writing it out */
    conf_edit_set(motapp, false, 0, "camera_dir", str);

    return;
}

/** Process each line from the config file. */
void conf_process(struct ctx_motapp *motapp, bool ismotapp,  FILE *fp, int threadnbr)
{

    char line[PATH_MAX], *cmd = NULL, *arg1 = NULL;
    char *beg = NULL, *end = NULL;
    std::string parm_nm, parm_val;

    while (fgets(line, PATH_MAX-1, fp)) {
        if (!(line[0] == '#' || line[0] == ';' || strlen(line) <  2)) {
            arg1 = NULL;

            /* Trim white space and any CR or LF at the end of the line. */
            end = line + strlen(line) - 1; /* Point to the last non-null character in the string. */
            while (end >= line && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
                end--;
            }
            *(end+1) = '\0';

            if (strlen(line) == 0) {
                continue;
            }

            /* Trim leading whitespace from the line and find command. */
            beg = line;
            while (*beg == ' ' || *beg == '\t') {
                beg++;
            }
            cmd = beg; /* Command starts here. */

            while (*beg != ' ' && *beg != '\t' && *beg != '=' && *beg != '\0') {
                beg++;
            }
            *beg = '\0'; /* Command string terminates here. */

            /* Trim space between command and argument. */
            beg++;

            if (strlen(beg) > 0) {
                while (*beg == ' ' || *beg == '\t' || *beg == '=' || *beg == '\n' || *beg == '\r') {
                    beg++;
                }

                /* Strip quotes from around arg */
                if ((beg[0] == '"' && beg[strlen(beg)-1] == '"') ||
                    (beg[0] == '\'' && beg[strlen(beg)-1] == '\'')) {
                    beg[strlen(beg)-1] = '\0';
                    beg++;
                }

                arg1 = beg; /* Argument starts here */
            }
            /* Ignore camera/dir in sub files */
            parm_nm = std::string(cmd);
            if (arg1 == NULL) {
                parm_val= std::string("");
            } else {
                parm_val= std::string(arg1);
            }

            if (mystreq(cmd,"camera_dir") && (threadnbr == 0) && (ismotapp == false) ) {
                conf_parm_camera_dir(motapp, parm_val);
            } else if (mystreq(cmd,"camera") && (threadnbr == 0) && (ismotapp == false)) {
                conf_parm_camera(motapp, parm_val);
            } else if (mystrne(cmd,"camera") && mystrne(cmd,"camera_dir")) {
                conf_edit_set(motapp, ismotapp, threadnbr, parm_nm, parm_val);
            }
        }
    }

    return;
}

/**  Write the configuration(s) to the log */
void conf_parms_log(struct ctx_cam **cam_list)
{
    int i, threadnbr;
    bool diff_val;
    std::string parm_val, parm_main;

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("Logging configuration parameters from all files"));
    threadnbr = 0;
    while (cam_list[threadnbr]!= NULL) {
        motion_log(INF, TYPE_ALL, NO_ERRNO,0
            ,_("Camera %d - Config file: %s"), threadnbr, cam_list[threadnbr]->conf_filename);
        i = 0;
        while (config_parms[i].parm_name != "") {
            diff_val = true;
            conf_edit_get(cam_list[threadnbr], config_parms[i].parm_name
                , parm_val ,config_parms[i].parm_cat);
            if (threadnbr > 0) {
                conf_edit_get(cam_list[0], config_parms[i].parm_name
                    , parm_main ,config_parms[i].parm_cat);
                if (parm_val == parm_main) {
                    diff_val = false;
                }
            }
            if (diff_val) {
                if ((config_parms[i].parm_name == "netcam_url") ||
                    (config_parms[i].parm_name == "netcam_userpass") ||
                    (config_parms[i].parm_name == "netcam_high_url") ||
                    (config_parms[i].parm_name == "webcontrol_authentication") ||
                    (config_parms[i].parm_name == "webcontrol_key") ||
                    (config_parms[i].parm_name == "webcontrol_cert") ||
                    (config_parms[i].parm_name == "database_user") ||
                    (config_parms[i].parm_name == "database_password"))
                {
                    motion_log(INF, TYPE_ALL, NO_ERRNO,0
                        ,_("%-25s <redacted>"), config_parms[i].parm_name.c_str());
                } else {
                    if ((config_parms[i].parm_name.compare(0,4,"text") == 0) ||
                        (parm_val.compare(0,1, " ") != 0)) {
                        motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s %s"
                            , config_parms[i].parm_name.c_str(), parm_val.c_str());
                    } else {
                        motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s \"%s\""
                            , config_parms[i].parm_name.c_str(), parm_val.c_str());
                    }
                }
            }
            i++;
        }
        threadnbr++;
    }
}

/**  Write the configuration(s) to file */
void conf_parms_write(struct ctx_motapp *motapp)
{
    std::string parm_val, parm_main;
    int indx, indx_cam, indx_next;
    char timestamp[32];
    FILE *conffile;

    indx_cam = 0;
    while (motapp->cam_list[indx_cam] != NULL) {

        conffile = myfopen(motapp->cam_list[indx_cam]->conf_filename, "w");

        if (!conffile) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to write configuration to %s")
                , motapp->cam_list[indx_cam]->conf_filename);
            indx_cam++;
            continue;
        }

        time_t now = time(0);
        strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

        fprintf(conffile, "; %s\n", motapp->cam_list[indx_cam]->conf_filename);
        fprintf(conffile, ";\n; This config file was generated by MotionPlus " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");

        indx = 0;
        indx_next = -1;
        while (config_parms[indx].parm_name != "") {
            conf_edit_get(motapp->cam_list[indx_cam], config_parms[indx].parm_name
                    , parm_val, config_parms[indx].parm_cat);
            conf_edit_get(motapp->cam_list[0], config_parms[indx].parm_name
                    , parm_main, config_parms[indx].parm_cat);

            if ((config_parms[indx].parm_name != "camera") &&
                (config_parms[indx].parm_name != "camera_dir") &&
                (config_parms[indx].parm_name != "conf_filename") &&
                ((indx_cam == 0) || ((parm_val != parm_main) &&
                 (config_parms[indx].parm_cat != PARM_CAT_13)))) {

                if (indx_next != (int)config_parms[indx].parm_cat) {
                    fprintf(conffile,"\n%s",";*************************************************\n");
                    fprintf(conffile,"%s%s\n", ";*****   "
                        , conf_cat_desc(config_parms[indx].parm_cat,false).c_str());
                    fprintf(conffile,"%s",";*************************************************\n");
                    indx_next = (int)config_parms[indx].parm_cat;
                }

                /* If there are embedded spaces, enclose in quotes */
                if (parm_val.compare(0, 1, " ") == 0) {
                    fprintf(conffile, "%s \"%s\"\n"
                        , config_parms[indx].parm_name.c_str(), parm_val.c_str());
                } else {
                    fprintf(conffile, "%s %s\n"
                        , config_parms[indx].parm_name.c_str(), parm_val.c_str());
                }
            }
            indx++;
        }

        /* Write out the camera_dir and camera last */
        if (indx_cam == 0) {
            fprintf(conffile, "%s","\n");
            indx = 0;
            while (config_parms[indx].parm_name != "") {
                if (config_parms[indx].parm_name == "camera_dir") {
                    conf_edit_get(motapp->cam_list[indx_cam], config_parms[indx].parm_name
                        , parm_val, config_parms[indx].parm_cat);
                    if (parm_val == "") {
                        parm_val = motapp->conf_filename;
                        parm_val = parm_val.substr(0, parm_val.find_last_of("/")+1) + "conf.d";
                    }
                    if (parm_val.compare(0, 1, " ") == 0) {
                        fprintf(conffile, "%s \"%s\"\n"
                            , config_parms[indx].parm_name.c_str(), parm_val.c_str());
                    } else {
                        fprintf(conffile, "%s %s\n"
                            , config_parms[indx].parm_name.c_str(), parm_val.c_str());
                    }
                }
                indx++;
            }
            if (motapp->cam_list[0]->from_conf_dir == false) {
                fprintf(conffile, "%s","\n");
                indx = 1;
                while (motapp->cam_list[indx] != NULL) {
                    if (parm_val.compare(0, 1, " ") == 0) {
                        fprintf(conffile, "camera \"%s\"\n"
                            , motapp->cam_list[indx]->conf_filename);
                    } else {
                        fprintf(conffile, "camera %s\n"
                            , motapp->cam_list[indx]->conf_filename);
                    }
                    indx++;
                }
            }
        }

        fprintf(conffile, "\n");
        myfclose(conffile);
        conffile = NULL;

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Configuration written to %s")
            , motapp->cam_list[indx_cam]->conf_filename);

        indx_cam++;
    }

}

void conf_init_app(struct ctx_motapp *motapp, int argc, char *argv[])
{
    FILE *fp = NULL;
    std::string filename;
    char path[PATH_MAX];

    motapp->argc = argc;
    motapp->argv = argv;

    conf_edit_dflt_app(motapp);

    conf_cmdline(motapp); /* Get the filename if provided */

    if (motapp->conf_filename != "") {
        filename = motapp->conf_filename;
        fp = fopen (filename.c_str(), "r");
    }

    if (!fp) {
        if (getcwd(path, sizeof(path)) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error getcwd"));
            exit(-1);
        }
        filename = path + std::string("/motionplus.conf");
        fp = fopen (filename.c_str(), "r");
    }

    if (!fp) {
        filename = std::string(getenv("HOME")) + std::string("/.motionplus/motionplus.conf");
        fp = fopen(filename.c_str(), "r");
    }

    if (!fp) {
        filename = std::string( sysconfdir ) + std::string("/motionplus.conf");
        fp = fopen(filename.c_str(), "r");
    }

    if (!fp) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("could not open configfile %s"), filename.c_str());
    }

    /* Now we process the motionplus.conf config file and close it. */
    if (fp) {
        conf_edit_set(motapp, true, 0, "conf_filename", filename);

        conf_process(motapp, true, fp, 0);

        myfclose(fp);

        conf_cmdline(motapp);

    } else {
        MOTION_LOG(CRT, TYPE_ALL, NO_ERRNO
            ,_("No config file to process, using default values"));
    }

    return;
}

void conf_init_cams(struct ctx_motapp *motapp)
{
    FILE *fp = NULL;
    int indx, retcd;

    motapp->cam_list = (struct ctx_cam**)calloc(sizeof(struct ctx_cam *), 2);
    motapp->cam_list[0] = new ctx_cam;
    memset(motapp->cam_list[0],0,sizeof(struct ctx_cam));

    motapp->cam_list[1] = NULL;

    motapp->cam_list[0]->motapp = motapp;
    motapp->cam_list[0]->conf = new ctx_config;

    conf_edit_dflt_cam(motapp->cam_list[0]);

    if (motapp->conf_filename != "") {
        retcd = snprintf(motapp->cam_list[0]->conf_filename
                , PATH_MAX, "%s", motapp->conf_filename.c_str());
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            motapp->cam_list[0]->conf_filename[0] = '\0';
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                ,_("Error using config file %s"), motapp->conf_filename.c_str());
        } else {
            fp = fopen (motapp->conf_filename.c_str(), "r");
        }
    }

    if (fp) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Processing thread 0 - config file %s"), motapp->conf_filename.c_str());
        conf_process(motapp, false, fp, 0);
        myfclose(fp);
    } else {
        MOTION_LOG(CRT, TYPE_ALL, NO_ERRNO
            ,_("No config file to process, using default values"));
    }

    if (motapp->cam_list[1] == NULL) {
        conf_camera_add(motapp);
    }

    indx=0;
    while (motapp->cam_list[indx] != NULL) {
        motapp->cam_list[indx]->pause = motapp->pause;
        motapp->cam_list[indx]->threadnr = indx;
        indx++;
    }

    return;
}

void conf_deinit(struct ctx_motapp *motapp)
{
    int indx;

    indx = 0;
    while (motapp->cam_list[indx] != NULL) {
        delete motapp->cam_list[indx]->conf;
        delete motapp->cam_list[indx];
        indx++;
    }

    if (motapp->cam_list != NULL) {
        free(motapp->cam_list);
    }
    motapp->cam_list = NULL;

}

