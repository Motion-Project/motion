
/*
 *    alg_sec.hpp
 *
 *      Algorithms for Secondary Detection Header
 *      Copyright 2019
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 *
 */
#ifndef _INCLUDE_ALG_SEC_H
#define _INCLUDE_ALG_SEC_H


#include <string>
#include <iostream>
#ifdef HAVE_OPENCV
    #include "opencv2/objdetect.hpp"
    #include "opencv2/highgui.hpp"
    #include "opencv2/imgproc.hpp"
#endif


struct ctx_algsec_model {
    std::string                 config;             //Source config line

    int                         method;
    std::string                 modelfile;          //Source model file
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
        cv::CascadeClassifier   haar_cascade;       //Haar Cascade (if applicable)
        cv::HOGDescriptor       hog;
    #endif
};

struct ctx_algsec {
    pthread_t               threadid;        /* thread i.d. for a secondary detection thread (if required). */
    volatile bool           closing;
    volatile bool           detecting;
    int                     frame_interval;
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

#endif