
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

#ifndef _INCLUDE_ALG_SEC_HPP_
#define _INCLUDE_ALG_SEC_HPP_

#ifdef HAVE_OPENCV
    #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wconversion"
        #include "opencv2/objdetect.hpp"
        #include "opencv2/dnn.hpp"
        #include "opencv2/highgui.hpp"
        #include "opencv2/imgproc.hpp"
    #pragma GCC diagnostic pop
#endif


struct ctx_algsec_model {
    std::string                 config;             //Source params line
    ctx_params                  *params;

    std::string                 model_file;
    int                         frame_interval;

    std::string                 method;
    std::string                 image_type;
    int                         rotate;

    double                      scalefactor;
    double                      threshold;          /* Threshold for motion to use on detection*/

    double                      hog_threshold_model;  /* Threshold fed into the opencv model*/
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

    bool                        isdetected;         /* Bool reset for each image as to whether a detection occurred */

    #ifdef HAVE_OPENCV
        cv::CascadeClassifier   haar_cascade;       /*Haar Cascade (if applicable) */
        cv::HOGDescriptor       hog;
        cv::dnn::Net            net;
    #endif
};

struct ctx_algsec {
    pthread_t               threadid;        /* thread i.d. for a secondary detection thread (if required). */
    volatile bool           thread_running;
    volatile bool           closing;
    volatile bool           detecting;
    int                     frame_cnt;
    int                     frame_missed;
    int                     too_slow;
    unsigned char           *image_norm;
    int                     width;
    int                     height;
    bool                    isdetected;         /* Bool reset for each Motion event as to whether a detection occurred */
    pthread_mutex_t         mutex;
    ctx_algsec_model        models;
};


void algsec_detect(ctx_dev *cam);
void algsec_init(ctx_dev *cam);
void algsec_deinit(ctx_dev *cam);

#endif /*_INCLUDE_ALG_SEC_HPP_*/