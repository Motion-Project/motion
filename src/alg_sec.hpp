
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


//example source config line:  human:75:25,car:60:30:4:15:A:1.012:3,dog:90:70
struct ctx_algsec_model {
    int                         method;
    std::string                 modelfile;          //Source model file
    std::string                 config;             //Source config line
    std::string                 parms[3][10];       //Parms as strings
    int                         parms_int[3][10];   //Parms convert to int
    float                       parms_float[3][10]; //Parms converted to float
    #ifdef HAVE_OPENCV
        cv::CascadeClassifier   haar_cascade;       //Haar Cascade (if applicable)
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
    struct ctx_algsec_model models[3];
};


void algsec_detect(ctx_cam *cam);
void algsec_init(ctx_cam *cam);
void algsec_deinit(ctx_cam *cam);

#endif