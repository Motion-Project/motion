/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 *  mmalcam.h
 *    Headers associated with functions in the mmalcam.c module.
 *    Copyright 2013 by Nicholas Tuckett
 */

#ifndef MMALCAM_H_
#define MMALCAM_H_

typedef struct mmalcam_context *mmalcam_context_ptr;

typedef struct mmalcam_context {
    struct context *cnt;        /* pointer to parent motion
                                context structure */
    int width;
    int height;
    int framerate;

    struct MMAL_COMPONENT_T *camera_component;
    struct MMAL_PORT_T *camera_capture_port;
    struct MMAL_POOL_T *camera_buffer_pool;
    struct MMAL_QUEUE_T *camera_buffer_queue;
    struct raspicam_camera_parameters_s *camera_parameters;
} mmalcam_context;

int mmalcam_start(struct context *cnt);
int mmalcam_next(struct context *cnt,  struct image_data *img_data);
void mmalcam_cleanup(struct mmalcam_context *mmalcam);

#endif /* MMALCAM_H_ */
