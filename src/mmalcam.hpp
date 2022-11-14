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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
 *    Copyright 2013 by Nicholas Tuckett
 */

#ifndef _INCLUDE_MMALCAM_HPP_
#define _INCLUDE_MMALCAM_HPP_

    typedef struct ctx_mmalcam *ctx_mmalcam_ptr;

    typedef struct ctx_mmalcam {
        ctx_cam *cam;        /* pointer to parent motion
                                    context structure */
        int width;
        int height;
        int framerate;
        #ifdef HAVE_MMAL
            struct MMAL_COMPONENT_T *camera_component;
            struct MMAL_PORT_T *camera_capture_port;
            struct MMAL_POOL_T *camera_buffer_pool;
            struct MMAL_QUEUE_T *camera_buffer_queue;
            struct raspicam_camera_parameters_s *camera_parameters;
        #endif
    } ctx_mmalcam;

    void mmalcam_start (ctx_cam *cam);
    int mmalcam_next (ctx_cam *cam, ctx_image_data *img_data);
    void mmalcam_cleanup (ctx_cam *cam);

#endif /* _INCLUDE_MMALCAM_HPP_ */
