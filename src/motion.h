/*    motion.h
 *
 *    Include file for motion.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_MOTION_H
#define _INCLUDE_MOTION_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
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

#if defined(HAVE_PTHREAD_NP_H)
    #include <pthread_np.h>
#endif

/* Forward declarations, used in functional definitions of headers */
struct ctx_rotate;
struct ctx_images;
struct ctx_image_data;
struct ctx_dbse;
struct ctx_mmalcam;

#include "logger.h"
#include "conf.h"
#include "track.h"
#include "netcam.h"
#include "movie.h"

int nls_enabled;

#define DEF_PALETTE             17

/* Default picture settings */
#define DEF_WIDTH              640
#define DEF_HEIGHT             480
#define DEF_QUALITY             75
#define DEF_CHANGES           1500

#define DEF_MAXFRAMERATE        15
#define DEF_NOISELEVEL          32

/* Minimum time between two 'actions' (email, sms, external) */
#define DEF_EVENT_GAP            60  /* 1 minutes */

#define DEF_INPUT               -1
#define DEF_VIDEO_DEVICE         "/dev/video0"

#define THRESHOLD_TUNE_LENGTH  256

#define MISSING_FRAMES_TIMEOUT  30  /* When failing to get picture frame from camera
                                       we reuse the previous frame until
                                       MISSING_FRAMES_TIMEOUT seconds has passed
                                       and then we show a grey image instead
                                     */

#define WATCHDOG_TMO            30   /* 30 sec max motion_loop interval */
#define WATCHDOG_KILL          -10   /* 10 sec grace period before calling thread cancel */

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

#define TRUE 1
#define FALSE 0
#define AVGCNT          30

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
    CAMERA_TYPE_MMAL,
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

struct vdev_usrctrl_ctx {
    char          *ctrl_name;       /* The name or description of the ID as requested by user*/
    int            ctrl_value;      /* The value that the user wants the control set to*/
};

struct vdev_context {
    /* As v4l2 gets rewritten, put thread specific items here
     * Rather than use conf options directly, copy from conf to here
     * to handle cross thread webui changes which could cause problems
     */
    struct vdev_usrctrl_ctx *usrctrl_array;     /*Array of the controls the user specified*/
    int usrctrl_count;                          /*Count of the controls the user specified*/
    int update_parms;                           /*Bool for whether to update the parameters on the device*/
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
    int64_t             idnbr_norm;
    int64_t             idnbr_high;
    struct timespec     imgts;
    int                 shot;           /* Sub second timestamp count */
    unsigned long       cent_dist;      /* Movement center to img center distance * Note: Dist is calculated distX*distX + distY*distY */
    unsigned int        flags;          /* See IMAGE_* defines */
    struct ctx_coord    location;       /* coordinates for center and size of last motion detection*/
    int                 total_labels;
};

struct ctx_images {
    struct ctx_image_data *image_ring;    /* The base address of the image ring buffer */
    struct ctx_image_data image_motion;   /* Picture buffer for motion images */
    struct ctx_image_data image_preview;  /* Picture buffer for best image when enables */

    unsigned char *ref;               /* The reference frame */
    unsigned char *mask;              /* Buffer for the mask file */
    unsigned char *smartmask;
    unsigned char *smartmask_final;
    unsigned char *common_buffer;
    unsigned char *image_substream;
    unsigned char *image_virgin;        /* Last picture frame with no text or locate overlay */
    unsigned char *image_vprvcy;        /* Virgin image with the privacy mask applied */
    unsigned char *mask_privacy;        /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_uv;     /* Buffer for the privacy U&V values */
    unsigned char *mask_privacy_high;      /* Buffer for the privacy mask values */
    unsigned char *mask_privacy_high_uv;   /* Buffer for the privacy U&V values */

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
    int picture_type;
};

struct ctx_stream_data {
    unsigned char   *jpeg_data; /* Image compressed as JPG */
    long            jpeg_size;  /* The number of bytes for jpg */
    int             cnct_count; /* Counter of the number of connections */
};

struct ctx_stream {
    pthread_mutex_t         mutex;
    struct MHD_Daemon       *daemon;
    char                    digest_rand[8];
    int                     finish;

    struct ctx_stream_data  norm;    /* Copy of the image to use for web stream*/
    struct ctx_stream_data  sub;     /* Copy of the image to use for web stream*/
    struct ctx_stream_data  motion;  /* Copy of the image to use for web stream*/
    struct ctx_stream_data  source;  /* Copy of the image to use for web stream*/
};

/*
 *  These used to be global variables but now each thread will have its
 *  own context
 */
struct ctx_cam {
    struct ctx_cam **cam_list;

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
    struct ctx_images imgs;
    struct trackoptions track;
    int                 track_posx;
    int                 track_posy;

    enum CAMERA_TYPE      camera_type;

    struct ctx_mmalcam      *mmalcam;
    struct ctx_netcam       *netcam;            /* this structure contains the context for normal RTSP connection */
    struct ctx_netcam       *netcam_high;       /* this structure contains the context for high resolution RTSP connection */
    struct vdev_context     *vdev;              /* Structure for v4l2 device information */
    struct ctx_image_data   *current_image;     /* Pointer to a structure where the image, diffs etc is stored */
    struct ctx_rotate       *rotate_data;       /* rotation data is thread-specific */
    unsigned int new_img;

    int locate_motion_mode;
    int locate_motion_style;
    int process_thisframe;

    int noise;
    int threshold;
    int threshold_maximum;
    int diffs_last[THRESHOLD_TUNE_LENGTH];
    int smartmask_speed;


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
    long    frame_wait[AVGCNT];   /* Last wait times through motion loop*/
    struct timespec frame_curr_ts;
    struct timespec frame_last_ts;

    time_t currenttime;
    time_t lasttime;
    time_t eventtime;
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

    struct ctx_dbse        *dbse;

    int movie_fps;
    char newfilename[PATH_MAX];
    char extpipefilename[PATH_MAX];
    char extpipecmdline[PATH_MAX];
    int movie_last_shot;

    struct ctx_movie   *movie_norm;
    struct ctx_movie   *movie_motion;
    struct ctx_movie   *movie_timelapse;
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

    int olddiffs;   //only need this in here for a printf later...do we need that printf?
    int smartmask_ratio;
    int smartmask_count;

    int previous_diffs, previous_location_x, previous_location_y;
    unsigned long int time_last_frame, time_current_frame;

    unsigned int smartmask_lastrate;

    unsigned int passflag;  //only purpose is to flag first frame vs all others.....
    int rolling_frame;

    struct MHD_Daemon   *webcontrol_daemon;
    char                webcontrol_digest_rand[8];
    int                 camera_id;

    struct ctx_stream      stream;

};

extern pthread_mutex_t global_lock;
extern volatile int threads_running;
extern FILE *ptr_logfile;
extern pthread_key_t tls_key_threadnr; /* key for thread number */


#endif /* _INCLUDE_MOTION_H */
