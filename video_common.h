
#ifndef _INCLUDE_VIDEO_COMMON_H
#define _INCLUDE_VIDEO_COMMON_H

/* video4linux error codes */
#define V4L2_GENERAL_ERROR    0x01    /* binary 000001 */
#define V4L2_BTTVLOST_ERROR   0x05    /* binary 000101 */
#define V4L2_FATAL_ERROR      -1


struct video_dev {
    struct video_dev *next;
    int usage_count;
    int fd_device;
    int fd_tuner;
    const char *video_device;
    const char *tuner_device;
    int input;
    int norm;
    int width;
    int height;
    int brightness;
    int contrast;
    int saturation;
    int hue;
    int power_line_frequency;
    unsigned long freq;
    int tuner_number;
    int fps;

    int channel;
    int channelset;

    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    int owner;
    int frames;

    int v4l2;
    void *v4l2_private;

    int size_map;
    int capture_method;
    int v4l_fmt;
    unsigned char *v4l_buffers[2];
    int v4l_curbuffer;
    int v4l_maxbuffer;
    int v4l_bufsize;

};

int vid_start(struct context *cnt);
int vid_next(struct context *cnt, unsigned char *map);
void vid_close(struct context *cnt);
void vid_mutex_destroy(void);
void vid_mutex_init(void);

void vid_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_uyvyto420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void vid_bayer2rgb24(unsigned char *dst, unsigned char *src, long int width, long int height);
void vid_y10torgb24(unsigned char *map, unsigned char *cap_map, int width, int height, int shift);
void vid_greytoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);

int vid_sonix_decompress(unsigned char *outp, unsigned char *inp, int width, int height);
int vid_do_autobright(struct context *cnt, struct video_dev *viddev);
int vid_mjpegtoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height, unsigned int size);


#endif
