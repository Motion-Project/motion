/*
 * video_freebsd.h
 *
 *    Include file for video_freebsd.c
 *    Copyright 2004 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_VIDEO_FREEBSD_H
#define _INCLUDE_VIDEO_FREEBSD_H

#ifdef HAVE_BKTR

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/bt8xx.h>
#else
#include <dev/bktr/ioctl_meteor.h>
#include <dev/bktr/ioctl_bt848.h>
#endif

#endif /* HAVE_BKTR */

#define array_elem(x) (sizeof(x) / sizeof((x)[0]))

/* video4linux error codes */
#define V4L2_GENERAL_ERROR  0x01   /* binary 000001 */
#define V4L2_BTTVLOST_ERROR 0x05   /* binary 000101 */
#define V4L2_FATAL_ERROR      -1

#define VIDEO_DEVICE          "/dev/bktr0"
#define IN_DEFAULT            0

#define BKTR_PAL                   0
#define BKTR_NTSC                  1
#define BKTR_SECAM                 2
#define BKTR_PAL_NC                3

#define BKTR_PAL_HEIGHT          576
#define BKTR_SECAM_HEIGHT        576
#define BKTR_NTSC_HEIGHT         480

#define BKTR_IN_COMPOSITE          0
#define BKTR_IN_TV                 1
#define BKTR_IN_COMPOSITE2         2
#define BKTR_IN_SVIDEO             3

#define BKTR_NORM_DEFAULT      BT848_IFORM_F_AUTO
#define BKTR_NORM_PAL          BT848_IFORM_F_PALBDGHI
#define BKTR_NORM_NTSC         BT848_IFORM_F_NTSCM
#define BKTR_NORM_SECAM        BT848_IFORM_F_SECAM

struct video_dev {
    struct video_dev *next;
    int usage_count;
    int fd_bktr;
    int fd_tuner;
    const char *video_device;
    const char *tuner_device;
    unsigned input;
    unsigned norm;
    int width;
    int height;
    int contrast;
    int saturation;
    int hue;
    int brightness;
    int channel;
    int channelset;
    unsigned long freq;

    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    int owner;
    int frames;

    /* Device type specific stuff: */
    int capture_method;
    int v4l_fmt;
    unsigned char *v4l_buffers[2];
    int v4l_curbuffer;
    int v4l_maxbuffer;
    int v4l_bufsize;
};

/* video functions, video_freebsd.c */
int vid_start(struct context *);
int vid_next(struct context *, unsigned char *);
void vid_close(struct context *);

void vid_mutex_init(void);
void vid_mutex_destroy(void);

#endif /* _INCLUDE_VIDEO_FREEBSD_H */
