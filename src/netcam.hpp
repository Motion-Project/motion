/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
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
    std::string service;
    std::string userpass;
    std::string host;
    int port;
    std::string path;
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

struct ctx_packet_item{
    AVPacket                 *packet;
    int64_t                   idnbr;
    bool                      iskey;
    bool                      iswritten;
};

struct ctx_filelist_item {
    std::string   fullnm;
    std::string   filenm;
    std::string   displaynm;
};

class cls_netcam {
    public:
        cls_netcam(cls_camera *p_cam, bool p_is_high);
        ~cls_netcam();

        cls_camera *cam;
        bool                      interrupted;      /* Boolean for whether interrupt has been tripped */
        enum NETCAM_STATUS        status;                /* Status of whether the camera is connecting, closed, etc*/
        struct timespec           ist_tm;    /* The time set before calling the av functions */
        struct timespec           icur_tm;  /* Time during the interrupt to determine duration since start*/
        int                       idur;      /* Seconds permitted before triggering a interrupt */
        std::string               camera_name;      /* The name of the camera as provided in the config file */
        std::string               cameratype;       /* String specifying Normal or High for use in logging */

        pthread_mutex_t           mutex;            /* mutex used with conditional waits */
        pthread_mutex_t           mutex_transfer;   /* mutex used with transferring stream info for pass-through */
        pthread_mutex_t           mutex_pktarray;   /* mutex used with the packet array */

        AVFormatContext          *transfer_format;       /* Format context just for transferring to pass-through */
        ctx_packet_item          *pktarray;              /* Pointer to array of packets for passthru processing */
        int                       pktarray_size;         /* The number of packets in array.  1 based */
        int                       video_stream_index;       /* Stream index associated with video from camera */
        int                       audio_stream_index;       /* Stream index associated with audio from camera */

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();

        int next(ctx_image_data *img_data);
        void noimage();
        void netcam_start();
        void netcam_stop();

    private:
        AVFormatContext          *format_context;        /* Main format context for the camera */
        AVCodecContext           *codec_context;         /* Codec being sent from the camera */
        AVStream                 *strm;
        AVFrame                  *frame;                 /* Reusable frame for images from camera */
        AVFrame                  *swsframe_in;           /* Used when resizing image sent from camera */
        AVFrame                  *swsframe_out;          /* Used when resizing image sent from camera */
        struct SwsContext        *swsctx;                /* Context for the resizing of the image */
        AVPacket                 *packet_recv;           /* The packet that is currently being processed */

        int                       pktarray_index;        /* The index to the most current packet in array */
        int64_t                   idnbr;                 /* A ID number to track the packet vs image */
        AVDictionary             *opts;                  /* AVOptions when opening the format context */
        int                       swsframe_size;         /* The size of the image after resizing */

        enum AVHWDeviceType       hw_type;
        enum AVPixelFormat        hw_pix_fmt;
        AVBufferRef              *hw_device_ctx;
        myAVCodec                *decoder;

        netcam_buff_ptr           img_recv;         /* The image buffer that is currently being processed */
        netcam_buff_ptr           img_latest;       /* The most recent image buffer that finished processing */

        bool                      high_resolution;  /* Boolean for whether this context is the Norm or High */

        bool                      first_image;      /* Boolean for whether we have captured the first image */
        bool                      passthrough;      /* Boolean for whether we are doing pass-through processing */

        std::string               path;             /* The connection string to use for the camera */
        std::string               service;          /* String specifying the type of camera http, rtsp, v4l2 */
        ctx_imgsize               imgsize;          /* The image size parameters */

        int                       capture_rate;     /* Frames per second from configuration file */
        int                       reconnect_count;  /* Count of the times reconnection is tried*/
        int                       src_fps;          /* The fps provided from source*/
        std::string               decoder_nm;       /* User requested decoder */

        struct timespec           connection_tm;    /* Time when camera was connected*/
        int64_t                   connection_pts;   /* PTS from the connection */
        int64_t                   last_pts;         /* PTS from the last packet read */
        int                       last_stream_index;    /* Stream index for last packet */
        bool                      pts_adj;          /* Bool for whether to use pts for timing */

        struct timespec           frame_prev_tm;    /* The time set before calling the av functions */
        struct timespec           frame_curr_tm;    /* Time during the interrupt to determine duration since start*/

        ctx_params                *params;          /* parameters for the camera */

        std::string               threadname;       /* The thread name*/
        int                       threadnbr;        /* The thread number */
        int         cfg_width;
        int         cfg_height;
        int         cfg_framerate;
        int         cfg_idur;
        std::string cfg_params;

        std::vector<ctx_filelist_item>    filelist;
        std::string                 filedir;
        int                       filenbr;

        void filelist_load();
        void check_buffsize(netcam_buff_ptr buff, size_t numbytes);
        char *url_match(regmatch_t m, const char *input);
        void url_invalid(ctx_url *parse_url);
        void url_parse(ctx_url *parse_url, std::string text_url);
        void free_pkt();
        int check_pixfmt();
        void pktarray_free();
        void context_null();
        void context_close();
        void pktarray_resize();
        void pktarray_add();
        int decode_sw();
        int decode_vaapi();
        int decode_cuda();
        int decode_drm();
        int decode_video();
        int decode_packet();
        void hwdecoders();
        void decoder_error(int retcd, const char* fnc_nm);
        int init_vaapi();
        int init_cuda();
        int init_drm();
        int init_swdecoder();
        int open_codec();
        int open_sws();
        int resize();
        void pkt_ts();
        int read_image();
        int ntc();
        void set_options();
        void set_path ();
        void set_parms ();
        int copy_stream();
        int open_context();
        int connect();

        void handler_wait();
        void handler_reconnect();
        void handler_startup();
        void handler_shutdown();

};

#endif /* _INCLUDE_NETCAM_HPP_ */
