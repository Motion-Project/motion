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

/*    motion.h
 *
 *    Include file for motion.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *
 */

#ifndef _INCLUDE_MOTION_H
#define _INCLUDE_MOTION_H

/* Forward declarations of structs */
struct images;
struct image_data;
struct rtsp_context;
struct ffmpeg;

#include "config.h"

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#ifndef __USE_GNU
    #define __USE_GNU
#endif
#include <ctype.h>
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
#include <regex.h>


#if defined(HAVE_PTHREAD_NP_H)
    #include <pthread_np.h>
#endif

#if defined(HAVE_MYSQL)
    #include <mysql.h>
#endif

#if defined(HAVE_MARIADB)
    #include <mysql.h>
#endif

#ifdef HAVE_SQLITE3
    #include <sqlite3.h>
#endif

#ifdef HAVE_PGSQL
    // avoid libpq-fe.h collision with motion.h #define CONNECTION_OK (below)
    #define CONNECTION_OK PGSQL_CONNECTION_OK
    #include <libpq-fe.h>
    // avoid redefinition warning for motion.h #define CONNECTION_OK below
    #undef CONNECTION_OK
#endif

#ifdef HAVE_FFMPEG

    #define MYFFVER (LIBAVFORMAT_VERSION_MAJOR * 1000)+LIBAVFORMAT_VERSION_MINOR

    #include <errno.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mathematics.h>
    #include <libavdevice/avdevice.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avio.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
    #if (MYFFVER >= 57083)
        #include "libavutil/hwcontext.h"
    #endif
#else
    #define MYFFVER 0
#endif

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

/* Due to the pointer magic method of conf, the conf.h and track.h must stay in here */
#include "conf.h"
#include "track.h"

#ifdef HAVE_MMAL
    #include "mmalcam.h"
#endif


/*
*  The macro below defines a version of sleep using nanosleep
* If a signal such as SIG_CHLD interrupts the sleep we just continue sleeping
*/
#define SLEEP(seconds, nanoseconds) {              \
                struct timespec tv;                \
                tv.tv_sec = (seconds);             \
                tv.tv_nsec = (nanoseconds);        \
                while (nanosleep(&tv, &tv) == -1); \
        }

/* Default picture settings */
#define DEF_WIDTH              640
#define DEF_HEIGHT             480
#define DEF_QUALITY             75
#define DEF_CHANGES           1500

#define DEF_MAXFRAMERATE        15
#define DEF_NOISELEVEL          32

/* Minimum time between two 'actions' (email, sms, external) */
#define DEF_EVENT_GAP            60  /* 1 minutes */

#define DEF_VIDEO_DEVICE         "/dev/video0"

#define THRESHOLD_TUNE_LENGTH  256

#define MISSING_FRAMES_TIMEOUT  30  /* When failing to get picture frame from camera
                                    we reuse the previous frame until
                                    MISSING_FRAMES_TIMEOUT seconds has passed
                                    and then we show a grey image instead
                                    */

#define CONNECTION_KO           "Lost connection"
#define CONNECTION_OK           "Connection OK"

#define DEF_MAXSTREAMS          10   /* Maximum number of stream clients per camera */
#define DEF_MAXWEBQUEUE         10   /* Maximum number of stream client in queue */

#define DEF_TIMESTAMP           "%Y-%m-%d\\n%T"
#define DEF_EVENTSTAMP          "%Y%m%d%H%M%S"

#define DEF_SNAPPATH            "%v-%Y%m%d%H%M%S-snapshot"
#define DEF_IMAGEPATH           "%v-%Y%m%d%H%M%S-%q"
#define DEF_MOVIEPATH           "%v-%Y%m%d%H%M%S"
#define DEF_TIMEPATH            "%Y%m%d-timelapse"

#define DEF_TIMELAPSE_MODE      "daily"

/* OUTPUT Image types */
#define IMAGE_TYPE_JPEG        0
#define IMAGE_TYPE_PPM         1
#define IMAGE_TYPE_WEBP        2
#define IMAGE_TYPE_GREY        3

/* Filetype defines */
#define FTYPE_IMAGE            1
#define FTYPE_IMAGE_SNAPSHOT   2
#define FTYPE_IMAGE_MOTION     4
#define FTYPE_MPEG             8
#define FTYPE_MPEG_MOTION     16
#define FTYPE_MPEG_TIMELAPSE  32

#define FTYPE_MPEG_ANY    (FTYPE_MPEG | FTYPE_MPEG_MOTION | FTYPE_MPEG_TIMELAPSE)
#define FTYPE_IMAGE_ANY   (FTYPE_IMAGE | FTYPE_IMAGE_SNAPSHOT | FTYPE_IMAGE_MOTION)

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
    CAMERA_TYPE_BKTR,
    CAMERA_TYPE_MMAL,
    CAMERA_TYPE_RTSP,
    CAMERA_TYPE_NETCAM
};

enum WEBUI_LEVEL{
WEBUI_LEVEL_ALWAYS     = 0,
WEBUI_LEVEL_LIMITED    = 1,
WEBUI_LEVEL_ADVANCED   = 2,
WEBUI_LEVEL_RESTRICTED = 3,
WEBUI_LEVEL_NEVER      = 99
};

struct params_item_ctx {
    char    *param_name;       /* The name or description of the ID as requested by user*/
    char    *param_value;      /* The value that the user wants the control set to*/
};

struct params_context {
    struct params_item_ctx *params_array;     /*Array of the controls the user specified*/
    int params_count;                         /*Count of the controls the user specified*/
    int update_params;                        /*Bool for whether to update the parameters on the device*/
};

struct image_data {
    unsigned char *image_norm;
    unsigned char *image_high;
    int diffs;
    int64_t        idnbr_norm;
    int64_t        idnbr_high;
    struct timeval timestamp_tv;
    int shot;                   /* Sub second timestamp count */

    /*
    * Movement center to img center distance
    * Note: Dist is calculated distX*distX + distY*distY
    */
    unsigned long cent_dist;

    unsigned int flags;         /* Se IMAGE_* defines */

    struct coord location;      /* coordinates for center and size of last motion detection*/

    int total_labels;

};

struct stream_data {
    unsigned char   *jpeg_data; /* Image compressed as JPG */
    long            jpeg_size;  /* The number of bytes for jpg */
    int             cnct_count; /* Counter of the number of connections */
};

/*
* DIFFERENCES BETWEEN imgs.width, conf.width AND rotate_data.cap_width
* (and the corresponding height values, of course)
* ===========================================================================
* Location      Purpose
*
* conf          The values in conf reflect width and height set in the
*               configuration file. These can be set via http remote control,
*               but they are not used internally by Motion, so it won't break
*               anything. These values are transferred to imgs in vid_start.
*
* imgs          The values in imgs are the actual output dimensions. Normally
*               the output dimensions are the same as the capture dimensions,
*               but for 90 or 270 degrees rotation, they are not. E.g., if
*               you capture at 320x240, and rotate 90 degrees, the output
*               dimensions are 240x320.
*               These values are set from the conf values in vid_start, or
*               from the first JPEG image in netcam_start. For 90 or 270
*               degrees rotation, they are swapped in rotate_init.
*
* rotate_data   The values in rotate_data are named cap_width and cap_height,
*               and contain the capture dimensions. The difference between
*               capture and output dimensions is explained above.
*               These values are set in rotate_init.
*/

struct images {
    struct image_data *image_ring;    /* The base address of the image ring buffer */
    int image_ring_size;
    int image_ring_in;                /* Index in image ring buffer we last added a image into */
    int image_ring_out;               /* Index in image ring buffer we want to process next time */

    unsigned char *ref;               /* The reference frame */
    struct image_data img_motion;     /* Picture buffer for motion images */
    int *ref_dyn;                     /* Dynamic objects to be excluded from reference frame */
    struct image_data image_virgin;   /* Last picture frame with no text or locate overlay */
    struct image_data image_vprvcy;   /* Virgin image with the privacy mask applied */
    struct image_data preview_image;  /* Picture buffer for best image when enables */
    unsigned char *mask;              /* Buffer for the mask file */
    unsigned char *smartmask;
    unsigned char *smartmask_final;
    unsigned char *common_buffer;
    unsigned char *substream_image;

    unsigned char *mask_privacy;      /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_uv;   /* Buffer for the privacy U&V values */

    unsigned char *mask_privacy_high;      /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_high_uv;   /* Buffer for the privacy U&V values */

    int *smartmask_buffer;
    int *labels;
    int *labelsize;
    int width;
    int height;
    int type;
    int picture_type;                 /* Output picture type IMAGE_JPEG, IMAGE_PPM */
    int size_norm;                    /* Number of bytes for normal size image */

    int width_high;
    int height_high;
    int size_high;                 /* Number of bytes for high resolution image */

    int motionsize;
    int labelgroup_max;
    int labels_above;
    int labelsize_max;
    int largest_label;
};

enum FLIP_TYPE {
    FLIP_TYPE_NONE,
    FLIP_TYPE_HORIZONTAL,
    FLIP_TYPE_VERTICAL
};

/* Contains data for image rotation, see rotate.c. */
struct rotdata {

    unsigned char *buffer_norm;  /* Temporary buffer for 90 and 270 degrees rotation of normal resolution image. */
    unsigned char *buffer_high;  /* Temporary buffer for 90 and 270 degrees rotation of high resolution image. */
    int degrees;              /* Degrees to rotate; copied from conf.rotate_deg. */
    enum FLIP_TYPE axis;      /* Rotate image over the Horizontal or Vertical axis. */

    int capture_width_norm;            /* Capture width of normal resolution image */
    int capture_height_norm;           /* Capture height of normal resolution image */

    int capture_width_high;            /* Capture width of high resolution image */
    int capture_height_high;           /* Capture height of high resolution image */

};

/*
*  These used to be global variables but now each thread will have its
*  own context
*/
struct context {
    FILE *extpipe;
    int extpipe_open;
    char conf_filename[PATH_MAX];
    int from_conf_dir;
    int threadnr;
    unsigned int daemon;
    char pid_file[PATH_MAX];
    char log_file[PATH_MAX];
    char log_type_str[6];
    int log_level;
    unsigned int log_type;

    struct config conf;
    struct images imgs;
    struct trackoptions track;
    int                 track_posx;
    int                 track_posy;

    enum CAMERA_TYPE      camera_type;
    struct netcam_context *netcam;
    #ifdef HAVE_MMAL
        struct mmalcam_context *mmalcam;
    #endif
    struct rtsp_context *rtsp;              /* this structure contains the context for normal RTSP connection */
    struct rtsp_context *rtsp_high;         /* this structure contains the context for high resolution RTSP connection */

    struct params_context *vdev;            /* Structure for v4l2 and bktr device information */

    struct image_data *current_image;       /* Pointer to a structure where the image, diffs etc is stored */
    unsigned int new_img;

    int locate_motion_mode;
    int locate_motion_style;
    int process_thisframe;
    struct rotdata rotate_data;              /* rotation data is thread-specific */

    int noise;
    int threshold;
    int threshold_maximum;
    int diffs_last[THRESHOLD_TUNE_LENGTH];
    int smartmask_speed;

    /* bktr and v4l2 params used for round robin */
    int param_input;
    long param_freq;
    int param_norm;

    /* Commands to the motion thread */
    volatile unsigned int snapshot;    /* Make a snapshot */
    volatile unsigned int event_stop;  /* Boolean for whether to stop a event */
    volatile unsigned int event_user;  /* Boolean for whether to user triggered an event */
    volatile unsigned int finish;      /* End the thread */
    volatile unsigned int restart;     /* Restart the thread when it ends */
    /* Is the motion thread running */
    volatile unsigned int running;
    /* Is the web control thread running */
    volatile unsigned int webcontrol_running;
    volatile unsigned int webcontrol_finish;      /* End the thread */
    volatile int watchdog;

    pthread_t thread_id;

    int event_nr;
    int prev_event;
    unsigned long long database_event_id;
    unsigned int lightswitch_framecounter;
    char text_event_string[PATH_MAX];        /* The text for conv. spec. %C - */
    int text_scale;

    int postcap;                             /* downcounter, frames left to to send post event */
    int shots;
    unsigned int detecting_motion;
    struct tm *currenttime_tm;
    struct tm *eventtime_tm;

    time_t currenttime;
    time_t lasttime;
    time_t eventtime;
    time_t movietime;
    time_t connectionlosttime;               /* timestamp from connection lost */

    unsigned int lastrate;
    unsigned int startup_frames;
    unsigned int moved;
    unsigned int pause;
    int missing_frame_counter;               /* counts failed attempts to fetch picture frame from camera */
    unsigned int lost_connection;

    int video_dev;
    int pipe;
    int mpipe;

    char hostname[PATH_MAX];

    int sql_mask;

    #ifdef HAVE_SQLITE3
        sqlite3 *database_sqlite3;
    #endif

    #if defined(HAVE_MYSQL)
        MYSQL *database_mysql;
    #endif

    #if defined(HAVE_MARIADB)
        MYSQL *database_mariadb;
    #endif

    #ifdef HAVE_PGSQL
        PGconn *database_pgsql;
        int    eid_db_format;          /* db event ID PQfformat() or PQgetlength() if binary */
        #define dbeid_undetermined -1  /* sql_query_start not present or not yet executed    */
        #define dbeid_no_return    -2  /* sql_query_start statement returned nothing         */
        #define dbeid_not_valid    -3  /* sql_query_start statement returned invalid value:  */
                                       /* multiple values or value not a positive integer    */
        #define dbeid_unk_format   -4  /* PGgetlength() returned unexpected size for bin fmt */
        #define dbeid_use_error    -5  /* %{dbeventid} used w/ no sql_query_start return val */
        #define dbeid_recovery     -6  /* PGSQL session in recovery after session failure    */
        #define dbeid_rec_fail     -7  /* PQresetStart failed; keep trying                   */
                                       /* NB: for values < -1, event ID recording is skipped */
                                       /* unless lost session recovery or SIGHUP occur       */
    #endif

    int movie_fps;
    char newfilename[PATH_MAX];
    char extpipefilename[PATH_MAX];
    char extpipecmdline[PATH_MAX];
    int movie_last_shot;

    struct ffmpeg   *ffmpeg_output;
    struct ffmpeg   *ffmpeg_output_motion;
    struct ffmpeg   *ffmpeg_timelapse;
    int             movie_passthrough;

    char timelapsefilename[PATH_MAX];
    char motionfilename[PATH_MAX];

    int area_minx[9], area_miny[9], area_maxx[9], area_maxy[9];
    int areadetect_eventnbr;
    /* ToDo Determine why we need these...just put it all into prepare? */
    unsigned long long int timenow, timebefore;

    unsigned int rate_limit;
    time_t lastframetime;
    int minimum_frame_time_downcounter;
    unsigned int get_image;    /* Flag used to signal that we capture new image when we run the loop */

    long int required_frame_time, frame_delay;

    long int rolling_average_limit;
    long int *rolling_average_data;
    unsigned long int rolling_average;

    int olddiffs;   //only need this in here for a printf later...do we need that printf?
    int smartmask_ratio;
    int smartmask_count;

    int previous_diffs, previous_location_x, previous_location_y;
    unsigned long int time_last_frame, time_current_frame;

    unsigned int smartmask_lastrate;

    unsigned int passflag;  //only purpose is to flag first frame vs all others.....
    int rolling_frame;

    struct MHD_Daemon   *webcontrol_daemon;
    struct MHD_Daemon   *webstream_daemon;
    char                webcontrol_digest_rand[8];
    char                webstream_digest_rand[8];
    int                 camera_id;

    pthread_mutex_t     mutex_stream;

    struct stream_data  stream_norm;    /* Copy of the image to use for web stream*/
    struct stream_data  stream_sub;     /* Copy of the image to use for web stream*/
    struct stream_data  stream_motion;  /* Copy of the image to use for web stream*/
    struct stream_data  stream_source;  /* Copy of the image to use for web stream*/

    struct params_context    *webcontrol_headers;  /* Headers for webcontrol */
    struct params_context    *stream_headers;  /* Headers for stream */


};

extern pthread_mutex_t global_lock;
extern volatile int threads_running;
extern FILE *ptr_logfile;

/* TLS keys below */
extern pthread_key_t tls_key_threadnr; /* key for thread number */
void motion_remove_pid(void);

#endif /* _INCLUDE_MOTION_H */
