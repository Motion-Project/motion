/*
 *    This file is part of MotionPlus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    MotionPlus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
*/

#ifndef _INCLUDE_VIDEO_COMMON_H
#define _INCLUDE_VIDEO_COMMON_H

void vid_yuv422to420p(unsigned char *img_dest, unsigned char *img_src, int width, int height);
void vid_yuv422pto420p(unsigned char *img_dest, unsigned char *img_src, int width, int height);
void vid_uyvyto420p(unsigned char *img_dest, unsigned char *img_src, int width, int height);
void vid_rgb24toyuv420p(unsigned char *img_dest, unsigned char *img_src, int width, int height);
void vid_bayer2rgb24(unsigned char *img_dst, unsigned char *img_src, long int width, long int height);
void vid_y10torgb24(unsigned char *img_dest, unsigned char *img_src, int width, int height, int shift);
void vid_greytoyuv420p(unsigned char *img_dest, unsigned char *img_src, int width, int height);
int vid_sonix_decompress(unsigned char *img_dest, unsigned char *img_src, int width, int height);
int vid_mjpegtoyuv420p(unsigned char *img_dest, unsigned char *img_src, int width, int height, unsigned int size);

#endif
