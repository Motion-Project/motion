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
 */

#ifndef _INCLUDE_LIBCAM_HPP_
#define _INCLUDE_LIBCAM_HPP_
    #ifdef HAVE_LIBCAM
        #include <queue>
        #include <sys/mman.h>
        #include <libcamera/libcamera.h>

        using namespace libcamera;

        /* Buffers and sizes for planes of image*/
        struct ctx_imgmap {
            uint8_t *buf;
            int     bufsz;
        };

        class cls_libcam {
            public:
                cls_libcam(){};
                ~cls_libcam(){};
                int cam_start(ctx_cam *cam);
                void cam_stop();
                int cam_next(ctx_image_data *img_data);
            private:
                ctx_cam     *camctx;
                ctx_params  *params;

                std::unique_ptr<CameraManager>          cam_mgr;
                std::shared_ptr<Camera>                 camera;
                std::unique_ptr<CameraConfiguration>    config;
                std::unique_ptr<FrameBufferAllocator>   frmbuf;
                std::vector<std::unique_ptr<Request>>   requests;

                std::queue<Request *>   req_queue;
                ControlList             controls;
                ctx_imgmap              membuf;
                bool                    started_cam;
                bool                    started_mgr;
                bool                    started_aqr;
                bool                    started_req;

                void cam_start_params(ctx_cam *ptr);
                int cam_start_mgr();
                int cam_start_config();
                int cam_start_req();
                int cam_start_capture();
                void req_complete(Request *request);
                int req_add(Request *request);
        };
    #else
        class cls_libcam {
            public:
                cls_libcam(){};
                ~cls_libcam(){};
        };
    #endif

    int libcam_start (ctx_cam *cam);
    int libcam_next (ctx_cam *cam, ctx_image_data *img_data);
    void libcam_cleanup (ctx_cam *cam);

#endif /* _INCLUDE_LIBCAM_HPP_ */
