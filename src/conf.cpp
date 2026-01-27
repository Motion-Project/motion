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
 * conf.cpp - Configuration Parameter Management
 *
 * This module defines and manages all configuration parameters, providing
 * validation, default values, hot-reload capability, and runtime editing
 * for application, camera, and sound settings.
 *
 * Architecture (refactored Dec 2024):
 * - parm_registry.hpp: O(1) lookup via hash map, scoped by PARM_SCOPE_APP/CAM/SND
 * - parm_structs.hpp: Lightweight ctx_parm_app/cam/snd structs for live device use
 * - conf_file.hpp: Separated file I/O (cls_config_file) from parameter editing
 *
 * SYNC REQUIREMENT: When changing default values in this file, also update:
 *   1. data/motion-dist.conf.in  - Config template (installed as starting config)
 *   2. frontend/src/utils/parameterMappings.ts - UI labels if showing defaults
 */

#include "motion.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "camera.hpp"
#include "sound.hpp"
#include "conf.hpp"
#include "cam_detect.hpp"
#include "parm_registry.hpp"
#include "conf_file.hpp"

/*Configuration parameters
 * hot_reload field indicates if parameter can be updated at runtime without restart:
 * - true: Parameter can be changed immediately via /config/set API
 * - false: Parameter requires daemon restart to take effect
 */
ctx_parm config_parms[] = {
    /* Category 00 - System parameters - NOT hot reloadable */
    {"daemon",                    PARM_TYP_BOOL,   PARM_CAT_00, PARM_LEVEL_ADVANCED, false},
    {"conf_filename",             PARM_TYP_STRING, PARM_CAT_00, PARM_LEVEL_ADVANCED, false},
    {"pid_file",                  PARM_TYP_STRING, PARM_CAT_00, PARM_LEVEL_ADVANCED, false},
    {"log_file",                  PARM_TYP_STRING, PARM_CAT_00, PARM_LEVEL_ADVANCED, false},
    {"log_level",                 PARM_TYP_LIST,   PARM_CAT_00, PARM_LEVEL_LIMITED,  false},
    {"log_fflevel",               PARM_TYP_LIST,   PARM_CAT_00, PARM_LEVEL_LIMITED,  false},
    {"log_type",                  PARM_TYP_LIST,   PARM_CAT_00, PARM_LEVEL_LIMITED,  false},
    {"native_language",           PARM_TYP_BOOL,   PARM_CAT_00, PARM_LEVEL_LIMITED,  false},

    /* Category 01 - Camera parameters - mostly NOT hot reloadable */
    {"device_name",               PARM_TYP_STRING, PARM_CAT_01, PARM_LEVEL_LIMITED,  true},   /* Display only */
    {"device_id",                 PARM_TYP_INT,    PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"device_tmo",                PARM_TYP_INT,    PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"pause",                     PARM_TYP_LIST,   PARM_CAT_01, PARM_LEVEL_LIMITED,  true},   /* Runtime control */
    {"schedule_params",           PARM_TYP_PARAMS, PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"picture_schedule_params",   PARM_TYP_PARAMS, PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"cleandir_params",           PARM_TYP_PARAMS, PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"target_dir",                PARM_TYP_STRING, PARM_CAT_01, PARM_LEVEL_ADVANCED, false},
    {"watchdog_tmo",              PARM_TYP_INT,    PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"watchdog_kill",             PARM_TYP_INT,    PARM_CAT_01, PARM_LEVEL_LIMITED,  false},
    {"config_dir",                PARM_TYP_STRING, PARM_CAT_01, PARM_LEVEL_ADVANCED, false},
    {"camera",                    PARM_TYP_STRING, PARM_CAT_01, PARM_LEVEL_ADVANCED, false},

    /* Category 02 - Source parameters - NOT hot reloadable (device changes) */
    {"v4l2_device",               PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"v4l2_params",               PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"netcam_url",                PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"netcam_params",             PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"netcam_high_url",           PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"netcam_high_params",        PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"netcam_userpass",           PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"libcam_device",             PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"libcam_params",             PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"libcam_buffer_count",       PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, false},
    {"libcam_brightness",         PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_contrast",           PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_gain",               PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_awb_enable",         PARM_TYP_BOOL,   PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_awb_mode",           PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_awb_locked",         PARM_TYP_BOOL,   PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_colour_temp",        PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_colour_gain_r",      PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_colour_gain_b",      PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_af_mode",            PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_lens_position",      PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_af_range",           PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_af_speed",           PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
    {"libcam_af_trigger",         PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},

    /* Category 03 - Image parameters - NOT hot reloadable (buffer realloc) */
    {"width",                     PARM_TYP_INT,    PARM_CAT_03, PARM_LEVEL_LIMITED,  false},
    {"height",                    PARM_TYP_INT,    PARM_CAT_03, PARM_LEVEL_LIMITED,  false},
    {"framerate",                 PARM_TYP_INT,    PARM_CAT_03, PARM_LEVEL_LIMITED,  false},
    {"rotate",                    PARM_TYP_LIST,   PARM_CAT_03, PARM_LEVEL_LIMITED,  false},
    {"flip_axis",                 PARM_TYP_LIST,   PARM_CAT_03, PARM_LEVEL_LIMITED,  false},

    /* Category 04 - Overlay parameters - HOT RELOADABLE */
    {"locate_motion_mode",        PARM_TYP_LIST,   PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"locate_motion_style",       PARM_TYP_LIST,   PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"text_left",                 PARM_TYP_STRING, PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"text_right",                PARM_TYP_STRING, PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"text_changes",              PARM_TYP_BOOL,   PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"text_scale",                PARM_TYP_LIST,   PARM_CAT_04, PARM_LEVEL_LIMITED,  true},
    {"text_event",                PARM_TYP_STRING, PARM_CAT_04, PARM_LEVEL_LIMITED,  true},

    /* Category 05 - Detection method parameters - HOT RELOADABLE */
    {"emulate_motion",            PARM_TYP_BOOL,   PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold",                 PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_maximum",         PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_sdevx",           PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_sdevy",           PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_sdevxy",          PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_ratio",           PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_ratio_change",    PARM_TYP_INT,    PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"threshold_tune",            PARM_TYP_BOOL,   PARM_CAT_05, PARM_LEVEL_LIMITED,  true},
    {"secondary_method",          PARM_TYP_LIST,   PARM_CAT_05, PARM_LEVEL_LIMITED,  false},  /* May need model reload */
    {"secondary_params",          PARM_TYP_PARAMS, PARM_CAT_05, PARM_LEVEL_LIMITED,  false},  /* May need model reload */

    /* Category 06 - Mask parameters - mostly HOT RELOADABLE */
    {"noise_level",               PARM_TYP_INT,    PARM_CAT_06, PARM_LEVEL_LIMITED,  true},
    {"noise_tune",                PARM_TYP_BOOL,   PARM_CAT_06, PARM_LEVEL_LIMITED,  true},
    {"despeckle_filter",          PARM_TYP_STRING, PARM_CAT_06, PARM_LEVEL_LIMITED,  true},
    {"area_detect",               PARM_TYP_STRING, PARM_CAT_06, PARM_LEVEL_LIMITED,  true},
    {"mask_file",                 PARM_TYP_STRING, PARM_CAT_06, PARM_LEVEL_ADVANCED, false},  /* Requires PGM reload */
    {"mask_privacy",              PARM_TYP_STRING, PARM_CAT_06, PARM_LEVEL_ADVANCED, false},  /* Requires PGM reload */
    {"smart_mask_speed",          PARM_TYP_LIST,   PARM_CAT_06, PARM_LEVEL_LIMITED,  true},

    /* Category 07 - Detect parameters - HOT RELOADABLE */
    {"lightswitch_percent",       PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"lightswitch_frames",        PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"minimum_motion_frames",     PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"static_object_time",        PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"event_gap",                 PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"pre_capture",               PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},
    {"post_capture",              PARM_TYP_INT,    PARM_CAT_07, PARM_LEVEL_LIMITED,  true},

    /* Category 08 - Script parameters - HOT RELOADABLE */
    {"on_event_start",            PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_event_end",              PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_picture_save",           PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_area_detected",          PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_motion_detected",        PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_movie_start",            PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_movie_end",              PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_camera_lost",            PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_camera_found",           PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_secondary_detect",       PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_action_user",            PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},
    {"on_sound_alert",            PARM_TYP_STRING, PARM_CAT_08, PARM_LEVEL_RESTRICTED, true},

    /* Category 09 - Picture parameters - mostly HOT RELOADABLE */
    {"picture_output",            PARM_TYP_LIST,   PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_output_motion",     PARM_TYP_LIST,   PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_type",              PARM_TYP_LIST,   PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_quality",           PARM_TYP_INT,    PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_exif",              PARM_TYP_STRING, PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_filename",          PARM_TYP_STRING, PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"snapshot_interval",         PARM_TYP_INT,    PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"snapshot_filename",         PARM_TYP_STRING, PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_max_per_event",     PARM_TYP_INT,    PARM_CAT_09, PARM_LEVEL_LIMITED,  true},
    {"picture_min_interval",      PARM_TYP_INT,    PARM_CAT_09, PARM_LEVEL_LIMITED,  true},

    /* Category 10 - Movie parameters - mostly NOT hot reloadable */
    {"movie_output",              PARM_TYP_BOOL,   PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Recording state */
    {"movie_output_motion",       PARM_TYP_BOOL,   PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Recording state */
    {"movie_max_time",            PARM_TYP_INT,    PARM_CAT_10, PARM_LEVEL_LIMITED,  true},
    {"movie_bps",                 PARM_TYP_INT,    PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_quality",             PARM_TYP_INT,    PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_encoder_preset",      PARM_TYP_LIST,   PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_container",           PARM_TYP_STRING, PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_passthrough",         PARM_TYP_BOOL,   PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_filename",            PARM_TYP_STRING, PARM_CAT_10, PARM_LEVEL_LIMITED,  true},
    {"movie_retain",              PARM_TYP_LIST,   PARM_CAT_10, PARM_LEVEL_LIMITED,  true},
    {"movie_all_frames",          PARM_TYP_BOOL,   PARM_CAT_10, PARM_LEVEL_LIMITED,  false},  /* Encoder config */
    {"movie_extpipe_use",         PARM_TYP_BOOL,   PARM_CAT_10, PARM_LEVEL_RESTRICTED, false},
    {"movie_extpipe",             PARM_TYP_STRING, PARM_CAT_10, PARM_LEVEL_RESTRICTED, false},

    /* Category 11 - Timelapse parameters - NOT hot reloadable */
    {"timelapse_interval",        PARM_TYP_INT,    PARM_CAT_11, PARM_LEVEL_LIMITED,  false},
    {"timelapse_mode",            PARM_TYP_LIST,   PARM_CAT_11, PARM_LEVEL_LIMITED,  false},
    {"timelapse_fps",             PARM_TYP_INT,    PARM_CAT_11, PARM_LEVEL_LIMITED,  false},
    {"timelapse_container",       PARM_TYP_LIST,   PARM_CAT_11, PARM_LEVEL_LIMITED,  false},
    {"timelapse_filename",        PARM_TYP_STRING, PARM_CAT_11, PARM_LEVEL_LIMITED,  true},

    /* Category 12 - Pipe parameters - NOT hot reloadable */
    {"video_pipe",                PARM_TYP_STRING, PARM_CAT_12, PARM_LEVEL_LIMITED,  false},
    {"video_pipe_motion",         PARM_TYP_STRING, PARM_CAT_12, PARM_LEVEL_LIMITED,  false},

    /* Category 13 - Webcontrol parameters - NOT hot reloadable */
    {"webcontrol_port",           PARM_TYP_INT,    PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_port2",          PARM_TYP_INT,    PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_base_path",      PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_ipv6",           PARM_TYP_BOOL,   PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_localhost",      PARM_TYP_BOOL,   PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_parms",          PARM_TYP_LIST,   PARM_CAT_13, PARM_LEVEL_NEVER,    false},
    {"webcontrol_interface",      PARM_TYP_LIST,   PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_auth_method",    PARM_TYP_LIST,   PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_authentication", PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_user_authentication", PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_tls",            PARM_TYP_BOOL,   PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_cert",           PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_key",            PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_headers",        PARM_TYP_PARAMS, PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_html",           PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_actions",        PARM_TYP_PARAMS, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_lock_minutes",   PARM_TYP_INT,    PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_lock_attempts",  PARM_TYP_INT,    PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_lock_script",    PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_RESTRICTED, false},
    {"webcontrol_trusted_proxies", PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_session_timeout", PARM_TYP_INT,   PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_html_path",      PARM_TYP_STRING, PARM_CAT_13, PARM_LEVEL_ADVANCED, false},
    {"webcontrol_spa_mode",       PARM_TYP_BOOL,   PARM_CAT_13, PARM_LEVEL_ADVANCED, false},

    /* Category 14 - Stream parameters - mostly NOT hot reloadable */
    {"stream_preview_scale",      PARM_TYP_INT,    PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_preview_newline",    PARM_TYP_BOOL,   PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_preview_params",     PARM_TYP_PARAMS, PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_preview_method",     PARM_TYP_LIST,   PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_preview_ptz",        PARM_TYP_BOOL,   PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_quality",            PARM_TYP_INT,    PARM_CAT_14, PARM_LEVEL_LIMITED,  true},   /* Can change stream quality */
    {"stream_grey",               PARM_TYP_BOOL,   PARM_CAT_14, PARM_LEVEL_LIMITED,  true},   /* Can toggle greyscale */
    {"stream_motion",             PARM_TYP_BOOL,   PARM_CAT_14, PARM_LEVEL_LIMITED,  true},   /* Can toggle motion view */
    {"stream_maxrate",            PARM_TYP_INT,    PARM_CAT_14, PARM_LEVEL_LIMITED,  true},   /* Can adjust rate */
    {"stream_scan_time",          PARM_TYP_INT,    PARM_CAT_14, PARM_LEVEL_LIMITED,  false},
    {"stream_scan_scale",         PARM_TYP_INT,    PARM_CAT_14, PARM_LEVEL_LIMITED,  false},

    /* Category 15 - Database parameters - NOT hot reloadable */
    {"database_type",             PARM_TYP_LIST,   PARM_CAT_15, PARM_LEVEL_ADVANCED, false},
    {"database_dbname",           PARM_TYP_STRING, PARM_CAT_15, PARM_LEVEL_ADVANCED, false},
    {"database_host",             PARM_TYP_STRING, PARM_CAT_15, PARM_LEVEL_ADVANCED, false},
    {"database_port",             PARM_TYP_INT,    PARM_CAT_15, PARM_LEVEL_ADVANCED, false},
    {"database_user",             PARM_TYP_STRING, PARM_CAT_15, PARM_LEVEL_RESTRICTED, false},
    {"database_password",         PARM_TYP_STRING, PARM_CAT_15, PARM_LEVEL_RESTRICTED, false},
    {"database_busy_timeout",     PARM_TYP_INT,    PARM_CAT_15, PARM_LEVEL_ADVANCED, false},

    /* Category 16 - SQL parameters - HOT RELOADABLE (just strings) */
    {"sql_event_start",           PARM_TYP_STRING, PARM_CAT_16, PARM_LEVEL_ADVANCED, true},
    {"sql_event_end",             PARM_TYP_STRING, PARM_CAT_16, PARM_LEVEL_ADVANCED, true},
    {"sql_movie_start",           PARM_TYP_STRING, PARM_CAT_16, PARM_LEVEL_ADVANCED, true},
    {"sql_movie_end",             PARM_TYP_STRING, PARM_CAT_16, PARM_LEVEL_ADVANCED, true},
    {"sql_pic_save",              PARM_TYP_STRING, PARM_CAT_16, PARM_LEVEL_ADVANCED, true},

    /* Category 17 - PTZ/Tracking parameters - HOT RELOADABLE (runtime control) */
    {"ptz_auto_track",            PARM_TYP_BOOL,   PARM_CAT_17, PARM_LEVEL_LIMITED,    true},
    {"ptz_wait",                  PARM_TYP_INT,    PARM_CAT_17, PARM_LEVEL_LIMITED,    true},
    {"ptz_move_track",            PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_pan_left",              PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_pan_right",             PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_tilt_up",               PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_tilt_down",             PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_zoom_in",               PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},
    {"ptz_zoom_out",              PARM_TYP_STRING, PARM_CAT_17, PARM_LEVEL_RESTRICTED, true},

    /* Category 18 - Sound parameters - NOT hot reloadable (device config) */
    {"snd_device",                PARM_TYP_STRING, PARM_CAT_18, PARM_LEVEL_ADVANCED, false},
    {"snd_params",                PARM_TYP_PARAMS, PARM_CAT_18, PARM_LEVEL_ADVANCED, false},
    {"snd_alerts",                PARM_TYP_ARRAY,  PARM_CAT_18, PARM_LEVEL_ADVANCED, false},
    {"snd_window",                PARM_TYP_LIST,   PARM_CAT_18, PARM_LEVEL_ADVANCED, false},
    {"snd_show",                  PARM_TYP_BOOL,   PARM_CAT_18, PARM_LEVEL_ADVANCED, false},

    /* Terminator */
    { "", (enum PARM_TYP)0, (enum PARM_CAT)0, (enum PARM_LEVEL)0, false }
};

/* Expand environment variables in configuration values
 * Supports $VAR and ${VAR} syntax
 * Returns expanded value or empty string if variable not set
 */
static std::string conf_expand_env(const std::string& value)
{
    if (value.empty() || value[0] != '$') {
        return value;
    }

    std::string env_name = value.substr(1);  // Remove '$'

    // Handle ${VAR} syntax
    if (!env_name.empty() && env_name[0] == '{' && env_name.back() == '}') {
        env_name = env_name.substr(1, env_name.length() - 2);
    }

    const char* env_val = getenv(env_name.c_str());
    if (env_val == nullptr) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            _("Environment variable %s not set"), env_name.c_str());
        return "";
    }

    return std::string(env_val);
}

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

/* Generic handlers for type-based parameter editing */
void cls_config::edit_generic_bool(bool& target, std::string& parm,
                                   enum PARM_ACT pact, bool default_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(target, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, target);
    }
}

void cls_config::edit_generic_int(int& target, std::string& parm,
                                  enum PARM_ACT pact, int default_val, int min_val, int max_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        int val = atoi(parm.c_str());
        if (val < min_val || val > max_val) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %d (range %d-%d)"), val, min_val, max_val);
        } else {
            target = val;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(target);
    }
}

void cls_config::edit_generic_float(float& target, std::string& parm,
                                    enum PARM_ACT pact, float default_val, float min_val, float max_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        float val = (float)atof(parm.c_str());
        if (val < min_val || val > max_val) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %.2f (range %.2f-%.2f)"), val, min_val, max_val);
        } else {
            target = val;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(target);
    }
}

void cls_config::edit_generic_string(std::string& target, std::string& parm,
                                     enum PARM_ACT pact, const std::string& default_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        target = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = target;
    }
}

void cls_config::edit_generic_list(std::string& target, std::string& parm,
                                   enum PARM_ACT pact, const std::string& default_val,
                                   const std::vector<std::string>& valid_values)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        bool valid = parm.empty();
        for (const auto& v : valid_values) {
            if (parm == v) { valid = true; break; }
        }
        if (!valid) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %s"), parm.c_str());
        } else {
            target = parm.empty() ? default_val : parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = target;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        for (size_t i = 0; i < valid_values.size(); i++) {
            if (i > 0) parm += ",";
            parm += "\"" + valid_values[i] + "\"";
        }
        parm += "]";
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","device_id",_("device_id"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
}

void cls_config::edit_webcontrol_html_path(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        webcontrol_html_path = std::string( configdir ) + std::string("/webui");
    } else if (pact == PARM_ACT_SET) {
        if (parm == "") {
            webcontrol_html_path = "";
        } else if (parm.substr(parm.length()-1,1) == "/") {
            webcontrol_html_path = parm.substr(0, parm.length()-1);
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,"Removing trailing '/' from webcontrol_html_path");
        } else {
            webcontrol_html_path = parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = webcontrol_html_path;
    }
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_html_path",_("webcontrol_html_path"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
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
    return;
    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snd_alerts",_("snd_alerts"));
}

/* Centralized parameter dispatch function - consolidates ~150 individual edit handlers */
void cls_config::dispatch_edit(const std::string& name, std::string& parm, enum PARM_ACT pact)
{
    // BOOLEANS
    if (name == "daemon") return edit_generic_bool(daemon, parm, pact, false);
    if (name == "native_language") return edit_generic_bool(native_language, parm, pact, true);
    if (name == "emulate_motion") return edit_generic_bool(emulate_motion, parm, pact, false);
    if (name == "threshold_tune") return edit_generic_bool(threshold_tune, parm, pact, false);
    if (name == "noise_tune") return edit_generic_bool(noise_tune, parm, pact, true);
    if (name == "movie_output") return edit_generic_bool(movie_output, parm, pact, false);
    if (name == "movie_output_motion") return edit_generic_bool(movie_output_motion, parm, pact, false);
    if (name == "movie_all_frames") return edit_generic_bool(movie_all_frames, parm, pact, false);
    if (name == "movie_extpipe_use") return edit_generic_bool(movie_extpipe_use, parm, pact, false);
    if (name == "webcontrol_localhost") return edit_generic_bool(webcontrol_localhost, parm, pact, false);
    if (name == "webcontrol_ipv6") return edit_generic_bool(webcontrol_ipv6, parm, pact, false);
    if (name == "webcontrol_tls") return edit_generic_bool(webcontrol_tls, parm, pact, false);
    if (name == "webcontrol_spa_mode") return edit_generic_bool(webcontrol_spa_mode, parm, pact, true);
    if (name == "stream_preview_newline") return edit_generic_bool(stream_preview_newline, parm, pact, false);
    if (name == "stream_grey") return edit_generic_bool(stream_grey, parm, pact, false);
    if (name == "stream_motion") return edit_generic_bool(stream_motion, parm, pact, false);
    if (name == "ptz_auto_track") return edit_generic_bool(ptz_auto_track, parm, pact, false);

    // INTEGERS with ranges
    if (name == "log_level") return edit_generic_int(log_level, parm, pact, 6, 1, 9);
    if (name == "log_fflevel") return edit_generic_int(log_fflevel, parm, pact, 3, 1, 9);
    if (name == "device_tmo") return edit_generic_int(device_tmo, parm, pact, 30, 1, INT_MAX);
    if (name == "watchdog_tmo") return edit_generic_int(watchdog_tmo, parm, pact, 90, 1, INT_MAX);
    if (name == "watchdog_kill") return edit_generic_int(watchdog_kill, parm, pact, 0, 0, INT_MAX);
    if (name == "libcam_buffer_count") return edit_generic_int(libcam_buffer_count, parm, pact, 4, 2, 8);
    if (name == "width") return edit_generic_int(width, parm, pact, 640, 64, 9999);
    if (name == "height") return edit_generic_int(height, parm, pact, 480, 64, 9999);
    if (name == "framerate") return edit_generic_int(framerate, parm, pact, 15, 2, 100);
    if (name == "rotate") return edit_generic_int(rotate, parm, pact, 0, 0, 270);
    if (name == "text_scale") return edit_generic_int(text_scale, parm, pact, 1, 1, 10);
    if (name == "threshold") return edit_generic_int(threshold, parm, pact, 1500, 1, 2147483647);
    if (name == "threshold_maximum") return edit_generic_int(threshold_maximum, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevx") return edit_generic_int(threshold_sdevx, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevy") return edit_generic_int(threshold_sdevy, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_sdevxy") return edit_generic_int(threshold_sdevxy, parm, pact, 0, 0, INT_MAX);
    if (name == "threshold_ratio") return edit_generic_int(threshold_ratio, parm, pact, 0, 0, 100);
    if (name == "threshold_ratio_change") return edit_generic_int(threshold_ratio_change, parm, pact, 64, 0, 255);
    if (name == "noise_level") return edit_generic_int(noise_level, parm, pact, 32, 1, 255);
    if (name == "smart_mask_speed") return edit_generic_int(smart_mask_speed, parm, pact, 0, 0, 10);
    if (name == "lightswitch_percent") return edit_generic_int(lightswitch_percent, parm, pact, 0, 0, 100);
    if (name == "lightswitch_frames") return edit_generic_int(lightswitch_frames, parm, pact, 5, 1, 1000);
    if (name == "minimum_motion_frames") return edit_generic_int(minimum_motion_frames, parm, pact, 1, 1, 10000);
    if (name == "static_object_time") return edit_generic_int(static_object_time, parm, pact, 10, 1, INT_MAX);
    if (name == "event_gap") return edit_generic_int(event_gap, parm, pact, 60, 0, 2147483647);
    if (name == "pre_capture") return edit_generic_int(pre_capture, parm, pact, 3, 0, 1000);
    if (name == "post_capture") return edit_generic_int(post_capture, parm, pact, 10, 0, 2147483647);
    if (name == "picture_quality") return edit_generic_int(picture_quality, parm, pact, 75, 1, 100);
    if (name == "snapshot_interval") return edit_generic_int(snapshot_interval, parm, pact, 0, 0, 2147483647);
    if (name == "picture_max_per_event") return edit_generic_int(picture_max_per_event, parm, pact, 0, 0, 100000);
    if (name == "picture_min_interval") return edit_generic_int(picture_min_interval, parm, pact, 0, 0, 60000);
    if (name == "movie_max_time") return edit_generic_int(movie_max_time, parm, pact, 120, 0, 2147483647);
    if (name == "movie_bps") return edit_generic_int(movie_bps, parm, pact, 400000, 0, INT_MAX);
    if (name == "movie_quality") return edit_generic_int(movie_quality, parm, pact, 60, 1, 100);
    if (name == "timelapse_interval") return edit_generic_int(timelapse_interval, parm, pact, 0, 0, 2147483647);
    if (name == "timelapse_fps") return edit_generic_int(timelapse_fps, parm, pact, 30, 1, 100);
    if (name == "webcontrol_port") return edit_generic_int(webcontrol_port, parm, pact, 8080, 0, 65535);
    if (name == "webcontrol_port2") return edit_generic_int(webcontrol_port2, parm, pact, 8081, 0, 65535);
    if (name == "webcontrol_parms") return edit_generic_int(webcontrol_parms, parm, pact, 3, 0, 3);
    if (name == "webcontrol_lock_minutes") return edit_generic_int(webcontrol_lock_minutes, parm, pact, 5, 0, INT_MAX);
    if (name == "webcontrol_lock_attempts") return edit_generic_int(webcontrol_lock_attempts, parm, pact, 5, 1, INT_MAX);
    if (name == "webcontrol_session_timeout") return edit_generic_int(webcontrol_session_timeout, parm, pact, 3600, 60, INT_MAX);
    if (name == "stream_preview_scale") return edit_generic_int(stream_preview_scale, parm, pact, 25, 1, 100);
    if (name == "stream_quality") return edit_generic_int(stream_quality, parm, pact, 60, 1, 100);
    if (name == "stream_maxrate") return edit_generic_int(stream_maxrate, parm, pact, 1, 0, 100);
    if (name == "stream_scan_time") return edit_generic_int(stream_scan_time, parm, pact, 5, 0, 3600);
    if (name == "stream_scan_scale") return edit_generic_int(stream_scan_scale, parm, pact, 2, 1, 32);
    if (name == "database_port") return edit_generic_int(database_port, parm, pact, 0, 0, 65535);
    if (name == "database_busy_timeout") return edit_generic_int(database_busy_timeout, parm, pact, 0, 0, INT_MAX);
    if (name == "ptz_wait") return edit_generic_int(ptz_wait, parm, pact, 1, 0, INT_MAX);

    // FLOATS with ranges - libcam parameters
    if (name == "libcam_brightness") return edit_generic_float(parm_cam.libcam_brightness, parm, pact, 0.0f, -1.0f, 1.0f);
    if (name == "libcam_contrast") return edit_generic_float(parm_cam.libcam_contrast, parm, pact, 1.0f, 0.0f, 32.0f);
    if (name == "libcam_colour_gain_r") return edit_generic_float(parm_cam.libcam_colour_gain_r, parm, pact, 0.0f, 0.0f, 8.0f);
    if (name == "libcam_colour_gain_b") return edit_generic_float(parm_cam.libcam_colour_gain_b, parm, pact, 0.0f, 0.0f, 8.0f);

    // FLOAT - libcam analog gain parameter (0=auto, 1.0-16.0 manual)
    if (name == "libcam_gain") return edit_generic_float(parm_cam.libcam_gain, parm, pact, 1.0f, 0.0f, 16.0f);
    if (name == "libcam_awb_mode") return edit_generic_int(parm_cam.libcam_awb_mode, parm, pact, 0, 0, 7);
    if (name == "libcam_colour_temp") return edit_generic_int(parm_cam.libcam_colour_temp, parm, pact, 0, 0, 10000);

    // AF parameters - AfMode: 0=Manual (default), 1=Auto, 2=Continuous
    if (name == "libcam_af_mode") return edit_generic_int(parm_cam.libcam_af_mode, parm, pact, 0, 0, 2);
    // LensPosition: dioptres (0=infinity, typical max ~10 for macro)
    if (name == "libcam_lens_position") return edit_generic_float(parm_cam.libcam_lens_position, parm, pact, 0.0f, 0.0f, 15.0f);
    // AfRange: 0=Normal, 1=Macro, 2=Full
    if (name == "libcam_af_range") return edit_generic_int(parm_cam.libcam_af_range, parm, pact, 0, 0, 2);
    // AfSpeed: 0=Normal, 1=Fast
    if (name == "libcam_af_speed") return edit_generic_int(parm_cam.libcam_af_speed, parm, pact, 0, 0, 1);
    // AfTrigger: 0=Start scan, 1=Cancel (action parameter)
    if (name == "libcam_af_trigger") return edit_generic_int(parm_cam.libcam_af_trigger, parm, pact, 0, 0, 1);

    // BOOLS - libcam AWB parameters
    if (name == "libcam_awb_enable") return edit_generic_bool(parm_cam.libcam_awb_enable, parm, pact, true);
    if (name == "libcam_awb_locked") return edit_generic_bool(parm_cam.libcam_awb_locked, parm, pact, false);

    // STRINGS (simple assignment)
    if (name == "conf_filename") return edit_generic_string(conf_filename, parm, pact, "");
    if (name == "pid_file") return edit_generic_string(pid_file, parm, pact, "");
    if (name == "device_name") return edit_generic_string(device_name, parm, pact, "");
    if (name == "v4l2_device") return edit_generic_string(v4l2_device, parm, pact, "");
    if (name == "v4l2_params") return edit_generic_string(v4l2_params, parm, pact, "");
    if (name == "netcam_url") return edit_generic_string(netcam_url, parm, pact, "");
    if (name == "netcam_params") return edit_generic_string(netcam_params, parm, pact, "");
    if (name == "netcam_high_url") return edit_generic_string(netcam_high_url, parm, pact, "");
    if (name == "netcam_high_params") return edit_generic_string(netcam_high_params, parm, pact, "");
    if (name == "netcam_userpass") {
        /* Apply environment variable expansion for security */
        if (pact == PARM_ACT_SET || pact == PARM_ACT_DFLT) {
            parm = conf_expand_env(parm);
        }
        return edit_generic_string(netcam_userpass, parm, pact, "");
    }
    if (name == "libcam_device") return edit_generic_string(libcam_device, parm, pact, "auto");
    if (name == "libcam_params") return edit_generic_string(libcam_params, parm, pact, "");
    if (name == "schedule_params") return edit_generic_string(schedule_params, parm, pact, "");
    if (name == "picture_schedule_params") return edit_generic_string(picture_schedule_params, parm, pact, "");
    if (name == "cleandir_params") return edit_generic_string(cleandir_params, parm, pact, "");
    if (name == "config_dir") return edit_generic_string(config_dir, parm, pact, "");
    if (name == "text_left") return edit_generic_string(text_left, parm, pact, "");
    if (name == "text_right") return edit_generic_string(text_right, parm, pact, "%Y-%m-%d\\n%T");
    if (name == "text_event") return edit_generic_string(text_event, parm, pact, "%Y%m%d%H%M%S");
    if (name == "despeckle_filter") return edit_generic_string(despeckle_filter, parm, pact, "EedDl");
    if (name == "area_detect") return edit_generic_string(area_detect, parm, pact, "");
    if (name == "mask_file") return edit_generic_string(mask_file, parm, pact, "");
    if (name == "mask_privacy") return edit_generic_string(mask_privacy, parm, pact, "");
    if (name == "secondary_params") return edit_generic_string(secondary_params, parm, pact, "");
    if (name == "on_event_start") return edit_generic_string(on_event_start, parm, pact, "");
    if (name == "on_event_end") return edit_generic_string(on_event_end, parm, pact, "");
    if (name == "on_picture_save") return edit_generic_string(on_picture_save, parm, pact, "");
    if (name == "on_area_detected") return edit_generic_string(on_area_detected, parm, pact, "");
    if (name == "on_motion_detected") return edit_generic_string(on_motion_detected, parm, pact, "");
    if (name == "on_movie_start") return edit_generic_string(on_movie_start, parm, pact, "");
    if (name == "on_movie_end") return edit_generic_string(on_movie_end, parm, pact, "");
    if (name == "on_camera_lost") return edit_generic_string(on_camera_lost, parm, pact, "");
    if (name == "on_camera_found") return edit_generic_string(on_camera_found, parm, pact, "");
    if (name == "on_secondary_detect") return edit_generic_string(on_secondary_detect, parm, pact, "");
    if (name == "on_action_user") return edit_generic_string(on_action_user, parm, pact, "");
    if (name == "on_sound_alert") return edit_generic_string(on_sound_alert, parm, pact, "");
    if (name == "picture_exif") return edit_generic_string(picture_exif, parm, pact, "");
    if (name == "movie_extpipe") return edit_generic_string(movie_extpipe, parm, pact, "");
    if (name == "video_pipe") return edit_generic_string(video_pipe, parm, pact, "");
    if (name == "video_pipe_motion") return edit_generic_string(video_pipe_motion, parm, pact, "");
    if (name == "webcontrol_base_path") return edit_generic_string(webcontrol_base_path, parm, pact, "/");
    if (name == "webcontrol_actions") return edit_generic_string(webcontrol_actions, parm, pact, "");
    if (name == "webcontrol_html") return edit_generic_string(webcontrol_html, parm, pact, "");
    if (name == "webcontrol_cert") return edit_generic_string(webcontrol_cert, parm, pact, "");
    if (name == "webcontrol_key") return edit_generic_string(webcontrol_key, parm, pact, "");
    if (name == "webcontrol_headers") return edit_generic_string(webcontrol_headers, parm, pact, "");
    if (name == "webcontrol_lock_script") return edit_generic_string(webcontrol_lock_script, parm, pact, "");
    if (name == "webcontrol_trusted_proxies") return edit_generic_string(webcontrol_trusted_proxies, parm, pact, "");
    if (name == "stream_preview_params") return edit_generic_string(stream_preview_params, parm, pact, "");
    if (name == "database_dbname") return edit_generic_string(database_dbname, parm, pact, "motion");
    if (name == "database_host") return edit_generic_string(database_host, parm, pact, "");
    if (name == "database_user") return edit_generic_string(database_user, parm, pact, "");
    if (name == "database_password") {
        /* Apply environment variable expansion for security */
        if (pact == PARM_ACT_SET || pact == PARM_ACT_DFLT) {
            parm = conf_expand_env(parm);
        }
        return edit_generic_string(database_password, parm, pact, "");
    }
    if (name == "sql_event_start") return edit_generic_string(sql_event_start, parm, pact, "");
    if (name == "sql_event_end") return edit_generic_string(sql_event_end, parm, pact, "");
    if (name == "sql_movie_start") return edit_generic_string(sql_movie_start, parm, pact, "");
    if (name == "sql_movie_end") return edit_generic_string(sql_movie_end, parm, pact, "");
    if (name == "sql_pic_save") return edit_generic_string(sql_pic_save, parm, pact, "");
    if (name == "ptz_pan_left") return edit_generic_string(ptz_pan_left, parm, pact, "");
    if (name == "ptz_pan_right") return edit_generic_string(ptz_pan_right, parm, pact, "");
    if (name == "ptz_tilt_up") return edit_generic_string(ptz_tilt_up, parm, pact, "");
    if (name == "ptz_tilt_down") return edit_generic_string(ptz_tilt_down, parm, pact, "");
    if (name == "ptz_zoom_in") return edit_generic_string(ptz_zoom_in, parm, pact, "");
    if (name == "ptz_zoom_out") return edit_generic_string(ptz_zoom_out, parm, pact, "");
    if (name == "ptz_move_track") return edit_generic_string(ptz_move_track, parm, pact, "");
    if (name == "snd_device") return edit_generic_string(snd_device, parm, pact, "");
    if (name == "snd_params") return edit_generic_string(snd_params, parm, pact, "");

    // LISTS (constrained string values)
    static const std::vector<std::string> log_type_values = {"ALL","COR","STR","ENC","NET","DBL","EVT","TRK","VID"};
    if (name == "log_type") return edit_generic_list(log_type_str, parm, pact, "ALL", log_type_values);

    static const std::vector<std::string> flip_axis_values = {"none","vertical","horizontal"};
    if (name == "flip_axis") return edit_generic_list(flip_axis, parm, pact, "none", flip_axis_values);

    static const std::vector<std::string> locate_motion_mode_values = {"off","on","preview"};
    if (name == "locate_motion_mode") return edit_generic_list(locate_motion_mode, parm, pact, "off", locate_motion_mode_values);

    static const std::vector<std::string> locate_motion_style_values = {"box","redbox","cross","redcross"};
    if (name == "locate_motion_style") return edit_generic_list(locate_motion_style, parm, pact, "box", locate_motion_style_values);

    static const std::vector<std::string> secondary_method_values = {"none","haar","hog","dnn"};
    if (name == "secondary_method") return edit_generic_list(secondary_method, parm, pact, "none", secondary_method_values);

    static const std::vector<std::string> picture_output_values = {"on","off","first","best","center"};
    if (name == "picture_output") return edit_generic_list(picture_output, parm, pact, "off", picture_output_values);

    static const std::vector<std::string> picture_output_motion_values = {"on","off","roi"};
    if (name == "picture_output_motion") return edit_generic_list(picture_output_motion, parm, pact, "off", picture_output_motion_values);

    static const std::vector<std::string> picture_type_values = {"jpg","webp","ppm"};
    if (name == "picture_type") return edit_generic_list(picture_type, parm, pact, "jpg", picture_type_values);

    static const std::vector<std::string> movie_encoder_preset_values = {"ultrafast","superfast","veryfast","faster","fast","medium","slow","slower","veryslow"};
    if (name == "movie_encoder_preset") return edit_generic_list(movie_encoder_preset, parm, pact, "veryfast", movie_encoder_preset_values);

    // movie_container accepts extended syntax: "container" or "container:codec"
    // e.g., "mp4", "mp4:libx264", "mkv:h264_v4l2m2m" - parsed in movie.cpp init_container()
    if (name == "movie_container") return edit_generic_string(movie_container, parm, pact, "mp4");

    if (name == "movie_passthrough") return edit_generic_bool(movie_passthrough, parm, pact, false);

    static const std::vector<std::string> timelapse_mode_values = {"off","hourly","daily","weekly","monthly"};
    if (name == "timelapse_mode") return edit_generic_list(timelapse_mode, parm, pact, "off", timelapse_mode_values);

    static const std::vector<std::string> timelapse_container_values = {"mkv","mp4","3gp"};
    if (name == "timelapse_container") return edit_generic_list(timelapse_container, parm, pact, "mkv", timelapse_container_values);

    static const std::vector<std::string> webcontrol_interface_values = {"default","auto"};
    if (name == "webcontrol_interface") return edit_generic_list(webcontrol_interface, parm, pact, "default", webcontrol_interface_values);

    static const std::vector<std::string> webcontrol_auth_method_values = {"none","basic","digest"};
    if (name == "webcontrol_auth_method") return edit_generic_list(webcontrol_auth_method, parm, pact, "none", webcontrol_auth_method_values);

    if (name == "webcontrol_authentication") {
        /* Apply environment variable expansion for security */
        if (pact == PARM_ACT_SET || pact == PARM_ACT_DFLT) {
            parm = conf_expand_env(parm);
        }
        /* Accept any username:password format - don't validate against a list */
        return edit_generic_string(webcontrol_authentication, parm, pact, "");
    }

    if (name == "webcontrol_user_authentication") {
        /* Apply environment variable expansion for security */
        if (pact == PARM_ACT_SET || pact == PARM_ACT_DFLT) {
            parm = conf_expand_env(parm);
        }
        /* Accept any username:password format - don't validate against a list */
        return edit_generic_string(webcontrol_user_authentication, parm, pact, "");
    }

    static const std::vector<std::string> stream_preview_method_values = {"mjpeg","snapshot"};
    if (name == "stream_preview_method") return edit_generic_list(stream_preview_method, parm, pact, "mjpeg", stream_preview_method_values);

    if (name == "stream_preview_ptz") return edit_generic_bool(stream_preview_ptz, parm, pact, true);

    static const std::vector<std::string> database_type_values = {"sqlite3","mariadb","mysql","postgresql"};
    if (name == "database_type") return edit_generic_list(database_type, parm, pact, "sqlite3", database_type_values);

    static const std::vector<std::string> snd_window_values = {"none","hamming","hann"};
    if (name == "snd_window") return edit_generic_list(snd_window, parm, pact, "hamming", snd_window_values);

    if (name == "snd_show") return edit_generic_bool(snd_show, parm, pact, false);

    // CUSTOM HANDLERS (preserved - contain special logic)
    if (name == "log_file") return edit_log_file(parm, pact);
    if (name == "target_dir") return edit_target_dir(parm, pact);
    if (name == "webcontrol_html_path") return edit_webcontrol_html_path(parm, pact);
    if (name == "text_changes") return edit_text_changes(parm, pact);
    if (name == "picture_filename") return edit_picture_filename(parm, pact);
    if (name == "movie_filename") return edit_movie_filename(parm, pact);
    if (name == "snapshot_filename") return edit_snapshot_filename(parm, pact);
    if (name == "timelapse_filename") return edit_timelapse_filename(parm, pact);
    if (name == "device_id") return edit_device_id(parm, pact);
    if (name == "pause") return edit_pause(parm, pact);
}

void cls_config::edit_cat00(std::string cmd, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(cmd, parm_val, pact);
}

void cls_config::edit_cat01(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat02(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat03(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat04(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat05(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat06(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat07(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat08(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat09(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat10(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat11(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat12(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat13(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat14(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat15(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat16(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat17(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
}

void cls_config::edit_cat18(std::string parm_nm, std::string &parm_val, enum PARM_ACT pact)
{
    dispatch_edit(parm_nm, parm_val, pact);
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

void cls_config::defaults()
{
    std::string dflt = "";

    /* Use registry for iteration (Phase 3 optimization) */
    const auto &all_parms = ctx_parm_registry::instance().all();
    for (const auto &parm : all_parms) {
        edit_cat(parm.parm_name, dflt, PARM_ACT_DFLT, parm.parm_cat);
    }
}

int cls_config::edit_set_active(std::string parm_nm, std::string parm_val)
{
    /* O(1) lookup via parameter registry (Phase 3 optimization) */
    const ctx_parm_ext *parm = ctx_parm_registry::instance().find(parm_nm);
    if (parm != nullptr) {
        edit_cat(parm_nm, parm_val, PARM_ACT_SET, parm->parm_cat);
        return 0;
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

    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
}

void cls_config::edit_list(std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat)
{
    edit_cat(parm_nm, parm_val, PARM_ACT_LIST, parm_cat);
}

std::string cls_config::type_desc(enum PARM_TYP ptype)
{
    if (ptype == PARM_TYP_BOOL) {           return "bool";
    } else if (ptype == PARM_TYP_INT) {     return "int";
    } else if (ptype == PARM_TYP_LIST) {    return "list";
    } else if (ptype == PARM_TYP_STRING) {  return "string";
    } else if (ptype == PARM_TYP_ARRAY) {   return "array";
    } else if (ptype == PARM_TYP_PARAMS) {  return "params";
    } else {                                return "error";
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
    printf("Motion version %s, Copyright 2020-2025\n",PACKAGE_VERSION);
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
    std::string parm_val;
    cls_camera *cam_cls;

    cam_cls = new cls_camera(app);
    cam_cls->conf_src = new cls_config(app);

    /* Use registry for iteration (Phase 3 optimization) */
    const auto &all_parms = ctx_parm_registry::instance().all();
    for (const auto &parm : all_parms) {
        if (parm.parm_name != "device_id") {
            app->conf_src->edit_get(parm.parm_name, parm_val, parm.parm_cat);
            cam_cls->conf_src->edit_set(parm.parm_name, parm_val);
        }
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
    cam_cls->cfg->parms_copy(cam_cls->conf_src);

    app->cam_list.push_back(cam_cls);
    app->cam_cnt = (int)app->cam_list.size();
}

/*
 * Add camera from detection system
 * Creates a new camera configuration with detected device parameters
 */
void cls_config::camera_add_from_detection(const ctx_detected_cam &detected)
{
    /* Create camera with empty filename (will be generated) */
    camera_add("", false);

    /* Get the newly created camera */
    cls_camera *cam = app->cam_list[app->cam_cnt - 1];

    /* Configure device-specific parameters */
    if (detected.type == CAM_DETECT_LIBCAM) {
        cam->conf_src->edit_set("libcam_device", detected.device_path);
    } else if (detected.type == CAM_DETECT_V4L2) {
        /* Use persistent device ID if available, otherwise use device path */
        cam->conf_src->edit_set("v4l2_device", detected.device_id);
    } else if (detected.type == CAM_DETECT_NETCAM) {
        cam->conf_src->edit_set("netcam_url", detected.device_path);
    }

    /* Set common parameters */
    cam->conf_src->edit_set("device_name", detected.device_name);
    cam->conf_src->edit_set("width", std::to_string(detected.default_width));
    cam->conf_src->edit_set("height", std::to_string(detected.default_height));
    cam->conf_src->edit_set("framerate", std::to_string(detected.default_fps));

    /* Copy updated configuration to runtime config */
    cam->cfg->parms_copy(cam->conf_src);

    /* Write configuration file */
    cam->conf_src->parms_write();

    /* Request camera start */
    pthread_mutex_lock(&app->mutex_camlst);
    cam->handler_stop = false;
    pthread_mutex_unlock(&app->mutex_camlst);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "Camera added from detection: %s [%s]",
        detected.device_name.c_str(), detected.device_path.c_str());
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
    std::string parm_val;
    cls_sound *snd_cls;

    snd_cls = new cls_sound(app);
    snd_cls->conf_src = new cls_config(app);

    /* Use registry for iteration (Phase 3 optimization) */
    const auto &all_parms = ctx_parm_registry::instance().all();
    for (const auto &parm : all_parms) {
        if (parm.parm_name != "device_id") {
            app->conf_src->edit_get(parm.parm_name, parm_val, parm.parm_cat);
            snd_cls->conf_src->edit_set(parm.parm_name, parm_val);
        }
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
    snd_cls->cfg->parms_copy(snd_cls->conf_src);

    app->snd_list.push_back(snd_cls);
    app->snd_cnt = (int)app->snd_list.size();
}

void cls_config::config_dir_parm(std::string confdir)
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
    /* Phase 4: Delegate file I/O to cls_config_file */
    cls_config_file file_handler(app, this);
    file_handler.process();
}

void cls_config::parms_log_parm(std::string parm_nm, std::string parm_vl)
{
    if ((parm_nm == "netcam_url") ||
        (parm_nm == "netcam_userpass") ||
        (parm_nm == "netcam_high_url") ||
        (parm_nm == "webcontrol_authentication") ||
        (parm_nm == "webcontrol_user_authentication") ||
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

void cls_config::parms_log()
{
    /* Phase 4: Delegate file I/O to cls_config_file */
    cls_config_file file_handler(app, this);
    file_handler.parms_log();
}

void cls_config::parms_write_parms(FILE *conffile, std::string parm_nm
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

void cls_config::parms_write_app()
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

    parms_write_parms(conffile, "", "", PARM_CAT_00, true);

    i=0;
    while (config_parms[i].parm_name != "") {
        parm_nm=config_parms[i].parm_name;
        parm_ct=config_parms[i].parm_cat;
        parm_typ=config_parms[i].parm_type;
        if ((parm_nm != "camera") && (parm_nm != "sound") &&
            (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
            (parm_typ != PARM_TYP_ARRAY)) {
            app->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
            parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
        }
        if (parm_typ == PARM_TYP_ARRAY) {
            app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
            for (it = parm_array.begin(); it != parm_array.end(); it++) {
                parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
            }
        }
        i++;
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->conf_src->from_conf_dir == false) {
            parms_write_parms(conffile, "camera"
                , app->cam_list[indx]->conf_src->conf_filename
                , PARM_CAT_01, false);
        }
    }

    for (indx=0; indx<app->snd_cnt; indx++) {
        if (app->snd_list[indx]->conf_src->from_conf_dir == false) {
            parms_write_parms(conffile, "sound"
                , app->snd_list[indx]->conf_src->conf_filename
                , PARM_CAT_01, false);
        }
    }

    fprintf(conffile, "\n");

    app->conf_src->edit_get("config_dir", parm_vl, PARM_CAT_01);
    parms_write_parms(conffile, "config_dir", parm_vl, PARM_CAT_01, false);

    fprintf(conffile, "\n");
    myfclose(conffile);

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        , _("Configuration written to %s")
        , app->conf_src->conf_filename.c_str());

}

void cls_config::parms_write_cam()
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

    for (indx=0; indx<app->cam_cnt; indx++) {
        conffile = myfopen(app->cam_list[indx]->conf_src->conf_filename.c_str(), "we");
        if (conffile == NULL) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , _("Failed to write configuration to %s")
                , app->cam_list[indx]->conf_src->conf_filename.c_str());
            return;
        }
        fprintf(conffile, "; %s\n", app->cam_list[indx]->conf_src->conf_filename.c_str());
        fprintf(conffile, ";\n; This config file was generated by Motion " VERSION "\n");
        fprintf(conffile, "; at %s\n", timestamp);
        fprintf(conffile, "\n\n");
        parms_write_parms(conffile, "", "", PARM_CAT_00, true);

        i=0;
        while (config_parms[i].parm_name != "") {
            parm_nm=config_parms[i].parm_name;
            parm_ct=config_parms[i].parm_cat;
            parm_typ=config_parms[i].parm_type;
            if ((parm_nm != "camera") && (parm_nm != "sound") &&
                (parm_nm != "config_dir") && (parm_nm != "conf_filename") &&
                (parm_typ != PARM_TYP_ARRAY) ) {
                app->conf_src->edit_get(parm_nm, parm_main, parm_ct);
                app->cam_list[indx]->conf_src->edit_get(parm_nm, parm_vl, parm_ct);
                if (parm_main != parm_vl) {
                    parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); it++) {
                    parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
                }
            }
            i++;
        }
        fprintf(conffile, "\n");
        myfclose(conffile);

        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            , _("Configuration written to %s")
            , app->cam_list[indx]->conf_src->conf_filename.c_str());
    }

}

void cls_config::parms_write_snd()
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
        parms_write_parms(conffile, "", "", PARM_CAT_00, true);

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
                    parms_write_parms(conffile, parm_nm, parm_vl, parm_ct, false);
                }
            }
            if (parm_typ == PARM_TYP_ARRAY) {
                app->conf_src->edit_get(parm_nm, parm_array, parm_ct);
                for (it = parm_array.begin(); it != parm_array.end(); ++it) {
                    parms_write_parms(conffile, parm_nm, it->c_str(), parm_ct, false);
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

void cls_config::parms_write()
{
    /* Phase 4: Delegate file I/O to cls_config_file */
    cls_config_file file_handler(app, this);
    file_handler.parms_write();
}

void cls_config::parms_copy(cls_config *src)
{
    std::string parm_val;

    /* Use registry for iteration (Phase 3 optimization) */
    const auto &all_parms = ctx_parm_registry::instance().all();
    for (const auto &parm : all_parms) {
        src->edit_get(parm.parm_name, parm_val, parm.parm_cat);
        edit_set(parm.parm_name, parm_val);
    }
}

void cls_config::parms_copy(cls_config *src, PARM_CAT p_cat)
{
    std::string parm_val;

    /* Use registry for category iteration (Phase 3 optimization) */
    const auto &cat_parms = ctx_parm_registry::instance().by_category(p_cat);
    for (const auto *parm : cat_parms) {
        src->edit_get(parm->parm_name, parm_val, p_cat);
        edit_set(parm->parm_name, parm_val);
    }
}

/*
 * Scoped copy operations - O(1) direct struct copy
 *
 * These methods copy entire parameter scopes at once using direct struct
 * assignment, replacing the O(n) iteration over config_parms[] array.
 *
 * Performance: ~50x faster than parms_copy() for full config copy
 * - parms_copy: O(n) string parsing for 182 parameters
 * - copy_*: O(1) direct memory copy of scoped struct
 */

void cls_config::copy_app(const cls_config *src)
{
    parm_app = src->parm_app;
}

void cls_config::copy_cam(const cls_config *src)
{
    parm_cam = src->parm_cam;
}

void cls_config::copy_snd(const cls_config *src)
{
    parm_snd = src->parm_snd;
}

void cls_config::init()
{
    /* Phase 4: Delegate file I/O to cls_config_file */
    cls_config_file file_handler(app, this);
    file_handler.init();
}

cls_config::cls_config(cls_motapp *p_app)
{
    app = p_app;
    defaults();
}

cls_config::~cls_config()
{

}
