
#ifndef _INCLUDE_VIDEO_COMMON_H
#define _INCLUDE_VIDEO_COMMON_H

struct vid_devctrl_ctx {
    char          *ctrl_name;       /* The name as provided by the device */
    char          *ctrl_iddesc;     /* A motion description of the ID number for the control*/
    int            ctrl_minimum;    /* The minimum value permitted as reported by device*/
    int            ctrl_maximum;    /* The maximum value permitted as reported by device*/
    int            ctrl_default;    /* The default value for the control*/
    int            ctrl_currval;    /* The current value the control was set to */
    int            ctrl_newval;     /* The new value to set for the control */
    unsigned int   ctrl_id;         /* The ID number for the control as provided by the device*/
    unsigned int   ctrl_type;       /* The type of control as reported by the device*/
    int            ctrl_menuitem;   /* bool for whether item is a menu item description */
};

struct video_dev {
    struct video_dev        *next;
    int                      usage_count;
    int                      fd_device;
    const char              *video_device;
    int                      input;
    int                      norm;
    int                      width;
    int                      height;
    unsigned long            frequency;
    int                      fps;
    int                      owner;
    int                      frames;
    int                      pixfmt_src;
    int                      buffer_count;
    pthread_mutex_t          mutex;
    pthread_mutexattr_t      attr;
    void                    *v4l2_private;
    struct vid_devctrl_ctx  *devctrl_array;     /*Array of all the controls in the device*/
    int                      devctrl_count;     /*Count of the controls in the device*/
    int                      starting;          /*Bool for whether the device is just starting*/
    int                      device_type;       /*Camera, tuner, etc as provided by driver enum*/
    int                      device_tuner;      /*Tuner number if applicable from driver*/

    /* BKTR Specific Items */
    int            bktr_method;
    int            bktr_bufsize;
    const char    *bktr_tuner;
    int            bktr_fdtuner;
    unsigned char *bktr_buffers[2];
    int            bktr_curbuffer;
    int            bktr_maxbuffer;

};

int vid_start(struct context *cnt);
int vid_next(struct context *cnt, struct image_data *img_data);
void vid_close(struct context *cnt);
void vid_mutex_destroy(void);
void vid_mutex_init(void);

int vid_parms_parse(struct context *cnt);

void vid_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_yuv422pto420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_uyvyto420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_bayer2rgb24(unsigned char *dst, unsigned char *src, long int width, long int height);
void vid_y10torgb24(unsigned char *map, unsigned char *cap_map, int width, int height, int shift);
void vid_greytoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);
int vid_sonix_decompress(unsigned char *outp, unsigned char *inp, int width, int height);
int vid_mjpegtoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height, unsigned int size);


#endif
