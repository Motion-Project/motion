#ifndef _INCLUDE_NETCAM_RTSP_H
#define _INCLUDE_NETCAM_RTSP_H

#include "netcam.h"

enum RTSP_STATUS {
    RTSP_NOTCONNECTED,   /* The camera has never connected */
    RTSP_CONNECTED,      /* The camera is currently connected */
    RTSP_RECONNECTING,   /* Motion is trying to reconnect to camera */
    RTSP_READINGIMAGE    /* Motion is reading a image from camera */
};

struct imgsize_context {
    int                   width;
    int                   height;
};

#ifdef HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>


typedef struct rtsp {
    AVFormatContext          *format_context;        /* Main format context for the camera */
    AVCodecContext           *codec_context;         /* Codec being sent from the camera */
    AVFrame                  *frame;                 /* Reusable frame for images from camera */
    AVFrame                  *swsframe_in;           /* Used when resizing image sent from camera */
    AVFrame                  *swsframe_out;          /* Used when resizing image sent from camera */
    struct SwsContext        *swsctx;                /* Context for the resizing of the image */
    AVPacket                  packet_recv;           /* The packet that is currently being processed */
    AVPacket                  packet_latest;         /* The most recent packet that has finished processing */
    AVDictionary             *opts;                  /* AVOptions when opening the format context */
    int                       swsframe_size;         /* The size of the image after resizing */
    int                       video_stream_index;    /* Stream index associated with video from camera */

    enum RTSP_STATUS          status;                /* Status of whether the camera is connecting, closed, etc*/
    struct timeval            interruptstarttime;    /* The time set before calling the av functions */
    struct timeval            interruptcurrenttime;  /* Time during the interrupt to determine duration since start*/
    int                       interruptduration;      /* Seconds permitted before triggering a interrupt */

    netcam_buff_ptr           img_recv;         /* The image buffer that is currently being processed */
    netcam_buff_ptr           img_latest;       /* The most recent image buffer that finished processing */

    int                       interrupted;      /* Boolean for whether interrupt has been tripped */
    int                       finish;           /* Boolean for whether we are finishing the application */
    int                       high_resolution;  /* Boolean for whether this context is the Norm or High */
    int                       handler_finished; /* Boolean for whether the handler is running or not */
    int                       first_image;      /* Boolean for whether we have captured the first image */
    int                       passthrough;      /* Boolean for whether we are doing pass-through processing */

    char                     *path;             /* The connection string to use for the camera */
    char                      service[5];       /* String specifying the type of camera http, rtsp, v4l2 */
    const char               *camera_name;      /* The name of the camera as provided in the config file */
    char                      cameratype[20];   /* String specifying Normal or High for use in logging */
    struct imgsize_context    imgsize;          /* The image size parameters */

    int                       rtsp_uses_tcp;    /* Flag from config for whether to use tcp transport */
    int                       v4l2_palette;     /* Palette from config for v4l2 devices */
    int                       frame_limit;      /* Frames per second from configuration file */

    char                      threadname[16];   /* The thread name*/
    int                       threadnbr;        /* The thread number */
    pthread_t                 thread_id;        /* thread i.d. for a camera-handling thread (if required). */
    pthread_mutex_t           mutex;            /* mutex used with conditional waits */

}rtsp_context;

#else /* Do not have FFmpeg */

typedef struct rtsp {
    int                   dummy;
}rtsp_context;

#endif /* end HAVE_FFMPEG  */

int netcam_rtsp_setup(struct context *cnt);
int netcam_rtsp_next(struct context *cnt, struct image_data *img_data);
void netcam_rtsp_cleanup(struct context *cnt, int init_retry_flag);

#endif /* _INCLUDE_NETCAM_RTSP_H */
