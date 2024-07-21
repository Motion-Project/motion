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
 *
*/

#ifndef _INCLUDE_MOTIONPLUS_HPP_
#define _INCLUDE_MOTIONPLUS_HPP_

#include "config.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#ifndef __USE_GNU
    #define __USE_GNU
#endif
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdint.h>
#include <pthread.h>
#include <microhttpd.h>
#include <string>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <regex.h>
#include <dirent.h>
#include <algorithm>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>

#if defined(HAVE_PTHREAD_NP_H)
    #include <pthread_np.h>
#endif

#include <errno.h>

#pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wconversion"
    extern "C" {
        #include <libavformat/avformat.h>
        #include <libavutil/imgutils.h>
        #include <libavutil/mathematics.h>
        #include <libavdevice/avdevice.h>
        #include <libavcodec/avcodec.h>
        #include <libavformat/avio.h>
        #include <libswscale/swscale.h>
        #include <libavutil/avutil.h>
        #include "libavutil/buffer.h"
        #include "libavutil/error.h"
        #include "libavutil/hwcontext.h"
        #include "libavutil/mem.h"
    }
#pragma GCC diagnostic pop

#ifdef HAVE_V4L2
    #if defined(HAVE_LINUX_VIDEODEV2_H)
        #include <linux/videodev2.h>
    #else
        #include <sys/videoio.h>
    #endif
#endif

#ifdef HAVE_ALSA
    extern "C" {
        #include <alsa/asoundlib.h>
    }
#endif

#ifdef HAVE_PULSE
    extern "C" {
        #include <pulse/simple.h>
        #include <pulse/error.h>
    }
#endif


#ifdef HAVE_FFTW3
    extern "C" {
        #include <fftw3.h>
    }
#endif

struct ctx_motapp;

struct ctx_images;
struct ctx_image_data;
struct ctx_dbse;
struct ctx_algsec;
struct ctx_config;

class cls_log;
class cls_movie;
class cls_netcam;
class cls_picture;
class cls_rotate;
class cls_v4l2cam;
class cls_convert;
class cls_libcam;
class cls_webu;
class cls_webu_ans;
class cls_webu_file;
class cls_webu_html;
class cls_webu_json;
class cls_webu_mpegts;
class cls_webu_post;
class cls_webu_common;
class cls_webu_stream;

#define MYFFVER (LIBAVFORMAT_VERSION_MAJOR * 1000)+LIBAVFORMAT_VERSION_MINOR

#define THRESHOLD_TUNE_LENGTH  256

/* Filetype defines */
#define FTYPE_IMAGE             1
#define FTYPE_IMAGE_SNAPSHOT    2
#define FTYPE_IMAGE_MOTION      4
#define FTYPE_MOVIE             8
#define FTYPE_MOVIE_MOTION     16
#define FTYPE_MOVIE_TIMELAPSE  32
#define FTYPE_IMAGE_ROI        64

#define FTYPE_MOVIE_ANY   (FTYPE_MOVIE | FTYPE_MOVIE_MOTION | FTYPE_MOVIE_TIMELAPSE)
#define FTYPE_IMAGE_ANY   (FTYPE_IMAGE | FTYPE_IMAGE_SNAPSHOT | FTYPE_IMAGE_MOTION | FTYPE_IMAGE_ROI)

/* What types of images files do we want to have */
#define NEWIMG_OFF        0
#define NEWIMG_ON         1
#define NEWIMG_FIRST      2
#define NEWIMG_BEST       4
#define NEWIMG_CENTER     8

#define LOCATE_OFF        0
#define LOCATE_ON         1
#define LOCATE_PREVIEW    2
#define LOCATE_BOX        1
#define LOCATE_REDBOX     2
#define LOCATE_CROSS      4
#define LOCATE_REDCROSS   8

#define LOCATE_NORMAL     1
#define LOCATE_BOTH       2

#define UPDATE_REF_FRAME  1
#define RESET_REF_FRAME   2

#define AVGCNT            30

/*
 * Structure to hold images information
 * The idea is that this should have all information about a picture e.g. diffs, timestamp etc.
 * The exception is the label information, it uses a lot of memory
 * When the image is stored all texts motion marks etc. is written to the image
 * so we only have to send it out when/if we want.
 */

/* A image can have detected motion in it, but dosn't trigger an event, if we use minimum_motion_frames */
#define IMAGE_MOTION     1
#define IMAGE_TRIGGER    2
#define IMAGE_SAVE       4
#define IMAGE_SAVED      8
#define IMAGE_PRECAP    16
#define IMAGE_POSTCAP   32

enum CAMERA_TYPE {
    CAMERA_TYPE_UNKNOWN,
    CAMERA_TYPE_V4L2,
    CAMERA_TYPE_LIBCAM,
    CAMERA_TYPE_NETCAM
};

enum WEBUI_LEVEL{
    WEBUI_LEVEL_ALWAYS     = 0,
    WEBUI_LEVEL_LIMITED    = 1,
    WEBUI_LEVEL_ADVANCED   = 2,
    WEBUI_LEVEL_RESTRICTED = 3,
    WEBUI_LEVEL_NEVER      = 99
};

enum FLIP_TYPE {
    FLIP_TYPE_NONE,
    FLIP_TYPE_HORIZONTAL,
    FLIP_TYPE_VERTICAL
};

enum MOTPLS_SIGNAL {
    MOTPLS_SIGNAL_NONE,
    MOTPLS_SIGNAL_ALARM,
    MOTPLS_SIGNAL_USR1,
    MOTPLS_SIGNAL_SIGHUP,
    MOTPLS_SIGNAL_SIGTERM
};

enum CAPTURE_RESULT {
    CAPTURE_SUCCESS,
    CAPTURE_FAILURE,
    CAPTURE_ATTEMPTED
};

enum DEVICE_STATUS {
    STATUS_CLOSED,   /* Camera is closed */
    STATUS_INIT,     /* First time initialize */
    STATUS_RESET,    /* Clean up and re-initialize */
    STATUS_OPENED    /* Successfully started the camera */
};

enum TIMELAPSE_TYPE {
    TIMELAPSE_NONE,         /* No timelapse, regular processing */
    TIMELAPSE_APPEND,       /* Use append version of timelapse */
    TIMELAPSE_NEW           /* Use create new file version of timelapse */
};

struct ctx_webu_clients {
    std::string                 clientip;
    bool                        authenticated;
    int                         conn_nbr;
    struct timespec             conn_time;
    int                         userid_fail_nbr;
};

struct ctx_params_item {
    std::string     param_name;       /* The name or description of the ID as requested by user*/
    std::string     param_value;      /* The value that the user wants the control set to*/
};

typedef std::list<ctx_params_item> p_lst;
typedef p_lst::iterator p_it;

struct ctx_params {
    p_lst   params_array;       /*List of the controls the user specified*/
    int     params_count;       /*Count of the controls the user specified*/
    bool    update_params;      /*Bool for whether to update the parameters on the device*/
    std::string params_desc;    /* Description of params*/
};

struct ctx_coord {
    int x;
    int y;
    int width;
    int height;
    int minx;
    int maxx;
    int miny;
    int maxy;
    int stddev_x;
    int stddev_y;
    int stddev_xy;
};

struct ctx_image_data {
    unsigned char       *image_norm;
    unsigned char       *image_high;
    int                 diffs;
    int                 diffs_raw;
    int                 diffs_ratio;
    int64_t             idnbr_norm;
    int64_t             idnbr_high;
    struct timespec     imgts;          /* Realtime for display */
    struct timespec     monots;         /* Montonic clock for timing */
    int                 shot;           /* Sub second timestamp count */
    unsigned long       cent_dist;      /* Movement center to img center distance * Note: Dist is calculated distX*distX + distY*distY */
    unsigned int        flags;          /* See IMAGE_* defines */
    ctx_coord           location;       /* coordinates for center and size of last motion detection*/
    int                 total_labels;
};

struct ctx_images {
    ctx_image_data *image_ring;    /* The base address of the image ring buffer */
    ctx_image_data image_motion;   /* Picture buffer for motion images */
    ctx_image_data image_preview;  /* Picture buffer for best image when enables */

    unsigned char *ref;               /* The reference frame */
    unsigned char *ref_next;          /* The reference frame */
    unsigned char *mask;              /* Buffer for the mask file */
    unsigned char *smartmask;
    unsigned char *smartmask_final;
    unsigned char *common_buffer;
    unsigned char *image_substream;
    unsigned char *image_virgin;            /* Last picture frame with no text or locate overlay */
    unsigned char *image_vprvcy;            /* Virgin image with the privacy mask applied */
    unsigned char *mask_privacy;            /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_uv;         /* Buffer for the privacy U&V values */
    unsigned char *mask_privacy_high;       /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_high_uv;    /* Buffer for the privacy U&V values */
    unsigned char *image_secondary;         /* Buffer for JPG from alg_sec methods */

    int ring_size;
    int ring_in;                /* Index in image ring buffer we last added a image into */
    int ring_out;               /* Index in image ring buffer we want to process next time */

    int *ref_dyn;               /* Dynamic objects to be excluded from reference frame */
    int *smartmask_buffer;
    int *labels;
    int *labelsize;

    int width;
    int height;
    int size_norm;                    /* Number of bytes for normal size image */

    int width_high;
    int height_high;
    int size_high;                 /* Number of bytes for high resolution image */

    int motionsize;
    int labelgroup_max;
    int labels_above;
    int labelsize_max;
    int largest_label;
    int size_secondary;             /* Size of the jpg put into image_secondary*/

};

struct ctx_all_loc {
    int     row;
    int     col;
    int     offset_row;
    int     offset_col;
    int     offset_user_row;
    int     offset_user_col;
    int     scale;
};

struct ctx_all_sizes {
    int     width;
    int     height;
    int     img_sz;     /* Image size*/
    bool    reset;
};


struct ctx_stream_data {
    unsigned char   *jpg_data;  /* Image compressed as JPG */
    int             jpg_sz;     /* The number of bytes for jpg */
    int             consumed;   /* Bool for whether the jpeg data was consumed*/
    unsigned char   *img_data;  /* The base data used for image */
    int             jpg_cnct;   /* Counter of the number of jpg connections*/
    int             ts_cnct;    /* Counter of the number of mpegts connections */
    int             all_cnct;   /* Counter of the number of all camera connections */
};

struct ctx_stream {
    pthread_mutex_t  mutex;
    ctx_stream_data  norm;       /* Copy of the image to use for web stream*/
    ctx_stream_data  sub;        /* Copy of the image to use for web stream*/
    ctx_stream_data  motion;     /* Copy of the image to use for web stream*/
    ctx_stream_data  source;     /* Copy of the image to use for web stream*/
    ctx_stream_data  secondary;  /* Copy of the image to use for web stream*/
};

struct ctx_snd_alert {
    int             alert_id;           /* Id number for the alert*/
    std::string     alert_nm;           /* Name of the alert*/
    int             volume_level;       /* Volume level required to consider the sample*/
    int             volume_count;       /* For each sample, number of times required to exceed volumne level*/
    double          freq_low;           /* Lowest frequency for detecting this alert*/
    double          freq_high;          /* Highest frequency for detecting this alert*/
    int             trigger_count;      /* Count of how many times it has been triggered so far*/
    int             trigger_threshold;  /* How many times does it need to be triggered before an event*/
    timespec        trigger_time;       /* The last time the trigger was invoked */
    int             trigger_duration;   /* Min. duration to trigger a new /event */
};

struct ctx_snd_alsa {
   #ifdef HAVE_ALSA
        int                     device_id;
        std::string             device_nm;
        snd_pcm_t               *pcm_dev;
        snd_pcm_info_t          *pcm_info;
        int                     card_id;
        snd_ctl_card_info_t     *card_info;
        snd_ctl_t               *ctl_hdl;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_pulse {
   #ifdef HAVE_PULSE
        pa_simple       *dev;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_fftw {
    #ifdef HAVE_FFTW3
        fftw_plan       ff_plan;
        double          *ff_in;
        fftw_complex    *ff_out;
        int             bin_max;
        int             bin_min;
        double          bin_size;
    #else
        int             dummy;
    #endif
};

struct ctx_snd_info {
    std::string                 source;         /* Source string in ALSA format e.g. hw:1,0*/
    int                         sample_rate;    /* Sample rate of sound source*/
    int                         channels;       /* Number of audio channels */
    std::list<ctx_snd_alert>    alerts;         /* list of sound alert criteria */
    int                         vol_min;        /* The minimum volume from alerts*/
    int                         vol_max;        /* Maximum volume of sample*/
    int                         vol_count;      /* Number of times volumne exceeded user specified volume level */
    int16_t                     *buffer;
    int                         buffer_size;
    int                         frames;
    std::string                 pulse_server;
    std::string                 trig_freq;
    std::string                 trig_nbr;
    std::string                 trig_nm;
    ctx_params                  *params;        /* Device parameters*/
    ctx_snd_fftw                *snd_fftw;      /* fftw for sound*/
    ctx_snd_alsa                *snd_alsa;      /* Alsa device for sound*/
    ctx_snd_pulse               *snd_pulse;     /* PulseAudio for sound*/
};

struct ctx_dev {

    ctx_motapp      *motapp;
    int             threadnr;
    pthread_t       thread_id;

    ctx_config      *conf;
    ctx_images      imgs;

    ctx_image_data  *current_image;     /* Pointer to a structure where the image, diffs etc is stored */
    ctx_algsec      *algsec;

    cls_movie       *movie_norm;
    cls_movie       *movie_motion;
    cls_movie       *movie_timelapse;
    cls_movie       *movie_extpipe;

    ctx_stream      stream;
    ctx_snd_info    *snd_info;      /* Values for sound processing*/

    cls_netcam      *netcam;            /* normal RTSP connection */
    cls_netcam      *netcam_high;       /* high resolution RTSP connection */
    cls_picture     *picture;
    cls_rotate      *rotate;
    cls_v4l2cam     *v4l2cam;
    cls_libcam      *libcam;

    bool                    algsec_inuse;        /*Bool for whether we have secondary detection*/
    int                     track_posx;
    int                     track_posy;
    int                     device_id;
    enum CAMERA_TYPE        camera_type;
    enum DEVICE_STATUS      device_status;
    unsigned int            new_img;
    int                     locate_motion_mode;
    int                     locate_motion_style;
    int                     noise;
    int                     threshold;
    int                     threshold_maximum;
    int                     diffs_last[THRESHOLD_TUNE_LENGTH];
    int                     smartmask_speed;

    volatile bool           snapshot;    /* Make a snapshot */
    volatile bool           event_stop;  /* Boolean for whether to stop a event */
    volatile bool           event_user;  /* Boolean for whether to user triggered an event */
    volatile bool           finish_dev;      /* End the device thread */
    volatile bool           restart_dev;     /* Restart the device thread when it ends */
    bool                    running_dev;     /* Device thread is running*/
    volatile int            watchdog;

    int                     event_curr_nbr;
    int                     event_prev_nbr;
    char                    eventid[20];        /* Cam ID + Date/Time 99999yyyymmddhhmmss */
    char                    text_event_string[PATH_MAX];        /* The text for conv. spec. %C - */
    int                     text_scale;

    int                     postcap;                             /* downcounter, frames left to to send post event */
    int                     shots_mt;   /* Monotonic clock shots count*/
    int                     shots_rt;   /* Realtime  clock shots count*/
    int                     ref_lag;
    bool                    detecting_motion;
    long                    frame_wait[AVGCNT];   /* Last wait times through motion loop*/

    struct timespec         frame_curr_ts;
    struct timespec         frame_last_ts;

    time_t                  lasttime;
    time_t                  movie_start_time;
    struct timespec         connectionlosttime;               /* timestamp from connection lost */
    unsigned int            lastrate;
    unsigned int            startup_frames;
    unsigned int            frame_skip;
    volatile bool           pause;
    int                     missing_frame_counter;               /* counts failed attempts to fetch picture frame from camera */
    unsigned int            lost_connection;

    int                     pipe;
    int                     mpipe;

    char                    hostname[PATH_MAX];
    char                    action_user[40];

    int                     movie_fps;
    bool                    movie_passthrough;

    int                     filetype;

    int area_minx[9], area_miny[9], area_maxx[9], area_maxy[9];
    int                     areadetect_eventnbr;

    int                     smartmask_ratio;
    int                     smartmask_count;
    unsigned int            smartmask_lastrate;
    int previous_diffs, previous_location_x, previous_location_y;
    bool                    passflag;  //flag first frame vs all others.

    ctx_all_loc             all_loc;    /* position on all camera image */

    pthread_mutex_t         parms_lock;
    bool                    parms_changed;  /*bool indicating if the parms have changed */

    uint64_t                info_diff_tot;
    uint64_t                info_diff_cnt;
    int                     info_sdev_min;
    int                     info_sdev_max;
    uint64_t                info_sdev_tot;

};

/*  ctx_motapp for whole motion application including all the cameras */
struct ctx_motapp {

    ctx_dev             **cam_list;
    ctx_dev             **snd_list;
    pthread_mutex_t     global_lock;

    volatile int        threads_running;
    volatile bool       finish_all;
    volatile bool       restart_all;
    volatile bool       reload_all;
    volatile bool       cam_add;        /* Bool for whether to add a camera to the list */
    volatile int        cam_delete;     /* 0 for no action, other numbers specify camera to remove */

    int                 argc;
    char                **argv;
    bool                pause;
    ctx_config          *conf;
    int                 cam_cnt;
    int                 snd_cnt;
    ctx_all_sizes       *all_sizes;
    cls_webu            *webu;
    ctx_dbse            *dbse;                      /* user specified database */

    bool                parms_changed;      /*bool indicating if the parms have changed */
    pthread_mutex_t     mutex_parms;        /* mutex used to lock when changing parms */
    pthread_mutex_t     mutex_camlst;       /* Lock the list of cams while adding/removing */
    pthread_mutex_t     mutex_post;         /* mutex to allow for processing of post actions*/


};

extern pthread_key_t tls_key_threadnr; /* key for thread number */

#endif /* _INCLUDE_MOTIONPLUS_HPP_ */
