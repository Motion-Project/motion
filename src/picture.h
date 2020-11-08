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
 *    picture.h
 *
 *      Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      Portions of this file are Copyright by Lionnel Maugis
 *
 */
#ifndef _INCLUDE_PICTURE_H_
#define _INCLUDE_PICTURE_H_

void overlay_smartmask(struct context *cnt, unsigned char *out);
void overlay_fixed_mask(struct context *cnt, unsigned char *out);
void put_fixed_mask(struct context *cnt, const char *file);
void overlay_largest_label(struct context *cnt, unsigned char *out);
int put_picture_memory(struct context *cnt, unsigned char* dest_image, int image_size
            , unsigned char *image, int quality, int width, int height);
void put_picture(struct context *cnt, char *file, unsigned char *image, int ftype);
unsigned char *get_pgm(FILE *picture, int width, int height);
void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst);
unsigned prepare_exif(unsigned char **exif, const struct context *cnt
            , const struct timeval *tv_in1, const struct coord *box);

#endif /* _INCLUDE_PICTURE_H_ */
