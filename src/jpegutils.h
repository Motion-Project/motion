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
 * jpegutils.h: Some Utility programs for dealing with
 *               JPEG encoded images
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *  Copyright (C) 2008 Angel Carpintero <motiondevelop@gmail.com>
 *
 */

#ifndef __JPEGUTILS_H__
#define __JPEGUTILS_H__

int jpgutl_decode_jpeg (unsigned char *jpeg_data_in, int jpeg_data_len
            , unsigned int width, unsigned int height, unsigned char *volatile img_out);
int jpgutl_put_yuv420p(unsigned char *dest_image, int image_size, unsigned char *input_image, int width
            , int height, int quality, struct context *cnt, struct timeval *tv1, struct coord *box);
int jpgutl_put_grey(unsigned char *dest_image, int image_size, unsigned char *input_image, int width
            , int height, int quality, struct context *cnt, struct timeval *tv1, struct coord *box);

#endif
