/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
  **
  ** conf.c
  **
  ** Initially wrote conf.c as part of the drpoxy package
  ** thanks to Matthew Pratt and others for their additions.
  **
  ** Copyright 1999 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  **
  **
*/

#include <dirent.h>
#include <string.h>
#include "translate.h"
#include "motion.h"
#include "util.h"
#include "logger.h"

#define EXTENSION ".conf"

struct config conf_template = {
    /* Overall system configuration parameters */
    /* daemon is directly cast into the cnt context rather than conf */
    .setup_mode =                      FALSE,
    .pid_file =                        NULL,
    .log_file =                        NULL,
    .log_level =                       LEVEL_DEFAULT+10,
    .log_type =                        NULL,
    .quiet =                           TRUE,
    .native_language =                 TRUE,
    .watchdog_tmo =                    30,
    .watchdog_kill =                   10,
    .camera_name =                     NULL,
    .camera_id =                       0,
    .camera_dir =                      NULL,
    .target_dir =                      NULL,

    /* Capture device configuration parameters */
    .video_device =                    DEF_VIDEO_DEVICE,
    .video_params =                    NULL,
    .auto_brightness =                 0,
    .tuner_device =                    NULL,
    .roundrobin_frames =               1,
    .roundrobin_skip =                 1,
    .roundrobin_switchfilter =         FALSE,

    .netcam_url =                      NULL,
    .netcam_params =                   NULL,
    .netcam_high_url=                  NULL,
    .netcam_high_params =              NULL,
    .netcam_userpass =                 NULL,

    .mmalcam_name =                    NULL,
    .mmalcam_params =                  NULL,

    /* Image processing configuration parameters */
    .width =                           DEF_WIDTH,
    .height =                          DEF_HEIGHT,
    .framerate =                       DEF_MAXFRAMERATE,
    .minimum_frame_time =              0,
    .rotate =                          0,
    .flip_axis =                       "none",
    .locate_motion_mode =              "off",
    .locate_motion_style =             "box",
    .text_left =                       NULL,
    .text_right =                      DEF_TIMESTAMP,
    .text_changes =                    FALSE,
    .text_scale =                      1,
    .text_event =                      DEF_EVENTSTAMP,

    /* Motion detection configuration parameters */
    .emulate_motion =                  FALSE,
    .pause =                           FALSE,
    .threshold =                       DEF_CHANGES,
    .threshold_maximum =               0,
    .threshold_tune =                  FALSE,
    .noise_level =                     DEF_NOISELEVEL,
    .noise_tune =                      TRUE,
    .despeckle_filter =                NULL,
    .area_detect =                     NULL,
    .mask_file =                       NULL,
    .mask_privacy =                    NULL,
    .smart_mask_speed =                0,
    .lightswitch_percent =             0,
    .lightswitch_frames =              5,
    .minimum_motion_frames =           1,
    .event_gap =                       DEF_EVENT_GAP,
    .pre_capture =                     0,
    .post_capture =                    0,

    /* Script execution configuration parameters */
    .on_event_start =                  NULL,
    .on_event_end =                    NULL,
    .on_picture_save =                 NULL,
    .on_motion_detected =              NULL,
    .on_area_detected =                NULL,
    .on_movie_start =                  NULL,
    .on_movie_end =                    NULL,
    .on_camera_lost =                  NULL,
    .on_camera_found =                 NULL,

    /* Picture output configuration parameters */
    .picture_output =                  "off",
    .picture_output_motion =           FALSE,
    .picture_type =                    "jpeg",
    .picture_quality =                 75,
    .picture_exif =                    NULL,
    .picture_filename =                DEF_IMAGEPATH,

    /* Snapshot configuration parameters */
    .snapshot_interval =               0,
    .snapshot_filename =               DEF_SNAPPATH,

    /* Movie output configuration parameters */
    .movie_output =                    TRUE,
    .movie_output_motion =             FALSE,
    .movie_max_time =                  120,
    .movie_bps =                       400000,
    .movie_quality =                   60,
    .movie_codec =                     "mkv",
    .movie_duplicate_frames =          FALSE,
    .movie_passthrough =               FALSE,
    .movie_filename =                  DEF_MOVIEPATH,
    .movie_extpipe_use =               FALSE,
    .movie_extpipe =                   NULL,

    /* Timelapse movie configuration parameters */
    .timelapse_interval =              0,
    .timelapse_mode =                  DEF_TIMELAPSE_MODE,
    .timelapse_fps =                   30,
    .timelapse_codec =                 "mpg",
    .timelapse_filename =              DEF_TIMEPATH,

    /* Loopback device configuration parameters */
    .video_pipe =                      NULL,
    .video_pipe_motion =               NULL,

    /* Webcontrol configuration parameters */
    .webcontrol_port =                 0,
    .webcontrol_ipv6 =                 FALSE,
    .webcontrol_localhost =            TRUE,
    .webcontrol_parms =                0,
    .webcontrol_interface =            0,
    .webcontrol_auth_method =          0,
    .webcontrol_authentication =       NULL,
    .webcontrol_tls =                  FALSE,
    .webcontrol_cert =                 NULL,
    .webcontrol_key =                  NULL,
    .webcontrol_header_params =        NULL,

    /* Live stream configuration parameters */
    .stream_port =                     0,
    .stream_localhost =                TRUE,
    .stream_auth_method =              0,
    .stream_authentication =           NULL,
    .stream_tls =                      FALSE,
    .stream_header_params =            NULL,
    .stream_preview_scale =            25,
    .stream_preview_newline =          FALSE,
    .stream_preview_method =           0,
    .stream_quality =                  50,
    .stream_grey =                     FALSE,
    .stream_motion =                   FALSE,
    .stream_maxrate =                  1,
    .stream_limit =                    0,

    /* Database and SQL configuration parameters */
    .database_type =                   NULL,
    .database_dbname =                 NULL,
    .database_host =                   "localhost",
    .database_port =                   0,
    .database_user =                   NULL,
    .database_password =               NULL,
    .database_busy_timeout =           0,

    .sql_log_picture =                 FALSE,
    .sql_log_snapshot =                FALSE,
    .sql_log_movie =                   FALSE,
    .sql_log_timelapse =               FALSE,
    .sql_query_start =                 NULL,
    .sql_query_stop =                  NULL,
    .sql_query =                       NULL

};


/* Forward Declares */
static void malloc_strings(struct context *cnt);
static void copy_bool(struct context *cnt, char *str, int val_ptr);
static void copy_int(struct context *cnt, char *str, int val_ptr);
static const char *print_bool(struct context **cnt, char **str,int parm, unsigned int threadnr);
static const char *print_string(struct context **cnt,char **str, int parm, unsigned int threadnr);
static const char *print_int(struct context **cnt, char **str, int parm, unsigned int threadnr);
static const char *print_camera(struct context **cnt, char **str, int parm, unsigned int threadnr);
static struct context **read_camera_dir(struct context **cnt, char *str, int val);
static struct context **config_camera(struct context **cnt, const char *str, int val);

static void usage(void);
static void config_parms_intl(void);

/* Pointer magic to determine relative addresses of variables to a
   struct context pointer */
#define CNT_OFFSET(varname) ((long)&((struct context *)NULL)->varname)
#define CONF_OFFSET(varname) ((long)&((struct context *)NULL)->conf.varname)
#define TRACK_OFFSET(varname) ((long)&((struct context *)NULL)->track.varname)

/* The sequence of these within here determines how they are presented to user
 * Note daemon goes directly to cnt context rather than conf.
 * Descriptions are limited to one line and few to no references to values since
 * the motion_guide.html is our single source of documentation and historically
 * these descriptions were not updated with revisions.
 */
config_param config_params[] = {
    {
    "daemon",
    "############################################################\n"
    "# System control configuration parameters\n"
    "############################################################\n\n"
    "# Start in daemon (background) mode and release terminal.",
    1,
    CNT_OFFSET(daemon),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "setup_mode",
    "# Start in Setup-Mode, daemon disabled.",
    0,
    CONF_OFFSET(setup_mode),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "pid_file",
    "# File to store the process ID.",
    1,
    CONF_OFFSET(pid_file),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "log_file",
    "# File to write logs messages into.  If not defined stderr and syslog is used.",
    1,
    CONF_OFFSET(log_file),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "log_level",
    "# Level of log messages [1..9] (EMG, ALR, CRT, ERR, WRN, NTC, INF, DBG, ALL).",
    1,
    CONF_OFFSET(log_level),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "log_type",
    "# Filter to log messages by type (COR, STR, ENC, NET, DBL, EVT, TRK, VID, ALL).",
    1,
    CONF_OFFSET(log_type),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "quiet",
    "# Do not sound beeps when detecting motion",
    0,
    CONF_OFFSET(quiet),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "native_language",
    "# Native language support.",
    1,
    CONF_OFFSET(native_language),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "watchdog_tmo",
    "# Watchdog timeout.",
    1,
    CONF_OFFSET(watchdog_tmo),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "watchdog_kill",
    "# Watchdog kill.",
    1,
    CONF_OFFSET(watchdog_kill),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "camera_name",
    "# User defined name for the camera.",
    0,
    CONF_OFFSET(camera_name),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "camera_id",
    "# Numeric identifier for the camera.",
    0,
    CONF_OFFSET(camera_id),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    /* camera and camera_dir must be last in this list */
    {
    "target_dir",
    "# Target directory for pictures, snapshots and movies",
    0,
    CONF_OFFSET(target_dir),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "video_device",
    "# Video device (e.g. /dev/video0) to be used for capturing.",
    0,
    CONF_OFFSET(video_device),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "video_params",
    "# Parameters to control video device.  See motion_guide.html",
    0,
    CONF_OFFSET(video_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "auto_brightness",
    "# The Motion method to use to change the brightness/exposure on video device.",
    0,
    CONF_OFFSET(auto_brightness),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "tuner_device",
    "# Device name (e.g. /dev/tuner0) to be used for capturing when using tuner as source",
    0,
    CONF_OFFSET(tuner_device),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "roundrobin_frames",
    "# Number of frames to capture in each roundrobin step",
    0,
    CONF_OFFSET(roundrobin_frames),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "roundrobin_skip",
    "# Number of frames to skip before each roundrobin step",
    0,
    CONF_OFFSET(roundrobin_skip),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "roundrobin_switchfilter",
    "# Try to filter out noise generated by roundrobin",
    0,
    CONF_OFFSET(roundrobin_switchfilter),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "netcam_url",
    "# The full URL of the network camera stream.",
    0,
    CONF_OFFSET(netcam_url),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "netcam_params",
    "# The parameters for the network camera.",
    0,
    CONF_OFFSET(netcam_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "netcam_high_url",
    "# Optional high resolution URL for rtsp/rtmp cameras only.",
    0,
    CONF_OFFSET(netcam_high_url),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "netcam_high_params",
    "# The parameters for the high resolution network camera.",
    0,
    CONF_OFFSET(netcam_high_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "netcam_userpass",
    "# Username and password for network camera. Syntax username:password",
    0,
    CONF_OFFSET(netcam_userpass),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "mmalcam_name",
    "# Name of mmal camera (e.g. vc.ril.camera for pi camera).",
    0,
    CONF_OFFSET(mmalcam_name),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "mmalcam_params",
    "# Camera control parameters (see raspivid/raspistill tool documentation)",
    0,
    CONF_OFFSET(mmalcam_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "width",
    "############################################################\n"
    "# Image Processing configuration parameters\n"
    "############################################################\n\n"
    "# Image width in pixels.",
    0,
    CONF_OFFSET(width),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "height",
    "# Image height in pixels.",
    0,
    CONF_OFFSET(height),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "framerate",
    "# Maximum number of frames to be captured per second.",
    0,
    CONF_OFFSET(framerate),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "minimum_frame_time",
    "# Minimum time in seconds between capturing picture frames from the camera.",
    0,
    CONF_OFFSET(minimum_frame_time),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "rotate",
    "# Number of degrees to rotate image.",
    0,
    CONF_OFFSET(rotate),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "flip_axis",
    "# Flip image over a given axis",
    0,
    CONF_OFFSET(flip_axis),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "locate_motion_mode",
    "# Draw a locate box around the moving object.",
    0,
    CONF_OFFSET(locate_motion_mode),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "locate_motion_style",
    "# Set the look and style of the locate box.",
    0,
    CONF_OFFSET(locate_motion_style),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "text_left",
    "# Text to be overlayed in the lower left corner of images",
    0,
    CONF_OFFSET(text_left),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "text_right",
    "# Text to be overlayed in the lower right corner of images.",
    0,
    CONF_OFFSET(text_right),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "text_changes",
    "# Overlay number of changed pixels in upper right corner of images.",
    0,
    CONF_OFFSET(text_changes),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "text_scale",
    "# Scale factor for text overlayed on images.",
    0,
    CONF_OFFSET(text_scale),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "text_event",
    "# The special event conversion specifier %C",
    0,
    CONF_OFFSET(text_event),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "emulate_motion",
    "############################################################\n"
    "# Motion detection configuration parameters\n"
    "############################################################\n\n"
    "# Always save pictures and movies even if there was no motion.",
    0,
    CONF_OFFSET(emulate_motion),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "pause",
    "# pause motion detection.",
    0,
    CONF_OFFSET(pause),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "threshold",
    "# Threshold for number of changed pixels that triggers motion.",
    0,
    CONF_OFFSET(threshold),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "threshold_maximum",
    "# The maximum threshold for number of changed pixels that triggers motion.",
    0,
    CONF_OFFSET(threshold_maximum),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "threshold_tune",
    "# Enable tuning of the threshold down if possible.",
    0,
    CONF_OFFSET(threshold_tune),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "noise_level",
    "# Noise threshold for the motion detection.",
    0,
    CONF_OFFSET(noise_level),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "noise_tune",
    "# Automatically tune the noise threshold",
    0,
    CONF_OFFSET(noise_tune),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "despeckle_filter",
    "# Despeckle the image using (E/e)rode or (D/d)ilate or (l)abel.",
    0,
    CONF_OFFSET(despeckle_filter),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "area_detect",
    "# Area number used to trigger the on_area_detected script.",
    0,
    CONF_OFFSET(area_detect),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "mask_file",
    "# Full path and file name for motion detection mask PGM file.",
    0,
    CONF_OFFSET(mask_file),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "mask_privacy",
    "# Full path and file name for privacy mask PGM file.",
    0,
    CONF_OFFSET(mask_privacy),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "smart_mask_speed",
    "# The value defining how slow or fast the smart motion mask created and used.",
    0,
    CONF_OFFSET(smart_mask_speed),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "lightswitch_percent",
    "# Percentage of image that triggers a lightswitch detected.",
    0,
    CONF_OFFSET(lightswitch_percent),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "lightswitch_frames",
    "# When lightswitch is detected, ignore this many frames",
    0,
    CONF_OFFSET(lightswitch_frames),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "minimum_motion_frames",
    "# Number of images that must contain motion to trigger an event.",
    0,
    CONF_OFFSET(minimum_motion_frames),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "event_gap",
    "# Gap in seconds of no motion detected that triggers the end of an event.",
    0,
    CONF_OFFSET(event_gap),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "pre_capture",
    "# The number of pre-captured (buffered) pictures from before motion.",
    0,
    CONF_OFFSET(pre_capture),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "post_capture",
    "# Number of frames to capture after motion is no longer detected.",
    0,
    CONF_OFFSET(post_capture),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "on_event_start",
    "############################################################\n"
    "# Script execution configuration parameters\n"
    "############################################################\n\n"
    "# Command to be executed when an event starts.",
    0,
    CONF_OFFSET(on_event_start),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_event_end",
    "# Command to be executed when an event ends.",
    0,
    CONF_OFFSET(on_event_end),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_picture_save",
    "# Command to be executed when a picture is saved.",
    0,
    CONF_OFFSET(on_picture_save),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_area_detected",
    "# Command to be executed when motion in a predefined area is detected",
    0,
    CONF_OFFSET(on_area_detected),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_motion_detected",
    "# Command to be executed when motion is detected",
    0,
    CONF_OFFSET(on_motion_detected),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_movie_start",
    "# Command to be executed when a movie file is created.",
    0,
    CONF_OFFSET(on_movie_start),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_movie_end",
    "# Command to be executed when a movie file is closed.",
    0,
    CONF_OFFSET(on_movie_end),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_camera_lost",
    "# Command to be executed when a camera can't be opened or if it is lost",
    0,
    CONF_OFFSET(on_camera_lost),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "on_camera_found",
    "# Command to be executed when a camera that was lost has been found.",
    0,
    CONF_OFFSET(on_camera_found),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "picture_output",
    "############################################################\n"
    "# Picture output configuration parameters\n"
    "############################################################\n\n"
    "# Output pictures when motion is detected",
    0,
    CONF_OFFSET(picture_output),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "picture_output_motion",
    "# Output pictures with only the pixels moving object (ghost images)",
    0,
    CONF_OFFSET(picture_output_motion),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "picture_type",
    "# Format for the output pictures.",
    0,
    CONF_OFFSET(picture_type),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "picture_quality",
    "# The quality (in percent) to be used in the picture compression",
    0,
    CONF_OFFSET(picture_quality),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "picture_exif",
    "# Text to include in a JPEG EXIF comment",
    0,
    CONF_OFFSET(picture_exif),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "picture_filename",
    "# File name(without extension) for pictures relative to target directory",
    0,
    CONF_OFFSET(picture_filename),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "snapshot_interval",
    "############################################################\n"
    "# Snapshot output configuration parameters\n"
    "############################################################\n\n"
    "# Make automated snapshot every N seconds",
    0,
    CONF_OFFSET(snapshot_interval),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "snapshot_filename",
    "# File name(without extension) for snapshots relative to target directory",
    0,
    CONF_OFFSET(snapshot_filename),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_output",
    "############################################################\n"
    "# Movie output configuration parameters\n"
    "############################################################\n\n"
    "# Create movies of motion events.",
    0,
    CONF_OFFSET(movie_output),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_output_motion",
    "# Create movies of moving pixels of motion events.",
    0,
    CONF_OFFSET(movie_output_motion),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_max_time",
    "# Maximum length of movie in seconds.",
    0,
    CONF_OFFSET(movie_max_time),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_bps",
    "# The fixed bitrate to be used by the movie encoder. Ignore quality setting",
    0,
    CONF_OFFSET(movie_bps),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_quality",
    "# The encoding quality of the movie. (0=use bitrate. 1=worst quality, 100=best)",
    0,
    CONF_OFFSET(movie_quality),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_codec",
    "# Container/Codec to used for the movie. See motion_guide.html",
    0,
    CONF_OFFSET(movie_codec),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_duplicate_frames",
    "# Duplicate frames to achieve \"framerate\" fps.",
    0,
    CONF_OFFSET(movie_duplicate_frames),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_passthrough",
    "# Pass through from the camera to the movie without decode/encoding.",
    0,
    CONF_OFFSET(movie_passthrough),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "movie_filename",
    "# File name(without extension) for movies relative to target directory",
    0,
    CONF_OFFSET(movie_filename),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_extpipe_use",
    "# Use pipe and external encoder for creating movies.",
    0,
    CONF_OFFSET(movie_extpipe_use),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "movie_extpipe",
    "# Full path and options for external encoder of movies from raw images",
    0,
    CONF_OFFSET(movie_extpipe),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "timelapse_interval",
    "############################################################\n"
    "# Timelapse output configuration parameters\n"
    "############################################################\n\n"
    "# Interval in seconds between timelapse captures.",
    0,
    CONF_OFFSET(timelapse_interval),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "timelapse_mode",
    "# Timelapse file rollover mode. See motion_guide.html for options and uses.",
    0,
    CONF_OFFSET(timelapse_mode),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "timelapse_fps",
    "# Frame rate for timelapse playback",
    0,
    CONF_OFFSET(timelapse_fps),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "timelapse_codec",
    "# Container/Codec for timelapse movie.",
    0,
    CONF_OFFSET(timelapse_codec),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "timelapse_filename",
    "# File name(without extension) for timelapse movies relative to target directory",
    0,
    CONF_OFFSET(timelapse_filename),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "video_pipe",
    "############################################################\n"
    "# Loopback pipe configuration parameters\n"
    "############################################################\n\n"
    "# v4l2 loopback device to receive normal images",
    0,
    CONF_OFFSET(video_pipe),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "video_pipe_motion",
    "# v4l2 loopback device to receive motion images",
    0,
    CONF_OFFSET(video_pipe_motion),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "webcontrol_port",
    "############################################################\n"
    "# Webcontrol configuration parameters\n"
    "############################################################\n\n"
    "# Port number used for the webcontrol.",
    1,
    CONF_OFFSET(webcontrol_port),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "webcontrol_ipv6",
    "# Enable IPv6 addresses.",
    0,
    CONF_OFFSET(webcontrol_ipv6),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "webcontrol_localhost",
    "# Restrict webcontrol connections to the localhost.",
    1,
    CONF_OFFSET(webcontrol_localhost),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "webcontrol_parms",
    "# Type of configuration options to allow via the webcontrol.",
    1,
    CONF_OFFSET(webcontrol_parms),
    copy_int,
    print_int,
    WEBUI_LEVEL_NEVER
    },
    {
    "webcontrol_interface",
    "# Method that webcontrol should use for interface with user.",
    1,
    CONF_OFFSET(webcontrol_interface),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "webcontrol_auth_method",
    "# The authentication method for the webcontrol",
    0,
    CONF_OFFSET(webcontrol_auth_method),
    copy_int,
    print_int,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "webcontrol_authentication",
    "# Authentication string for the webcontrol. Syntax username:password",
    1,
    CONF_OFFSET(webcontrol_authentication),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "webcontrol_tls",
    "# Use ssl / tls for the webcontrol",
    0,
    CONF_OFFSET(webcontrol_tls),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "webcontrol_cert",
    "# Full path and file name of the certificate file for tls",
    1,
    CONF_OFFSET(webcontrol_cert),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "webcontrol_key",
    "# Full path and file name of the key file for tls",
    1,
    CONF_OFFSET(webcontrol_key),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "webcontrol_header_params",
    "# The header parameters for webcontrol",
    0,
    CONF_OFFSET(webcontrol_header_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },

    {
    "stream_port",
    "############################################################\n"
    "# Live stream configuration parameters\n"
    "############################################################\n\n"
    "# The port number for the live stream.",
    0,
    CONF_OFFSET(stream_port),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "stream_localhost",
    "# Restrict stream connections to the localhost.",
    0,
    CONF_OFFSET(stream_localhost),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "stream_auth_method",
    "# Authentication method for live stream.",
    0,
    CONF_OFFSET(stream_auth_method),
    copy_int,
    print_int,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "stream_authentication",
    "# The authentication string for the stream. Syntax username:password",
    1,
    CONF_OFFSET(stream_authentication),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "stream_tls",
    "# Use ssl / tls for stream.",
    0,
    CONF_OFFSET(stream_tls),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "stream_header_params",
    "# The header parameters for the stream",
    0,
    CONF_OFFSET(stream_header_params),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "stream_preview_scale",
    "# Percentage to scale the stream image on the webcontrol.",
    0,
    CONF_OFFSET(stream_preview_scale),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_preview_newline",
    "# Have the stream image start on a new line of the webcontrol",
    0,
    CONF_OFFSET(stream_preview_newline),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_preview_method",
    "# Method for showing stream on webcontrol.",
    0,
    CONF_OFFSET(stream_preview_method),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_quality",
    "# Quality of the jpeg images produced for stream.",
    0,
    CONF_OFFSET(stream_quality),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_grey",
    "# Provide the stream images in black and white",
    0,
    CONF_OFFSET(stream_grey),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_motion",
    "# Output frames at 1 fps when no motion is detected.",
    0,
    CONF_OFFSET(stream_motion),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_maxrate",
    "# Maximum framerate of images provided for stream",
    0,
    CONF_OFFSET(stream_maxrate),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "stream_limit",
    "# Limit the number of images per connection",
    0,
    CONF_OFFSET(stream_limit),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "database_type",
    "############################################################\n"
    "# Database and SQL Configuration parameters\n"
    "############################################################\n\n"
    "# The type of database being used if any.",
    0,
    CONF_OFFSET(database_type),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "database_dbname",
    "# Database name to use. For sqlite3, the full path and name.",
    0,
    CONF_OFFSET(database_dbname),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "database_host",
    "# The host on which the database is located",
    0,
    CONF_OFFSET(database_host),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "database_port",
    "# Port used by the database.",
    0,
    CONF_OFFSET(database_port),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "database_user",
    "# User account name for database.",
    0,
    CONF_OFFSET(database_user),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "database_password",
    "# User password for database.",
    0,
    CONF_OFFSET(database_password),
    copy_string,
    print_string,
    WEBUI_LEVEL_RESTRICTED
    },
    {
    "database_busy_timeout",
    "# Database wait for unlock time",
    0,
    CONF_OFFSET(database_busy_timeout),
    copy_int,
    print_int,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "sql_log_picture",
    "# Log to the database when creating motion triggered image file",
    0,
    CONF_OFFSET(sql_log_picture),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "sql_log_snapshot",
    "# Log to the database when creating a snapshot image file",
    0,
    CONF_OFFSET(sql_log_snapshot),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "sql_log_movie",
    "# Log to the database when creating motion triggered movie file",
    0,
    CONF_OFFSET(sql_log_movie),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "sql_log_timelapse",
    "# Log to the database when creating timelapse movie file",
    0,
    CONF_OFFSET(sql_log_timelapse),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "sql_query_start",
    "# SQL query at event start.  See motion_guide.html",
    0,
    CONF_OFFSET(sql_query_start),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "sql_query_stop",
    "# SQL query at event stop.  See motion_guide.html",
    0,
    CONF_OFFSET(sql_query_stop),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "sql_query",
    "# SQL query string that is sent to the database.  See motion_guide.html",
    0,
    CONF_OFFSET(sql_query),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    {
    "track_type",
    "############################################################\n"
    "# Tracking configuration parameters\n"
    "############################################################\n\n"
    "# Method used by tracking camera. See motion_guide.html",
    0,
    TRACK_OFFSET(type),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_auto",
    "# Enable auto tracking",
    0,
    TRACK_OFFSET(active),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_port",
    "# Serial port of motor",
    0,
    TRACK_OFFSET(port),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_motorx",
    "# Motor number for x-axis",
    0,
    TRACK_OFFSET(motorx),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_motorx_reverse",
    "# Set motorx reverse",
    0,
    TRACK_OFFSET(motorx_reverse),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_motory",
    "# Motor number for y-axis",
    0,
    TRACK_OFFSET(motory),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_motory_reverse",
    "# Set motory reverse",
    0,
    TRACK_OFFSET(motory_reverse),
    copy_bool,
    print_bool,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_maxx",
    "# Maximum value on x-axis",
    0,
    TRACK_OFFSET(maxx),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_minx",
    "# Minimum value on x-axis",
    0,
    TRACK_OFFSET(minx),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_maxy",
    "# Maximum value on y-axis",
    0,
    TRACK_OFFSET(maxy),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_miny",
    "# Minimum value on y-axis",
    0,
    TRACK_OFFSET(miny),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_homex",
    "# Center value on x-axis",
    0,
    TRACK_OFFSET(homex),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_homey",
    "# Center value on y-axis",
    0,
    TRACK_OFFSET(homey),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_iomojo_id",
    "# ID of an iomojo camera if used",
    0,
    TRACK_OFFSET(iomojo_id),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_step_angle_x",
    "# Angle in degrees the camera moves per step on the X-axis with auto-track",
    0,
    TRACK_OFFSET(step_angle_x),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_step_angle_y",
    "# Angle in degrees the camera moves per step on the Y-axis with auto-track.",
    0,
    TRACK_OFFSET(step_angle_y),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_move_wait",
    "# Delay to wait for after tracking movement as number of picture frames.",
    0,
    TRACK_OFFSET(move_wait),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_speed",
    "# Speed to set the motor to (stepper motor option)",
    0,
    TRACK_OFFSET(speed),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_stepsize",
    "# Number of steps to make (stepper motor option)",
    0,
    TRACK_OFFSET(stepsize),
    copy_int,
    print_int,
    WEBUI_LEVEL_LIMITED
    },
    {
    "track_generic_move",
    "# Command to execute to move a camera in generic tracking mode",
    0,
    TRACK_OFFSET(generic_move),
    copy_string,
    print_string,
    WEBUI_LEVEL_LIMITED
    },
    {
    "camera",
    "##############################################################\n"
    "# Camera config files - One for each camera.\n"
    "##############################################################",
    1,
    0,
    copy_string,
    print_camera,
    WEBUI_LEVEL_ADVANCED
    },
    /* using a conf.d style camera addition */
    {
    "camera_dir",
    "##############################################################\n"
    "# Directory to read '.conf' files for cameras.\n"
    "##############################################################",
    1,
    CONF_OFFSET(camera_dir),
    copy_string,
    print_string,
    WEBUI_LEVEL_ADVANCED
    },
    { NULL, NULL, 0, 0, NULL, NULL, 0 }
};

/*
 * Array of deprecated config options:
 * When deprecating an option, remove it from above (config_params array)
 * and create an entry in this array of name, last version, info,
 * and (if applicable) a replacement conf value and copy funcion.
 * Upon reading a deprecated config option, a warning will be logged
 * with the given information and last version it was used in.
 * If set, the given value will be copied into the conf value
 * for backwards compatibility.
 */
dep_config_param dep_config_params[] = {
    {
    "thread",
    "3.4.1",
    "The \"thread\" option has been replaced by the \"camera\"",
    0,
    "camera",
    copy_string
    },
    {
    "ffmpeg_timelapse",
    "4.0.1",
    "\"ffmpeg_timelapse\" replaced with \"timelapse_interval\"",
    CONF_OFFSET(timelapse_interval),
    "timelapse_interval",
    copy_int
    },
    {
    "ffmpeg_timelapse_mode",
    "4.0.1",
    "\"ffmpeg_timelapse_mode\" replaced with \"timelapse_mode\"",
    CONF_OFFSET(timelapse_mode),
    "timelapse_mode",
    copy_string
    },
    {
    "brightness",
    "4.1.1",
    "\"brightness\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "contrast",
    "4.1.1",
    "\"contrast\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "saturation",
    "4.1.1",
    "\"saturation\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "hue",
    "4.1.1",
    "\"hue\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "power_line_frequency",
    "4.1.1",
    "\"power_line_frequency\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "text_double",
    "4.1.1",
    "\"text_double\" replaced with \"text_scale\"",
    CONF_OFFSET(text_scale),
    "text_scale",
    NULL
    },
    {
    "webcontrol_html_output",
    "4.1.1",
    "\"webcontrol_html_output\" replaced with \"webcontrol_interface\"",
    CONF_OFFSET(webcontrol_interface),
    "webcontrol_interface",
    NULL
    },
    {
     "lightswitch",
    "4.1.1",
    "\"lightswitch\" replaced with \"lightswitch_percent\"",
    CONF_OFFSET(lightswitch_percent),
    "lightswitch_percent",
    copy_int
    },
    {
    "ffmpeg_output_movies",
    "4.1.1",
    "\"ffmpeg_output_movies\" replaced with \"movie_output\"",
    CONF_OFFSET(movie_output),
    "movie_output",
    copy_bool
    },
    {
    "ffmpeg_output_debug_movies",
    "4.1.1",
    "\"ffmpeg_output_debug_movies\" replaced with \"movie_output_motion\"",
    CONF_OFFSET(movie_output_motion),
    "movie_output_motion",
    copy_bool
    },
    {
    "max_movie_time",
    "4.1.1",
    "\"max_movie_time\" replaced with \"movie_max_time\"",
    CONF_OFFSET(movie_max_time),
    "movie_max_time",
    copy_int
    },
    {
    "ffmpeg_bps",
    "4.1.1",
    "\"ffmpeg_bps\" replaced with \"movie_bps\"",
    CONF_OFFSET(movie_bps),
    "movie_bps",
    copy_int
    },
    {
    "ffmpeg_variable_bitrate",
    "4.1.1",
    "\"ffmpeg_variable_bitrate\" replaced with \"movie_quality\"",
    CONF_OFFSET(movie_quality),
    "movie_quality",
    copy_int
    },
    {
    "ffmpeg_video_codec",
    "4.1.1",
    "\"ffmpeg_video_codec\" replaced with \"movie_codec\"",
    CONF_OFFSET(movie_codec),
    "movie_codec",
    copy_string
    },
    {
    "ffmpeg_duplicate_frames",
    "4.1.1",
    "\"ffmpeg_duplicate_frames\" replaced with \"movie_duplicate_frames\"",
    CONF_OFFSET(movie_duplicate_frames),
    "movie_duplicate_frames",
    copy_bool
    },
    {
    "ffmpeg_passthrough",
    "4.1.1",
    "\"ffmpeg_passthrough\" replaced with \"movie_passthrough\"",
    CONF_OFFSET(movie_passthrough),
    "movie_passthrough",
    copy_bool
    },
    {
    "use_extpipe",
    "4.1.1",
    "\"use_extpipe\" replaced with \"movie_extpipe_use\"",
    CONF_OFFSET(movie_extpipe_use),
    "movie_extpipe_use",
    copy_bool
    },
    {
    "extpipe",
    "4.1.1",
    "\"extpipe\" replaced with \"movie_extpipe\"",
    CONF_OFFSET(movie_extpipe),
    "movie_extpipe",
    copy_string
    },
    {
    "output_pictures",
    "4.1.1",
    "\"output_pictures\" replaced with \"picture_output\"",
    CONF_OFFSET(picture_output),
    "picture_output",
    copy_string
    },
    {
    "output_debug_pictures",
    "4.1.1",
    "\"output_debug_pictures\" replaced with \"picture_output_motion\"",
    CONF_OFFSET(picture_output_motion),
    "picture_output_motion",
    copy_bool
    },
    {
    "quality",
    "4.1.1",
    "\"quality\" replaced with \"picture_quality\"",
    CONF_OFFSET(picture_quality),
    "picture_quality",
    copy_int
    },
    {
    "exif_text",
    "4.1.1",
    "\"exif_text\" replaced with \"picture_exif\"",
    CONF_OFFSET(picture_exif),
    "picture_exif",
    copy_string
    },
    {
    "motion_video_pipe",
    "4.1.1",
    "\"motion_video_pipe\" replaced with \"video_pipe_motion\"",
    CONF_OFFSET(video_pipe_motion),
    "video_pipe_motion",
    copy_string
    },
    {
    "ipv6_enabled",
    "4.1.1",
    "\"ipv6_enabled\" replaced with \"webcontrol_ipv6\"",
    CONF_OFFSET(webcontrol_ipv6),
    "webcontrol_ipv6",
    copy_bool
    },
    {
    "switchfilter",
    "4.1.1",
    "\"switchfilter\" replaced with \"roundrobin_switchfilter\"",
    CONF_OFFSET(roundrobin_switchfilter),
    "roundrobin_switchfilter",
    copy_bool
    },
    {
    "logfile",
    "4.1.1",
    "\"logfile\" replaced with \"log_file\"",
    CONF_OFFSET(log_file),
    "log_file",
    copy_string
    },
    {
    "process_id_file",
    "4.1.1",
    "\"process_id_file\" replaced with \"pid_file\"",
    CONF_OFFSET(pid_file),
    "pid_file",
    copy_string
    },
    {
    "vid_control_params",
    "4.3.2",
    "\"vid_control_params\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "v4l2_palette",
    "4.3.2",
    "\"v4l2_palette\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "input",
    "4.3.2",
    "\"input\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "norm",
    "4.3.2",
    "\"norm\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "frequency",
    "4.3.2",
    "\"frequency\" replaced with \"video_params\"",
    CONF_OFFSET(video_params),
    "video_params",
    NULL
    },
    {
    "rtsp_uses_tcp",
    "4.3.2",
    "\"rtsp_uses_tcp\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_use_tcp",
    "4.3.2",
    "\"netcam_use_tcp\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_rate",
    "4.3.2",
    "\"netcam_rate\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_ratehigh",
    "4.3.2",
    "\"netcam_ratehigh\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_decoder",
    "4.3.2",
    "\"netcam_decoder\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_proxy",
    "4.3.2",
    "\"netcam_proxy\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_keepalive",
    "4.3.2",
    "\"netcam_keepalive\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "netcam_tolerant_check",
    "4.3.2",
    "\"netcam_tolerant_check\" replaced with \"netcam_params\"",
    CONF_OFFSET(netcam_params),
    "netcam_params",
    NULL
    },
    {
    "videodevice",
    "4.3.2",
    "\"videodevice\" replaced with \"video_device\"",
    CONF_OFFSET(video_device),
    "video_device",
    copy_string
    },
    {
    "tunerdevice",
    "4.3.2",
    "\"tunerdevice\" replaced with \"tuner_device\"",
    CONF_OFFSET(tuner_device),
    "tuner_device",
    copy_string
    },
    {
    "mmalcam_control_params",
    "4.3.2",
    "\"mmalcam_control_params\" replaced with \"mmalcam_params\"",
    CONF_OFFSET(mmalcam_params),
    "mmalcam_params",
    copy_string
    },
    {
    "netcam_highres",
    "4.3.2",
    "\"netcam_highres\" replaced with \"netcam_high_url\"",
    CONF_OFFSET(netcam_high_url),
    "netcam_high_url",
    copy_string
    },
    {
    "webcontrol_cors_header",
    "4.3.2",
    "\"webcontrol_cors_header\" replaced with \"webcontrol_header_params\"",
    CONF_OFFSET(webcontrol_header_params),
    "webcontrol_header_params",
    NULL
    },
    {
    "stream_cors_header",
    "4.3.2",
    "\"stream_cors_header\" replaced with \"stream_header_params\"",
    CONF_OFFSET(stream_header_params),
    "stream_header_params",
    NULL
    },

    { NULL, NULL, NULL, 0, NULL, NULL}
};

/**
 * copy_bool
 *      Assigns a config option to a new boolean value.
 */
static void copy_bool(struct context *cnt, char *str, int val_ptr)
{
    void *tmp;

    tmp = (char *)cnt + (int)val_ptr;
    if (mystreq(str, "1") || mystrceq(str, "yes") || mystrceq(str, "on")) {
        *((int *)tmp) = 1;
    } else {
        *((int *)tmp) = 0;
    }
}

/**
 * copy_int
 *      Assigns a config option to a new integer value.
 */
static void copy_int(struct context *cnt, char *str, int val_ptr)
{
    void *tmp;

    tmp = (char *)cnt + val_ptr;
    if (mystrceq(str, "yes") || mystrceq(str, "on")) {
        *((int *)tmp) = 1;
    } else if (mystrceq(str, "no") || mystrceq(str, "off")) {
        *((int *)tmp) = 0;
    } else {
        *((int *)tmp) = atoi(str);
    }
}

/**
 * copy_string
 *      Assigns a new string value to a config option.
 */
void copy_string(struct context *cnt, char *str, int val_ptr)
{
    char **tmp;

    tmp = (char **)((char *)cnt + val_ptr);

    if (*tmp != NULL) {
        free(*tmp);
        *tmp = NULL;
    }

    if (str != NULL) {
        if (strlen(str) > 0) {
            *tmp = mymalloc(strlen(str)+1);
            sprintf(*tmp,"%s",str);
        }
    }
}

/**
 * copy_video_params
 *      Assign old values into new config option.
 */
static void copy_video_params(struct context *cnt, char *config_val, int config_indx)
{
    int indx, parmval;
    struct params_context *params;
    struct params_context *params_depr;

    if (config_val == NULL) {
        return;
    }

    /* If the depreciated option is the default, then just return */
    parmval = atoi(config_val);
    if (mystreq(dep_config_params[config_indx].name,"power_line_frequency") &&
        (parmval == -1)) {
        return;
    }

    if ((mystreq(dep_config_params[config_indx].name,"brightness") ||
         mystreq(dep_config_params[config_indx].name,"contrast") ||
         mystreq(dep_config_params[config_indx].name,"saturation") ||
         mystreq(dep_config_params[config_indx].name,"hue")) &&
        (parmval == 0)) {
        return;
    }

    params = mymalloc(sizeof(struct params_context));
    util_parms_parse(params, cnt->conf.video_params, TRUE);

    if (mystreq(dep_config_params[config_indx].name,"power_line_frequency")) {
        util_parms_add_update(params, "power line frequency", config_val);
    } else if (mystreq(dep_config_params[config_indx].name,"v4l2_palette")) {
        util_parms_add_update(params, "palette", config_val);
    } else if (mystreq(dep_config_params[config_indx].name,"vid_control_params")) {
        params_depr = mymalloc(sizeof(struct params_context));
        util_parms_parse(params_depr, config_val, TRUE);
        for (indx = 0; indx < params_depr->params_count; indx++) {
            util_parms_add_update(params
                , params_depr->params_array[indx].param_name
                , params_depr->params_array[indx].param_value);
        }
        util_parms_free(params_depr);
        free(params_depr);
    } else {
        util_parms_add_update(params, dep_config_params[config_indx].name, config_val);
    }

    util_parms_update(params, cnt, "video_params");
    util_parms_free(params);
    free(params);

    return;
}

/**
 * copy_netcam_params
 *      Assigns a new string value to a config option.
 */
static void copy_netcam_params(struct context *cnt, char *config_val, int config_indx)
{
    struct params_context *params;

    if (config_val == NULL) {
        return;
    }

    if (mystreq(dep_config_params[config_indx].name,"netcam_ratehigh")) {
        params = mymalloc(sizeof(struct params_context));
        util_parms_parse(params, cnt->conf.netcam_high_params, TRUE);
        util_parms_add_update(params, "capture_rate", config_val);
        util_parms_update(params, cnt, "netcam_high_params");
        util_parms_free(params);
        free(params);

    } else {
        params = mymalloc(sizeof(struct params_context));
        util_parms_parse(params, cnt->conf.netcam_params, TRUE);

        if (mystreq(dep_config_params[config_indx].name,"netcam_use_tcp") ||
            mystreq(dep_config_params[config_indx].name,"rtsp_uses_tcp")) {
            if (mystrceq(config_val,"on")) {
                util_parms_add_update(params, "rtsp_transport", "tcp");
            } else {
                util_parms_add_update(params, "rtsp_transport", "udp");
            }

        } else if (mystreq(dep_config_params[config_indx].name,"netcam_decoder")) {
            util_parms_add_update(params, "decoder",config_val);

        } else if (mystreq(dep_config_params[config_indx].name,"netcam_rate")) {
            util_parms_add_update(params, "capture_rate", config_val);

        } else if (mystreq(dep_config_params[config_indx].name,"netcam_proxy")) {
            util_parms_add_update(params, "proxy", config_val);

        } else if (mystreq(dep_config_params[config_indx].name,"netcam_keepalive")) {
            util_parms_add_update(params, "keepalive", config_val);

        } else if (mystreq(dep_config_params[config_indx].name,"netcam_tolerant_check")) {
            util_parms_add_update(params, "tolerant_check", config_val);
        }

        util_parms_update(params, cnt, "netcam_params");
        util_parms_free(params);
        free(params);
    }

    return;
}

/**
 * copy_webcontrol_header
 *      Assigns a new string value to a config option.
 */
static void copy_webcontrol_header(struct context *cnt, char *config_val)
{
    struct params_context *params;

    if (config_val == NULL) {
        return;
    }

    params = mymalloc(sizeof(struct params_context));
    util_parms_parse(params, cnt->conf.webcontrol_header_params, TRUE);
    util_parms_add_update(params, "Access-Control-Allow-Origin", config_val);
    util_parms_update(params, cnt, "webcontrol_header_params");
    util_parms_free(params);
    free(params);

    return;
}

/**
 * copy_stream_header
 *      Assigns a new string value to a config option.
 */
static void copy_stream_header(struct context *cnt, char *config_val)
{
    struct params_context *params;

    if (config_val == NULL) {
        return;
    }

    params = mymalloc(sizeof(struct params_context));
    util_parms_parse(params, cnt->conf.stream_header_params, TRUE);
    util_parms_add_update(params, "Access-Control-Allow-Origin", config_val);
    util_parms_update(params, cnt, "stream_header_params");
    util_parms_free(params);
    free(params);

    return;
}

/**
 * copy_text_double
 *      Converts the bool of text_double to a 1 or 2 in text_scale
 */
static void copy_text_double(struct context *cnt, char *config_val)
{
    if (mystreq(config_val, "1") ||
        mystrceq(config_val, "yes") ||
        mystrceq(config_val, "on")) {
        cnt->conf.text_scale = 2;
    } else {
        cnt->conf.text_scale = 1;
    }
    return;
}

/**
 * copy_html_output
 *      Converts the webcontrol_html_output to the webcontrol_interface option.
 */
static void copy_html_output(struct context *cnt, char *config_val)
{
    if (mystreq(config_val, "1") ||
        mystrceq(config_val, "yes") ||
        mystrceq(config_val, "on")) {
        cnt->conf.webcontrol_interface = 0;
    } else {
        cnt->conf.webcontrol_interface = 1;
    }
    return;
}

/**
 * conf_cmdline
 *      Sets the conf struct options as defined by the Command-line.
 *      Any option already set from a config file are overridden.
 *
 * Returns nothing.
 */
static void conf_cmdline(struct context *cnt, int thread)
{
    struct config *conf = &cnt->conf;
    int c;

    while ((c = getopt(conf->argc, conf->argv, "bc:d:hmns?p:k:l:")) != EOF) {
        switch (c) {
        case 'c':
            if (thread == -1) {
                strcpy(cnt->conf_filename, optarg);
            }
            break;
        case 'b':
            cnt->daemon = 1;
            break;
        case 'n':
            cnt->daemon = 0;
            break;
        case 's':
            conf->setup_mode = 1;
            break;
        case 'd':
            /* No validation - just take what user gives. */
            if (thread == -1) {
                cnt->log_level = (unsigned int)atoi(optarg);
            }
            break;
        case 'k':
            if (thread == -1) {
                strncpy(cnt->log_type_str, optarg, sizeof(cnt->log_type_str) - 1);
                cnt->log_type_str[sizeof(cnt->log_type_str) - 1] = '\0';
            }
            break;
        case 'p':
            if (thread == -1) {
                strncpy(cnt->pid_file, optarg, sizeof(cnt->pid_file) - 1);
                cnt->pid_file[sizeof(cnt->pid_file) - 1] = '\0';
            }
            break;
        case 'l':
            if (thread == -1) {
                strncpy(cnt->log_file, optarg, sizeof(cnt->log_file) - 1);
                cnt->log_file[sizeof(cnt->log_file) - 1] = '\0';
            }
            break;
        case 'm':
            cnt->pause = 1;
            break;
        case 'h':
        case '?':
        default:
             usage();
             exit(1);
        }
    }

    optind = 1;
}

/**
 * conf_cmdparse
 *      Sets a config option given by 'cmd' to the value given by 'arg1'.
 *      Based on the name of the option it searches through the struct 'config_params'
 *      for an option where the config_params[i].param_name matches the option.
 *      By calling the function pointed to by config_params[i].copy the option gets
 *      assigned.
 *
 * Returns context struct.
 */
struct context **conf_cmdparse(struct context **cnt, char *param_name, char *param_val)
{
    int indx;

    if (param_name == NULL) {
        return cnt;
    }

    indx = 0;
    while (config_params[indx].param_name != NULL) {
        if (mystrceq(param_name, config_params[indx].param_name)) {
            if (mystreq(param_name, "camera"))  {
                cnt = config_camera(cnt, param_val, config_params[indx].conf_value);
            } else if (mystreq(param_name, "camera_dir"))  {
                cnt = read_camera_dir(cnt, param_val, config_params[indx].conf_value);
            } else {
                config_params[indx].copy(*cnt, param_val, config_params[indx].conf_value);
            }

            return cnt;
        }
        indx++;
    }

    /* Now check deprecated options */
    indx = 0;
    while (dep_config_params[indx].name != NULL) {
        if (mystreq(param_name, dep_config_params[indx].name)) {
            MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("%s after version %s")
                , dep_config_params[indx].info
                , dep_config_params[indx].last_version);

            if (mystreq(dep_config_params[indx].name,"brightness") ||
                mystreq(dep_config_params[indx].name,"contrast") ||
                mystreq(dep_config_params[indx].name,"saturation") ||
                mystreq(dep_config_params[indx].name,"hue") ||
                mystreq(dep_config_params[indx].name,"power_line_frequency") ||
                mystreq(dep_config_params[indx].name,"v4l2_palette") ||
                mystreq(dep_config_params[indx].name,"input") ||
                mystreq(dep_config_params[indx].name,"norm") ||
                mystreq(dep_config_params[indx].name,"frequency") ||
                mystreq(dep_config_params[indx].name,"vid_control_params")) {
                copy_video_params(*cnt, param_val, indx);

            } else if (mystreq(dep_config_params[indx].name,"netcam_decoder") ||
                mystreq(dep_config_params[indx].name,"netcam_use_tcp") ||
                mystreq(dep_config_params[indx].name,"rtsp_uses_tcp") ||
                mystreq(dep_config_params[indx].name,"netcam_rate")  ||
                mystreq(dep_config_params[indx].name,"netcam_ratehigh") ||
                mystreq(dep_config_params[indx].name,"netcam_proxy") ||
                mystreq(dep_config_params[indx].name,"netcam_tolerant_check") ||
                mystreq(dep_config_params[indx].name,"netcam_keepalive")) {
                copy_netcam_params(*cnt, param_val, indx);

            } else if (mystreq(dep_config_params[indx].name,"webcontrol_cors_header"))  {
                copy_webcontrol_header(*cnt, param_val);

            } else if (mystreq(dep_config_params[indx].name,"stream_cors_header"))  {
                copy_stream_header(*cnt, param_val);

            } else if (mystreq(dep_config_params[indx].name,"text_double"))  {
                copy_text_double(*cnt, param_val);

            } else if (mystreq(dep_config_params[indx].name,"webcontrol_html_output"))  {
                copy_html_output(*cnt, param_val);

            } else if (mystreq(dep_config_params[indx].name,"thread"))  {
                cnt = config_camera(cnt, param_val, dep_config_params[indx].conf_value);

            } else if (dep_config_params[indx].copy != NULL) {
                dep_config_params[indx].copy(*cnt, param_val, dep_config_params[indx].conf_value);
            }
            return cnt;
        }
        indx++;
    }

    /* If we get here, it's unknown to us. */
    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), param_name);
    return cnt;
}

/**
 * conf_process
 *      Walks through an already open config file line by line
 *      Any line starting with '#' or ';' or empty lines are ignored as a comments.
 *      Any non empty line is processed so that the first word is the name of an option 'cmd'
 *      and the rest of the line is the argument 'arg1'
 *      White space before the first word, between option and argument and end of the line
 *      is discarded. A '=' between option and first word in argument is also discarded.
 *      Quotation marks round the argument are also discarded.
 *      For each option/argument pair the function conf_cmdparse is called which takes
 *      care of assigning the value to the option in the config structures.
 *
 * Returns context struct.
 */
static struct context **conf_process(struct context **cnt, FILE *fp)
{
    /* Process each line from the config file. */

    char line[PATH_MAX], *cmd = NULL, *arg1 = NULL;
    char *beg = NULL, *end = NULL;

    while (fgets(line, PATH_MAX-1, fp)) {
        if (!(line[0] == '#' || line[0] == ';' || strlen(line) <  2)) {
            /* skipcomment */
            arg1 = NULL;

            /* Trim white space and any CR or LF at the end of the line. */
            end = line + strlen(line) - 1; /* Point to the last non-null character in the string. */
            while (end >= line && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
                end--;
            }

            *(end+1) = '\0';

            /* If line is only whitespace we continue to the next line. */
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


                /*
                 * If argument is in "" we will strip them off
                 * It is important that we can use "" so that we can use
                 * leading spaces in text_left and text_right.
                 */
                /* For the config values of 'params' we leave on the quotes.
                 * These parameters use the util_parms_parse routine that
                 * will strip away the quotes if they are there
                 */
                if (strstr(cmd, "params") == NULL) {
                    if ((beg[0] == '"' && beg[strlen(beg)-1] == '"') ||
                        (beg[0] == '\'' && beg[strlen(beg)-1] == '\'')) {
                        beg[strlen(beg)-1] = '\0';
                        beg++;
                    }
                }

                arg1 = beg; /* Argument starts here */
            }
            /* Else arg1 stays null pointer */

            cnt = conf_cmdparse(cnt, cmd, arg1);
        }
    }

    return cnt;
}

/**
 * conf_print
 *       Is used to write out the config file(s) motion.conf and any camera
 *       config files. The function is called when using http remote control.
 *
 * Returns nothing.
 */
void conf_print(struct context **cnt)
{
    const char *retval;
    char *val;
    unsigned int i, thread;
    FILE *conffile;

    for (thread = 0; cnt[thread]; thread++) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,_("Writing config file to %s")
            ,cnt[thread]->conf_filename);

        conffile = myfopen(cnt[thread]->conf_filename, "w");

        if (!conffile) {
            continue;
        }

        char timestamp[32];
        time_t now = time(0);
        strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%S", localtime(&now));

        fprintf(conffile, "# %s\n", cnt[thread]->conf_filename);
        fprintf(conffile, "#\n# This config file was generated by motion " VERSION "\n");
        fprintf(conffile, "# at %s\n", timestamp);
        fprintf(conffile, "\n\n");

        for (i = 0; config_params[i].param_name; i++) {
            retval = config_params[i].print(cnt, NULL, i, thread);
            /* If config parameter has a value (not NULL) print it to the config file. */
            if (retval) {
                fprintf(conffile, "%s\n", config_params[i].param_help);
                /*
                 * If the option is a text_* and first char is a space put
                 * quotation marks around to allow leading spaces.
                 */
                if (strncmp(config_params[i].param_name, "text", 4) || strncmp(retval, " ", 1)) {
                    fprintf(conffile, "%s %s\n\n", config_params[i].param_name, retval);
                } else {
                    fprintf(conffile, "%s \"%s\"\n\n", config_params[i].param_name, retval);
                }
            } else {
                val = NULL;
                config_params[i].print(cnt, &val, i, thread);
                /*
                 * It can either be a camera file parameter or a disabled parameter.
                 * If it is a camera parameter write it out.
                 * Else write the disabled option to the config file but with a
                 * comment mark in front of the parameter name.
                 */
                if (val) {
                    fprintf(conffile, "%s\n", config_params[i].param_help);

                    if (strlen(val) > 0) {
                        fprintf(conffile, "%s\n", val);
                    } else {
                        fprintf(conffile, "; camera %s/camera1.conf\n", sysconfdir);
                    }

                    free(val);
                } else if (thread == 0) {
                    char value[PATH_MAX];
                    /* The 'camera_dir' option should keep the installed default value */
                    if (!strncmp(config_params[i].param_name, "camera_dir", 10)) {
                        sprintf(value, "%s", sysconfdir"/conf.d");
                    } else {
                        sprintf(value, "%s", "value");
                    }

                    fprintf(conffile, "%s\n", config_params[i].param_help);
                    fprintf(conffile, "; %s %s\n\n", config_params[i].param_name, value);
                }
            }
        }

        fprintf(conffile, "\n");
        myfclose(conffile);
        conffile = NULL;
    }
}

/**
 * conf_load
 * Is the main function, called from motion.c
 * The function sets the important context structure "cnt" including
 * loading the config parameters from config files and Command-line.
 * The following takes place in the function:
 * - The default start values for cnt stored in the struct conf_template
 *   are copied to cnt[0] which is the default context structure common to
 *   all threads.
 * - All config (cnt.conf) struct members pointing to a string are changed
 *   so that they point to a malloc'ed piece of memory containing a copy of
 *   the string given in conf_template.
 * - motion.conf is opened and processed. The process populates the cnt[0] and
 *   for each camera config file it populates a cnt[1], cnt[2]... for each
 *   camera.
 * - Finally it processes the options given in the Command-line. This is done
 *   for each camera cnt[i] so that the Command-line options overrides any
 *   option given by motion.conf or a camera config file.
 *
 * Returns context struct.
 */
struct context **conf_load(struct context **cnt)
{
    FILE *fp = NULL;
    char filename[PATH_MAX];
    int i, retcd;
    /* We preserve argc and argv because they get overwritten by the memcpy command. */
    char **argv = cnt[0]->conf.argv;
    int argc = cnt[0]->conf.argc;

    /*
     * Copy the template config structure with all the default config values
     * into cnt[0]->conf
     */
    memcpy(&cnt[0]->conf, &conf_template, sizeof(struct config));

    /*
     * For each member of cnt[0] which is a pointer to a string
     * if the member points to a string in conf_template and is not NULL.
     * 1. Reserve (malloc) memory for the string.
     * 2. Copy the conf_template given string to the reserved memory.
     * 3. Change the cnt[0] member (char*) pointing to the string in reserved memory.
     * This ensures that we can free and malloc the string when changed
     * via http remote control or config file or Command-line options.
     */
    malloc_strings(cnt[0]);

    /* Restore the argc and argv */
    cnt[0]->conf.argv = argv;
    cnt[0]->conf.argc = argc;

    /*
     * Open the motion.conf file. We try in this sequence:
     * 1. Command-line
     * 2. current working directory
     * 3. $HOME/.motion/motion.conf
     * 4. sysconfdir/motion.conf
     */
    /* Get filename , pid file & log file from Command-line. */
    cnt[0]->log_type_str[0] = 0;
    cnt[0]->conf_filename[0] = 0;
    cnt[0]->pid_file[0] = 0;
    cnt[0]->log_file[0] = 0;
    cnt[0]->log_level = -1;

    conf_cmdline(cnt[0], -1);

    if (cnt[0]->conf_filename[0]) {
        /* User has supplied filename on Command-line. */
        strncpy(filename, cnt[0]->conf_filename, PATH_MAX-1);
        filename[PATH_MAX-1] = '\0';
        fp = fopen (filename, "r");
    }

    if (!fp) {  /* Command-line didn't work, try current dir. */
        char path[PATH_MAX];

        if (cnt[0]->conf_filename[0]) {
            MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
                ,_("Configfile %s not found - trying defaults.")
                ,filename);
        }

        if (getcwd(path, sizeof(path)) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, _("Error getcwd"));
            exit(-1);
        }

        snprintf(filename, PATH_MAX, "%.*s/motion.conf"
            , (int)(PATH_MAX-1-strlen("/motion.conf"))
            , path);
        fp = fopen (filename, "r");
    }

    if (!fp) {  /* Specified file does not exist... try default file. */
        snprintf(filename, PATH_MAX, "%s/.motion/motion.conf", getenv("HOME"));
        fp = fopen(filename, "r");

        if (!fp) {
            snprintf(filename, PATH_MAX, "%s/motion.conf", sysconfdir);
            fp = fopen(filename, "r");

            if (!fp) { /* There is no config file.... use defaults. */
                MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
                    ,_("could not open configfile %s")
                    ,filename);
            }
        }
    }

    /* Now we process the motion.conf config file and close it. */
    if (fp) {
        retcd = snprintf(cnt[0]->conf_filename
            ,sizeof(cnt[0]->conf_filename)
            ,"%s",filename);
        if ((retcd < 0) || (retcd >= (int)sizeof(cnt[0]->conf_filename))) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Invalid file name %s"), filename);
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                ,_("Processing thread 0 - config file %s"), filename);
            cnt = conf_process(cnt, fp);
            myfclose(fp);
        }
    } else {
        MOTION_LOG(CRT, TYPE_ALL, NO_ERRNO
            ,_("No config file to process, using default values"));
    }


    /*
     * For each thread (given by cnt[i]) being not null
     * cnt is an array of pointers to a context type structure
     * cnt[0] is the default context structure
     * cnt[1], cnt[2], ... are context structures for each thread
     * Command line options always wins over config file options
     * so we go through each thread and overrides any set Command-line
     * options.
     */
    i = -1;

    while (cnt[++i]) {
        conf_cmdline(cnt[i], i);
    }

    /* If pid file was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->pid_file[0]) {
        if (cnt[0]->conf.pid_file != NULL) {
            free(cnt[0]->conf.pid_file);
            cnt[0]->conf.pid_file = NULL;
        }
        cnt[0]->conf.pid_file = mymalloc(strlen(cnt[0]->pid_file) + 1);
        sprintf(cnt[0]->conf.pid_file, "%s", cnt[0]->pid_file);
    }

    /* If log file was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_file[0]) {
        if (cnt[0]->conf.log_file != NULL) {
            free(cnt[0]->conf.log_file);
            cnt[0]->conf.log_file = NULL;
        }
        cnt[0]->conf.log_file = mymalloc(strlen(cnt[0]->log_file) + 1);
        sprintf(cnt[0]->conf.log_file, "%s", cnt[0]->log_file);
    }

    /* If log type string was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_type_str[0]) {
        if (cnt[0]->conf.log_type != NULL) {
            free(cnt[0]->conf.log_type);
            cnt[0]->conf.log_type = NULL;
        }
        cnt[0]->conf.log_type = mymalloc(strlen(cnt[0]->log_type_str) + 1);
        sprintf(cnt[0]->conf.log_type, "%s", cnt[0]->log_type_str);
    }

    /* if log level was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_level != -1) {
        cnt[0]->conf.log_level = cnt[0]->log_level;
    }

    config_parms_intl();

    return cnt;
}

/**
 * conf_output_parms
 *      Dump config options to log, useful for support purposes.
 *      Redact sensitive information and re-add quotation marks where needed (see conf_print).
 *      Not using the MOTION_LOG macro here to skip the function naming,
 *      and produce a slightly cleaner dump.
 *
 * Returns nothing
 */
void conf_output_parms(struct context **cnt)
{
    unsigned int i, t = 0;
    const char *name, *value;

    while(cnt[++t]) {
        continue;
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO
        ,_("Writing configuration parameters from all files (%d):"), t);
    for (t = 0; cnt[t]; t++) {
        motion_log(INF, TYPE_ALL, NO_ERRNO,0
            ,_("Thread %d - Config file: %s"), t, cnt[t]->conf_filename);
        i = 0;
        while (config_params[i].param_name != NULL) {
            name=config_params[i].param_name;
            if ((value = config_params[i].print(cnt, NULL, i, t)) != NULL) {
                if (mystreq(name, "netcam_url") ||
                    mystreq(name, "netcam_userpass") ||
                    mystreq(name, "netcam_high_url") ||
                    mystreq(name, "stream_authentication") ||
                    mystreq(name, "webcontrol_authentication") ||
                    mystreq(name, "webcontrol_key") ||
                    mystreq(name, "webcontrol_cert") ||
                    mystreq(name, "database_user") ||
                    mystreq(name, "database_password")) {
                    motion_log(INF, TYPE_ALL, NO_ERRNO,0
                        ,_("%-25s <redacted>"), name);

                } else if (mystreq(name, "webcontrol_header_params") &&
                    (cnt[t]->conf.webcontrol_localhost == FALSE)) {
                    motion_log(INF, TYPE_ALL, NO_ERRNO,0
                        ,_("%-25s <redacted>"), name);

                } else if (mystreq(name, "stream_header_params") &&
                    (cnt[t]->conf.stream_localhost == FALSE)) {
                    motion_log(INF, TYPE_ALL, NO_ERRNO,0
                        ,_("%-25s <redacted>"), name);

                } else {
                    if (strncmp(name, "text", 4) || strncmp(value, " ", 1)) {
                        motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s %s", name, value);
                    } else {
                        motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s \"%s\"", name, value);
                    }
                }
            } else {
                if (t == 0) {
                    motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s ", name);
                }
            }
            i++;
        }
    }
}

/**
 * malloc_strings
 *      goes through the members of a context structure.
 *      For each context structure member which is a pointer to a string it does this:
 *      If the member points to a string and is not NULL
 *      1. Reserve (malloc) memory for the string
 *      2. Copy the original string to the reserved memory
 *      3. Change the cnt member (char*) pointing to the string in reserved memory
 *      This ensures that we can free and malloc the string if it is later changed
 *
 * Returns nothing.
 */
static void malloc_strings(struct context *cnt)
{
    unsigned int i = 0;
    char **val;
    while (config_params[i].param_name != NULL) {
        if (config_params[i].copy == copy_string) {
            /* if member is a string */
            /* val is made to point to a pointer to the current string. */
            val = (char **)((char *)cnt+config_params[i].conf_value);

            /*
             * If there is a string, malloc() space for it, copy
             * the string to new space, and point to the new
             * string. we don't free() because we're copying a
             * static string.
             */
            *val = mystrdup(*val);
        }
        i++;
    }
}

/**
 * print_bool
 *      Returns a pointer to string containing boolean value 'on' / 'off' or NULL.
 *
 * Returns const char *.
 */
static const char *print_bool(struct context **cnt, char **str, int parm, unsigned int threadnr)
{
    int val = config_params[parm].conf_value;

    (void)str;

    if (threadnr && *(int*)((char *)cnt[threadnr] + val) == *(int*)((char *)cnt[0] + val)) {
        return NULL;
    }

    if (*(int*)((char *)cnt[threadnr] + val)) {
        return "on";
    } else {
        return "off";
    }
}

/**
 * print_string
 *      Returns a pointer to a string containing the value of the config option,
 *      If the thread number is not 0 the string is compared with the value of the same
 *      option in thread 0.
 *
 * Returns If the option is not defined NULL is returned.
 *         If the value is the same, NULL is returned which means that
 *         the option is not written to the camera config file.
 */
static const char *print_string(struct context **cnt, char **str, int parm, unsigned int threadnr)
{
    int val = config_params[parm].conf_value;
    const char **cptr0, **cptr1;

    (void)str;

    /* Check for NULL also. */
    cptr0 = (const char **)((char *)cnt[0] + val);
    cptr1 = (const char **)((char *)cnt[threadnr] + val);

    if ((threadnr) && (*cptr0 != NULL) && (*cptr1 != NULL) && (mystreq(*cptr0, *cptr1))) {
        return NULL;
    }

    return *cptr1;
}

/**
 * print_int
 *      Returns a pointer to a string containing the integer of the config option value.
 *      If the thread number is not 0 the integer is compared with the value of the same
 *      option in thread 0.
 *
 * Returns If the option is different, const char *
 *         If the option is the same, NULL is returned which means that
 *         the option is not written to the camera config file.
 */
static const char *print_int(struct context **cnt, char **str, int parm, unsigned int threadnr)
{
    static char retval[20];
    int val = config_params[parm].conf_value;

    (void)str;

    if (threadnr && *(int*)((char *)cnt[threadnr] + val) == *(int*)((char *)cnt[0] + val)) {
        return NULL;
    }

    sprintf(retval, "%d", *(int*)((char *)cnt[threadnr] + val));

    return retval;
}

/**
 * print_camera
 *      Modifies a pointer to a string with each 'camera' line.
 *      Does nothing if single threaded or no pointer was supplied.
 *
 * Returns NULL
 */
static const char *print_camera(struct context **cnt, char **str, int parm, unsigned int threadnr)
{
    char *retval;
    unsigned int i = 0;

    (void)parm;

    if (!str || threadnr) {
        return NULL;
    }

    retval = mymalloc(1);
    retval[0] = 0;

    while (cnt[++i]) {
        /* Skip config files loaded from conf directory */
        if (cnt[i]->from_conf_dir) {
            continue;
        }

        retval = myrealloc(retval, strlen(retval) + strlen(cnt[i]->conf_filename) + 10,
                           "print_camera");
        sprintf(retval + strlen(retval), "camera %s\n", cnt[i]->conf_filename);
    }

    *str = retval;

    return NULL;
}

/**
 * read_camera_dir
 *     Read the directory finding all *.conf files in the path
 *     When found calls config_camera
 */
static struct context **read_camera_dir(struct context **cnt, char *str, int val)
{
    DIR *dp;
    struct dirent *ep;
    size_t name_len;
    int i;

    char conf_file[PATH_MAX];

    /* Store the given config value to allow writing it out */
    copy_string(*cnt, str, val);

    dp = opendir(str);
    if (dp != NULL) {
        while( (ep = readdir(dp)) ) {
            name_len = strlen(ep->d_name);
            if (name_len > strlen(EXTENSION) &&
                (strncmp(EXTENSION,(ep->d_name + name_len - strlen(EXTENSION)),
                        strlen(EXTENSION)) == 0 )) {
                memset(conf_file, '\0', sizeof(conf_file));
                snprintf(conf_file, sizeof(conf_file) - 1, "%s/%s",
                            str, ep->d_name);
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                    ,_("Processing config file %s"), conf_file );
                cnt = config_camera(cnt, conf_file, 0);
                /* The last context thread would be ours,
                 * set it as created from conf directory.
                 */
                i = 0;
                while (cnt[++i]) {
                    continue;
                }
                cnt[i-1]->from_conf_dir = 1;
	        }
        }
        closedir(dp);
    } else {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera directory config %s not found"), str);
        return cnt;
    }

    return cnt;
}

/**
 * config_camera
 *      Is called during initial config file loading each time Motion
 *      finds a camera option in motion.conf
 *      The size of the context array is increased and the main context's values are
 *      copied to the new thread.
 *
 *      cnt  - pointer to the array of pointers pointing to the context structures
 *      str  - pointer to a string which is the filename of the camera config file
 *      val  - is not used. It is defined to be function header compatible with
 *            copy_int, copy_bool and copy_string.
 */
static struct context **config_camera(struct context **cnt, const char *str, int val)
{
    int i;
    FILE *fp;

    (void)val;

    if (cnt[0]->threadnr) {
        return cnt;
    }

    fp = fopen(str, "r");

    if (!fp) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO
            ,_("Camera config file %s not found"), str);
        return cnt;
    }

    /* Find the current number of threads defined. */
    i = -1;

    while (cnt[++i]) {
        continue;
    }

    /*
     * Make space for the threads + the terminating NULL pointer
     * in the array of pointers to context structures
     * First thread is 0 so the number of threads is i + 1
     * plus an extra for the NULL pointer. This gives i + 2
     */
    cnt = myrealloc(cnt, sizeof(struct context *) * (i + 2), "config_camera");

    /* Now malloc space for an additional context structure for thread nr. i */
    cnt[i] = mymalloc(sizeof(struct context));

    /* And make this an exact clone of the context structure for thread 0 */
    memcpy(cnt[i], cnt[0], sizeof(struct context));

    /*
     * All the integers are copies of the actual value.
     * The strings are all pointers to strings so we need to create
     * unique malloc'ed space for all the strings that are not NULL and
     * change the string pointers to point to the new strings.
     * malloc_strings takes care of this.
     */
    malloc_strings(cnt[i]);

    /* Mark the end if the array of pointers to context structures. */
    cnt[i + 1] = NULL;

    /* Process the camera's config file and notify user on console. */
    strcpy(cnt[i]->conf_filename, str);
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
        ,_("Processing camera config file %s"), str);
    conf_process(cnt + i, fp);

    /* Finally we close the camera config file. */
    myfclose(fp);

    return cnt;
}

/**
 * usage
 *      Prints usage and options allowed from Command-line.
 *
 * Returns nothing.
 */
static void usage()
{
    printf("motion Version "VERSION", Copyright 2000-2021 Jeroen Vreeken/Folkert van Heusden/Kenneth Lavrsen/Motion-Project maintainers\n");
    printf("\nHome page :\t https://motion-project.github.io/ \n");
    printf("\nusage:\tmotion [options]\n");
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
    printf("-m\t\t\tDisable motion detection at startup.\n");
    printf("-h\t\t\tShow this screen.\n");
    printf("\n");
    printf("Motion is configured using a config file only. If none is supplied,\n");
    printf("it will read motion.conf from current directory, ~/.motion or %s.\n", sysconfdir);
    printf("\n");
}

static void config_parms_intl()
{
    /* This function prints out the configuration parms side by side
     * with the translations.  It is currently disabled but put into
     * the code so that they can be found by xgettext.  If enabled, then
     * it will be printed when called from the conf_load.
     */

    if (FALSE) {
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","daemon",_("daemon"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","setup_mode",_("setup_mode"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pid_file",_("pid_file"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_file",_("log_file"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_level",_("log_level"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","log_type",_("log_type"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","quiet",_("quiet"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_tmo",_("watchdog_tmo"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","watchdog_kill",_("watchdog_kill"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","native_language",_("native_language"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_name",_("camera_name"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_id",_("camera_id"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","target_dir",_("target_dir"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_device",_("video_device"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_params",_("video_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","auto_brightness",_("auto_brightness"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","tuner_device",_("tuner_device"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_frames",_("roundrobin_frames"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_skip",_("roundrobin_skip"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","roundrobin_switchfilter",_("roundrobin_switchfilter"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_url",_("netcam_url"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_params",_("netcam_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_url",_("netcam_high_url"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_high_params",_("netcam_high_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","netcam_userpass",_("netcam_userpass"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_name",_("mmalcam_name"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mmalcam_params",_("mmalcam_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","width",_("width"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","height",_("height"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","framerate",_("framerate"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_frame_time",_("minimum_frame_time"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","rotate",_("rotate"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","flip_axis",_("flip_axis"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_mode",_("locate_motion_mode"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","locate_motion_style",_("locate_motion_style"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_left",_("text_left"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_right",_("text_right"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_changes",_("text_changes"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_scale",_("text_scale"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","text_event",_("text_event"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","emulate_motion",_("emulate_motion"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pause",_("pause"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold",_("threshold"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_maximum",_("threshold_maximum"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","threshold_tune",_("threshold_tune"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_level",_("noise_level"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","noise_tune",_("noise_tune"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","despeckle_filter",_("despeckle_filter"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","area_detect",_("area_detect"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_file",_("mask_file"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","mask_privacy",_("mask_privacy"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","smart_mask_speed",_("smart_mask_speed"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_percent",_("lightswitch_percent"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","lightswitch_frames",_("lightswitch_frames"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","minimum_motion_frames",_("minimum_motion_frames"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","event_gap",_("event_gap"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","pre_capture",_("pre_capture"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","post_capture",_("post_capture"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_start",_("on_event_start"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_event_end",_("on_event_end"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_picture_save",_("on_picture_save"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_area_detected",_("on_area_detected"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_motion_detected",_("on_motion_detected"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_start",_("on_movie_start"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_movie_end",_("on_movie_end"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_lost",_("on_camera_lost"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","on_camera_found",_("on_camera_found"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output",_("picture_output"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_output_motion",_("picture_output_motion"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_type",_("picture_type"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_quality",_("picture_quality"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_exif",_("picture_exif"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","picture_filename",_("picture_filename"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_interval",_("snapshot_interval"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","snapshot_filename",_("snapshot_filename"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output",_("movie_output"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_output_motion",_("movie_output_motion"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_max_time",_("movie_max_time"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_bps",_("movie_bps"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_quality",_("movie_quality"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_codec",_("movie_codec"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_duplicate_frames",_("movie_duplicate_frames"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_passthrough",_("movie_passthrough"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_filename",_("movie_filename"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe_use",_("movie_extpipe_use"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","movie_extpipe",_("movie_extpipe"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_interval",_("timelapse_interval"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_mode",_("timelapse_mode"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_fps",_("timelapse_fps"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_codec",_("timelapse_codec"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","timelapse_filename",_("timelapse_filename"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe",_("video_pipe"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","video_pipe_motion",_("video_pipe_motion"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_port",_("webcontrol_port"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_ipv6",_("webcontrol_ipv6"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_localhost",_("webcontrol_localhost"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_parms",_("webcontrol_parms"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_interface",_("webcontrol_interface"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_auth_method",_("webcontrol_auth_method"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_authentication",_("webcontrol_authentication"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_tls",_("webcontrol_tls"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_cert",_("webcontrol_cert"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_key",_("webcontrol_key"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","webcontrol_header_params",_("webcontrol_header_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_port",_("stream_port"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_localhost",_("stream_localhost"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_auth_method",_("stream_auth_method"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_authentication",_("stream_authentication"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_tls",_("stream_tls"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_header_params",_("stream_header_params"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_scale",_("stream_preview_scale"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_newline",_("stream_preview_newline"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_preview_method",_("stream_preview_method"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_quality",_("stream_quality"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_grey",_("stream_grey"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_motion",_("stream_motion"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_maxrate",_("stream_maxrate"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","stream_limit",_("stream_limit"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_type",_("database_type"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_dbname",_("database_dbname"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_host",_("database_host"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_port",_("database_port"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_user",_("database_user"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_password",_("database_password"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","database_busy_timeout",_("database_busy_timeout"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_picture",_("sql_log_picture"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_snapshot",_("sql_log_snapshot"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_movie",_("sql_log_movie"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_log_timelapse",_("sql_log_timelapse"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_start",_("sql_query_start"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query_stop",_("sql_query_stop"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","sql_query",_("sql_query"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_type",_("track_type"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_auto",_("track_auto"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_port",_("track_port"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_motorx",_("track_motorx"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_motorx_reverse",_("track_motorx_reverse"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_motory",_("track_motory"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_motory_reverse",_("track_motory_reverse"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_maxx",_("track_maxx"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_minx",_("track_minx"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_maxy",_("track_maxy"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_miny",_("track_miny"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_homex",_("track_homex"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_homey",_("track_homey"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_iomojo_id",_("track_iomojo_id"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_x",_("track_step_angle_x"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_step_angle_y",_("track_step_angle_y"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_move_wait",_("track_move_wait"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_speed",_("track_speed"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_stepsize",_("track_stepsize"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","track_generic_move",_("track_generic_move"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera",_("camera"));
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO,"%s:%s","camera_dir",_("camera_dir"));

    }
}
