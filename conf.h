/*
  **
  ** conf.h - function prototypes for the config handling routines
  **
  ** Originally written for the dproxy package by Matthew Pratt.
  **
  ** Copyright 2000 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

#ifndef _INCLUDE_CONF_H
#define _INCLUDE_CONF_H

/* 
    more parameters may be added later.
 */
struct config {
    int setup_mode;
    int width;
    int height;
    int quality;
    int rotate_deg;
    int max_changes;
    int threshold_tune;
    const char *output_normal;
    int motion_img;
    int output_all;
    int gap;
    int maxmpegtime;
    int snapshot_interval;
    const char *locate;
    int input;
    int norm;
    int frame_limit;
    int quiet;
    int ppm;
    int noise;
    int noise_tune;
    int minimum_frame_time;
    int lightswitch;
    int autobright;
    int brightness;
    int contrast;
    int saturation;
    int hue;
    int roundrobin_frames;
    int roundrobin_skip;
    int pre_capture;
    int post_capture;
    int switchfilter;
    int ffmpeg_cap_new;
    int ffmpeg_cap_motion;
    int ffmpeg_bps;
    int ffmpeg_vbr;
    int ffmpeg_deinterlace;
    const char *ffmpeg_video_codec;
    int webcam_port;
    int webcam_quality;
    int webcam_motion;
    int webcam_maxrate;
    int webcam_localhost;
    int webcam_limit;
    int control_port;
    int control_localhost;
    int control_html_output;
    const char *control_authentication;
    unsigned long frequency;
    int tuner_number;
    int timelapse;
    const char *timelapse_mode; 
#if (defined(BSD))
    const char *tuner_device;
#endif
    const char *video_device;
    short unsigned int v4l2_palette;
    const char *vidpipe;
    const char *filepath;
    const char *jpegpath;
    const char *mpegpath;
    const char *snappath;
    const char *timepath;
    char *on_event_start;
    char *on_event_end;
    const char *mask_file;
    int smart_mask_speed;
    int sql_log_image;
    int sql_log_snapshot;
    int sql_log_mpeg;
    int sql_log_timelapse;
    const char *sql_query;
    const char *mysql_db;
    const char *mysql_host;
    const char *mysql_user;
    const char *mysql_password;
    char *on_picture_save;
    char *on_area_detected;
    char *on_motion_detected;
    char *on_movie_start;
    char *on_movie_end;
    char *on_camera_lost;
    const char *motionvidpipe;
    const char *netcam_url;
    const char *netcam_userpass;
    const char *netcam_http;
    const char *netcam_proxy;
    unsigned int netcam_tolerant_check;
    const char *pgsql_db;
    const char *pgsql_host;
    const char *pgsql_user;
    const char *pgsql_password;
    int pgsql_port;
    int text_changes;
    const char *text_left;
    const char *text_right;
    const char *text_event;
    int text_double;
    const char *despeckle;
    const char *area_detect;
    int minimum_motion_frames;
    char *pid_file;
    int argc;
    char **argv;
};

/** 
 * typedef for a param copy function. 
 */
typedef struct context ** (* conf_copy_func)(struct context **, const char *, int);
typedef const char *(* conf_print_func)(struct context **, char **, int, unsigned short int);

/**
 * description for parameters in the config file
 */
typedef struct {
    const char * param_name;        /* name for this parameter                  */
    const char * param_help;        /* short explanation for parameter          */
    unsigned short int main_thread; /* belong only to main thread when value>0  */
    int conf_value;                 /* pointer to a field in struct context     */
    conf_copy_func  copy;           /* a function to set the value in 'config'  */
    conf_print_func print;          /* a function to output the value to a file */
} config_param; 


extern config_param config_params[];

struct context **conf_load(struct context **);
struct context **conf_cmdparse(struct context **, const char *, const char *);
const char *config_type(config_param *);
void conf_print(struct context **);
void malloc_strings (struct context *);
char *mystrdup(const char *);
char *mystrcpy(char *, const char *);
struct context **copy_string(struct context **, const char *, int);

#ifndef HAVE_GET_CURRENT_DIR_NAME
char *get_current_dir_name(void);
#endif


#endif /* _INCLUDE_CONF_H */
