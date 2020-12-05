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
 *    Copyright 2020 MotionMrDave@gmail.com
 */


#ifndef _INCLUDE_CONF_H
#define _INCLUDE_CONF_H

    struct ctx_cam;
    struct ctx_motapp;

    struct ctx_config {
        /* Overall system configuration parameters */
        int             quiet;
        std::string     camera_name;
        int             camera_id;
        std::string     camera_dir;
        std::string     target_dir;

        /* Capture device configuration parameters */
        std::string     v4l2_device;
        std::string     v4l2_params;

        std::string     netcam_url;
        std::string     netcam_params;
        std::string     netcam_high_url;
        std::string     netcam_high_params;
        std::string     netcam_userpass;

        std::string     mmalcam_name;
        std::string     mmalcam_params;

        /* Image processing configuration parameters */
        int             width;
        int             height;
        int             framerate;
        int             minimum_frame_time;
        int             rotate;
        std::string     flip_axis;
        std::string     locate_motion_mode;
        std::string     locate_motion_style;
        std::string     text_left;
        std::string     text_right;
        int             text_changes;
        int             text_scale;
        std::string     text_event;

        /* Motion detection configuration parameters */
        int             emulate_motion;
        int             primary_method;
        int             threshold;
        int             threshold_maximum;
        int             threshold_sdevx;
        int             threshold_sdevy;
        int             threshold_sdevxy;
        int             threshold_ratio;
        int             threshold_tune;
        int             secondary_interval;
        int             secondary_method;
        std::string     secondary_params;
        int             noise_level;
        int             noise_tune;
        std::string     despeckle_filter;
        std::string     area_detect;
        std::string     mask_file;
        std::string     mask_privacy;
        int             smart_mask_speed;
        int             lightswitch_percent;
        int             lightswitch_frames;
        int             minimum_motion_frames;
        int             event_gap;
        int             pre_capture;
        int             post_capture;

        /* Script execution configuration parameters */
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

        /* Picture output configuration parameters */
        std::string     picture_output;
        std::string     picture_output_motion;
        std::string     picture_type;
        int             picture_quality;
        std::string     picture_exif;
        std::string     picture_filename;

        /* Snapshot configuration parameters */
        int             snapshot_interval;
        std::string     snapshot_filename;

        /* Movie output configuration parameters */
        int             movie_output;
        int             movie_output_motion;
        int             movie_max_time;
        int             movie_bps;
        int             movie_quality;
        std::string     movie_codec;
        int             movie_passthrough;
        std::string     movie_filename;
        int             movie_extpipe_use;
        std::string     movie_extpipe;

        /* Timelapse movie configuration parameters */
        int             timelapse_interval;
        std::string     timelapse_mode;
        int             timelapse_fps;
        std::string     timelapse_codec;
        std::string     timelapse_filename;

        /* Loopback device configuration parameters */
        std::string     video_pipe;
        std::string     video_pipe_motion;

        /* Webcontrol configuration parameters */
        int             webcontrol_port;
        int             webcontrol_ipv6;
        int             webcontrol_localhost;
        int             webcontrol_parms;
        int             webcontrol_interface;
        int             webcontrol_auth_method;
        std::string     webcontrol_authentication;
        int             webcontrol_tls;
        std::string     webcontrol_cert;
        std::string     webcontrol_key;
        std::string     webcontrol_cors_header;

        /* Live stream configuration parameters */
        int             stream_port;
        int             stream_localhost;
        int             stream_auth_method;
        std::string     stream_authentication;
        int             stream_tls;
        std::string     stream_cors_header;
        int             stream_preview_scale;
        int             stream_preview_newline;
        int             stream_preview_method;
        int             stream_quality;
        int             stream_grey;
        int             stream_motion;
        int             stream_maxrate;

        /* Database and SQL configuration parameters */
        std::string     database_type;
        std::string     database_dbname;
        std::string     database_host;
        int             database_port;
        std::string     database_user;
        std::string     database_password;
        int             database_busy_timeout;

        int             sql_log_picture;
        int             sql_log_snapshot;
        int             sql_log_movie;
        int             sql_log_timelapse;
        std::string     sql_query_start;
        std::string     sql_query_stop;
        std::string     sql_query;

        int             track_type;
        int             track_auto;
        int             track_step_angle_x;
        int             track_step_angle_y;
        int             track_move_wait;
        std::string     track_generic_move;
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
        const std::string   parm_name;     /* name for this parameter                  */
        const std::string   parm_help;     /* short explanation for parameter          */
        int                 main_thread;    /* belong only to main thread when value>0  */
        enum PARM_TYP       parm_type;      /* char string of either bool,int,string,etc.  */
        enum PARM_CAT       parm_cat;
        int                 webui_level;    /* Enum to display in webui: 0,1,2,3,99(always to never)*/
    };

    /** Deprecated parameters in the config file  */
    struct ctx_parm_depr{
        const std::string   parm_name;     /* Name of the deprecated option */
        const std::string   last_version;  /* Last version this option was used in */
        const std::string   info;          /* Short text on why it was deprecated (removed, replaced with, etc) */
        const std::string   newname;       /* Name of the new parameter */
    };

    extern struct ctx_parm config_parms[];
    extern struct ctx_parm_depr config_parms_depr[];

    void conf_init_app(struct ctx_motapp *motapp, int argc, char* argv[]);
    void conf_init_cams(struct ctx_motapp *motapp);
    void conf_deinit(struct ctx_motapp *motapp);
    void conf_parms_log(struct ctx_cam **cam_list);
    void conf_parms_write(struct ctx_cam **cam_list);

    void conf_edit_set(struct ctx_motapp *motapp, int threadnbr,std::string parm_nm, std::string parm_val);
    void conf_edit_set(struct ctx_motapp *motapp, int threadnbr,const char *parm_nm_chr, std::string parm_val);
    void conf_edit_set(struct ctx_motapp *motapp, int threadnbr,std::string parm_nm, const char *parm_chr);
    void conf_edit_set(struct ctx_motapp *motapp, int threadnbr,const char *parm_nm_chr, const char *parm_val_chr);

    void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, std::string &parm_val, enum PARM_CAT parm_cat);
    void conf_edit_get(struct ctx_cam *cam, std::string parm_nm, char *parm_chr, enum PARM_CAT parm_cat);

#endif /* _INCLUDE_CONF_H */
