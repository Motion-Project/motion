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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
 *
*/


#ifndef _INCLUDE_MOVIE_HPP_
#define _INCLUDE_MOVIE_HPP_

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdint.h>

struct ctx_image_data; /* forward declare for functions */
struct ctx_netcam;

enum TIMELAPSE_TYPE {
    TIMELAPSE_NONE,         /* No timelapse, regular processing */
    TIMELAPSE_APPEND,       /* Use append version of timelapse */
    TIMELAPSE_NEW           /* Use create new file version of timelapse */
};

/* Enumeration of the user requested codecs that need special handling */
enum USER_CODEC {
    USER_CODEC_V4L2M2M,    /* Requested codec for movie is h264_v4l2m2m */
    USER_CODEC_H264OMX,    /* Requested h264_omx */
    USER_CODEC_MPEG4OMX,   /* Requested mpeg4_omx */
    USER_CODEC_DEFAULT     /* All other default codecs */
};


struct ctx_movie {
    AVFormatContext     *oc;
    AVStream            *strm_video;
    AVStream            *strm_audio;
    AVCodecContext      *ctx_codec;
    myAVCodec           *codec;
    AVPacket            *pkt;
    AVFrame             *picture;       /* contains default image pointers */
    AVDictionary        *opts;
    struct ctx_netcam   *netcam_data;
    int                 width;
    int                 height;
    enum TIMELAPSE_TYPE tlapse;
    int                 fps;
    int                 bps;
    char                filename[PATH_MAX];
    int                 quality;
    const char          *container_name;
    int64_t             last_pts;
    int64_t             base_pts;
    int64_t             pass_audio_base;
    int64_t             pass_video_base;
    bool                test_mode;
    int                 gop_cnt;
    struct timespec     start_time;
    bool                high_resolution;
    bool                motion_images;
    bool                passthrough;
    enum USER_CODEC     preferred_codec;
    char                *nal_info;
    int                 nal_info_len;
};


void movie_global_init(void);
void movie_global_deinit(void);
void movie_avcodec_log(void *, int, const char *, va_list);

int movie_open(struct ctx_movie *movie);
int movie_put_image(struct ctx_movie *movie, struct ctx_image_data *img_data, const struct timespec *tv1);
void movie_close(struct ctx_movie *movie);
void movie_reset_start_time(struct ctx_movie *movie, const struct timespec *tv1);
int movie_init_timelapse(struct ctx_cam *cam, struct timespec *ts1);
int movie_init_norm(struct ctx_cam *cam, struct timespec *ts1);
int movie_init_motion(struct ctx_cam *cam, struct timespec *ts1);
void movie_free(struct ctx_movie *movie);

#endif /* #define _INCLUDE_MOVIE_HPP_ */
