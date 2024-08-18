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

class cls_motapp;
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

enum MOTPLS_SIGNAL {
    MOTPLS_SIGNAL_NONE,
    MOTPLS_SIGNAL_ALARM,
    MOTPLS_SIGNAL_USR1,
    MOTPLS_SIGNAL_SIGHUP,
    MOTPLS_SIGNAL_SIGTERM
};

enum DEVICE_STATUS {
    STATUS_CLOSED,   /* Device is closed */
    STATUS_INIT,     /* First time initialize */
    STATUS_OPENED    /* Successfully started the device */
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

class cls_motapp {
    public:
        cls_motapp();
        ~cls_motapp();

        std::vector<cls_camera*>    cam_list;
        std::vector<cls_sound*>     snd_list;

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

        pthread_mutex_t     mutex_camlst;       /* Lock the list of cams while adding/removing */
        pthread_mutex_t     mutex_post;         /* mutex to allow for processing of post actions*/

        void signal_process();
        bool check_devices();
        void init(int p_argc, char *p_argv[]);
        void deinit();
        void camera_add();
        void camera_delete();

    private:
        void pid_write();
        void pid_remove();
        void daemon();
        void av_init();
        void av_deinit();
        void allcams_init();
        void device_ids();
        void ntc();
        void watchdog(uint camindx);

};

#endif /* _INCLUDE_MOTIONPLUS_HPP_ */
