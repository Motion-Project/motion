/*
  **
  ** conf.c
  **
  ** I originally wrote conf.c as part of the drpoxy package
  ** thanks to Matthew Pratt and others for their additions.
  **
  ** Copyright 1999 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

/**
 * How to add a config option :
 *
 *   1. think twice, there are settings enough
 *
 *   2. add a field to 'struct config' (conf.h) and 'struct config conf'
 *
 *   4. add a entry to the config_params array below, if your
 *      option should be configurable by the config file.
 */
#include <dirent.h>
#include <string.h>

#include "motion.h"

#if (defined(BSD) && !defined(PWCBSD))
#include "video_freebsd.h"
#else
#include "video.h"
#endif /* BSD */

#define EXTENSION ".conf"

#ifndef HAVE_GET_CURRENT_DIR_NAME
char *get_current_dir_name(void)
{
    char *buf = mymalloc(MAXPATHLEN);
    getwd(buf);
    return buf;
}
#endif

#define stripnewline(x) {if ((x)[strlen(x)-1]=='\n') (x)[strlen(x) - 1] = 0; }

struct config conf_template = {
    width:                          DEF_WIDTH,
    height:                         DEF_HEIGHT,
    quality:                        DEF_QUALITY,
    camera_id:                      0,
    rotate_deg:                     0,
    max_changes:                    DEF_CHANGES,
    threshold_tune:                 0,
    output_pictures:                "on",
    motion_img:                     0,
    emulate_motion:                 0,
    event_gap:                      DEF_EVENT_GAP,
    max_movie_time:                 DEF_MAXMOVIETIME,
    snapshot_interval:              0,
    locate_motion_mode:             "off",
    locate_motion_style:            "box",
    input:                          IN_DEFAULT,
    norm:                           0,
    frame_limit:                    DEF_MAXFRAMERATE,
    quiet:                          1,
    picture_type:                   "jpeg",
    noise:                          DEF_NOISELEVEL,
    noise_tune:                     1,
    minimum_frame_time:             0,
    lightswitch:                    0,
    autobright:                     0,
    brightness:                     0,
    contrast:                       0,
    saturation:                     0,
    hue:                            0,
    power_line_frequency:           -1,
    roundrobin_frames:              1,
    roundrobin_skip:                1,
    pre_capture:                    0,
    post_capture:                   0,
    switchfilter:                   0,
    ffmpeg_output:                  0,
    extpipe:                        NULL,
    useextpipe:                     0,
    ffmpeg_output_debug:            0,
    ffmpeg_bps:                     DEF_FFMPEG_BPS,
    ffmpeg_vbr:                     DEF_FFMPEG_VBR,
    ffmpeg_video_codec:             DEF_FFMPEG_CODEC,
#ifdef HAVE_SDL
    sdl_threadnr:                   0,
#endif
    ipv6_enabled:                   0,
    stream_port:                    0,
    stream_quality:                 50,
    stream_motion:                  0,
    stream_maxrate:                 1,
    stream_localhost:               1,
    stream_limit:                   0,
    stream_auth_method:             0,
    stream_authentication:          NULL,
    stream_preview_scale:           25,
    stream_preview_newline:         0,
    webcontrol_port:                0,
    webcontrol_localhost:           1,
    webcontrol_html_output:         1,
    webcontrol_authentication:      NULL,
    frequency:                      0,
    tuner_number:                   0,
    timelapse:                      0,
    timelapse_mode:                 DEF_TIMELAPSE_MODE,
#if (defined(BSD))
    tuner_device:                   NULL,
#endif
    video_device:                   VIDEO_DEVICE,
    v4l2_palette:                   DEF_PALETTE,
    vidpipe:                        NULL,
    filepath:                       NULL,
    imagepath:                      DEF_IMAGEPATH,
    moviepath:                      DEF_MOVIEPATH,
    snappath:                       DEF_SNAPPATH,
    timepath:                       DEF_TIMEPATH,
    on_event_start:                 NULL,
    on_event_end:                   NULL,
    mask_file:                      NULL,
    smart_mask_speed:               0,
#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)
    sql_log_image:                  1,
    sql_log_snapshot:               1,
    sql_log_movie:                  0,
    sql_log_timelapse:              0,
    sql_query:                      DEF_SQL_QUERY,
    database_type:                  NULL,
    database_dbname:                NULL,
    database_host:                  "localhost",
    database_user:                  NULL,
    database_password:              NULL,
    database_port:                  0,
    database_busy_timeout:           0,
#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || define(HAVE_SQLITE3) */
    on_picture_save:                NULL,
    on_motion_detected:             NULL,
    on_area_detected:               NULL,
    on_movie_start:                 NULL,
    on_movie_end:                   NULL,
    on_camera_lost:                 NULL,
    motionvidpipe:                  NULL,
    netcam_url:                     NULL,
    netcam_userpass:                NULL,
    netcam_keepalive:               "off",
    netcam_proxy:                   NULL,
    netcam_tolerant_check:          0,
    rtsp_uses_tcp:                  1,
    text_changes:                   0,
    text_left:                      NULL,
    text_right:                     DEF_TIMESTAMP,
    text_event:                     DEF_EVENTSTAMP,
    text_double:                    0,
    despeckle_filter:               NULL,
    area_detect:                    NULL,
    minimum_motion_frames:          1,
    exif_text:                      NULL,
    pid_file:                       NULL,
    log_file:                       NULL,
    log_level:                      LEVEL_DEFAULT+10,
    log_type_str:                   NULL,
    thread_dir:                     sysconfdir "/conf.d"
};


static struct context **copy_bool(struct context **, const char *, int);
static struct context **copy_int(struct context **, const char *, int);
static struct context **config_thread(struct context **cnt, const char *str, int val);
static struct context **read_thread_dir(struct context **cnt, const char *str,
                                            int val);

static const char *print_bool(struct context **, char **, int, unsigned int);
static const char *print_int(struct context **, char **, int, unsigned int);
static const char *print_string(struct context **, char **, int, unsigned int);
static const char *print_thread(struct context **, char **, int, unsigned int);

static void usage(void);

/* Pointer magic to determine relative addresses of variables to a
   struct context pointer */
#define CNT_OFFSET(varname) ((long)&((struct context *)NULL)->varname)
#define CONF_OFFSET(varname) ((long)&((struct context *)NULL)->conf.varname)
#define TRACK_OFFSET(varname) ((long)&((struct context *)NULL)->track.varname)

config_param config_params[] = {
    {
    "daemon",
    "############################################################\n"
    "# Daemon\n"
    "############################################################\n\n"
    "# Start in daemon (background) mode and release terminal (default: off)",
    1,
    CNT_OFFSET(daemon),
    copy_bool,
    print_bool
    },
    {
    "process_id_file",
    "#File to store the process ID, also called pid file. (default: not defined)",
    1,
    CONF_OFFSET(pid_file),
    copy_string,
    print_string
    },
    {
    "setup_mode",
    "############################################################\n"
    "# Basic Setup Mode\n"
    "############################################################\n\n"
    "# Start in Setup-Mode, daemon disabled. (default: off)",
    0,
    CONF_OFFSET(setup_mode),
    copy_bool,
    print_bool
    },
    {
    "logfile",
    "# Use a file to save logs messages, if not defined stderr and syslog is used. (default: not defined)",
    1,
    CONF_OFFSET(log_file),
    copy_string,
    print_string
    },
    {
    "log_level",
    "# Level of log messages [1..9] (EMG, ALR, CRT, ERR, WRN, NTC, INF, DBG, ALL). (default: 6 / NTC)",
    1,
    CONF_OFFSET(log_level),
    copy_int,
    print_int
    },
    {
    "log_type",
    "# Filter to log messages by type (COR, STR, ENC, NET, DBL, EVT, TRK, VID, ALL). (default: ALL)",
    1,
    CONF_OFFSET(log_type_str),
    copy_string,
    print_string
    },
    {
    "videodevice",
    "\n###########################################################\n"
    "# Capture device options\n"
    "############################################################\n\n"
    "# Videodevice to be used for capturing  (default /dev/video0)\n"
    "# for FreeBSD default is /dev/bktr0",
    0,
    CONF_OFFSET(video_device),
    copy_string,
    print_string
    },
    {
    "v4l2_palette",
    "# v4l2_palette allows to choose preferable palette to be use by motion\n"
    "# to capture from those supported by your videodevice. (default: 17)\n"
    "# E.g. if your videodevice supports both V4L2_PIX_FMT_SBGGR8 and\n"
    "# V4L2_PIX_FMT_MJPEG then motion will by default use V4L2_PIX_FMT_MJPEG.\n"
    "# Setting v4l2_palette to 2 forces motion to use V4L2_PIX_FMT_SBGGR8\n"
    "# instead.\n"
    "#\n"
    "# Values :\n"
    "# V4L2_PIX_FMT_SN9C10X : 0  'S910'\n"
    "# V4L2_PIX_FMT_SBGGR16 : 1  'BYR2'\n"
    "# V4L2_PIX_FMT_SBGGR8  : 2  'BA81'\n"
    "# V4L2_PIX_FMT_SPCA561 : 3  'S561'\n"
    "# V4L2_PIX_FMT_SGBRG8  : 4  'GBRG'\n"
    "# V4L2_PIX_FMT_SGRBG8  : 5  'GRBG'\n"
    "# V4L2_PIX_FMT_PAC207  : 6  'P207'\n"
    "# V4L2_PIX_FMT_PJPG    : 7  'PJPG'\n"
    "# V4L2_PIX_FMT_MJPEG   : 8  'MJPEG'\n"
    "# V4L2_PIX_FMT_JPEG    : 9  'JPEG'\n"
    "# V4L2_PIX_FMT_RGB24   : 10 'RGB3'\n"
    "# V4L2_PIX_FMT_SPCA501 : 11 'S501'\n"
    "# V4L2_PIX_FMT_SPCA505 : 12 'S505'\n"
    "# V4L2_PIX_FMT_SPCA508 : 13 'S508'\n"
    "# V4L2_PIX_FMT_UYVY    : 14 'UYVY'\n"
    "# V4L2_PIX_FMT_YUYV    : 15 'YUYV'\n"
    "# V4L2_PIX_FMT_YUV422P : 16 '422P'\n"
    "# V4L2_PIX_FMT_YUV420  : 17 'YU12'\n"
    "#",
    0,
    CONF_OFFSET(v4l2_palette),
    copy_int,
    print_int
    },
#if (defined(BSD))
    {
    "tunerdevice",
    "# Tuner device to be used for capturing using tuner as source (default /dev/tuner0)\n"
    "# This is ONLY used for FreeBSD. Leave it commented out for Linux",
    0,
    CONF_OFFSET(tuner_device),
    copy_string,
    print_string
    },
#endif
    {
    "input",
    "# The video input to be used (default: -1)\n"
    "# Should normally be set to 0 or 1 for video/TV cards, and -1 for USB cameras",
    0,
    CONF_OFFSET(input),
    copy_int,
    print_int
    },
    {
    "norm",
    "# The video norm to use (only for video capture and TV tuner cards)\n"
    "# Values: 0 (PAL), 1 (NTSC), 2 (SECAM), 3 (PAL NC no colour). Default: 0 (PAL)",
    0,
    CONF_OFFSET(norm),
    copy_int,
    print_int
    },
    {
    "frequency",
    "# The frequency to set the tuner to (kHz) (only for TV tuner cards) (default: 0)",
    0,
    CONF_OFFSET(frequency),
    copy_int,
    print_int
    },
    {
    "rotate",
    "# Rotate image this number of degrees. The rotation affects all saved images as\n"
    "# well as movies. Valid values: 0 (default = no rotation), 90, 180 and 270.",
    0,
    CONF_OFFSET(rotate_deg),
    copy_int,
    print_int
    },
    {
    "width",
    "# Image width (pixels). Valid range: Camera dependent, default: 352",
    0,
    CONF_OFFSET(width),
    copy_int,
    print_int
    },
    {
    "camera_id",
    "Id used to label the camera when inserting data into SQL or saving the"
    "camera image to disk.  This is better than using thread ID so that there"
    "always is a consistent label",
    0,
    CONF_OFFSET(camera_id),
    copy_int,
    print_int
    },
    {
    "height",
    "# Image height (pixels). Valid range: Camera dependent, default: 288",
    0,
    CONF_OFFSET(height),
    copy_int,
    print_int
    },
    {
    "framerate",
    "# Maximum number of frames to be captured per second.\n"
    "# Valid range: 2-100. Default: 100 (almost no limit).",
    0,
    CONF_OFFSET(frame_limit),
    copy_int,
    print_int
    },
    {
    "minimum_frame_time",
    "# Minimum time in seconds between capturing picture frames from the camera.\n"
    "# Default: 0 = disabled - the capture rate is given by the camera framerate.\n"
    "# This option is used when you want to capture images at a rate lower than 2 per second.",
    0,
    CONF_OFFSET(minimum_frame_time),
    copy_int,
    print_int
    },
    {
    "netcam_url",
    "# URL to use if you are using a network camera, size will be autodetected (incl http:// ftp:// mjpg:// or file:///)\n"
    "# Must be a URL that returns single jpeg pictures or a raw mjpeg stream. Default: Not defined",
    0,
    CONF_OFFSET(netcam_url),
    copy_string,
    print_string
    },
    {
    "netcam_userpass",
    "# Username and password for network camera (only if required). Default: not defined\n"
    "# Syntax is user:password",
    0,
    CONF_OFFSET(netcam_userpass),
    copy_string,
    print_string
    },
    {
    "netcam_keepalive",
    "# The setting for keep-alive of network socket, should improve performance on compatible net cameras.\n"
    "# off:   The historical implementation using HTTP/1.0, closing the socket after each http request.\n"
    "# force: Use HTTP/1.0 requests with keep alive header to reuse the same connection.\n"
    "# on:    Use HTTP/1.1 requests that support keep alive as default.\n"
    "# Default: off",
    0,
    CONF_OFFSET(netcam_keepalive),
    copy_string,
    print_string
    },
    {
    "netcam_proxy",
    "# URL to use for a netcam proxy server, if required, e.g. \"http://myproxy\".\n"
    "# If a port number other than 80 is needed, use \"http://myproxy:1234\".\n"
    "# Default: not defined",
    0,
    CONF_OFFSET(netcam_proxy),
    copy_string,
    print_string
    },
    {
    "netcam_tolerant_check",
    "# Set less strict jpeg checks for network cameras with a poor/buggy firmware.\n"
    "# Default: off",
    0,
    CONF_OFFSET(netcam_tolerant_check),
    copy_bool,
    print_bool
    },
    {
    "rtsp_uses_tcp",
    "# RTSP connection uses TCP to communicate to the camera. Can prevent image corruption.\n"
    "# Default: on",
    1,
    CONF_OFFSET(rtsp_uses_tcp),
    copy_bool,
    print_bool
    },
    {
    "auto_brightness",
    "# Let motion regulate the brightness of a video device (default: off).\n"
    "# The auto_brightness feature uses the brightness option as its target value.\n"
    "# If brightness is zero auto_brightness will adjust to average brightness value 128.\n"
    "# Only recommended for cameras without auto brightness",
    0,
    CONF_OFFSET(autobright),
    copy_bool,
    print_bool
    },
    {
    "brightness",
    "# Set the initial brightness of a video device.\n"
    "# If auto_brightness is enabled, this value defines the average brightness level\n"
    "# which Motion will try and adjust to.\n"
    "# Valid range 0-255, default 0 = disabled",
    0,
    CONF_OFFSET(brightness),
    copy_int,
    print_int
    },
    {
    "contrast",
    "# Set the contrast of a video device.\n"
    "# Valid range 0-255, default 0 = disabled",
    0,
    CONF_OFFSET(contrast),
    copy_int,
    print_int
    },
    {
    "saturation",
    "# Set the saturation of a video device.\n"
    "# Valid range 0-255, default 0 = disabled",
    0,
    CONF_OFFSET(saturation),
    copy_int,
    print_int
    },
    {
    "hue",
    "# Set the hue of a video device (NTSC feature).\n"
    "# Valid range 0-255, default 0 = disabled",
    0,
    CONF_OFFSET(hue),
    copy_int,
    print_int
    },
    {
    "power_line_frequency",
    "# Set the power line frequency to help cancel flicker by compensating\n"
    "# for light intensity ripple.  (default: -1).\n"
    "# This can help reduce power line light flicker.\n"
    "# Valuse :\n"
    "# do not modify the device setting       : -1\n"
    "# V4L2_CID_POWER_LINE_FREQUENCY_DISABLED : 0\n"
    "# V4L2_CID_POWER_LINE_FREQUENCY_50HZ     : 1\n"
    "# V4L2_CID_POWER_LINE_FREQUENCY_60HZ     : 2\n"
    "# V4L2_CID_POWER_LINE_FREQUENCY_AUTO     : 3",
    0,
    CONF_OFFSET(power_line_frequency),
    copy_int,
    print_int
    },
    {
    "roundrobin_frames",
    "\n############################################################\n"
    "# Round Robin (multiple inputs on same video device name)\n"
    "############################################################\n\n"
    "# Number of frames to capture in each roundrobin step (default: 1)",
    0,
    CONF_OFFSET(roundrobin_frames),
    copy_int,
    print_int
    },
    {
    "roundrobin_skip",
    "# Number of frames to skip before each roundrobin step (default: 1)",
    0,
    CONF_OFFSET(roundrobin_skip),
    copy_int,
    print_int
    },
    {
    "switchfilter",
    "# Try to filter out noise generated by roundrobin (default: off)",
    0,
    CONF_OFFSET(switchfilter),
    copy_bool,
    print_bool
    },
    {
    "threshold",
    "\n############################################################\n"
    "# Motion Detection Settings:\n"
    "############################################################\n\n"
    "# Threshold for number of changed pixels in an image that\n"
    "# triggers motion detection (default: 1500)",
    0,
    CONF_OFFSET(max_changes),
    copy_int,
    print_int
    },
    {
    "threshold_tune",
    "# Automatically tune the threshold down if possible (default: off)",
    0,
    CONF_OFFSET(threshold_tune),
    copy_bool,
    print_bool
    },
    {
    "noise_level",
    "# Noise threshold for the motion detection (default: 32)",
    0,
    CONF_OFFSET(noise),
    copy_int,
    print_int
    },
    {
    "noise_tune",
    "# Automatically tune the noise threshold (default: on)",
    0,
    CONF_OFFSET(noise_tune),
    copy_bool,
    print_bool
    },
    {
    "despeckle_filter",
    "# Despeckle motion image using (e)rode or (d)ilate or (l)abel (Default: not defined)\n"
    "# Recommended value is EedDl. Any combination (and number of) of E, e, d, and D is valid.\n"
    "# (l)abeling must only be used once and the 'l' must be the last letter.\n"
    "# Comment out to disable",
    0,
    CONF_OFFSET(despeckle_filter),
    copy_string,
    print_string
    },
    {
    "area_detect",
    "# Detect motion in predefined areas (1 - 9). Areas are numbered like that:  1 2 3\n"
    "# A script (on_area_detected) is started immediately when motion is         4 5 6\n"
    "# detected in one of the given areas, but only once during an event.        7 8 9\n"
    "# One or more areas can be specified with this option. Take care: This option\n"
    "# does NOT restrict detection to these areas! (Default: not defined)",
    0,
    CONF_OFFSET(area_detect),
    copy_string,
    print_string
    },
    {
    "mask_file",
    "# PGM file to use as a sensitivity mask.\n"
    "# Full path name to. (Default: not defined)",
    0,
    CONF_OFFSET(mask_file),
    copy_string,
    print_string
    },
    {
    "smart_mask_speed",
    "# Dynamically create a mask file during operation (default: 0)\n"
    "# Adjust speed of mask changes from 0 (off) to 10 (fast)",
    0,
    CONF_OFFSET(smart_mask_speed),
    copy_int,
    print_int
    },
    {
    "lightswitch",
    "# Ignore sudden massive light intensity changes given as a percentage of the picture\n"
    "# area that changed intensity. If set to 1, motion will do some kind of\n"
    "# auto-lightswitch. Valid range: 0 - 100 , default: 0 = disabled",

    0,
    CONF_OFFSET(lightswitch),
    copy_int,
    print_int
    },
    {
    "minimum_motion_frames",
    "# Picture frames must contain motion at least the specified number of frames\n"
    "# in a row before they are detected as true motion. At the default of 1, all\n"
    "# motion is detected. Valid range: 1 to thousands, recommended 1-5",
    0,
    CONF_OFFSET(minimum_motion_frames),
    copy_int,
    print_int
    },
    {
    "pre_capture",
    "# Specifies the number of pre-captured (buffered) pictures from before motion\n"
    "# was detected that will be output at motion detection.\n"
    "# Recommended range: 0 to 5 (default: 0)\n"
    "# Do not use large values! Large values will cause Motion to skip video frames and\n"
    "# cause unsmooth movies. To smooth movies use larger values of post_capture instead.",
    0,
    CONF_OFFSET(pre_capture),
    copy_int,
    print_int
    },
    {
    "post_capture",
    "# Number of frames to capture after motion is no longer detected (default: 0)",
    0,
    CONF_OFFSET(post_capture),
    copy_int,
    print_int
    },
    {
    "event_gap",
    "# Event Gap is the seconds of no motion detection that triggers the end of an event.\n"
    "# An event is defined as a series of motion images taken within a short timeframe.\n"
    "# Recommended value is 60 seconds (Default). The value -1 is allowed and disables\n"
    "# events causing all Motion to be written to one single movie file and no pre_capture.\n"
    "# If set to 0, motion is running in gapless mode. Movies don't have gaps anymore. An\n"
    "# event ends right after no more motion is detected and post_capture is over.",
    0,
    CONF_OFFSET(event_gap),
    copy_int,
    print_int
    },
    {
    "max_movie_time",
    "# Maximum length in seconds of a movie\n"
    "# When value is exceeded a new movie file is created. (Default: 0 = infinite)",
    0,
    CONF_OFFSET(max_movie_time),
    copy_int,
    print_int
    },
    {
    "emulate_motion",
    "# Always save images even if there was no motion (default: off)",
    0,
    CONF_OFFSET(emulate_motion),
    copy_bool,
    print_bool
    },
    {
    "output_pictures",
    "\n############################################################\n"
    "# Image File Output\n"
    "############################################################\n\n"
    "# Output 'normal' pictures when motion is detected (default: on)\n"
    "# Valid values: on, off, first, best, center\n"
    "# When set to 'first', only the first picture of an event is saved.\n"
    "# Picture with most motion of an event is saved when set to 'best'.\n"
    "# Picture with motion nearest center of picture is saved when set to 'center'.\n"
    "# Can be used as preview shot for the corresponding movie.",
    0,
    CONF_OFFSET(output_pictures),
    copy_string,
    print_string
    },
    {
    "output_debug_pictures",
    "# Output pictures with only the pixels moving object (ghost images) (default: off)",
    0,
    CONF_OFFSET(motion_img),
    copy_bool,
    print_bool
    },
    {
    "quality",
    "# The quality (in percent) to be used by the jpeg compression (default: 75)",
    0,
    CONF_OFFSET(quality),
    copy_int,
    print_int
    },
    {
    "picture_type",
    "# Type of output images\n"
    "# Valid values: jpeg, ppm (default: jpeg)",
    0,
    CONF_OFFSET(picture_type),
    copy_string,
    print_string
    },
#ifdef HAVE_FFMPEG
    {
    "ffmpeg_output_movies",
    "\n############################################################\n"
    "# FFMPEG related options\n"
    "# Film (movie) file output, and deinterlacing of the video input\n"
    "# The options movie_filename and timelapse_filename are also used\n"
    "# by the ffmpeg feature\n"
    "############################################################\n\n"
    "# Use ffmpeg to encode movies in realtime (default: off)",
    0,
    CONF_OFFSET(ffmpeg_output),
    copy_bool,
    print_bool
    },
    {
    "ffmpeg_output_debug_movies",
    "# Use ffmpeg to make movies with only the pixels moving\n"
    "# object (ghost images) (default: off)",
    0,
    CONF_OFFSET(ffmpeg_output_debug),
    copy_bool,
    print_bool
    },
    {
    "ffmpeg_timelapse",
    "# Use ffmpeg to encode a timelapse movie\n"
    "# Default value 0 = off - else save frame every Nth second",
    0,
    CONF_OFFSET(timelapse),
    copy_int,
    print_int
    },
    {
    "ffmpeg_timelapse_mode",
    "# The file rollover mode of the timelapse video\n"
    "# Valid values: hourly, daily (default), weekly-sunday, weekly-monday, monthly, manual",
    0,
    CONF_OFFSET(timelapse_mode),
    copy_string,
    print_string
    },
    {
    "ffmpeg_bps",
    "# Bitrate to be used by the ffmpeg encoder (default: 400000)\n"
    "# This option is ignored if ffmpeg_variable_bitrate is not 0 (disabled)",
    0,
    CONF_OFFSET(ffmpeg_bps),
    copy_int,
    print_int
    },
    {
    "ffmpeg_variable_bitrate",
    "# Enables and defines variable bitrate for the ffmpeg encoder.\n"
    "# ffmpeg_bps is ignored if variable bitrate is enabled.\n"
    "# Valid values: 0 (default) = fixed bitrate defined by ffmpeg_bps,\n"
    "# or the range 2 - 31 where 2 means best quality and 31 is worst.",
    0,
    CONF_OFFSET(ffmpeg_vbr),
    copy_int,
    print_int
    },
    {
    "ffmpeg_video_codec",
    "# Codec to used by ffmpeg for the video compression.\n"
    "# Timelapse movies are always made in mpeg1 format independent from this option.\n"
    "# Supported formats are: mpeg1 (ffmpeg-0.4.8 only), mpeg4 (default), and msmpeg4.\n"
    "# mpeg1 - gives you files with extension .mpg\n"
    "# mpeg4 or msmpeg4 - gives you files with extension .avi\n"
    "# msmpeg4 is recommended for use with Windows Media Player because\n"
    "# it requires no installation of codec on the Windows client.\n"
    "# swf - gives you a flash film with extension .swf\n"
    "# flv - gives you a flash video with extension .flv\n"
    "# ffv1 - FF video codec 1 for Lossless Encoding ( experimental )\n"
    "# mov - QuickTime ( testing )\n"
    "# ogg - Ogg/Theora ( testing )\n"
    "# mp4 - MPEG-4 Part 14 H264 encoding\n"
    "# mkv - Matroska H264 encoding\n"
    "# hevc - H.265 / HEVC (High Efficiency Video Coding)",
    0,
    CONF_OFFSET(ffmpeg_video_codec),
    copy_string,
    print_string
    },
    {
    "ffmpeg_duplicate_frames",
    "# True to duplicate frames to achieve \"framerate\" fps, but enough\n"
    "duplicated frames and the video appears to freeze once a second.",
    0,
    CONF_OFFSET(ffmpeg_duplicate_frames),
    copy_bool,
    print_bool
    },
#endif /* HAVE_FFMPEG */
#ifdef HAVE_SDL
     {
    "sdl_threadnr",
    "\n############################################################\n"
    "# SDL Window\n"
    "############################################################\n\n"
    "# Number of motion thread to show in SDL Window (default: 0 = disabled)",
    1,
    CONF_OFFSET(sdl_threadnr),
    copy_int,
    print_int
    },
#endif /* HAVE_SDL */
    {
    "use_extpipe",
    "\n############################################################\n"
    "# External pipe to video encoder\n"
    "# Replacement for FFMPEG builtin encoder for ffmpeg_output_movies only.\n"
    "# The options movie_filename and timelapse_filename are also used\n"
    "# by the ffmpeg feature\n"
    "############################################################\n\n"
    "# Bool to enable or disable extpipe (default: off)",
    0,
    CONF_OFFSET(useextpipe),
    copy_bool,
    print_bool
    },
    {
    "extpipe",
    "# External program (full path and opts) to pipe raw video to\n"
    "# Generally, use '-' for STDIN...",
    0,
    CONF_OFFSET(extpipe),
    copy_string,
    print_string
    },
    {
    "snapshot_interval",
    "\n############################################################\n"
    "# Snapshots (Traditional Periodic Webcam File Output)\n"
    "############################################################\n\n"
    "# Make automated snapshot every N seconds (default: 0 = disabled)",
    0,
    CONF_OFFSET(snapshot_interval),
    copy_int,
    print_int
    },
    {
    "locate_motion_mode",
    "\n############################################################\n"
    "# Text Display\n"
    "# %Y = year, %m = month, %d = date,\n"
    "# %H = hour, %M = minute, %S = second, %T = HH:MM:SS,\n"
    "# %v = event, %q = frame number, %t = thread (camera) number,\n"
    "# %D = changed pixels, %N = noise level, \\n = new line,\n"
    "# %i and %J = width and height of motion area,\n"
    "# %K and %L = X and Y coordinates of motion center\n"
    "# %C = value defined by text_event - do not use with text_event!\n"
    "# You can put quotation marks around the text to allow\n"
    "# leading spaces\n"
    "############################################################\n\n"
    "# Locate and draw a box around the moving object.\n"
    "# Valid values: on, off, preview (default: off)\n"
    "# Set to 'preview' will only draw a box in preview_shot pictures.",
    0,
    CONF_OFFSET(locate_motion_mode),
    copy_string,
    print_string
    },
    {
    "locate_motion_style",
    "# Set the look and style of the locate box if enabled.\n"
    "# Valid values: box, redbox, cross, redcross (default: box)\n"
    "# Set to 'box' will draw the traditional box.\n"
    "# Set to 'redbox' will draw a red box.\n"
    "# Set to 'cross' will draw a little cross to mark center.\n"
    "# Set to 'redcross' will draw a little red cross to mark center.",
    0,
    CONF_OFFSET(locate_motion_style),
    copy_string,
    print_string
    },
    {
    "text_right",
    "# Draws the timestamp using same options as C function strftime(3)\n"
    "# Default: %Y-%m-%d\\n%T = date in ISO format and time in 24 hour clock\n"
    "# Text is placed in lower right corner",
    0,
    CONF_OFFSET(text_right),
    copy_string,
    print_string
    },
    {
    "text_left",
    "# Draw a user defined text on the images using same options as C function strftime(3)\n"
    "# Default: Not defined = no text\n"
    "# Text is placed in lower left corner",
    0,
    CONF_OFFSET(text_left),
    copy_string,
    print_string
    },
    {
    "text_changes",
    "# Draw the number of changed pixed on the images (default: off)\n"
    "# Will normally be set to off except when you setup and adjust the motion settings\n"
    "# Text is placed in upper right corner",
    0,
    CONF_OFFSET(text_changes),
    copy_bool,
    print_bool
    },
    {
    "text_event",
    "# This option defines the value of the special event conversion specifier %C\n"
    "# You can use any conversion specifier in this option except %C. Date and time\n"
    "# values are from the timestamp of the first image in the current event.\n"
    "# Default: %Y%m%d%H%M%S\n"
    "# The idea is that %C can be used filenames and text_left/right for creating\n"
    "# a unique identifier for each event.",
    0,
    CONF_OFFSET(text_event),
    copy_string,
    print_string
    },
    {
    "text_double",
    "# Draw characters at twice normal size on images. (default: off)",
    0,
    CONF_OFFSET(text_double),
    copy_bool,
    print_bool
    },
    {
    "exif_text",
    "# Text to include in a JPEG EXIF comment\n"
    "# May be any text, including conversion specifiers.\n"
    "# The EXIF timestamp is included independent of this text.",
    0,
    CONF_OFFSET(exif_text),
    copy_string,
    print_string
    },
    {
    "target_dir",
    "\n############################################################\n"
    "# Target Directories and filenames For Images And Films\n"
    "# For the options snapshot_, picture_, movie_ and timelapse_filename\n"
    "# you can use conversion specifiers\n"
    "# %Y = year, %m = month, %d = date,\n"
    "# %H = hour, %M = minute, %S = second,\n"
    "# %v = event, %q = frame number, %t = thread (camera) number,\n"
    "# %D = changed pixels, %N = noise level,\n"
    "# %i and %J = width and height of motion area,\n"
    "# %K and %L = X and Y coordinates of motion center\n"
    "# %C = value defined by text_event\n"
    "# Quotation marks round string are allowed.\n"
    "############################################################\n\n"
    "# Target base directory for pictures and films\n"
    "# Recommended to use absolute path. (Default: current working directory)",
    0,
    CONF_OFFSET(filepath),
    copy_string,
    print_string
    },
    {
    "snapshot_filename",
    "# File path for snapshots (jpeg or ppm) relative to target_dir\n"
    "# Default: "DEF_SNAPPATH"\n"
    "# Default value is equivalent to legacy oldlayout option\n"
    "# For Motion 3.0 compatible mode choose: %Y/%m/%d/%H/%M/%S-snapshot\n"
    "# File extension .jpg or .ppm is automatically added so do not include this.\n"
    "# Note: A symbolic link called lastsnap.jpg created in the target_dir will always\n"
    "# point to the latest snapshot, unless snapshot_filename is exactly 'lastsnap'",
    0,
    CONF_OFFSET(snappath),
    copy_string,
    print_string
    },
    {
    "picture_filename",
    "# File path for motion triggered images (jpeg or ppm) relative to target_dir\n"
    "# Default: "DEF_IMAGEPATH"\n"
    "# Default value is equivalent to legacy oldlayout option\n"
    "# For Motion 3.0 compatible mode choose: %Y/%m/%d/%H/%M/%S-%q\n"
    "# File extension .jpg or .ppm is automatically added so do not include this\n"
    "# Set to 'preview' together with best-preview feature enables special naming\n"
    "# convention for preview shots. See motion guide for details",
    0,
    CONF_OFFSET(imagepath),
    copy_string,
    print_string
    },
#ifdef HAVE_FFMPEG
    {
    "movie_filename",
    "# File path for motion triggered ffmpeg films (movies) relative to target_dir\n"
    "# Default: "DEF_MOVIEPATH"\n"
    "# Default value is equivalent to legacy oldlayout option\n"
    "# For Motion 3.0 compatible mode choose: %Y/%m/%d/%H%M%S\n"
    "# File extension .mpg or .avi is automatically added so do not include this\n"
    "# This option was previously called ffmpeg_filename",
    0,
    CONF_OFFSET(moviepath),
    copy_string,
    print_string
    },
    {
    "timelapse_filename",
    "# File path for timelapse movies relative to target_dir\n"
    "# Default: "DEF_TIMEPATH"\n"
    "# Default value is near equivalent to legacy oldlayout option\n"
    "# For Motion 3.0 compatible mode choose: %Y/%m/%d-timelapse\n"
    "# File extension .mpg is automatically added so do not include this",
    0,
    CONF_OFFSET(timepath),
    copy_string,
    print_string
    },
#endif /* HAVE_FFMPEG */
    {
    "ipv6_enabled",
    "\n############################################################\n"
    "# Global Network Options\n"
    "############################################################\n\n"
    "# Enable or disable IPV6 for http control and stream (default: off)",
    0,
    CONF_OFFSET(ipv6_enabled),
    copy_bool,
    print_bool
    },
    {
    "stream_port",
    "\n############################################################\n"
    "# Live Stream Server\n"
    "############################################################\n\n"
    "# The mini-http server listens to this port for requests (default: 0 = disabled)",
    0,
    CONF_OFFSET(stream_port),
    copy_int,
    print_int
    },
    {
    "stream_quality",
    "# Quality of the jpeg (in percent) images produced (default: 50)",
    0,
    CONF_OFFSET(stream_quality),
    copy_int,
    print_int
    },
    {
    "stream_motion",
    "# Output frames at 1 fps when no motion is detected and increase to the\n"
    "# rate given by stream_maxrate when motion is detected (default: off)",
    0,
    CONF_OFFSET(stream_motion),
    copy_bool,
    print_bool
    },
    {
    "stream_maxrate",
    "# Maximum framerate for streams (default: 1)",
    0,
    CONF_OFFSET(stream_maxrate),
    copy_int,
    print_int
    },
    {
    "stream_localhost",
    "# Restrict stream connections to localhost only (default: on)",
    0,
    CONF_OFFSET(stream_localhost),
    copy_bool,
    print_bool
    },
    {
    "stream_limit",
    "# Limits the number of images per connection (default: 0 = unlimited)\n"
    "# Number can be defined by multiplying actual stream rate by desired number of seconds\n"
    "# Actual stream rate is the smallest of the numbers framerate and stream_maxrate",
    0,
    CONF_OFFSET(stream_limit),
    copy_int,
    print_int
    },
    {
    "stream_auth_method",
    "# Set the authentication method (default: 0)\n"
    "# 0 = disabled \n"
    "# 1 = Basic authentication\n"
    "# 2 = MD5 digest (the safer authentication)\n",
    0,
    CONF_OFFSET(stream_auth_method),
    copy_int,
    print_int
    },
    {
    "stream_authentication",
    "# Authentication for the stream. Syntax username:password\n"
    "# Default: not defined (Disabled)",
    1,
    CONF_OFFSET(stream_authentication),
    copy_string,
    print_string
    },
    {
    "stream_preview_scale",
    "# Percentage to scale the preview stream image (default: 25)\n",
    0,
    CONF_OFFSET(stream_preview_scale),
    copy_int,
    print_int
    },
    {
    "stream_preview_newline",
    "# Have stream preview image start on a new line (default: no)\n",
    0,
    CONF_OFFSET(stream_preview_newline),
    copy_bool,
    print_bool
    },
    {
    "webcontrol_port",
    "\n############################################################\n"
    "# HTTP Based Control\n"
    "############################################################\n\n"
    "# TCP/IP port for the http server to listen on (default: 0 = disabled)",
    1,
    CONF_OFFSET(webcontrol_port),
    copy_int,
    print_int
    },
    {
    "webcontrol_localhost",
    "# Restrict control connections to localhost only (default: on)",
    1,
    CONF_OFFSET(webcontrol_localhost),
    copy_bool,
    print_bool
    },
    {
    "webcontrol_html_output",
    "# Output for http server, select off to choose raw text plain (default: on)",
    1,
    CONF_OFFSET(webcontrol_html_output),
    copy_bool,
    print_bool
    },
    {
    "webcontrol_authentication",
    "# Authentication for the http based control. Syntax username:password\n"
    "# Default: not defined (Disabled)",
    1,
    CONF_OFFSET(webcontrol_authentication),
    copy_string,
    print_string
    },
    {
    "track_type",
    "\n############################################################\n"
    "# Tracking (Pan/Tilt)\n"
    "############################################################\n\n"
    "# Type of tracker (0=none (default), 1=stepper, 2=iomojo, 3=pwc, 4=generic, 5=uvcvideo, 6=servo)\n"
    "# The generic type enables the definition of motion center and motion size to\n"
    "# be used with the conversion specifiers for options like on_motion_detected",
    0,
    TRACK_OFFSET(type),
    copy_int,
    print_int
    },
    {
    "track_auto",
    "# Enable auto tracking (default: off)",
    0,
    TRACK_OFFSET(active),
    copy_bool,
    print_bool
    },
    {
    "track_port",
    "# Serial port of motor (default: none)",
    0,
    TRACK_OFFSET(port),
    copy_string,
    print_string
    },
    {
    "track_motorx",
    "# Motor number for x-axis (default: 0)",
    0,
    TRACK_OFFSET(motorx),
    copy_int,
    print_int
    },
    {
    "track_motorx_reverse",
    "# Set motorx reverse (default: off)",
    0,
    TRACK_OFFSET(motorx_reverse),
    copy_bool,
    print_bool
    },
    {
    "track_motory",
    "# Motor number for y-axis (default: 0)",
    0,
    TRACK_OFFSET(motory),
    copy_int,
    print_int
    },
    {
    "track_motory_reverse",
    "# Set motory reverse (default: off)",
    0,
    TRACK_OFFSET(motory_reverse),
    copy_bool,
    print_bool
    },
    {
    "track_maxx",
    "# Maximum value on x-axis (default: 0)",
    0,
    TRACK_OFFSET(maxx),
    copy_int,
    print_int
    },
    {
    "track_minx",
    "# Minimum value on x-axis (default: 0)",
    0,
    TRACK_OFFSET(minx),
    copy_int,
    print_int
    },
    {
    "track_maxy",
    "# Maximum value on y-axis (default: 0)",
    0,
    TRACK_OFFSET(maxy),
    copy_int,
    print_int
    },
    {
    "track_miny",
    "# Minimum value on y-axis (default: 0)",
    0,
    TRACK_OFFSET(miny),
    copy_int,
    print_int
    },
    {
    "track_homex",
    "# Center value on x-axis (default: 0)",
    0,
    TRACK_OFFSET(homex),
    copy_int,
    print_int
    },
    {
    "track_homey",
    "# Center value on y-axis (default: 0)",
    0,
    TRACK_OFFSET(homey),
    copy_int,
    print_int
    },
    {
    "track_iomojo_id",
    "# ID of an iomojo camera if used (default: 0)",
    0,
    TRACK_OFFSET(iomojo_id),
    copy_int,
    print_int
    },
    {
    "track_step_angle_x",
    "# Angle in degrees the camera moves per step on the X-axis\n"
    "# with auto-track (default: 10)\n"
    "# Currently only used with pwc type cameras",
    0,
    TRACK_OFFSET(step_angle_x),
    copy_int,
    print_int
    },
    {
    "track_step_angle_y",
    "# Angle in degrees the camera moves per step on the Y-axis\n"
    "# with auto-track (default: 10)\n"
    "# Currently only used with pwc type cameras",
    0,
    TRACK_OFFSET(step_angle_y),
    copy_int,
    print_int
    },
    {
    "track_move_wait",
    "# Delay to wait for after tracking movement as number\n"
    "# of picture frames (default: 10)",
    0,
    TRACK_OFFSET(move_wait),
    copy_int,
    print_int
    },
    {
    "track_speed",
    "# Speed to set the motor to (stepper motor option) (default: 255)",
    0,
    TRACK_OFFSET(speed),
    copy_int,
    print_int
    },
    {
    "track_stepsize",
    "# Number of steps to make (stepper motor option) (default: 40)",
    0,
    TRACK_OFFSET(stepsize),
    copy_int,
    print_int
    },
    {
    "quiet",
    "\n############################################################\n"
    "# External Commands, Warnings and Logging:\n"
    "# You can use conversion specifiers for the on_xxxx commands\n"
    "# %Y = year, %m = month, %d = date,\n"
    "# %H = hour, %M = minute, %S = second,\n"
    "# %v = event, %q = frame number, %t = thread (camera) number,\n"
    "# %D = changed pixels, %N = noise level,\n"
    "# %i and %J = width and height of motion area,\n"
    "# %K and %L = X and Y coordinates of motion center\n"
    "# %C = value defined by text_event\n"
    "# %f = filename with full path\n"
    "# %n = number indicating filetype\n"
    "# Both %f and %n are only defined for on_picture_save,\n"
    "# on_movie_start and on_movie_end\n"
    "# Quotation marks round string are allowed.\n"
    "############################################################\n\n"
    "# Do not sound beeps when detecting motion (default: on)\n"
    "# Note: Motion never beeps when running in daemon mode.",
    0,
    CONF_OFFSET(quiet),
    copy_bool,
    print_bool
    },
    {
    "on_event_start",
    "# Command to be executed when an event starts. (default: none)\n"
    "# An event starts at first motion detected after a period of no motion defined by event_gap ",
    0,
    CONF_OFFSET(on_event_start),
    copy_string,
    print_string
    },
    {
    "on_event_end",
    "# Command to be executed when an event ends after a period of no motion\n"
    "# (default: none). The period of no motion is defined by option event_gap.",
    0,
    CONF_OFFSET(on_event_end),
    copy_string,
    print_string
    },
    {
    "on_picture_save",
    "# Command to be executed when a picture (.ppm|.jpg) is saved (default: none)\n"
    "# To give the filename as an argument to a command append it with %f",
    0,
    CONF_OFFSET(on_picture_save),
    copy_string,
    print_string
    },
    {
    "on_motion_detected",
    "# Command to be executed when a motion frame is detected (default: none)",
    0,
    CONF_OFFSET(on_motion_detected),
    copy_string,
    print_string
    },
    {
    "on_area_detected",
    "# Command to be executed when motion in a predefined area is detected\n"
    "# Check option 'area_detect'. (default: none)",
    0,
    CONF_OFFSET(on_area_detected),
    copy_string,
    print_string
    },
#ifdef HAVE_FFMPEG
    {
    "on_movie_start",
    "# Command to be executed when a movie file (.mpg|.avi) is created. (default: none)\n"
    "# To give the filename as an argument to a command append it with %f",
    0,
    CONF_OFFSET(on_movie_start),
    copy_string,
    print_string
    },
    {
    "on_movie_end",
    "# Command to be executed when a movie file (.mpg|.avi) is closed. (default: none)\n"
    "# To give the filename as an argument to a command append it with %f",
    0,
    CONF_OFFSET(on_movie_end),
    copy_string,
    print_string
    },
#endif /* HAVE_FFMPEG */
    {
    "on_camera_lost",
    "# Command to be executed when a camera can't be opened or if it is lost\n"
    "# NOTE: There is situations when motion don't detect a lost camera!\n"
    "# It depends on the driver, some drivers dosn't detect a lost camera at all\n"
    "# Some hangs the motion thread. Some even hangs the PC! (default: none)",
    0,
    CONF_OFFSET(on_camera_lost),
    copy_string,
    print_string
    },

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)
    {
    "sql_log_picture",
    "\n############################################################\n"
    "# Common Options for database features.\n"
    "# Options require the database options to be active also.\n"
    "############################################################\n\n"
    "# Log to the database when creating motion triggered image file  (default: on)",
    0,
    CONF_OFFSET(sql_log_image),
    copy_bool,
    print_bool
    },
    {
    "sql_log_snapshot",
    "# Log to the database when creating a snapshot image file (default: on)",
    0,
    CONF_OFFSET(sql_log_snapshot),
    copy_bool,
    print_bool
    },
    {
    "sql_log_movie",
    "# Log to the database when creating motion triggered movie file (default: off)",
    0,
    CONF_OFFSET(sql_log_movie),
    copy_bool,
    print_bool
    },
    {
    "sql_log_timelapse",
    "# Log to the database when creating timelapse movie file (default: off)",
    0,
    CONF_OFFSET(sql_log_timelapse),
    copy_bool,
    print_bool
    },
    {
    "sql_query",
    "# SQL query string that is sent to the database\n"
    "# Use same conversion specifiers has for text features\n"
    "# Additional special conversion specifiers are\n"
    "# %n = the number representing the file_type\n"
    "# %f = filename with full path\n"
    "# Create tables :\n"
    "##\n"
    "# Mysql\n"
    "# CREATE TABLE security (camera int, filename char(80) not null, frame int, file_type int, time_stamp timestamp(14), event_time_stamp timestamp(14));\n"
    "#\n"
    "# Postgresql\n"
    "# CREATE TABLE security (camera int, filename char(80) not null, frame int, file_type int, time_stamp timestamp without time zone, event_time_stamp timestamp without time zone);\n"
    "#\n"
    "# Default value:\n"
    "# insert into security(camera, filename, frame, file_type, time_stamp, text_event) values('%t', '%f', '%q', '%n', '%Y-%m-%d %T', '%C')",
    0,
    CONF_OFFSET(sql_query),
    copy_string,
    print_string
    },
    {
    "database_type",
    "\n############################################################\n"
    "# Database Options \n"
    "############################################################\n\n"
    "# database type : mysql, postgresql, sqlite3 (default : not defined)",
    0,
    CONF_OFFSET(database_type),
    copy_string,
    print_string
    },
    {
    "database_dbname",
    "# database to log to (default: not defined)",
    0,
    CONF_OFFSET(database_dbname),
    copy_string,
    print_string
    },
    {
    "database_host",
    "# The host on which the database is located (default: not defined)",
    0,
    CONF_OFFSET(database_host),
    copy_string,
    print_string
    },
    {
    "database_user",
    "# User account name for database (default: not defined)",
    0,
    CONF_OFFSET(database_user),
    copy_string,
    print_string
    },
    {
    "database_password",
    "# User password for database (default: not defined)",
    0,
    CONF_OFFSET(database_password),
    copy_string,
    print_string
    },
    {
    "database_port",
    "# Port on which the database is located (default: not defined)\n"
    "# mysql 3306 , postgresql 5432 (default: not defined)",
    0,
    CONF_OFFSET(database_port),
    copy_int,
    print_int
    },
    {
    "database_busy_timeout",
    "# Database wait for unlock time (default: 0)",
    0,
    CONF_OFFSET(database_busy_timeout),
    copy_int,
    print_int
    },
#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3) */
    {
    "video_pipe",
    "\n############################################################\n"
    "# Video Loopback Device (vloopback project)\n"
    "############################################################\n\n"
    "# Output images to a video4linux loopback device\n"
    "# The value '-' means next available (default: not defined)",
    0,
    CONF_OFFSET(vidpipe),
    copy_string,
    print_string
    },
    {
    "motion_video_pipe",
    "# Output motion images to a video4linux loopback device\n"
    "# The value '-' means next available (default: not defined)",
    0,
    CONF_OFFSET(motionvidpipe),
    copy_string,
    print_string
    },
    {
    "thread",
    "\n##############################################################\n"
    "# Thread config files - One for each camera.\n"
    "# Except if only one camera - You only need this config file.\n"
    "# If you have more than one camera you MUST define one thread\n"
    "# config file for each camera in addition to this config file.\n"
    "##############################################################\n",
    1,
    0,
    config_thread,
    print_thread
    },

    /* using a conf.d style camera addition */
    {
    "thread_dir",
    "\n##############################################################\n"
    "# Thread config directory - One for each camera.\n"
    "##############################################################\n",
    1,
    CONF_OFFSET(thread_dir),
    read_thread_dir,
    print_string
    },
    { NULL, NULL, 0, 0, NULL, NULL }
};

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

    /*
     * For the string options, we free() if necessary and malloc()
     * if necessary. This is accomplished by calling mystrcpy();
     * see this function for more information.
     */
    while ((c = getopt(conf->argc, conf->argv, "bc:d:hmns?p:k:l:")) != EOF)
        switch (c) {
        case 'c':
            if (thread == -1)
                strcpy(cnt->conf_filename, optarg);
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
            if (thread == -1)
                cnt->log_level = (unsigned int)atoi(optarg);
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
struct context **conf_cmdparse(struct context **cnt, const char *cmd, const char *arg1)
{
    unsigned int i = 0;

    if (!cmd)
        return cnt;

    /*
     * We search through config_params until we find a param_name that matches
     * our option given by cmd (or reach the end = NULL).
     */
    while (config_params[i].param_name != NULL) {
        if (!strncasecmp(cmd, config_params[i].param_name , 255 + 50)) { // Why +50?

            /* If config_param is string we don't want to check arg1. */
            if (strcmp(config_type(&config_params[i]), "string")) {
                if (config_params[i].conf_value && !arg1)
                    return cnt;
            }

            /*
             * We call the function given by the pointer config_params[i].copy
             * If the option is a bool, copy_bool is called.
             * If the option is an int, copy_int is called.
             * If the option is a string, copy_string is called.
             * If the option is a thread, config_thread is called.
             * The arguments to the function are:
             *  cnt  - a pointer to the context structure.
             *  arg1 - a pointer to the new option value (represented as string).
             *  config_params[i].conf_value - an integer value which is a pointer
             *  to the context structure member relative to the pointer cnt.
             */
            cnt = config_params[i].copy(cnt, arg1, config_params[i].conf_value);
            return cnt;
        }
        i++;
    }

    /* We reached the end of config_params without finding a matching option. */
    MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, "%s: Unknown config option \"%s\"",
               cmd);

    return cnt;
}

/**
 * conf_process
 *      Walks through an already open config file line by line
 *      Any line starting with '#' or ';' or empty lines are ignored as a comments.
 *      Any non empty line is process so that the first word is the name of an option 'cnd'
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
        if (!(line[0] == '#' || line[0] == ';' || strlen(line) <  2)) {/* skipcomment */

            arg1 = NULL;

            /* Trim white space and any CR or LF at the end of the line. */
            end = line + strlen(line) - 1; /* Point to the last non-null character in the string. */
            while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
                end--;

            *(end+1) = '\0';

            /* If line is only whitespace we continue to the next line. */
            if (strlen(line) == 0)
                continue;

            /* Trim leading whitespace from the line and find command. */
            beg = line;
            while (*beg == ' ' || *beg == '\t')
                beg++;


            cmd = beg; /* Command starts here. */

            while (*beg != ' ' && *beg != '\t' && *beg != '=' && *beg != '\0')
                beg++;

            *beg = '\0'; /* Command string terminates here. */

            /* Trim space between command and argument. */
            beg++;

            if (strlen(beg) > 0) {
                while (*beg == ' ' || *beg == '\t' || *beg == '=' || *beg == '\n' || *beg == '\r')
                    beg++;


                /*
                 * If argument is in "" we will strip them off
                 * It is important that we can use "" so that we can use
                 * leading spaces in text_left and text_right.
                 */
                if ((beg[0] == '"' && beg[strlen(beg)-1] == '"') ||
                    (beg[0] == '\'' && beg[strlen(beg)-1] == '\'')) {
                    beg[strlen(beg)-1] = '\0';
                    beg++;
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
 *       Is used to write out the config file(s) motion.conf and any thread
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
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s: Writing config file to %s",
                   cnt[thread]->conf_filename);

        conffile = myfopen(cnt[thread]->conf_filename, "w");

        if (!conffile)
            continue;

        fprintf(conffile, "# %s\n", cnt[thread]->conf_filename);
        fprintf(conffile, "#\n# This config file was generated by motion " VERSION "\n");
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
                if (strncmp(config_params[i].param_name, "text", 4) || strncmp(retval, " ", 1))
                    fprintf(conffile, "%s %s\n\n", config_params[i].param_name, retval);
                else
                    fprintf(conffile, "%s \"%s\"\n\n", config_params[i].param_name, retval);
            } else {
                val = NULL;
                config_params[i].print(cnt, &val, i, thread);
                /*
                 * It can either be a thread file parameter or a disabled parameter.
                 * If it is a thread parameter write it out.
                 * Else write the disabled option to the config file but with a
                 * comment mark in front of the parameter name.
                 */
                if (val) {
                    fprintf(conffile, "%s\n", config_params[i].param_help);
                    fprintf(conffile, "%s\n", val);

                    if (strlen(val) == 0)
                        fprintf(conffile, "; thread %s/motion/thread1.conf\n", sysconfdir);

                    free(val);
                } else if (thread == 0) {
                    fprintf(conffile, "%s\n", config_params[i].param_help);
                    fprintf(conffile, "; %s value\n\n", config_params[i].param_name);
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
 *   for each thread config file it populates a cnt[1], cnt[2]... for each
 *   thread
 * - Finally it process the options given in the Command-line. This is done
 *   for each thread cnt[i] so that the Command-line options overrides any
 *   option given by motion.conf or a thread config file.
 *
 * Returns context struct.
 */
struct context **conf_load(struct context **cnt)
{
    FILE *fp = NULL;
    char filename[PATH_MAX];
    int i;
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
     * 4. sysconfig/motion.conf
     */
    /* Get filename , pid file & log file from Command-line. */
    cnt[0]->log_type_str[0] = 0;
    cnt[0]->conf_filename[0] = 0;
    cnt[0]->pid_file[0] = 0;
    cnt[0]->log_file[0] = 0;
    cnt[0]->log_level = -1;

    conf_cmdline(cnt[0], -1);

    if (cnt[0]->conf_filename[0]) { /* User has supplied filename on Command-line. */
      strncpy(filename, cnt[0]->conf_filename, PATH_MAX-1);
      filename[PATH_MAX-1] = '\0';
      fp = fopen (filename, "r");
    }

    if (!fp) {  /* Command-line didn't work, try current dir. */
        char *path = NULL;

        if (cnt[0]->conf_filename[0])
            MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO, "%s: Configfile %s not found - trying defaults.",
                       filename);

        if ((path = get_current_dir_name()) == NULL) {
            MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO, "%s: Error get_current_dir_name");
            exit(-1);
        }

        snprintf(filename, PATH_MAX, "%s/motion.conf", path);
        fp = fopen (filename, "r");
        free(path);
    }

    if (!fp) {  /* Specified file does not exist... try default file. */
        snprintf(filename, PATH_MAX, "%s/.motion/motion.conf", getenv("HOME"));
        fp = fopen(filename, "r");

        if (!fp) {
            snprintf(filename, PATH_MAX, "%s/motion/motion.conf", sysconfdir);
            fp = fopen(filename, "r");

            if (!fp) /* There is no config file.... use defaults. */
                MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO, "%s: could not open configfile %s",
                           filename);
        }
    }

    /* Now we process the motion.conf config file and close it. */
    if (fp) {
      strncpy(cnt[0]->conf_filename, filename, sizeof(cnt[0]->conf_filename) - 1);
      cnt[0]->conf_filename[sizeof(cnt[0]->conf_filename) - 1] = '\0';
      MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s: Processing thread 0 - config file %s",
		 filename);
      cnt = conf_process(cnt, fp);
      myfclose(fp);
    } else {
        MOTION_LOG(CRT, TYPE_ALL, NO_ERRNO, "%s: Not config file to process using default values");
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

    while (cnt[++i])
        conf_cmdline(cnt[i], i);

    /* If pid file was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->pid_file[0])
        cnt[0]->conf.pid_file = mystrcpy(cnt[0]->conf.pid_file, cnt[0]->pid_file);

    /* If log file was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_file[0])
        cnt[0]->conf.log_file = mystrcpy(cnt[0]->conf.log_file, cnt[0]->log_file);

    /* If log type string was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_type_str[0])
        cnt[0]->conf.log_type_str = mystrcpy(cnt[0]->conf.log_type_str, cnt[0]->log_type_str);

    /* if log level was passed from Command-line copy to main thread conf struct. */
    if (cnt[0]->log_level != -1)
        cnt[0]->conf.log_level = cnt[0]->log_level;

    return cnt;
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
void malloc_strings(struct context *cnt)
{
    unsigned int i = 0;
    char **val;
    while (config_params[i].param_name != NULL) {
        if (config_params[i].copy == copy_string) { /* if member is a string */
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

/************************************************************************
 * copy functions
 *
 *   copy_bool   - convert a bool representation to int
 *   copy_int    - convert a string to int
 *   copy_string - just a string copy
 *
 * @param str     - A char *, pointing to a string representation of the
 *                  value.
 * @param val_ptr - points to the place where to store the value relative
 *                  to pointer pointing to the given context structure
 * @cnt           - points to a context structure for a thread
 *
 * The function is given a pointer cnt to a context structure and a pointer val_ptr
 * which is an integer giving the position of the structure member relative to the
 * pointer of the context structure.
 * If the context structure is for thread 0 (cnt[0]->threadnr is zero) then the
 * function also sets the value for all the child threads since thread 0 is the
 * global thread.
 * If the thread given belongs to a child thread (cnt[0]->threadnr is not zero)
 * the function will only assign the value for the given thread.
 ***********************************************************************/

/**
 * copy_bool
 *      Assigns a config option to a new boolean value.
 *      The boolean is given as a string in str which is converted to 0 or 1
 *      by the function. Values 1, yes and on are converted to 1 ignoring case.
 *      Any other value is converted to 0.
 *
 * Returns context struct.
 */
static struct context **copy_bool(struct context **cnt, const char *str, int val_ptr)
{
    void *tmp;
    int i;

    i = -1;
    while (cnt[++i]) {
        tmp = (char *)cnt[i]+(int)val_ptr;

        if (!strcmp(str, "1") || !strcasecmp(str, "yes") || !strcasecmp(str, "on")) {
            *((int *)tmp) = 1;
        } else {
            *((int *)tmp) = 0;
        }

        if (cnt[0]->threadnr)
            return cnt;
    }

    return cnt;
}

/**
 * copy_int
 *      Assigns a config option to a new integer value.
 *      The integer is given as a string in str which is converted to integer
 *      by the function.
 *
 * Returns context struct.
 */
static struct context **copy_int(struct context **cnt, const char *str, int val_ptr)
{
    void *tmp;
    int i;

    i = -1;
    while (cnt[++i]) {
        tmp = (char *)cnt[i]+val_ptr;
        *((int *)tmp) = atoi(str);

        if (cnt[0]->threadnr)
            return cnt;
    }

    return cnt;
}

/**
 * copy_string
 *      Assigns a new string value to a config option.
 *      Strings are handled differently from bool and int.
 *      the char *conf->option that we are working on is free()'d
 *      (if memory for it has already been malloc()'d), and set to
 *      a freshly malloc()'d string with the value from str,
 *      or NULL if str is blank.
 *
 * Returns context struct.
 */
struct context **copy_string(struct context **cnt, const char *str, int val_ptr)
{
    char **tmp;
    int i;

    i = -1;

    while (cnt[++i]) {
        tmp = (char **)((char *)cnt[i] + val_ptr);

        /*
         * mystrcpy assigns the new string value
         * including free'ing and reserving new memory for it.
         */
        *tmp = mystrcpy(*tmp, str);

        /*
         * Set the option on all threads if setting the option
         * for thread 0; otherwise just set that one thread's option.
         */
        if (cnt[0]->threadnr)
            return cnt;
    }

    return cnt;
}


/**
 * mystrcpy
 *      Is used to assign string type fields (e.g. config options)
 *      In a way so that we the memory is malloc'ed to fit the string.
 *      If a field is already pointing to a string (not NULL) the memory of the
 *      old string is free'd and new memory is malloc'ed and filled with the
 *      new string is copied into the the memory and with the char pointer
 *      pointing to the new string.
 *
 *      from - pointer to the new string we want to copy
 *      to   - the pointer to the current string (or pointing to NULL)
 *              If not NULL the memory it points to is free'd.
 *
 * Returns pointer to the new string which is in malloc'ed memory
 * FIXME The strings that are malloc'ed with this function should be freed
 * when the motion program is terminated normally instead of relying on the
 * OS to clean up.
 */
char *mystrcpy(char *to, const char *from)
{
    /*
     * Free the memory used by the to string, if such memory exists,
     * and return a pointer to a freshly malloc()'d string with the
     * same value as from.
     */

    if (to != NULL)
        free(to);

    return mystrdup(from);
}


/**
 * mystrdup
 *      Truncates the string to the length given by the environment
 *      variable PATH_MAX to ensure that config options can always contain
 *      a really long path but no more than that.
 *
 * Returns a pointer to a freshly malloc()'d string with the same
 *      value as the string that the input parameter 'from' points to,
 *      or NULL if the from string is 0 characters.
 */
char *mystrdup(const char *from)
{
    char *tmp;
    size_t stringlength;

    if (from == NULL || !strlen(from)) {
        tmp = NULL;
    } else {
        stringlength = strlen(from);
        stringlength = (stringlength < PATH_MAX ? stringlength : PATH_MAX);
        tmp = mymalloc(stringlength + 1);
        strncpy(tmp, from, stringlength);

        /*
         * We must ensure the string always has a NULL terminator.
         * This necessary because strncpy will not append a NULL terminator
         * if the original string is greater than string length.
         */
        tmp += stringlength;
        *tmp = '\0';
        tmp -= stringlength;
    }

    return tmp;
}

/**
 * config_type
 *      Returns a pointer to string containing value the type of config parameter passed.
 *
 * Returns const char *.
 */
const char *config_type(config_param *configparam)
{
    if (configparam->copy == copy_string)
        return "string";
    if (configparam->copy == copy_int)
        return "int";
    if (configparam->copy == copy_bool)
        return "bool";

    return "unknown";
}


/**
 * print_bool
 *      Returns a pointer to string containing boolean value 'on' / 'off' or NULL.
 *
 * Returns const char *.
 */
static const char *print_bool(struct context **cnt, char **str ATTRIBUTE_UNUSED,
                              int parm, unsigned int threadnr)
{
    int val = config_params[parm].conf_value;

    if (threadnr &&
        *(int*)((char *)cnt[threadnr] + val) == *(int*)((char *)cnt[0] + val))
        return NULL;

    if (*(int*)((char *)cnt[threadnr] + val))
        return "on";
    else
        return "off";
}

/**
 * print_string
 *      Returns a pointer to a string containing the value of the config option,
 *      If the thread number is not 0 the string is compared with the value of the same
 *      option in thread 0.
 *
 * Returns If the option is not defined NULL is returned.
 *         If the value is the same, NULL is returned which means that
 *         the option is not written to the thread config file.
 */
static const char *print_string(struct context **cnt,
                                char **str ATTRIBUTE_UNUSED, int parm,
                                unsigned int threadnr)
{
    int val = config_params[parm].conf_value;
    const char **cptr0, **cptr1;

    /* strcmp does not like NULL so we have to check for this also. */
    cptr0 = (const char **)((char *)cnt[0] + val);
    cptr1 = (const char **)((char *)cnt[threadnr] + val);

    if ((threadnr) && (*cptr0 != NULL) && (*cptr1 != NULL) && (!strcmp(*cptr0, *cptr1)))
        return NULL;

    return *cptr1;
}

static const char *print_int(struct context **cnt, char **str ATTRIBUTE_UNUSED,
                             int parm, unsigned int threadnr)
{
    static char retval[20];
    int val = config_params[parm].conf_value;

    if (threadnr &&
        *(int*)((char *)cnt[threadnr] + val) == *(int*)((char *)cnt[0] + val))
        return NULL;

    sprintf(retval, "%d", *(int*)((char *)cnt[threadnr] + val));

    return retval;
}


static const char *print_thread(struct context **cnt, char **str,
                                int parm ATTRIBUTE_UNUSED, unsigned int threadnr)
{
    char *retval;
    unsigned int i = 0;

    if (!str || threadnr)
        return NULL;

    retval = mymalloc(1);
    retval[0] = 0;

    while (cnt[++i]) {
        retval = myrealloc(retval, strlen(retval) + strlen(cnt[i]->conf_filename) + 10,
                           "print_thread");
        sprintf(retval + strlen(retval), "thread %s\n", cnt[i]->conf_filename);
    }

    *str = retval;

    return NULL;
}


/**
 * config_thread_dir
 *     Read the directory finding all *.conf files in the path
 *     when calls config_thread
 */

static struct context **read_thread_dir(struct context **cnt, const char *str,
                                            int val ATTRIBUTE_UNUSED)
{
    DIR *dp;
    struct dirent *ep;
    int name_len;

    char conf_file[PATH_MAX];

    dp = opendir(str);
    if (dp != NULL)
    {
        while( (ep = readdir(dp)) )
        {
            name_len = strlen(ep->d_name);
            if (name_len > strlen(EXTENSION) &&
                    (strncmp(EXTENSION,
                                (ep->d_name + name_len - strlen(EXTENSION)),
                                strlen(EXTENSION)) == 0
                    )
                )
            {
                memset(conf_file, '\0', sizeof(conf_file));
                snprintf(conf_file, sizeof(conf_file) - 1, "%s/%s",
                            str, ep->d_name);
                MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
                    "%s: Processing config file %s", conf_file );
                cnt = config_thread(cnt, conf_file, 0);
            }
        }
        closedir(dp);
    }
    else
    {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO, "%s: Thread directory config "
                    "%s not found", str);
    }

    return cnt;
}

/**
 * config_thread
 *      Is called during initial config file loading each time Motion
 *      finds a thread option in motion.conf
 *      The size of the context array is increased and the main context's values are
 *      copied to the new thread.
 *
 *      cnt  - pointer to the array of pointers pointing to the context structures
 *      str  - pointer to a string which is the filename of the thread config file
 *      val  - is not used. It is defined to be function header compatible with
 *            copy_int, copy_bool and copy_string.
 */
static struct context **config_thread(struct context **cnt, const char *str,
                                      int val ATTRIBUTE_UNUSED)
{
    int i;
    FILE *fp;

    if (cnt[0]->threadnr)
        return cnt;

    fp = fopen(str, "r");

    if (!fp) {
        MOTION_LOG(ALR, TYPE_ALL, SHOW_ERRNO, "%s: Thread config file %s not found",
                   str);
        return cnt;
    }

    /* Find the current number of threads defined. */
    i = -1;

    while (cnt[++i]);

    /*
     * Make space for the threads + the terminating NULL pointer
     * in the array of pointers to context structures
     * First thread is 0 so the number of threads is i + 1
     * plus an extra for the NULL pointer. This gives i + 2
     */
    cnt = myrealloc(cnt, sizeof(struct context *) * (i + 2), "config_thread");

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

    /* Process the thread's config file and notify user on console. */
    strcpy(cnt[i]->conf_filename, str);
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, "%s: Processing config file %s",
               str);
    conf_process(cnt + i, fp);

    /* Finally we close the thread config file. */
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
    printf("motion Version "VERSION", Copyright 2000-2005 Jeroen Vreeken/Folkert van Heusden/Kenneth Lavrsen\n");
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
    printf("it will read motion.conf from current directory, ~/.motion or %s/motion.\n", sysconfdir);
    printf("\n");
}
