#include "netcam.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>


struct rtsp_context {
	AVFormatContext*      format_context;
	AVCodecContext*       codec_context;
	int                   video_stream_index;
	char*                 path;
	char*                 user;
	char*                 pass;
};

//int netcam_setup_rtsp(netcam_context_ptr netcam, struct url_t *url);
struct rtsp_context *rtsp_new_context(void);
void netcam_shutdown_rtsp(netcam_context_ptr netcam);
int rtsp_connect(netcam_context_ptr netcam);
int netcam_read_rtsp_image(netcam_context_ptr netcam);
