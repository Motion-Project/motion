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

#ifndef WITHOUT_V4L

#ifdef __NetBSD__
#include <dev/ic/bt8xx.h>
#elif __OpenBSD__
#include <dev/ic/bt8xx.h>
#elif defined(OLD_BKTR)
#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>
#else
#include <dev/bktr/ioctl_meteor.h>
#include <dev/bktr/ioctl_bt848.h>
#endif

#endif /* !WITHOUT_V4L */

/* bktr (video4linux) stuff FIXME more modes not only these */

/* not used yet FIXME ! only needed for tuner use */
/*
#define TV_INPUT_NTSCM    BT848_IFORM_F_NTSCM
#define TV_INPUT_NTSCJ    BT848_IFORM_F_NTSCJ
#define TV_INPUT_PALBDGHI BT848_IFORM_F_PALBDGHI
#define TV_INPUT_PALM     BT848_IFORM_F_PALM
#define TV_INPUT_PALN     BT848_IFORM_F_PALN
#define TV_INPUT_SECAM    BT848_IFORM_F_SECAM
#define TV_INPUT_PALNCOMB BT848_IFORM_F_RSVD
*/

/* video4linux error codes */
#define V4L_GENERAL_ERROR  0x01   /* binary 000001 */
#define V4L_BTTVLOST_ERROR 0x05   /* binary 000101 */
#define V4L_FATAL_ERROR      -1

#define NORM_DEFAULT    0x00800 // METEOR_FMT_AUTOMODE
#define NORM_PAL        0x00200 // METEOR_FMT_PAL
#define NORM_NTSC       0x00100 // METEOR_FMT_NTSC
#define NORM_SECAM      0x00400 // METEOR_FMT_SECAM
#define NORM_PAL_NC     0x00200 // METEOR_FMT_PAL /* Greyscale howto ?! FIXME */

#define NORM_DEFAULT_NEW      BT848_IFORM_F_AUTO
#define NORM_PAL_NEW          BT848_IFORM_F_PALBDGHI
#define NORM_NTSC_NEW         BT848_IFORM_F_NTSCM
#define NORM_SECAM_NEW        BT848_IFORM_F_SECAM
#define NORM_PAL_NC_NEW       BT848_IFORM_F_AUTO /* FIXME */

#define PAL                   0
#define NTSC                  1
#define SECAM                 2
#define PAL_NC                3

#define PAL_HEIGHT          576
#define SECAM_HEIGHT        576
#define NTSC_HEIGHT         480

#define BSD_VIDFMT_NONE       0
#define BSD_VIDFMT_YV12       1
#define BSD_VIDFMT_I420       2
#define BSD_VIDFMT_YV16       3
#define BSD_VIDFMT_YUY2       4
#define BSD_VIDFMT_UYVY       5
#define BSD_VIDFMT_RV15       6
#define BSD_VIDFMT_RV16       7
#define BSD_VIDFMT_LAST       8


#define IN_DEFAULT            0
#define IN_COMPOSITE          0
#define IN_TV                 1
#define IN_COMPOSITE2         2
#define IN_SVIDEO             3

#define CAPTURE_SINGLE        0
#define CAPTURE_CONTINOUS     1

#define VIDEO_DEVICE          "/dev/bktr0"
#define TUNER_DEVICE          "/dev/tuner0"

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
#ifndef WITHOUT_V4L
     int capture_method;
    int v4l_fmt;
    unsigned char *v4l_buffers[2];
    int v4l_curbuffer;
    int v4l_maxbuffer;
    int v4l_bufsize;
#endif
};

/* video functions, video_freebsd.c */
int vid_start(struct context *);
int vid_next(struct context *, unsigned char *);
void vid_close(struct context *);

#ifndef WITHOUT_V4L
void vid_init(void);
void vid_cleanup(void);
#endif

#endif /* _INCLUDE_VIDEO_FREEBSD_H */
