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

void bktr_mutex_init(void);
void bktr_mutex_destroy(void);

int bktr_start(struct context *cnt);
int bktr_next(struct context *cnt,  struct image_data *img_data);
void bktr_cleanup(struct context *cnt);

#endif /* _INCLUDE_VIDEO_FREEBSD_H */
