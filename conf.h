/*
 *
 * conf.h - function prototypes for the config handling routines
 *
 * Originally written for the dproxy package by Matthew Pratt.
 *
 * Copyright 2000 Jeroen Vreeken (pe1rxq@chello.nl)
 *
 * This software is licensed under the terms of the GNU General
 * Public License (GPL). Please see the file COPYING for details.
 *
 *
 */

#ifndef _INCLUDE_CONF_H
#define _INCLUDE_CONF_H

/*
 * More parameters may be added later.
 */
struct config {
    const char *camera_name;
    unsigned int log_level;
    char *log_type_str;
    char *log_file;
    int setup_mode;
    int width;
    int height;
    int camera_id;
    int quality;
    const char *flip_axis;
    int rotate_deg;
    int max_changes;
    int threshold_tune;
    const char *output_pictures;
    int ffmpeg_duplicate_frames;
    int motion_img;
    int emulate_motion;
    int event_gap;
    int max_movie_time;
    int snapshot_interval;
    const char *locate_motion_mode;
    const char *locate_motion_style;
    int input;
    int norm;
    int frame_limit;
    int quiet;
    int useextpipe; /* ext_pipe on or off */
    const char *extpipe; /* full Command-line for pipe -- must accept YUV420P images  */
    const char *picture_type;
    int noise;
    int noise_tune;
    int minimum_frame_time;
    int lightswitch;
    int autobright;
    char *vid_control_params;
    int roundrobin_frames;
    int roundrobin_skip;
    int pre_capture;
    int post_capture;
    int switchfilter;
    int ffmpeg_output;
    int ffmpeg_output_debug;
    int ffmpeg_bps;
    int ffmpeg_vbr;
    int ffmpeg_passthrough;
    const char *ffmpeg_video_codec;
    int ipv6_enabled;
    int stream_port;
    int substream_port;
    int stream_quality;
    int stream_motion;
    int stream_maxrate;
    int stream_localhost;
    int stream_limit;
    int stream_auth_method;
    const char *stream_authentication;
    int stream_preview_scale;
    int stream_preview_newline;
    int webcontrol_port;
    int webcontrol_localhost;
    int webcontrol_html_output;
    int webcontrol_parms;
    const char *webcontrol_authentication;
    unsigned long frequency;
    int tuner_number;
    int         timelapse_interval;
    int         timelapse_fps;
    const char *timelapse_mode;
    const char *timelapse_codec;
    const char *tuner_device;
    const char *video_device;
    int v4l2_palette;
    const char *vidpipe;
    const char *filepath;
    const char *imagepath;
    const char *moviepath;
    const char *snappath;
    const char *timepath;
    char *on_event_start;
    char *on_event_end;
    const char *mask_file;
    const char *mask_privacy;
    int smart_mask_speed;
    int sql_log_image;
    int sql_log_snapshot;
    int sql_log_movie;
    int sql_log_timelapse;
    const char *sql_query_start;
    const char *sql_query;
    const char *database_type;
    const char *database_dbname;
    const char *database_host;
    const char *database_user;
    const char *database_password;
    int database_busy_timeout;
    int database_port;
    char *on_picture_save;
    char *on_area_detected;
    char *on_motion_detected;
    char *on_movie_start;
    char *on_movie_end;
    char *on_camera_lost;
    char *on_camera_found;
    const char *motionvidpipe;
    const char *netcam_url;
    const char *netcam_highres;
    const char *netcam_userpass;
    const char *netcam_keepalive;
    const char *netcam_proxy;
    unsigned int netcam_tolerant_check;
    unsigned int rtsp_uses_tcp;
    const char *mmalcam_name;
    const char *mmalcam_control_params;
    int text_changes;
    const char *text_left;
    const char *text_right;
    const char *text_event;
    int text_double;
    const char *despeckle_filter;
    const char *area_detect;
    const char *camera_dir;
    int minimum_motion_frames;
    const char *exif_text;
    char *pid_file;
    int argc;
    char **argv;
};

/**
 * typedef for a param copy function.
 */
typedef struct context ** (* conf_copy_func)(struct context **, const char *, int);
typedef const char *(* conf_print_func)(struct context **, char **, int, unsigned int);

/**
 * description for parameters in the config file
 */
typedef struct {
    const char *param_name;           /* name for this parameter                  */
    const char *param_help;           /* short explanation for parameter          */
    unsigned int main_thread;         /* belong only to main thread when value>0  */
    int conf_value;                   /* pointer to a field in struct context     */
    conf_copy_func  copy;             /* a function to set the value in 'config'  */
    conf_print_func print;            /* a function to output the value to a file */
    int  webui_level;                 /* Enum to display in webui: 0,1,2,3,99(always to never)*/
} config_param;

extern config_param config_params[];

/**
 * description for deprecated parameters in the config file
 */
typedef struct {
    const char *name;           /* Name of the deprecated option */
    const char *last_version;   /* Last version this option was used in */
    const char *info;           /* Short text on why it was deprecated (removed, replaced with, etc) */
    int conf_value;             /* Pointer to the replacement field in struct context */
    conf_copy_func copy;        /* Function to set the replacement value */
} dep_config_param;

extern dep_config_param dep_config_params[];

struct context **conf_load(struct context **);
struct context **copy_string(struct context **, const char *, int);
struct context **conf_cmdparse(struct context **, const char *, const char *);
void conf_output_parms(struct context **cnt);
const char *config_type(config_param *);
void conf_print(struct context **);
char *mystrdup(const char *);
char *mystrcpy(char *, const char *);

#endif /* _INCLUDE_CONF_H */
