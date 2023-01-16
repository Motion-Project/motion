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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 *
*/


#ifndef _INCLUDE_NETCAM_HPP_
#define _INCLUDE_NETCAM_HPP_

#define NETCAM_BUFFSIZE 4096

enum NETCAM_STATUS {
    NETCAM_CONNECTED,      /* The camera is currently connected */
    NETCAM_READINGIMAGE,   /* Motion is reading a image from camera */
    NETCAM_NOTCONNECTED,   /* The camera has never connected */
    NETCAM_RECONNECTING   /* Motion is trying to reconnect to camera */
};

struct ctx_imgsize {
    int                   width;
    int                   height;
};

struct ctx_url {
    char *service;
    char *userpass;
    char *host;
    int port;
    char *path;
};

/*
 * We use a special "triple-buffer" technique.  There are
 * three separate buffers (latest, receiving and jpegbuf)
 * which are each described using a struct netcam_image_buff
 */
typedef struct netcam_image_buff {
    char *ptr;
    int content_length;
    size_t size;                    /* total allocated size */
    size_t used;                    /* bytes already used */
    struct timespec image_time;      /* time this image was received */
} netcam_buff;
typedef netcam_buff *netcam_buff_ptr;

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavformat/avio.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
    #include "libavutil/buffer.h"
    #include "libavutil/error.h"
    #include "libavutil/hwcontext.h"
    #include "libavutil/mem.h"
}
struct ctx_packet_item{
    AVPacket                 *packet;
    int64_t                   idnbr;
    bool                      iskey;
    bool                      iswritten;
};

struct ctx_netcam {

    AVFormatContext          *format_context;        /* Main format context for the camera */
    AVCodecContext           *codec_context;         /* Codec being sent from the camera */
    AVStream                 *strm;
    AVFrame                  *frame;                 /* Reusable frame for images from camera */
    AVFrame                  *swsframe_in;           /* Used when resizing image sent from camera */
    AVFrame                  *swsframe_out;          /* Used when resizing image sent from camera */
    struct SwsContext        *swsctx;                /* Context for the resizing of the image */
    AVPacket                 *packet_recv;           /* The packet that is currently being processed */
    AVFormatContext          *transfer_format;       /* Format context just for transferring to pass-through */
    ctx_packet_item          *pktarray;              /* Pointer to array of packets for passthru processing */
    int                       pktarray_size;         /* The number of packets in array.  1 based */
    int                       pktarray_index;        /* The index to the most current packet in array */
    int64_t                   idnbr;                 /* A ID number to track the packet vs image */
    AVDictionary             *opts;                  /* AVOptions when opening the format context */
    int                       swsframe_size;         /* The size of the image after resizing */
    int                       video_stream_index;    /* Stream index associated with video from camera */
    int                       audio_stream_index;    /* Stream index associated with video from camera */
    int                       last_stream_index;     /* Index of the last packet read */

    enum AVHWDeviceType       hw_type;
    enum AVPixelFormat        hw_pix_fmt;
    AVBufferRef              *hw_device_ctx;
    myAVCodec                *decoder;

    enum NETCAM_STATUS        status;                /* Status of whether the camera is connecting, closed, etc*/
    struct timespec           interruptstarttime;    /* The time set before calling the av functions */
    struct timespec           interruptcurrenttime;  /* Time during the interrupt to determine duration since start*/
    int                       interruptduration;      /* Seconds permitted before triggering a interrupt */

    netcam_buff_ptr           img_recv;         /* The image buffer that is currently being processed */
    netcam_buff_ptr           img_latest;       /* The most recent image buffer that finished processing */

    bool                      interrupted;      /* Boolean for whether interrupt has been tripped */
    bool                      finish;           /* Boolean for whether we are finishing the application */
    bool                      high_resolution;  /* Boolean for whether this context is the Norm or High */
    bool                      handler_finished; /* Boolean for whether the handler is running or not */
    bool                      first_image;      /* Boolean for whether we have captured the first image */
    bool                      passthrough;      /* Boolean for whether we are doing pass-through processing */

    char                     *path;             /* The connection string to use for the camera */
    char                      service[5];       /* String specifying the type of camera http, rtsp, v4l2 */
    char                      camera_name[PATH_MAX];      /* The name of the camera as provided in the config file */
    char                      cameratype[30];   /* String specifying Normal or High for use in logging */
    ctx_imgsize               imgsize;          /* The image size parameters */

    int                       capture_rate;     /* Frames per second from configuration file */
    int                       reconnect_count;  /* Count of the times reconnection is tried*/
    int                       src_fps;          /* The fps provided from source*/
    char                      *decoder_nm;      /* User requested decoder */

    struct timespec           connection_tm;    /* Time when camera was connected*/
    int64_t                   connection_pts;   /* PTS from the connection */
    int64_t                   last_pts;         /* PTS from the last packet read */
    bool                      pts_adj;          /* Bool for whether to use pts for timing */

    struct timespec           frame_prev_tm;    /* The time set before calling the av functions */
    struct timespec           frame_curr_tm;    /* Time during the interrupt to determine duration since start*/
    ctx_motapp                *motapp;          /* Pointer to parent application context  */
    ctx_config                *conf;            /* Pointer to conf parms of parent cam*/
    ctx_params                *params;          /* parameters for the camera */

    char                      threadname[16];   /* The thread name*/
    int                       threadnbr;        /* The thread number */
    pthread_t                 thread_id;        /* thread i.d. for a camera-handling thread (if required). */
    pthread_mutex_t           mutex;            /* mutex used with conditional waits */
    pthread_mutex_t           mutex_transfer;   /* mutex used with transferring stream info for pass-through */
    pthread_mutex_t           mutex_pktarray;   /* mutex used with the packet array */

};

void netcam_start(ctx_dev *cam);
int netcam_next(ctx_dev *cam, ctx_image_data *img_data);
void netcam_cleanup(ctx_dev *cam);

#endif /* _INCLUDE_NETCAM_HPP_ */
