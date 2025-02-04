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
#ifndef _INCLUDE_ROTATE_HPP_
#define _INCLUDE_ROTATE_HPP_

enum FLIP_TYPE {
    FLIP_TYPE_NONE,
    FLIP_TYPE_HORIZONTAL,
    FLIP_TYPE_VERTICAL
};

class cls_rotate {
    public:
        cls_rotate(cls_camera *p_cam);
        ~cls_rotate();

        void process(ctx_image_data *img_data);

    private:
        cls_camera *cam;

        u_char *buffer_norm; /* Temp low res buffer for 90 and 270 degrees rotation */
        u_char *buffer_high; /* Temp high res buffer for 90 and 270 degrees rotation */
        int degrees;                /* Degrees to rotate;  */
        enum FLIP_TYPE axis;        /* Rotate image over the Horizontal or Vertical axis. */

        int capture_width_norm;     /* Capture width of normal resolution image */
        int capture_height_norm;    /* Capture height of normal resolution image */

        int capture_width_high;     /* Capture width of high resolution image */
        int capture_height_high;    /* Capture height of high resolution image */

        void reverse_inplace_quad(u_char *src, int size);
        void flip_inplace_horizontal(u_char *src, int width, int height);
        void flip_inplace_vertical(u_char *src, int width, int height);
        void rot90cw(u_char *src, u_char *dst, int size, int width, int height);
        void rot90ccw(u_char *src, u_char *dst, int size, int width, int height);


};

#endif /* _INCLUDE_ROTATE_HPP_ */
