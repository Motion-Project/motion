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

#ifndef _INCLUDE_FFMPEG_H_
#define _INCLUDE_FFMPEG_H_

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdint.h>
#include "config.h"
struct image_data; /* forward declare for functions */
struct rtsp_context;

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

#ifdef HAVE_FFMPEG

    struct ffmpeg {
        AVFormatContext *oc;
        AVStream *video_st;
        AVCodecContext *ctx_codec;
        my_AVCodec      *codec;
        AVPacket *pkt;
        AVFrame *picture;       /* contains default image pointers */
        AVDictionary *opts;
        struct rtsp_context *rtsp_data;
        int width;
        int height;
        enum TIMELAPSE_TYPE tlapse;
        int fps;
        int bps;
        char *filename;
        int quality;
        const char *codec_name;
        int64_t last_pts;
        int64_t base_pts;
        int test_mode;
        int gop_cnt;
        struct timeval start_time;
        int            high_resolution;
        int            motion_images;
        int            passthrough;
        enum USER_CODEC     preferred_codec;
        char *nal_info;
        int  nal_info_len;
    };
#else
    struct ffmpeg {
        struct rtsp_context *rtsp_data;
        int width;
        int height;
        enum TIMELAPSE_TYPE tlapse;
        int fps;
        int bps;
        char *filename;
        int quality;
        const char *codec_name;
        int64_t last_pts;
        int64_t base_pts;
        int test_mode;
        int gop_cnt;
        struct timeval start_time;
        int            high_resolution;
        int            motion_images;
        int            passthrough;
    };
#endif // HAVE_FFMPEG

void ffmpeg_global_init(void);
void ffmpeg_global_deinit(void);
void ffmpeg_avcodec_log(void *, int, const char *, va_list);

int ffmpeg_open(struct ffmpeg *ffmpeg);
int ffmpeg_put_image(struct ffmpeg *ffmpeg, struct image_data *img_data, const struct timeval *tv1);
void ffmpeg_close(struct ffmpeg *ffmpeg);
void ffmpeg_reset_movie_start_time(struct ffmpeg *ffmpeg, const struct timeval *tv1);

#endif /* _INCLUDE_FFMPEG_H_ */
