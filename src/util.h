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

/**
 *      util.h
 *
 *      Headers associated with utility and "my" routines in util.c module.
 *
 */

#ifndef _INCLUDE_UTIL_H
#define _INCLUDE_UTIL_H

#ifdef HAVE_FFMPEG

    #if ( MYFFVER >= 56000)
        #define MY_PIX_FMT_YUV420P   AV_PIX_FMT_YUV420P
        #define MY_PIX_FMT_YUVJ420P  AV_PIX_FMT_YUVJ420P
        #define MyPixelFormat AVPixelFormat
    #else  //Old ffmpeg pixel formats
        #define MY_PIX_FMT_YUV420P   PIX_FMT_YUV420P
        #define MY_PIX_FMT_YUVJ420P  PIX_FMT_YUVJ420P
        #define MyPixelFormat PixelFormat
    #endif  //myffver >= 56000

    #if ( MYFFVER > 54006)
        #define MY_FLAG_READ       AVIO_FLAG_READ
        #define MY_FLAG_WRITE      AVIO_FLAG_WRITE
        #define MY_FLAG_READ_WRITE AVIO_FLAG_READ_WRITE
    #else  //Older versions
        #define MY_FLAG_READ       URL_RDONLY
        #define MY_FLAG_WRITE      URL_WRONLY
        #define MY_FLAG_READ_WRITE URL_RDWR
    #endif

    /*********************************************/
    #if ( MYFFVER >= 56000)
        #define MY_CODEC_ID_MSMPEG4V2 AV_CODEC_ID_MSMPEG4V2
        #define MY_CODEC_ID_FLV1      AV_CODEC_ID_FLV1
        #define MY_CODEC_ID_FFV1      AV_CODEC_ID_FFV1
        #define MY_CODEC_ID_NONE      AV_CODEC_ID_NONE
        #define MY_CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
        #define MY_CODEC_ID_H264      AV_CODEC_ID_H264
        #define MY_CODEC_ID_HEVC      AV_CODEC_ID_HEVC
    #else
        #define MY_CODEC_ID_MSMPEG4V2 CODEC_ID_MSMPEG4V2
        #define MY_CODEC_ID_FLV1      CODEC_ID_FLV1
        #define MY_CODEC_ID_FFV1      CODEC_ID_FFV1
        #define MY_CODEC_ID_NONE      CODEC_ID_NONE
        #define MY_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
        #define MY_CODEC_ID_H264      CODEC_ID_H264
        #define MY_CODEC_ID_HEVC      CODEC_ID_H264
    #endif

    /*********************************************/
    #if ( MYFFVER >= 57000)
        #define MY_CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
        #define MY_CODEC_FLAG_QSCALE        AV_CODEC_FLAG_QSCALE
    #else
        #define MY_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
        #define MY_CODEC_FLAG_QSCALE        CODEC_FLAG_QSCALE
    #endif

    AVFrame *my_frame_alloc(void);
    void my_frame_free(AVFrame *frame);
    void my_packet_unref(AVPacket pkt);
    void my_avcodec_close(AVCodecContext *codec_context);
    int my_image_get_buffer_size(enum MyPixelFormat pix_fmt, int width, int height);
    int my_image_copy_to_buffer(AVFrame *frame,uint8_t *buffer_ptr,enum MyPixelFormat pix_fmt,int width,int height,int dest_size);
    int my_image_fill_arrays(AVFrame *frame,uint8_t *buffer_ptr,enum MyPixelFormat pix_fmt,int width,int height);
    int my_copy_packet(AVPacket *dest_pkt, AVPacket *src_pkt);

#endif /* HAVE_FFMPEG */

void *mymalloc(size_t nbytes);
void *myrealloc(void *ptr, size_t size, const char *desc);
FILE *myfopen(const char *path, const char *mode);
int myfclose(FILE *fh);
size_t mystrftime(const struct context *cnt, char *s, size_t max, const char *userformat
            , const struct timeval *tv1, const char *filename, int sqltype);
int mycreate_path(const char *path);

char *mystrcpy(char *to, const char *from);
char *mystrdup(const char *from);

void util_threadname_set(const char *abbr, int threadnbr, const char *threadname);
void util_threadname_get(char *threadname);
int util_check_passthrough(struct context *cnt);
void util_trim(char *parm);
void util_parms_free(struct params_context *parameters);
void util_parms_parse(struct params_context *parameters, char *confparm);
void util_parms_add_default(struct params_context *parameters, const char *parm_nm, const char *parm_vl);

int mystrceq(const char *var1, const char *var2);
int mystrcne(const char *var1, const char *var2);
int mystreq(const char *var1, const char *var2);
int mystrne(const char *var1, const char *var2);

#endif