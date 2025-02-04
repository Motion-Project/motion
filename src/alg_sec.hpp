
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

#ifndef _INCLUDE_ALG_SEC_HPP_
#define _INCLUDE_ALG_SEC_HPP_

#ifdef HAVE_OPENCV
    #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wconversion"
        #include <opencv2/objdetect.hpp>
        #include <opencv2/dnn.hpp>
        #include <opencv2/highgui.hpp>
        #include <opencv2/imgproc.hpp>
        #include <opencv2/imgcodecs.hpp>
        #include <opencv2/videoio.hpp>
        #include <opencv2/video.hpp>
    #pragma GCC diagnostic pop
#endif

class cls_algsec {
    public:
        cls_algsec(cls_camera *p_cam);
        ~cls_algsec();

        void        detect();
        bool        detected;

        std::string method;
        pthread_mutex_t mutex;

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();

    private:
        #ifdef HAVE_OPENCV
            cls_camera      *cam;
            bool            in_process;
            bool            is_started;
            bool            first_pass;
            int             frame_cnt;
            int             frame_missed;
            int             too_slow;
            u_char          *image_norm;
            int             width;
            int             height;
            int             cfg_framerate;
            int             cfg_log_level;
            std::string     cfg_target_dir;

            void handler_startup();
            void handler_shutdown();

            void load_params();
            void params_defaults();
            void params_model();
            void params_log();

            void load_dnn();
            void load_haar();
            void load_hog();
            void detect_dnn();
            void detect_haar();
            void detect_hog();
            void get_image(cv::Mat &mat_dst);
            void get_image_roi(cv::Mat &mat_src, cv::Mat &mat_dst);
            void label_image(cv::Mat &mat_dst, double confidence, cv::Point classIdPoint);
            void label_image(cv::Mat &mat_dst, std::vector<cv::Rect> &src_pos
                , std::vector<double> &src_weights);
            void image_show(cv::Mat &mat_dst);
            void debug_notice(cv::Mat &mat_dst,bool isdetect);

            std::string                 config;
            ctx_params                  *params;

            std::string                 model_file;
            int                         frame_interval;
            std::string                 image_type;
            int                         rotate;

            double                      scalefactor;
            double                      threshold;

            double                      hog_threshold_model;
            int                         hog_winstride;
            int                         hog_padding;

            int                         haar_minneighbors;
            int                         haar_flags;
            int                         haar_minsize;
            int                         haar_maxsize;

            std::string                 dnn_config;
            std::string                 dnn_framework;
            std::string                 dnn_classes_file;

            int                         dnn_backend;
            int                         dnn_target;
            std::vector<std::string>    dnn_classes;
            int                         dnn_width;
            int                         dnn_height;
            double                      dnn_scale;

            cv::CascadeClassifier   haar_cascade;       /*Haar Cascade (if applicable) */
            cv::HOGDescriptor       hog;
            cv::dnn::Net            net;
        #endif

};

#endif /*_INCLUDE_ALG_SEC_HPP_*/