#ifndef _INCLUDE_NETCAM_H
#define _INCLUDE_NETCAM_H

struct context;
struct image_data;

#define NETCAM_GENERAL_ERROR       0x02          /* binary 000010 */
#define NETCAM_RESTART_ERROR       0x12          /* binary 010010 */
#define NETCAM_BUFFSIZE 4096

enum NETCAM_STATUS {
    NETCAM_CONNECTED,      /* The camera is currently connected */
    NETCAM_READINGIMAGE,   /* Motion is reading a image from camera */
    NETCAM_NOTCONNECTED,   /* The camera has never connected */
    NETCAM_RECONNECTING   /* Motion is trying to reconnect to camera */
};

struct imgsize_context {
    int                   width;
    int                   height;
};

/*
 * struct url_t is used when parsing the user-supplied URL, as well as
 * when attempting to connect to the netcam.
 */
struct url_t {
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
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}
struct packet_item{
    AVPacket                  packet;
    int64_t                   idnbr;
    int                       iskey;
    int                       iswritten;
    struct timespec           timestamp_ts;
};

struct ctx_netcam {
    AVFormatContext          *format_context;        /* Main format context for the camera */
    AVCodecContext           *codec_context;         /* Codec being sent from the camera */
    AVFrame                  *frame;                 /* Reusable frame for images from camera */
    AVFrame                  *swsframe_in;           /* Used when resizing image sent from camera */
    AVFrame                  *swsframe_out;          /* Used when resizing image sent from camera */
    struct SwsContext        *swsctx;                /* Context for the resizing of the image */
    AVPacket                  packet_recv;           /* The packet that is currently being processed */
    AVFormatContext          *transfer_format;       /* Format context just for transferring to pass-through */
    struct packet_item       *pktarray;              /* Pointer to array of packets for passthru processing */
    int                       pktarray_size;         /* The number of packets in array.  1 based */
    int                       pktarray_index;        /* The index to the most current packet in array */
    int64_t                   idnbr;                 /* A ID number to track the packet vs image */
    AVDictionary             *opts;                  /* AVOptions when opening the format context */
    int                       swsframe_size;         /* The size of the image after resizing */
    int                       video_stream_index;    /* Stream index associated with video from camera */

    enum NETCAM_STATUS        status;                /* Status of whether the camera is connecting, closed, etc*/
    struct timespec           interruptstarttime;    /* The time set before calling the av functions */
    struct timespec           interruptcurrenttime;  /* Time during the interrupt to determine duration since start*/
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
    char                      camera_name[PATH_MAX];      /* The name of the camera as provided in the config file */
    char                      cameratype[30];   /* String specifying Normal or High for use in logging */
    struct imgsize_context    imgsize;          /* The image size parameters */

    int                       rtsp_uses_tcp;    /* Flag from config for whether to use tcp transport */
    int                       v4l2_palette;     /* Palette from config for v4l2 devices */
    int                       framerate;        /* Frames per second from configuration file */
    int                       reconnect_count;  /* Count of the times reconnection is tried*/
    int                       src_fps;          /* The fps provided from source*/

    struct timespec           frame_prev_tm;    /* The time set before calling the av functions */
    struct timespec           frame_curr_tm;    /* Time during the interrupt to determine duration since start*/
    struct ctx_motapp         *motapp;          /* Pointer to parent application context  */
    struct ctx_config         *conf;            /* Pointer to conf parms of parent cam*/

    char                      threadname[16];   /* The thread name*/
    int                       threadnbr;        /* The thread number */
    pthread_t                 thread_id;        /* thread i.d. for a camera-handling thread (if required). */
    pthread_mutex_t           mutex;            /* mutex used with conditional waits */
    pthread_mutex_t           mutex_transfer;   /* mutex used with transferring stream info for pass-through */
    pthread_mutex_t           mutex_pktarray;   /* mutex used with the packet array */

};

int netcam_setup(struct ctx_cam *cam);
int netcam_next(struct ctx_cam *cam, struct ctx_image_data *img_data);
void netcam_cleanup(struct ctx_cam *cam, int init_retry_flag);

#endif /* _INCLUDE_NETCAM_H */
