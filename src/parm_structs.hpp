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
 * parm_structs.hpp - Scoped Parameter Structures
 *
 * This module defines lightweight parameter structures for different scopes:
 * - ctx_parm_app: Application-level parameters (daemon, webcontrol, database)
 * - ctx_parm_cam: Camera device parameters (detection, capture, output)
 * - ctx_parm_snd: Sound device parameters (sound alerts)
 *
 * These structures reduce memory footprint for camera/sound devices by only
 * including parameters they actually use, instead of the full 130+ parameter set.
 *
 * Part of the configuration system refactoring for Pi 5 performance optimization.
 * See doc/plans/ConfigParam-Refactor-20251211-1730.md for full design.
 */

#ifndef _INCLUDE_PARM_STRUCTS_HPP_
#define _INCLUDE_PARM_STRUCTS_HPP_

#include <string>
#include <list>

/*
 * Application-level parameters (PARM_CAT_00, PARM_CAT_13, PARM_CAT_15, PARM_CAT_16)
 *
 * These parameters are only needed by the main application process,
 * not by individual camera or sound device threads.
 */
struct ctx_parm_app {
    /* System parameters (PARM_CAT_00) */
    bool            daemon;
    std::string     pid_file;
    std::string     log_file;
    std::string     log_type_str;
    int             log_level;
    int             log_fflevel;
    int             log_type;
    bool            native_language;

    /* Webcontrol parameters (PARM_CAT_13) */
    int             webcontrol_port;
    int             webcontrol_port2;
    std::string     webcontrol_base_path;
    bool            webcontrol_ipv6;
    bool            webcontrol_localhost;
    int             webcontrol_parms;
    std::string     webcontrol_interface;
    std::string     webcontrol_auth_method;
    std::string     webcontrol_authentication;
    std::string     webcontrol_user_authentication;  /* View-only user credentials (optional) */
    bool            webcontrol_tls;
    std::string     webcontrol_cert;
    std::string     webcontrol_key;
    std::string     webcontrol_headers;
    std::string     webcontrol_html;
    std::string     webcontrol_actions;
    int             webcontrol_lock_minutes;
    int             webcontrol_lock_attempts;
    std::string     webcontrol_lock_script;
    std::string     webcontrol_trusted_proxies;  /* IPs allowed to set X-Forwarded-For */
    int             webcontrol_session_timeout;   /* Session timeout in seconds */
    std::string     webcontrol_html_path;        /* Path to React build files */
    bool            webcontrol_spa_mode;         /* Enable SPA fallback routing */

    /* Database parameters (PARM_CAT_15) */
    std::string     database_type;
    std::string     database_dbname;
    std::string     database_host;
    int             database_port;
    std::string     database_user;
    std::string     database_password;
    int             database_busy_timeout;

    /* SQL parameters (PARM_CAT_16) */
    std::string     sql_event_start;
    std::string     sql_event_end;
    std::string     sql_movie_start;
    std::string     sql_movie_end;
    std::string     sql_pic_save;
};

/*
 * Camera device parameters
 *
 * These parameters are used by camera devices for capture, detection, and output.
 * This is the largest struct as cameras use most parameters.
 *
 * Categories included:
 * - PARM_CAT_01: camera
 * - PARM_CAT_02: source
 * - PARM_CAT_03: image
 * - PARM_CAT_04: overlay
 * - PARM_CAT_05: method (detection)
 * - PARM_CAT_06: masks
 * - PARM_CAT_07: detect
 * - PARM_CAT_08: scripts
 * - PARM_CAT_09: picture
 * - PARM_CAT_10: movies
 * - PARM_CAT_11: timelapse
 * - PARM_CAT_12: pipes
 * - PARM_CAT_14: streams
 * - PARM_CAT_17: tracking
 */
struct ctx_parm_cam {
    /* Camera device parameters (PARM_CAT_01) */
    std::string     device_name;
    int             device_id;
    std::string     config_dir;
    std::string     target_dir;
    int             watchdog_tmo;
    int             watchdog_kill;
    int             device_tmo;
    std::string     pause;
    std::string     schedule_params;
    std::string     picture_schedule_params;
    std::string     cleandir_params;

    /* Source parameters (PARM_CAT_02) */
    std::string     v4l2_device;
    std::string     v4l2_params;
    std::string     netcam_url;
    std::string     netcam_params;
    std::string     netcam_high_url;
    std::string     netcam_high_params;
    std::string     netcam_userpass;
    std::string     libcam_device;
    std::string     libcam_params;
    int             libcam_buffer_count;
    float           libcam_brightness;
    float           libcam_contrast;
    float           libcam_gain;
    bool            libcam_awb_enable;
    int             libcam_awb_mode;
    bool            libcam_awb_locked;
    int             libcam_colour_temp;
    float           libcam_colour_gain_r;
    float           libcam_colour_gain_b;

    /* Autofocus parameters */
    int             libcam_af_mode;         // 0=Manual, 1=Auto, 2=Continuous
    float           libcam_lens_position;   // Dioptres (0=infinity, 2=0.5m)
    int             libcam_af_range;        // 0=Normal, 1=Macro, 2=Full
    int             libcam_af_speed;        // 0=Normal, 1=Fast
    int             libcam_af_trigger;      // 0=Start AF scan, 1=Cancel (action param)

    /* Image parameters (PARM_CAT_03) */
    int             width;
    int             height;
    int             framerate;
    int             rotate;
    std::string     flip_axis;

    /* Overlay parameters (PARM_CAT_04) */
    std::string     locate_motion_mode;
    std::string     locate_motion_style;
    std::string     text_left;
    std::string     text_right;
    bool            text_changes;
    int             text_scale;
    std::string     text_event;

    /* Detection method parameters (PARM_CAT_05) - HOT PATH */
    bool            emulate_motion;
    int             threshold;
    int             threshold_maximum;
    int             threshold_sdevx;
    int             threshold_sdevy;
    int             threshold_sdevxy;
    int             threshold_ratio;
    int             threshold_ratio_change;
    bool            threshold_tune;
    std::string     secondary_method;
    std::string     secondary_params;

    /* Mask parameters (PARM_CAT_06) */
    int             noise_level;
    bool            noise_tune;
    std::string     despeckle_filter;
    std::string     area_detect;
    std::string     mask_file;
    std::string     mask_privacy;
    int             smart_mask_speed;

    /* Detect parameters (PARM_CAT_07) - HOT PATH */
    int             lightswitch_percent;
    int             lightswitch_frames;
    int             minimum_motion_frames;
    int             event_gap;
    int             static_object_time;
    int             post_capture;
    int             pre_capture;

    /* Script parameters (PARM_CAT_08) */
    std::string     on_event_start;
    std::string     on_event_end;
    std::string     on_picture_save;
    std::string     on_area_detected;
    std::string     on_motion_detected;
    std::string     on_movie_start;
    std::string     on_movie_end;
    std::string     on_camera_lost;
    std::string     on_camera_found;
    std::string     on_secondary_detect;
    std::string     on_action_user;
    std::string     on_sound_alert;

    /* Picture output parameters (PARM_CAT_09) */
    std::string     picture_output;
    std::string     picture_output_motion;
    std::string     picture_type;
    int             picture_quality;
    std::string     picture_exif;
    std::string     picture_filename;
    int             snapshot_interval;
    std::string     snapshot_filename;
    int             picture_max_per_event;  /* Maximum pictures per motion event (0=unlimited) */
    int             picture_min_interval;   /* Minimum milliseconds between pictures (0=no limit) */

    /* Movie output parameters (PARM_CAT_10) */
    bool            movie_output;
    bool            movie_output_motion;
    int             movie_max_time;
    int             movie_bps;
    int             movie_quality;
    std::string     movie_encoder_preset;
    std::string     movie_container;
    bool            movie_passthrough;
    std::string     movie_filename;
    std::string     movie_retain;
    bool            movie_all_frames;
    bool            movie_extpipe_use;
    std::string     movie_extpipe;

    /* Timelapse parameters (PARM_CAT_11) */
    int             timelapse_interval;
    std::string     timelapse_mode;
    int             timelapse_fps;
    std::string     timelapse_container;
    std::string     timelapse_filename;

    /* Pipe parameters (PARM_CAT_12) */
    std::string     video_pipe;
    std::string     video_pipe_motion;

    /* Stream parameters (PARM_CAT_14) */
    int             stream_preview_scale;
    bool            stream_preview_newline;
    std::string     stream_preview_params;
    std::string     stream_preview_method;
    bool            stream_preview_ptz;
    int             stream_quality;
    bool            stream_grey;
    bool            stream_motion;
    int             stream_maxrate;
    int             stream_scan_time;
    int             stream_scan_scale;

    /* Tracking/PTZ parameters (PARM_CAT_17) */
    bool            ptz_auto_track;
    int             ptz_wait;
    std::string     ptz_move_track;
    std::string     ptz_pan_left;
    std::string     ptz_pan_right;
    std::string     ptz_tilt_up;
    std::string     ptz_tilt_down;
    std::string     ptz_zoom_in;
    std::string     ptz_zoom_out;
};

/*
 * Sound device parameters (PARM_CAT_18)
 *
 * These parameters are only used by sound alert devices.
 * Smallest struct for minimal memory footprint.
 */
struct ctx_parm_snd {
    std::string             snd_device;
    std::string             snd_params;
    std::list<std::string>  snd_alerts;
    std::string             snd_window;
    bool                    snd_show;
};

#endif /* _INCLUDE_PARM_STRUCTS_HPP_ */
