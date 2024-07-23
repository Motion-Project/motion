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

#ifndef _INCLUDE_UTIL_HPP_
#define _INCLUDE_UTIL_HPP_

#define MY_PIX_FMT_YUV420P   AV_PIX_FMT_YUV420P
#define MY_PIX_FMT_YUVJ420P  AV_PIX_FMT_YUVJ420P
#define MyPixelFormat AVPixelFormat

#define MY_FLAG_READ       AVIO_FLAG_READ
#define MY_FLAG_WRITE      AVIO_FLAG_WRITE
#define MY_FLAG_READ_WRITE AVIO_FLAG_READ_WRITE

#define MY_CODEC_ID_MSMPEG4V2  AV_CODEC_ID_MSMPEG4V2
#define MY_CODEC_ID_FLV1       AV_CODEC_ID_FLV1
#define MY_CODEC_ID_FFV1       AV_CODEC_ID_FFV1
#define MY_CODEC_ID_NONE       AV_CODEC_ID_NONE
#define MY_CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
#define MY_CODEC_ID_H264       AV_CODEC_ID_H264
#define MY_CODEC_ID_HEVC       AV_CODEC_ID_HEVC
#define MY_CODEC_ID_THEORA     AV_CODEC_ID_THEORA
#define MY_CODEC_ID_VP8        AV_CODEC_ID_VP8
#define MY_CODEC_ID_VP9        AV_CODEC_ID_VP9

#define MY_CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#define MY_CODEC_FLAG_QSCALE        AV_CODEC_FLAG_QSCALE

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
    typedef const AVCodec myAVCodec; /* Version independent definition for AVCodec*/
#else
    typedef AVCodec myAVCodec; /* Version independent definition for AVCodec*/
#endif

#ifdef HAVE_GETTEXT
    #include <libintl.h>
    extern int  _nl_msg_cat_cntr;    /* Required for changing the locale dynamically */
#endif

#define _(STRING) mytranslate_text(STRING, 2)

#define SLEEP(seconds, nanoseconds) {              \
                struct timespec ts1;                \
                ts1.tv_sec = seconds;             \
                ts1.tv_nsec = (long)nanoseconds;        \
                while (nanosleep(&ts1, &ts1) == -1); \
        }

#if MHD_VERSION >= 0x00097002
    typedef enum MHD_Result mhdrslt; /* Version independent return result from MHD */
#else
    typedef int             mhdrslt; /* Version independent return result from MHD */
#endif
/* Version independent uint */
#if (MYFFVER <= 60016)
    typedef uint8_t myuint;
#else
    typedef const uint8_t myuint;
#endif

    void mymemset(void *dst, int src, int sz);
    void mymemset(void *dst, int src, ulong sz);
    void mymemcpy(void *dst, void *src, int sz);
    void mymemcpy(void *dst, void *src, ulong sz);

    void myfree(void *ptr_addr);
    #define mydelete(x) {if(x!=nullptr) {delete x; x=nullptr;}}

    void *mymalloc(size_t nbytes);
    void *mymalloc(uint nbytes);
    void *mymalloc(int nbytes);

    void *myrealloc(void *ptr, size_t size, const char *desc);
    int mycreate_path(const char *path);
    FILE *myfopen(const char *path, const char *mode);
    int myfclose(FILE *fh);
    size_t mystrftime(ctx_dev *cam, char *s, size_t max
        , const char *userformat, const char *filename);
    void mypicname(ctx_dev *cam
        , char* fullname, std::string fmtstr
        , std::string basename, std::string extname);
    void util_exec_command(ctx_dev *cam, const char *command, const char *filename);

    void mythreadname_set(const char *abbr, int threadnbr, const char *threadname);
    void mythreadname_get(char *threadname);
    void mythreadname_get(std::string &threadname);

    char* mytranslate_text(const char *msgid, int setnls);
    void mytranslate_init(void);

    int mystrceq(const char* var1, const char* var2);
    int mystrcne(const char* var1, const char* var2);
    int mystreq(const char* var1, const char* var2);
    int mystrne(const char* var1, const char* var2);
    void myltrim(std::string &parm);
    void myrtrim(std::string &parm);
    void mytrim(std::string &parm);
    void myunquote(std::string &parm);

    void myframe_key(AVFrame *frame);
    void myframe_interlaced(AVFrame *frame);
    AVPacket *mypacket_alloc(AVPacket *pkt);

    void util_parms_parse(ctx_params *params, std::string parm_desc, std::string confline);
    void util_parms_add_default(ctx_params *params, std::string parm_nm, std::string parm_vl);
    void util_parms_add_default(ctx_params *params, std::string parm_nm, int parm_vl);
    void util_parms_add(ctx_params *params, std::string parm_nm, std::string parm_val);
    void util_parms_update(ctx_params *params, std::string &confline);

    int mtoi(std::string parm);
    int mtoi(char *parm);
    float mtof(char *parm);
    float mtof(std::string parm);
    bool mtob(std::string parm);
    bool mtob(char *parm);
    long mtol(std::string parm);
    long mtol(char *parm);
    std::string mtok(std::string &parm, std::string tok);

#endif /* _INCLUDE_UTIL_HPP_ */
