/*	video.h
 *
 *	Include file for video.c
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
#endif

/* video4linux stuff */
#define NORM_DEFAULT    0
#define NORM_PAL        0
#define NORM_NTSC       1
#define NORM_SECAM      2
#define NORM_PAL_NC	3
#define IN_DEFAULT      8
#define IN_TV           0
#define IN_COMPOSITE    1
#define IN_COMPOSITE2   2
#define IN_SVIDEO       3

/* video4linux error codes */
#define V4L_GENERAL_ERROR    0x01          /* binary 000001 */
#define V4L_BTTVLOST_ERROR   0x05          /* binary 000101 */
#define V4L_FATAL_ERROR      -1


#define VIDEO_DEVICE "/dev/video0"

struct video_dev {
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

	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
	int owner;
	int frames;
	
	/* Device type specific stuff: */
#ifndef WITHOUT_V4L	
	/* v4l */
	int size_map;
	int v4l_fmt;
	unsigned char *v4l_buffers[2];
	int v4l_curbuffer;
	int v4l_maxbuffer;
	int v4l_bufsize;
#endif
};

/* video functions, video.c */
int vid_start(struct context *);
int vid_next(struct context *, unsigned char *map);
#ifndef WITHOUT_V4L
void vid_init(void);
int vid_startpipe(const char *devname, int width, int height, int);
int vid_putpipe(int dev, unsigned char *image, int);
void vid_close(void);
void vid_cleanup(void);
#endif

#endif /* _INCLUDE_VIDEO_H */
