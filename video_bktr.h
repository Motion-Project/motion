/*
 * video_bktr.h
 *
 *    Include file for video_bktr.c
 *    Copyright 2004 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_VIDEO_BKTR_H
#define _INCLUDE_VIDEO_BKTR_H

#ifdef HAVE_BKTR

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/bt8xx.h>
#else
#include <dev/bktr/ioctl_meteor.h>
#include <dev/bktr/ioctl_bt848.h>
#endif

#endif /* HAVE_BKTR */

#define array_elem(x) (sizeof(x) / sizeof((x)[0]))

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

void bktr_mutex_init(void);
void bktr_mutex_destroy(void);

int bktr_start(struct context *cnt);
int bktr_next(struct context *cnt, unsigned char *map);
void bktr_cleanup(struct context *cnt);

#endif /* _INCLUDE_VIDEO_FREEBSD_H */
