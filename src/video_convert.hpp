/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
*/

#ifndef _INCLUDE_VIDEO_COMMON_HPP_
#define _INCLUDE_VIDEO_COMMON_HPP_

typedef struct {
    int is_abs;
    int len;
    int val;
} sonix_table;

class cls_convert {
    public:
        cls_convert(cls_camera *p_cam, int p_pix, int p_w, int p_h);
        ~cls_convert();
        int process(u_char *img_dest, u_char *img_src, int clen);

    private:
        cls_camera *cam;

        int width;
        int height;
        int pixfmt_src;
        u_char  *common_buffer;


        void sonix_decompress_init(sonix_table *table);
        void rgb_bgr(u_char *img_dst, u_char *img_src, int rgb);

        void yuv422to420p(u_char *img_dest, u_char *img_src);
        void yuv422pto420p(u_char *img_dest, u_char *img_src);
        void uyvyto420p(u_char *img_dest, u_char *img_src);
        void rgb24toyuv420p(u_char *img_dest, u_char *img_src);
        void bgr24toyuv420p(u_char *img_dest, u_char *img_src);
        void bayer2rgb24(u_char *img_dst, u_char *img_src);
        void y10torgb24(u_char *img_dest, u_char *img_src, int shift);
        void greytoyuv420p(u_char *img_dest, u_char *img_src);
        int sonix_decompress(u_char *img_dest, u_char *img_src);
        int mjpegtoyuv420p(u_char *img_dest, u_char *img_src, int size);


};


#endif /* _INCLUDE_VIDEO_COMMON_HPP_ */
