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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 */
#ifndef _INCLUDE_ROTATE_HPP_
#define _INCLUDE_ROTATE_HPP_

    struct ctx_cam;

    struct ctx_rotate {
        unsigned char *buffer_norm; /* Temp low res buffer for 90 and 270 degrees rotation */
        unsigned char *buffer_high; /* Temp high res buffer for 90 and 270 degrees rotation */
        int degrees;                /* Degrees to rotate; copied from conf.rotate_deg. */
        enum FLIP_TYPE axis;        /* Rotate image over the Horizontal or Vertical axis. */

        int capture_width_norm;     /* Capture width of normal resolution image */
        int capture_height_norm;    /* Capture height of normal resolution image */

        int capture_width_high;     /* Capture width of high resolution image */
        int capture_height_high;    /* Capture height of high resolution image */
    };

    void rotate_init(struct ctx_cam *cam);
    void rotate_deinit(struct ctx_cam *cam);
    int rotate_map(struct ctx_cam *cam, struct ctx_image_data *img_data);

#endif /* _INCLUDE_ROTATE_HPP_ */
