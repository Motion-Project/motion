#include "netcam.h"

#ifdef HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>

struct rtsp_context {
    AVFormatContext*      format_context;
    AVCodecContext*       codec_context;
    AVFrame               *frame;
    int                   video_stream_index;
    char*                 path;
    char*                 user;
    char*                 pass;
    int                   readingframe;
    int                   connected;
    struct timeval        startreadtime;
};
#else
/****************************************
 * Dummy context for when no FFMPEG/Libav
 * is on machine.  These need to be primitive
 * data types
 *****************************************/
struct rtsp_context {
    int*                  format_context; 
    int                   readingframe;
    int                   connected;    
};
#endif

struct rtsp_context *rtsp_new_context(void);
void netcam_shutdown_rtsp(netcam_context_ptr netcam);
int netcam_connect_rtsp(netcam_context_ptr netcam);
int netcam_read_rtsp_image(netcam_context_ptr netcam);
int netcam_setup_rtsp(netcam_context_ptr netcam, struct url_t *url);
