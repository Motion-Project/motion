/*
 * mmalcam.hpp
 *
 *    Include file for mmalcam.c
 *
 *    Copyright 2013 by Nicholas Tuckett
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
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
