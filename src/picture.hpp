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
 *
*/
#ifndef _INCLUDE_PICTURE_HPP_
#define _INCLUDE_PICTURE_HPP_

#ifdef HAVE_WEBP
    #include <webp/encode.h>
    #include <webp/mux.h>
#endif /* HAVE_WEBP */

class cls_picture {
    public:
        cls_picture(ctx_dev *p_cam);
        ~cls_picture();


        int put_memory(u_char* img_dst
            , int image_size, u_char *image, int quality, int width, int height);
        void save_norm( char *file, u_char *image);
        void save_roi( char *file, u_char *image);
        u_char *load_pgm(FILE *picture, int width, int height);
        void scale_img(int width_src, int height_src, u_char *img_src, u_char *img_dst);
        void save_preview();

    private:
        ctx_dev *cam;

        bool stream_grey;
        std::string picture_type;
        std::string mask_file;
        std::string mask_privacy;
        int cfg_w;
        int cfg_h;
        int picture_quality;


        #ifdef HAVE_WEBP
            void webp_exif(WebPMux* webp_mux
                , timespec *ts1, ctx_coord *box);
        #endif
        void save_webp(FILE *fp, u_char *image
            , int width, int height, int quality
            , timespec *ts1, ctx_coord *box);
        void save_yuv420p(FILE *fp, u_char *image
            , int width, int height, int quality
            , timespec *ts1, ctx_coord *box);
        void save_grey(FILE *picture, u_char *image
            , int width, int height, int quality
            , timespec *ts1, ctx_coord *box);
        void save_ppm(FILE *picture, u_char *image, int width, int height);
        void pic_write(FILE *picture, u_char *image, int quality);
        void write_mask(const char *file);
        void init_privacy();
        void init_mask();

};


#endif /* _INCLUDE_PICTURE_HPP_ */
