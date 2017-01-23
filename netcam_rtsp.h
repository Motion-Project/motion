#ifndef _INCLUDE_NETCAM_RTSP_H
#define _INCLUDE_NETCAM_RTSP_H

#include "netcam.h"

#ifdef HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

struct rtsp_context {
    AVFormatContext*      format_context;
    AVCodecContext*       codec_context;
    AVFrame*              frame;
    AVFrame*              swsframe_in;
    AVFrame*              swsframe_out;
    int                   swsframe_size;
    int                   video_stream_index;
    char*                 path;
    char*                 user;
    char*                 pass;
    enum RTSP_STATUS      status;
    struct timeval        startreadtime;
    struct SwsContext*   swsctx;
};

#else /* Do not have FFmpeg */

struct rtsp_context {
    int*                  format_context;
    enum RTSP_STATUS      status;
};

#endif /* end HAVE_FFMPEG  */

struct rtsp_context *rtsp_new_context(void);
void netcam_shutdown_rtsp(netcam_context_ptr netcam);
int netcam_connect_rtsp(netcam_context_ptr netcam);
int netcam_read_rtsp_image(netcam_context_ptr netcam);
int netcam_setup_rtsp(netcam_context_ptr netcam, struct url_t *url);
int netcam_next_rtsp(unsigned char *image , netcam_context_ptr netcam);

#endif /* _INCLUDE_NETCAM_RTSP_H */
