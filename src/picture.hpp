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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 *
*/
#ifndef _INCLUDE_PICTURE_HPP_
#define _INCLUDE_PICTURE_HPP_

    struct ctx_dev;

    int pic_put_memory(struct ctx_dev *cam, unsigned char* dest_image
        , int image_size, unsigned char *image, int quality, int width, int height);
    void pic_save_norm(struct ctx_dev *cam, char *file, unsigned char *image, int ftype);
    void pic_save_roi(struct ctx_dev *cam, char *file, unsigned char *image);
    unsigned char *pic_load_pgm(FILE *picture, int width, int height);
    void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst);
    void pic_save_preview(struct ctx_dev *cam);
    void pic_init_privacy(struct ctx_dev *cam);
    void pic_init_mask(struct ctx_dev *cam);

#endif /* _INCLUDE_PICTURE_HPP_ */
