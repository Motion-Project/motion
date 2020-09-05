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
 *
*/


#ifndef __JPEGUTILS_H__
#define __JPEGUTILS_H__

    int jpgutl_decode_jpeg (unsigned char *jpeg_data_in, int jpeg_data_len,
                        unsigned int width, unsigned int height, unsigned char *volatile img_out);

    int jpgutl_put_yuv420p(unsigned char *, int image, unsigned char *, int, int, int, struct ctx_cam *cam, struct timespec *, struct ctx_coord *);
    int jpgutl_put_grey(unsigned char *, int image, unsigned char *, int, int, int, struct ctx_cam *cam, struct timespec *, struct ctx_coord *);

#endif
