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

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define BSD
#endif

/* Includes */
#ifdef HAVE_MYSQL
#include <mysql.h>
#endif

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

#ifdef HAVE_PGSQL
#include <libpq-fe.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdint.h>

#define _LINUX_TIME_H 1
#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L)) && (!defined(BSD))
#include <linux/videodev.h>
#endif

#include <pthread.h>

#include "logger.h"
#include "conf.h"
#include "stream.h"
#include "webhttpd.h"

#ifdef HAVE_SDL
#include "sdl.h"
#endif


/**
 * ATTRIBUTE_UNUSED:
 *
 * Macro used to signal to GCC unused function parameters
 */
#ifdef __GNUC__
#ifdef HAVE_ANSIDECL_H
#include <ansidecl.h>
#endif
#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((unused))
#endif
#else
#define ATTRIBUTE_UNUSED
#endif

/* strerror_r() XSI vs GNU */
#if (defined(BSD)) || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
#define XSI_STRERROR_R
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

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define VIDEO_PALETTE_GREY      1       /* Linear greyscale */
#define VIDEO_PALETTE_HI240     2       /* High 240 cube (BT848) */
#define VIDEO_PALETTE_RGB565    3       /* 565 16 bit RGB */
#define VIDEO_PALETTE_RGB24     4       /* 24bit RGB */
#define VIDEO_PALETTE_RGB32     5       /* 32bit RGB */
#define VIDEO_PALETTE_RGB555    6       /* 555 15bit RGB */
#define VIDEO_PALETTE_YUV422    7       /* YUV422 capture */
#define VIDEO_PALETTE_YUYV      8
#define VIDEO_PALETTE_UYVY      9       /* The great thing about standards is ... */
#define VIDEO_PALETTE_YUV420    10
#define VIDEO_PALETTE_YUV411    11      /* YUV411 capture */
#define VIDEO_PALETTE_RAW       12      /* RAW capture (BT848) */
#define VIDEO_PALETTE_YUV422P   13      /* YUV 4:2:2 Planar */
#define VIDEO_PALETTE_YUV411P   14      /* YUV 4:1:1 Planar */
#define VIDEO_PALETTE_YUV420P   15      /* YUV 4:2:0 Planar */
#define VIDEO_PALETTE_YUV410P   16      /* YUV 4:1:0 Planar */
#define VIDEO_PALETTE_PLANAR    13      /* start of planar entries */
#define VIDEO_PALETTE_COMPONENT 7       /* start of component entries */

#define DEF_PALETTE             17

/* Default picture settings */
#define DEF_WIDTH              352
#define DEF_HEIGHT             288
#define DEF_QUALITY             75
#define DEF_CHANGES           1500

#define DEF_MAXFRAMERATE       100
#define DEF_NOISELEVEL          32

/* Minimum time between two 'actions' (email, sms, external) */
#define DEF_EVENT_GAP            60  /* 1 minutes */
#define DEF_MAXMOVIETIME       3600  /* 60 minutes */

#define DEF_FFMPEG_BPS      400000
#define DEF_FFMPEG_VBR           0
#define DEF_FFMPEG_CODEC   "mpeg4"

#define THRESHOLD_TUNE_LENGTH  256

#define MISSING_FRAMES_TIMEOUT  30  /* When failing to get picture frame from camera
                                       we reuse the previous frame until
                                       MISSING_FRAMES_TIMEOUT seconds has passed
                                       and then we show a grey image instead
                                     */

#define WATCHDOG_TMO            30   /* 30 sec max motion_loop interval */
#define WATCHDOG_OFF          -127   /* Turn off watchdog, used when we wants to quit a thread */

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

/* Do not break this line into two or more. Must be ONE line */
#define DEF_SQL_QUERY "sql_query insert into security(camera, filename, frame, file_type, time_stamp, event_time_stamp) values('%t', '%f', '%q', '%n', '%Y-%m-%d %T', '%C')"


/* OUTPUT Image types */
#define IMAGE_TYPE_JPEG        0
#define IMAGE_TYPE_PPM         1

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

#define BUFSIZE_1MEG      (1024 * 1024)

/* Forward declaration, used in track.h */
struct images;

#include "track.h"
#include "netcam.h"

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

struct image_data {
    unsigned char *image;
    int diffs;
    time_t timestamp;           /* Timestamp when image was captured */
    struct tm timestamp_tm;
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

/* date/time drawing, draw.c */
int draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text, unsigned int factor);
int initialize_chars(void);

struct images {
    struct image_data *image_ring;    /* The base address of the image ring buffer */
    int image_ring_size;
    int image_ring_in;                /* Index in image ring buffer we last added a image into */
    int image_ring_out;               /* Index in image ring buffer we want to process next time */

    unsigned char *ref;               /* The reference frame */
    unsigned char *out;               /* Picture buffer for motion images */
    uint16_t *ref_dyn;                /* Dynamic objects to be excluded from reference frame */
    unsigned char *image_virgin;      /* Last picture frame with no text or locate overlay */
    struct image_data preview_image;  /* Picture buffer for best image when enables */
    unsigned char *mask;              /* Buffer for the mask file */
    unsigned char *smartmask;
    unsigned char *smartmask_final;
    unsigned char *common_buffer;
    int *smartmask_buffer;
    int *labels;
    int *labelsize;
    int width;
    int height;
    int type;
    int picture_type;                 /* Output picture type IMAGE_JPEG, IMAGE_PPM */        
    int size;
    int motionsize;
    int labelgroup_max;
    int labels_above;
    int labelsize_max;
    int largest_label;
};

/* Contains data for image rotation, see rotate.c. */
struct rotdata {
    /* Temporary buffer for 90 and 270 degrees rotation. */
    unsigned char *temp_buf;
    /* 
     * Degrees to rotate; copied from conf.rotate_deg. This is the value
     * that is actually used. The value of conf.rotate_deg cannot be used
     * because it can be changed by motion-control, and changing rotation
     * while Motion is running just causes problems.
     */
    int degrees;
    /*
     * Capture width and height - different from output width and height if 
     * rotating 90 or 270 degrees. 
     */
    int cap_width;
    int cap_height;
};

/*
 *  These used to be global variables but now each thread will have its
 *  own context
 */
struct context {
    FILE *extpipe;
    int extpipe_open;
    char conf_filename[PATH_MAX];
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
    struct netcam_context *netcam;
    struct image_data *current_image;        /* Pointer to a structure where the image, diffs etc is stored */
    unsigned int new_img;

    int locate_motion_mode;
    int locate_motion_style;
    int process_thisframe;
    struct rotdata rotate_data;              /* rotation data is thread-specific */

    int noise;
    int threshold;
    int diffs_last[THRESHOLD_TUNE_LENGTH];
    int smartmask_speed;

    /* Commands to the motion thread */
    volatile unsigned int snapshot;    /* Make a snapshot */
    volatile unsigned int makemovie;   /* End a movie */
    volatile unsigned int finish;      /* End the thread */
    volatile unsigned int restart;     /* Restart the thread when it ends */
    /* Is the motion thread running */
    volatile unsigned int running;
    volatile int watchdog;

    pthread_t thread_id;

    int event_nr;
    int prev_event;
    unsigned int lightswitch_framecounter;
    char text_event_string[PATH_MAX];        /* The text for conv. spec. %C -
                                                we assume PATH_MAX normally 4096 characters is fine */
    int postcap;                             /* downcounter, frames left to to send post event */

    int shots;
    unsigned int detecting_motion;
    struct tm *currenttime_tm;
    struct tm *eventtime_tm;

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

#if (defined(BSD))
    int tuner_dev;
#endif
    int video_dev;
    int pipe;
    int mpipe;

    struct stream stream;
    int stream_count;
    
#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL) || defined(HAVE_SQLITE3)
    int sql_mask;
#endif

#ifdef HAVE_SQLITE3
    sqlite3 *database_sqlite3;
#endif

#ifdef HAVE_MYSQL
    MYSQL *database;
#endif

#ifdef HAVE_PGSQL
    PGconn *database_pg;
#endif

    int movie_fps;
    char newfilename[PATH_MAX];
    char extpipefilename[PATH_MAX];
    int movie_last_shot;

#ifdef HAVE_FFMPEG
    struct ffmpeg *ffmpeg_output;
    struct ffmpeg *ffmpeg_output_debug;
    struct ffmpeg *ffmpeg_timelapse;
    struct ffmpeg *ffmpeg_smartmask;
    char timelapsefilename[PATH_MAX];
    char motionfilename[PATH_MAX];
#endif
};

extern pthread_mutex_t global_lock;
extern volatile int threads_running;
extern FILE *ptr_logfile;

/* TLS keys below */
extern pthread_key_t tls_key_threadnr; /* key for thread number */

int http_bindsock(int, int, int);
void * mymalloc(size_t);
void * myrealloc(void *, size_t, const char *);
FILE * myfopen(const char *, const char *, size_t);
int myfclose(FILE *);
size_t mystrftime(const struct context *, char *, size_t, const char *, const struct tm *, const char *, int);
int create_path(const char *);
#endif /* _INCLUDE_MOTION_H */
