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
 * video_bktr.h
 *
 *    Include file for video_bktr.c
 *    Copyright 2004 by Angel Carpintero (motiondevelop@gmail.com)
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
