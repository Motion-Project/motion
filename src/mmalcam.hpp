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
 *    Copyright 2013 by Nicholas Tuckett
 */

#ifndef MMALCAM_H_
#define MMALCAM_H_

    typedef struct ctx_mmalcam *ctx_mmalcam_ptr;

    typedef struct ctx_mmalcam {
        struct ctx_cam *cam;        /* pointer to parent motion
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

    int mmalcam_start (struct ctx_cam *cam);
    int mmalcam_next (struct ctx_cam *cam, struct ctx_image_data *img_data);
    void mmalcam_cleanup (struct ctx_mmalcam *mmalcam);

#endif /* MMALCAM_H_ */
