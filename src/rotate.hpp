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
 */
#ifndef _INCLUDE_ROTATE_H
#define _INCLUDE_ROTATE_H

    struct ctx_cam;
/**
 * rotate_init
 *
 *  Sets up rotation data by allocating a temporary buffer for 90/270 degrees
 *  rotation, and by determining the right rotate-180-degrees function.
 *
 * Parameters:
 *
 *  cam - current thread's context structure
 *
 * Returns: nothing
 */
void rotate_init(struct ctx_cam *cam);

/**
 * rotate_deinit
 *
 *  Frees memory allocated by rotate_init.
 *
 * Parameters:
 *
 *   cam - current thread's context structure
 */
void rotate_deinit(struct ctx_cam *cam);

/**
 * rotate_map
 *
 *  Rotates the image stored in img according to the rotation data
 *  available in cam. Rotation is performed clockwise. Supports 90,
 *  180 and 270 degrees rotation. 180 degrees rotation is performed
 *  in-place by simply reversing the image data, which is a very
 *  fast operation. 90 and 270 degrees rotation are performed using
 *  a temporary buffer and a somewhat more complicated algorithm,
 *  which makes them slower.
 *
 *  Note that to the caller, all rotations will seem as they are
 *  performed in-place.
 *
 * Parameters:
 *
 *   img_data - the image data to rotate
 *   cam - current thread's context structure
 *
 * Returns:
 *
 *   0  - success
 *   -1 - failure (rare, shouldn't happen)
 */
/* Contains data for image rotation, see rotate.c. */
struct ctx_rotate {

    unsigned char *buffer_norm;  /* Temporary buffer for 90 and 270 degrees rotation of normal resolution image. */
    unsigned char *buffer_high;  /* Temporary buffer for 90 and 270 degrees rotation of high resolution image. */
    int degrees;              /* Degrees to rotate; copied from conf.rotate_deg. */
    enum FLIP_TYPE axis;      /* Rotate image over the Horizontal or Vertical axis. */

    int capture_width_norm;            /* Capture width of normal resolution image */
    int capture_height_norm;           /* Capture height of normal resolution image */

    int capture_width_high;            /* Capture width of high resolution image */
    int capture_height_high;           /* Capture height of high resolution image */

};

int rotate_map(struct ctx_cam *cam, struct ctx_image_data *img_data);

#endif
