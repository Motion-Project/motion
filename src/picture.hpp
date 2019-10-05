/*
 *    picture.hpp
 *
 *      Copyright 2002 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      Portions of this file are Copyright by Lionnel Maugis
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_PICTURE_H_
#define _INCLUDE_PICTURE_H_

    struct ctx_cam;

    int pic_put_memory(struct ctx_cam *cam, unsigned char* dest_image
        , int image_size, unsigned char *image, int quality, int width, int height);
    void pic_save_norm(struct ctx_cam *cam, char *file, unsigned char *image, int ftype);
    unsigned char *pic_load_pgm(FILE *picture, int width, int height);
    void pic_scale_img(int width_src, int height_src, unsigned char *img_src, unsigned char *img_dst);
    void pic_save_preview(struct ctx_cam *cam, struct ctx_image_data *img);
    void pic_init_privacy(struct ctx_cam *cam);
    void pic_init_mask(struct ctx_cam *cam);

#endif /* _INCLUDE_PICTURE_H_ */
