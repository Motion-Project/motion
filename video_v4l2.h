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
int v4l2_next(struct context *cnt,  struct image_data *img_data);
void v4l2_cleanup(struct context *cnt);
int v4l2_palette_valid(char *video_device, int v4l2_palette);
int v4l2_parms_valid(char *video_device, int v4l2_palette, int v4l2_fps, int v4l2_width, int v4l2_height);
void v4l2_palette_fourcc(int v4l2_palette, char *fourcc);

#endif /* _INCLUDE_VIDEO_V4L2_H */
