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
                cls_libcam(ctx_dev *p_cam);
                ~cls_libcam();
                int next(ctx_image_data *img_data);
            private:
                ctx_dev     *cam;
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

                std::string             conf_libcam_params;
                std::string             conf_libcam_device;
                int                     conf_width;
                int                     conf_height;

                void log_orientation();
                void log_controls();
                void log_draft();

                int libcam_start();
                void libcam_stop();

                void start_params();
                int start_mgr();
                int start_config();
                int start_req();
                int start_capture();
                void config_orientation();
                void config_controls();
                void config_control_item(std::string pname, std::string pvalue);
                void req_complete(libcamera::Request *request);
                int req_add(libcamera::Request *request);


        };
    #else
        #define LIBCAMVER 0
        class cls_libcam {
            public:
                cls_libcam(ctx_dev *p_cam);
                ~cls_libcam();
                int next(ctx_image_data *img_data);
        };
    #endif

#endif /* _INCLUDE_LIBCAM_HPP_ */
