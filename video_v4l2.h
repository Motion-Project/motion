/*    video_v4l2.h
 *
 *    Include file for video_v4l2.c
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_VIDEO_V4L2_H
#define _INCLUDE_VIDEO_V4L2_H

void v4l2_mutex_init(void);
void v4l2_mutex_destroy(void);

int v4l2_start(struct context *cnt);
int v4l2_next(struct context *cnt, unsigned char *map);
void v4l2_cleanup(struct context *cnt);

#endif /* _INCLUDE_VIDEO_V4L2_H */
