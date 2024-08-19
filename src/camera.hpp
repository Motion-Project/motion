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


#ifndef _INCLUDE_CAMERA_HPP_
#define _INCLUDE_CAMERA_HPP_

#define IMAGE_MOTION     1
#define IMAGE_TRIGGER    2
#define IMAGE_SAVE       4
#define IMAGE_SAVED      8
#define IMAGE_PRECAP    16
#define IMAGE_POSTCAP   32

enum CAMERA_TYPE {
    CAMERA_TYPE_UNKNOWN,
    CAMERA_TYPE_V4L2,
    CAMERA_TYPE_LIBCAM,
    CAMERA_TYPE_NETCAM
};
enum CAPTURE_RESULT {
    CAPTURE_SUCCESS,
    CAPTURE_FAILURE,
    CAPTURE_ATTEMPTED
};


struct ctx_coord {
    int x;
    int y;
    int width;
    int height;
    int minx;
    int maxx;
    int miny;
    int maxy;
    int stddev_x;
    int stddev_y;
    int stddev_xy;
};

struct ctx_image_data {
    u_char       *image_norm;
    u_char       *image_high;
    int                 diffs;
    int                 diffs_raw;
    int                 diffs_ratio;
    int64_t             idnbr_norm;
    int64_t             idnbr_high;
    struct timespec     imgts;          /* Realtime for display */
    struct timespec     monots;         /* Montonic clock for timing */
    int                 shot;           /* Sub second timestamp count */
    unsigned long       cent_dist;      /* Movement center to img center distance * Note: Dist is calculated distX*distX + distY*distY */
    unsigned int        flags;          /* See IMAGE_* defines */
    ctx_coord           location;       /* coordinates for center and size of last motion detection*/
    int                 total_labels;
};

struct ctx_images {
    ctx_image_data *image_ring;    /* The base address of the image ring buffer */
    ctx_image_data image_motion;   /* Picture buffer for motion images */
    ctx_image_data image_preview;  /* Picture buffer for best image when enables */

    u_char *ref;               /* The reference frame */
    u_char *ref_next;          /* The reference frame */
    u_char *mask;              /* Buffer for the mask file */
    u_char *common_buffer;
    u_char *image_substream;
    u_char *image_virgin;            /* Last picture frame with no text or locate overlay */
    u_char *image_vprvcy;            /* Virgin image with the privacy mask applied */
    u_char *mask_privacy;            /* Buffer for the privacy mask values */
    u_char *mask_privacy_uv;         /* Buffer for the privacy U&V values */
    u_char *mask_privacy_high;       /* Buffer for the privacy mask values */
    u_char *mask_privacy_high_uv;    /* Buffer for the privacy U&V values */
    u_char *image_secondary;         /* Buffer for JPG from alg_sec methods */

    int ring_size;
    int ring_in;                /* Index in image ring buffer we last added a image into */
    int ring_out;               /* Index in image ring buffer we want to process next time */

    int *ref_dyn;               /* Dynamic objects to be excluded from reference frame */
    int *labels;
    int *labelsize;

    int width;
    int height;
    int size_norm;                    /* Number of bytes for normal size image */

    int width_high;
    int height_high;
    int size_high;                 /* Number of bytes for high resolution image */

    int motionsize;
    int labelgroup_max;
    int labels_above;
    int labelsize_max;
    int largest_label;
    int size_secondary;             /* Size of the jpg put into image_secondary*/

};

struct ctx_stream_data {
    u_char   *jpg_data;  /* Image compressed as JPG */
    int             jpg_sz;     /* The number of bytes for jpg */
    int             consumed;   /* Bool for whether the jpeg data was consumed*/
    u_char   *img_data;  /* The base data used for image */
    int             jpg_cnct;   /* Counter of the number of jpg connections*/
    int             ts_cnct;    /* Counter of the number of mpegts connections */
    int             all_cnct;   /* Counter of the number of all camera connections */
};

struct ctx_stream {
    pthread_mutex_t  mutex;
    ctx_stream_data  norm;       /* Copy of the image to use for web stream*/
    ctx_stream_data  sub;        /* Copy of the image to use for web stream*/
    ctx_stream_data  motion;     /* Copy of the image to use for web stream*/
    ctx_stream_data  source;     /* Copy of the image to use for web stream*/
    ctx_stream_data  secondary;  /* Copy of the image to use for web stream*/
};

class cls_camera {
    public:
        cls_camera(cls_motapp *p_app);
        ~cls_camera();

        cls_motapp      *app;
        cls_config      *cfg;
        cls_config      *conf_src;
        ctx_images      imgs;
        ctx_stream      stream;
        ctx_image_data  *current_image;
        cls_alg         *alg;
        cls_algsec      *algsec;
        cls_rotate      *rotate;
        cls_netcam      *netcam;
        cls_netcam      *netcam_high;
        ctx_all_loc     all_loc;
        cls_draw        *draw;
        cls_picture     *picture;

        bool            handler_stop;
        bool            handler_finished;
        pthread_t       handler_thread;
        void            handler();
        void            handler_startup();
        void            handler_shutdown();

        bool    restart;
        int     threadnr;
        int     noise;
        bool    detecting_motion;
        int     event_curr_nbr;
        int     event_prev_nbr;
        int     threshold;
        int     lastrate;
        int     frame_skip;
        int     lost_connection;
        int     text_scale;
        int     watchdog;
        bool    movie_passthrough;
        char    eventid[20];
        char    text_event_string[PATH_MAX];
        char    hostname[PATH_MAX];
        char    action_user[40];
        int     movie_fps;
        bool    passflag;
        int     pipe;
        int     mpipe;
        bool    pause;
        int     missing_frame_counter;

        uint64_t                info_diff_tot;
        uint64_t                info_diff_cnt;
        int                     info_sdev_min;
        int                     info_sdev_max;
        uint64_t                info_sdev_tot;

        bool    action_snapshot;    /* Make a snapshot */
        bool    event_stop;  /* Boolean for whether to stop a event */
        bool    event_user;  /* Boolean for whether to user triggered an event */

        enum DEVICE_STATUS      device_status;
        enum CAMERA_TYPE        camera_type;
        struct timespec         connectionlosttime;

    private:
        cls_movie       *movie_norm;
        cls_movie       *movie_motion;
        cls_movie       *movie_timelapse;
        cls_movie       *movie_extpipe;
        cls_v4l2cam     *v4l2cam;
        cls_libcam      *libcam;

        pthread_t       thread_id;
        int             track_posx;
        int             track_posy;
        int             threshold_maximum;

        int                     postcap;                             /* downcounter, frames left to to send post event */
        int                     shots_mt;   /* Monotonic clock shots count*/
        int                     shots_rt;   /* Realtime  clock shots count*/
        struct timespec         frame_curr_ts;
        struct timespec         frame_last_ts;
        time_t                  lasttime;
        time_t                  movie_start_time;
        int                     startup_frames;
        int area_minx[9], area_miny[9], area_maxx[9], area_maxy[9];
        int                     areadetect_eventnbr;
        int previous_diffs, previous_location_x, previous_location_y;


        void ring_resize();
        void ring_destroy();
        void ring_process_debug();
        void ring_process_image();
        void ring_process();
        void info_reset();
        void movie_start();
        void movie_end();
        void detected_trigger();
        void track_center();
        void track_move();
        void detected();
        void mask_privacy();
        void cam_close();
        void cam_start();
        int cam_next(ctx_image_data *img_data);
        void init_camera_type();
        void init_firstimage();
        void check_szimg();
        void init_areadetect();
        void init_buffers();
        void init_values();
        void init_cam_start();
        void init_ref();
        void init_conf();
        void cleanup();
        void init();
        void areadetect();
        void prepare();
        void resetimages();
        void retry();
        int capture();
        void detection();
        void tuning();
        void overlay();
        void actions_emulate();
        void actions_motion();
        void actions_event();
        void actions();
        void snapshot();
        void timelapse();
        void loopback();
        void frametiming();


};

#endif /* _INCLUDE_CAMERA_HPP_ */
