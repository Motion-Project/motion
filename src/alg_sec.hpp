
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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 */

#ifndef _INCLUDE_ALG_SEC_HPP_
#define _INCLUDE_ALG_SEC_HPP_


#include <string>
#include <iostream>
#ifdef HAVE_OPENCV
    #include "opencv2/objdetect.hpp"
    #include "opencv2/highgui.hpp"
    #include "opencv2/imgproc.hpp"
#endif


struct ctx_algsec_model {
    std::string                 config;             //Source params line
    struct ctx_params           *algsec_params;


    std::string                 model_file;
    int                         frame_interval;

    std::string                 method;
    std::string                 imagetype;
    int                         rotate;

    float                       scalefactor;
    float                       threshold_model;    /* Threshold fed into the opencv model*/
    float                       threshold_motion;   /* Threshold for motion to use on detection*/

    int                         hog_winstride;
    int                         hog_padding;


    int                         haar_minneighbors;
    int                         haar_flags;
    int                         haar_minsize;
    int                         haar_maxsize;
    bool                        isdetected;         /* Bool reset for each image as to whether a detection occurred */
    #ifdef HAVE_OPENCV
        cv::CascadeClassifier   haar_cascade;       /*Haar Cascade (if applicable) */
        cv::HOGDescriptor       hog;
    #endif
};

struct ctx_algsec {
    pthread_t               threadid;        /* thread i.d. for a secondary detection thread (if required). */
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
    struct ctx_algsec_model models;
};


void algsec_detect(ctx_cam *cam);
void algsec_init(ctx_cam *cam);
void algsec_deinit(ctx_cam *cam);

#endif /*_INCLUDE_ALG_SEC_HPP_*/