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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
 */

#ifndef _INCLUDE_LIBCAM_HPP_
#define _INCLUDE_LIBCAM_HPP_
    #ifdef HAVE_LIBCAM
        #include <queue>
        #include <sys/mman.h>
        #include <libcamera/libcamera.h>

        #define LIBCAMVER (LIBCAMERA_VERSION_MAJOR * 1000000)+(LIBCAMERA_VERSION_MINOR* 1000) + LIBCAMERA_VERSION_PATCH

        /* Buffers and sizes for planes of image*/
        struct ctx_imgmap {
            uint8_t *buf;
            int     bufsz;
        };

        class cls_libcam {
            public:
                cls_libcam(){};
                ~cls_libcam(){};
                int cam_start(ctx_dev *cam);
                void cam_stop();
                int cam_next(ctx_image_data *img_data);
            private:
                ctx_dev     *camctx;
                ctx_params  *params;

                std::unique_ptr<libcamera::CameraManager>          cam_mgr;
                std::shared_ptr<libcamera::Camera>                 camera;
                std::unique_ptr<libcamera::CameraConfiguration>    config;
                std::unique_ptr<libcamera::FrameBufferAllocator>   frmbuf;
                std::vector<std::unique_ptr<libcamera::Request>>   requests;

                std::queue<libcamera::Request *>   req_queue;
                libcamera::ControlList             controls;
                ctx_imgmap              membuf;
                bool                    started_cam;
                bool                    started_mgr;
                bool                    started_aqr;
                bool                    started_req;

                void cam_log_orientation();
                void cam_log_controls();
                void cam_log_draft();

                void cam_start_params(ctx_dev *ptr);
                int cam_start_mgr();
                int cam_start_config();
                int cam_start_req();
                int cam_start_capture();
                void cam_config_orientation();
                void cam_config_controls();
                void req_complete(libcamera::Request *request);
                int req_add(libcamera::Request *request);
                bool cam_parm_bool(char *parm);
                float cam_parm_single(char *parm);
                void cam_config_control_item(char *pmm, char *pval);
        };
    #else
        #define LIBCAMVER 0
        class cls_libcam {
            public:
                cls_libcam(){};
                ~cls_libcam(){};
        };
    #endif

    void libcam_start (ctx_dev *cam);
    int libcam_next (ctx_dev *cam, ctx_image_data *img_data);
    void libcam_cleanup (ctx_dev *cam);

#endif /* _INCLUDE_LIBCAM_HPP_ */
