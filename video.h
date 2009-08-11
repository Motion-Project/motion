/*    video.h
 *
 *    Include file for video.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_VIDEO_H
#define _INCLUDE_VIDEO_H

#define _LINUX_TIME_H 1
#ifndef WITHOUT_V4L
#include <linux/videodev.h>
#include <sys/mman.h>
#include "pwc-ioctl.h"
#endif

/* video4linux stuff */
#define NORM_DEFAULT            0
#define NORM_PAL                0
#define NORM_NTSC               1
#define NORM_SECAM              2
#define NORM_PAL_NC             3
#define IN_DEFAULT              8
#define IN_TV                   0
#define IN_COMPOSITE            1
#define IN_COMPOSITE2           2
#define IN_SVIDEO               3

/* video4linux error codes */
#define V4L_GENERAL_ERROR    0x01    /* binary 000001 */
#define V4L_BTTVLOST_ERROR   0x05    /* binary 000101 */
#define V4L_FATAL_ERROR        -1

#define VIDEO_DEVICE            "/dev/video0"

struct video_dev {
    struct video_dev *next;
    int usage_count;
    int fd;
    const char *video_device;
    int input;
    int width;
    int height;
    int brightness;
    int contrast;
    int saturation;
    int hue;
    unsigned long freq;
    int tuner_number;
    int fps;

    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    int owner;
    int frames;

    /* Device type specific stuff: */
#ifndef WITHOUT_V4L
    /* v4l */
    int v4l2;
    void *v4l2_private;

    int size_map;
    int v4l_fmt;
    unsigned char *v4l_buffers[2];
    int v4l_curbuffer;
    int v4l_maxbuffer;
    int v4l_bufsize;
#endif
};

/* video functions, video_common.c */
int vid_start(struct context *);
int vid_next(struct context *, unsigned char *map);
void vid_close(struct context *cnt);
void vid_cleanup(void);
void vid_init(void);
void conv_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height);
void conv_uyvyto420p(unsigned char *map, unsigned char *cap_map, unsigned int width, unsigned int height);
void conv_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height);
int conv_jpeg2yuv420(struct context *cnt, unsigned char *dst, netcam_buff * buff, int width, int height);
int sonix_decompress(unsigned char *outp, unsigned char *inp,int width, int height);
void bayer2rgb24(unsigned char *dst, unsigned char *src, long int width, long int height);
int vid_do_autobright(struct context *cnt, struct video_dev *viddev);
int mjpegtoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height, unsigned int size);

#ifndef WITHOUT_V4L
/* video functions, video.c */
int vid_startpipe(const char *dev_name, int width, int height, int);
int vid_putpipe(int dev, unsigned char *image, int);
unsigned char *v4l_start(struct context *cnt, struct video_dev *viddev, int width, int height,
                         int input, int norm, unsigned long freq, int tuner_number);
void v4l_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, 
                   int height, int input, int norm, int skip, unsigned long freq, int tuner_number);
int v4l_next(struct video_dev *viddev, unsigned char *map, int width, int height);

/* video2.c */
unsigned char *v4l2_start(struct context *cnt, struct video_dev *viddev, int width, int height,
                          int input, int norm, unsigned long freq, int tuner_number);
void v4l2_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, 
                    int height, struct config *conf);
int v4l2_next(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height);
void v4l2_close(struct video_dev *viddev);
void v4l2_cleanup(struct video_dev *viddev);

#endif /* WITHOUT_V4L */

#endif                /* _INCLUDE_VIDEO_H */
