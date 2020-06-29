/*
 *    This file is part of Motionplus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motionplus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motionplus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
 *
*/
#ifndef _INCLUDE_PICTURE_H_
#define _INCLUDE_PICTURE_H_

    struct ctx_cam;

    int pic_put_memory(struct ctx_cam *cam, unsigned char* dest_image
        , int image_size, unsigned char *image, int quality, int width, int height);
    void pic_save_norm(struct ctx_cam *cam, char *file, unsigned char *image, int ftype);
    void pic_save_roi(struct ctx_cam *cam, char *file, unsigned char *image);
    unsigned char *pic_load_pgm(FILE *picture, int width, int height);
    void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst);
    void pic_save_preview(struct ctx_cam *cam, struct ctx_image_data *img);
    void pic_init_privacy(struct ctx_cam *cam);
    void pic_init_mask(struct ctx_cam *cam);

#endif /* _INCLUDE_PICTURE_H_ */
