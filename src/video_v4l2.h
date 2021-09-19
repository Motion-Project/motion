/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 *  video_v4l2.h
 *    Headers associated with functions in the video_v4l2.c module.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
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
