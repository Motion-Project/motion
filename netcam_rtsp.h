#include "netcam.h"

#ifdef HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>

#endif /* end HAVE_FFMPEG  */

struct rtsp_context {
#ifdef HAVE_FFMPEG
    AVFormatContext*      format_context;
    AVCodecContext*       codec_context;
    AVFrame*              frame;
    int                   video_stream_index;
    char*                 path;
    char*                 user;
    char*                 pass;
    int                   readingframe;
    int                   status;
    struct timeval        startreadtime;

#else /* Do not have FFmpeg */
    int*                  format_context; 
    int                   readingframe;
    int                   status;
#endif /* end HAVE_FFMPEG  */
};

struct rtsp_context *rtsp_new_context(void);
void netcam_shutdown_rtsp(netcam_context_ptr netcam);
int netcam_connect_rtsp(netcam_context_ptr netcam);
int netcam_read_rtsp_image(netcam_context_ptr netcam);
int netcam_setup_rtsp(netcam_context_ptr netcam, struct url_t *url);
