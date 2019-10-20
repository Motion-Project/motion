/*
 *
 * conf.hpp - function prototypes for the config handling routines
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

    struct ctx_cam;
    struct ctx_motapp;

    struct ctx_config {
        /* Overall system configuration parameters */
        int             quiet;
        char*           camera_name;
        int             camera_id;
        char*           camera_dir;
        char*           target_dir;

        /* Capture device configuration parameters */
        char*           videodevice;
        char*           vid_control_params;
        int             v4l2_palette;
        int             input;
        int             norm;
        unsigned long   frequency;
        char*           tuner_device;
        int             roundrobin_frames;
        int             roundrobin_skip;
        int             roundrobin_switchfilter;

        char*           netcam_url;
        char*           netcam_highres;
        char*           netcam_userpass;
        int             netcam_use_tcp;

        char*           mmalcam_name;
        char*           mmalcam_control_params;

        /* Image processing configuration parameters */
        int             width;
        int             height;
        int             framerate;
        int             minimum_frame_time;
        int             rotate;
        char*           flip_axis;
        char*           locate_motion_mode;
        char*           locate_motion_style;
        char*           text_left;
        char*           text_right;
        int             text_changes;
        int             text_scale;
        char*           text_event;

        /* Motion detection configuration parameters */
        int             emulate_motion;
        int             primary_method;
        int             threshold;
        int             threshold_maximum;
        int             threshold_sdevx;
        int             threshold_sdevy;
        int             threshold_sdevxy;
        int             threshold_tune;
        int             secondary_interval;
        int             secondary_method;
        char*           secondary_model;
        char*           secondary_config;
        int             secondary_method2;
        char*           secondary_model2;
        char*           secondary_config2;
        int             secondary_method3;
        char*           secondary_model3;
        char*           secondary_config3;
        int             noise_level;
        int             noise_tune;
        char*           despeckle_filter;
        char*           area_detect;
        char*           mask_file;
        char*           mask_privacy;
        int             smart_mask_speed;
        int             lightswitch_percent;
        int             lightswitch_frames;
        int             minimum_motion_frames;
        int             event_gap;
        int             pre_capture;
        int             post_capture;

        /* Script execution configuration parameters */
        char*           on_event_start;
        char*           on_event_end;
        char*           on_picture_save;
        char*           on_area_detected;
        char*           on_motion_detected;
        char*           on_movie_start;
        char*           on_movie_end;
        char*           on_camera_lost;
        char*           on_camera_found;

        /* Picture output configuration parameters */
        char*           picture_output;
        int             picture_output_motion;
        char*           picture_type;
        int             picture_quality;
        char*           picture_exif;
        char*           picture_filename;

        /* Snapshot configuration parameters */
        int             snapshot_interval;
        char*           snapshot_filename;

        /* Movie output configuration parameters */
        int             movie_output;
        int             movie_output_motion;
        int             movie_max_time;
        int             movie_bps;
        int             movie_quality;
        char*           movie_codec;
        int             movie_passthrough;
        char*           movie_filename;
        int             movie_extpipe_use;
        char*           movie_extpipe;

        /* Timelapse movie configuration parameters */
        int             timelapse_interval;
        char*           timelapse_mode;
        int             timelapse_fps;
        char*           timelapse_codec;
        char*           timelapse_filename;

        /* Loopback device configuration parameters */
        char*           video_pipe;
        char*           video_pipe_motion;

        /* Webcontrol configuration parameters */
        int             webcontrol_port;
        int             webcontrol_ipv6;
        int             webcontrol_localhost;
        int             webcontrol_parms;
        int             webcontrol_interface;
        int             webcontrol_auth_method;
        char*           webcontrol_authentication;
        int             webcontrol_tls;
        char*           webcontrol_cert;
        char*           webcontrol_key;
        char*           webcontrol_cors_header;

        /* Live stream configuration parameters */
        int             stream_port;
        int             stream_localhost;
        int             stream_auth_method;
        char*           stream_authentication;
        int             stream_tls;
        char*           stream_cors_header;
        int             stream_preview_scale;
        int             stream_preview_newline;
        int             stream_preview_method;
        int             stream_quality;
        int             stream_grey;
        int             stream_motion;
        int             stream_maxrate;

        /* Database and SQL configuration parameters */
        char*           database_type;
        char*           database_dbname;
        char*           database_host;
        int             database_port;
        char*           database_user;
        char*           database_password;
        int             database_busy_timeout;

        int             sql_log_picture;
        int             sql_log_snapshot;
        int             sql_log_movie;
        int             sql_log_timelapse;
        char*           sql_query_start;
        char*           sql_query_stop;
        char*           sql_query;

        int             track_type;
        int             track_auto;
        int             track_step_angle_x;
        int             track_step_angle_y;
        int             track_move_wait;
        char*           track_generic_move;
    };

    enum PARM_CAT{
        PARM_CAT_00
        ,PARM_CAT_01
        ,PARM_CAT_02
        ,PARM_CAT_03
        ,PARM_CAT_04
        ,PARM_CAT_05
    };
    enum PARM_TYP{
        PARM_TYP_STRING
        ,PARM_TYP_INT
        ,PARM_TYP_BOOL
    };

    /** Current parameters in the config file */
    struct ctx_parm {
        const char*         parm_name;     /* name for this parameter                  */
        const char*         parm_help;     /* short explanation for parameter          */
        int                 main_thread;    /* belong only to main thread when value>0  */
        enum PARM_TYP       parm_type;      /* char string of either bool,int,string,etc.  */
        enum PARM_CAT       parm_cat;
        int                 webui_level;    /* Enum to display in webui: 0,1,2,3,99(always to never)*/
    };

    /** Deprecated parameters in the config file  */
    struct ctx_parm_depr{
        const char*         parm_name;     /* Name of the deprecated option */
        const char*         last_version;  /* Last version this option was used in */
        const char*         info;          /* Short text on why it was deprecated (removed, replaced with, etc) */
        const char*         newname;       /* Name of the new parameter */
    };

    extern struct ctx_parm config_parms[];
    extern struct ctx_parm_depr config_parms_depr[];

    void conf_init_app(struct ctx_motapp *motapp, int argc, char* argv[]);
    void conf_init_cams(struct ctx_motapp *motapp);
    void conf_deinit(struct ctx_motapp *motapp);
    void conf_parms_log(struct ctx_cam **cam_list);
    void conf_parms_write(struct ctx_cam **cam_list);

#endif /* _INCLUDE_CONF_H */
