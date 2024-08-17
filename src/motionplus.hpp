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

class cls_camera;
class cls_sound;
class cls_algsec;
class cls_alg;
class cls_config;
class cls_dbse;
class cls_draw;
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

#define AVGCNT            30

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
    STATUS_CLOSED,   /* Device is closed */
    STATUS_INIT,     /* First time initialize */
    STATUS_OPENED    /* Successfully started the device */
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
/* TODO  Change this to lst_p and it_p so we have a type then descr.  Avoids
  conflicts with var names which I'll try not to have start with a abbrev
  for their type.  Types with start with something indicating their type.
  cls, ctx, it, lst, vec, etc)
*/
typedef std::list<ctx_params_item> p_lst;
typedef p_lst::iterator p_it;

struct ctx_params {
    p_lst   params_array;       /*List of the controls the user specified*/
    int     params_count;       /*Count of the controls the user specified*/
    bool    update_params;      /*Bool for whether to update the parameters on the device*/
    std::string params_desc;    /* Description of params*/
};

/* Record structure of motionplus table */
struct ctx_movie_item {
    bool        found;      /*Bool for whether the file exists*/
    int64_t     record_id;  /*record_id*/
    int         device_id;  /*camera id */
    std::string movie_nm;  /*Name of the movie file*/
    std::string movie_dir; /*Directory of the movie file */
    std::string full_nm;   /*Full name of the movie file with dir*/
    int64_t     movie_sz;   /*Size of the movie file in bytes*/
    int         movie_dtl;  /*Date in yyyymmdd format for the movie file*/
    std::string movie_tmc; /*Movie time 12h format*/
    std::string movie_tml; /*Movie time 24h format*/
    int         diff_avg;   /*Average diffs for motion frames */
    int         sdev_min;   /*std dev min */
    int         sdev_max;   /*std dev max */
    int         sdev_avg;   /*std dev average */
};
typedef std::list<ctx_movie_item> lst_movies;
typedef lst_movies::iterator it_movies;

/* Column item attributes in the motionplus table */
struct ctx_col_item {
    bool        found;      /*Bool for whether the col in existing db*/
    std::string col_nm;     /*Name of the column*/
    std::string col_typ;    /*Data type of the column*/
    int         col_idx;    /*Sequence index*/
};
typedef std::list<ctx_col_item> lst_cols;
typedef lst_cols::iterator it_cols;

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

typedef std::vector<cls_camera*> vec_cam;
typedef std::vector<cls_sound*> vec_snd;

struct ctx_motapp {

    vec_cam     cam_list;
    vec_snd     snd_list;

    bool    reload_all;
    bool    cam_add;
    int     cam_delete;     /* 0 for no action, other numbers specify camera to remove */
    int     cam_cnt;
    int     snd_cnt;
    int     argc;
    char    **argv;
    bool    pause;

    cls_config          *conf_src;
    cls_config          *cfg;
    ctx_all_sizes       *all_sizes;
    cls_webu            *webu;
    cls_dbse            *dbse;

    bool                parms_changed;      /*bool indicating if the parms have changed */
    pthread_mutex_t     global_lock;
    pthread_mutex_t     mutex_parms;        /* mutex used to lock when changing parms */
    pthread_mutex_t     mutex_camlst;       /* Lock the list of cams while adding/removing */
    pthread_mutex_t     mutex_post;         /* mutex to allow for processing of post actions*/

};

#endif /* _INCLUDE_MOTIONPLUS_HPP_ */
