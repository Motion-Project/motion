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

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "alg_sec.hpp"

#ifdef HAVE_OPENCV

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>

using namespace cv;

static void algsec_img_show(ctx_cam *cam, Mat &mat_src
        , std::vector<Rect> &src_pos, std::vector<double> &src_weights
        , ctx_algsec_model &algmdl)
{

    std::vector<Rect> fltr_pos;
    std::vector<double> fltr_weights;
    std::string testdir;
    std::size_t indx0, indx1;
    std::vector<uchar> buff;    //buffer for coding
    std::vector<int> param(2);
    char wstr[10];

    algmdl.isdetected = false;
    for (indx0=0; indx0<src_pos.size(); indx0++) {
        Rect r = src_pos[indx0];
        double w = src_weights[indx0];

        for (indx1=0; indx1<src_pos.size(); indx1++) {
            if (indx1 != indx0 && (r & src_pos[indx1])==r) {
                break;
            }
        }
        if ((indx1==src_pos.size()) && (w > algmdl.threshold_motion)) {
            fltr_pos.push_back(r);
            fltr_weights.push_back(w);
            algmdl.isdetected = true;
        }
    }

    /* We check the size so that we at least fill in the first image so the
     * web stream will have something to start with.  After feeding in at least
     * the first image, we rely upon the connection count to tell us whether we
     * need to expend the CPU to compress and load the secondary images */
    if ((cam->stream.secondary.cnct_count >0) ||
        (cam->imgs.size_secondary == 0) ||
        (cam->motapp->log_level >= DBG)) {

        if (cam->motapp->log_level >= DBG) {
            imwrite(cam->conf->target_dir  + "/src_" + algmdl.method + ".jpg", mat_src);
        }

        if (algmdl.isdetected) {
            for (indx0=0; indx0<fltr_pos.size(); indx0++) {
                Rect r = fltr_pos[indx0];
                r.x += cvRound(r.width*0.1);
                r.width = cvRound(r.width*0.8);
                r.y += cvRound(r.height*0.06);
                r.height = cvRound(r.height*0.9);
                rectangle(mat_src, r.tl(), r.br(), cv::Scalar(0,255,0), 2);
                snprintf(wstr, 10, "%.4f", fltr_weights[indx0]);
                putText(mat_src, wstr, Point(r.x,r.y), FONT_HERSHEY_PLAIN, 1, 255, 1);
            }
            if (cam->motapp->log_level >= DBG) {
                imwrite(cam->conf->target_dir  + "/detect_" + algmdl.method + ".jpg", mat_src);
            }
        }

        param[0] = cv::IMWRITE_JPEG_QUALITY;
        param[1] = 75;
        cv::imencode(".jpg", mat_src, buff, param);
        pthread_mutex_lock(&cam->algsec->mutex);
            std::copy(buff.begin(), buff.end(), cam->imgs.image_secondary);
            cam->imgs.size_secondary = (int)buff.size();
        pthread_mutex_unlock(&cam->algsec->mutex);
    }
}

static void algsec_img_roi(ctx_cam *cam, Mat &mat_src, Mat &mat_dst)
{

    cv::Rect roi;
    int width,height, x, y;

    /* Lets make the box square */

    width = cam->current_image->location.width;
    height= cam->current_image->location.height;

    if (width > cam->imgs.height) {
        width =cam->imgs.height;
    }
    if (height > cam->imgs.width) {
        height =cam->imgs.width;
    }

    if (width > height) {
        height = width;
        x = cam->current_image->location.minx;
        y = cam->current_image->location.miny - ((width - height)/2);

        if (y < 0) {
            y = 0;
        }
        if ((y+height) > cam->imgs.height) {
            y = cam->imgs.height - height;
        }
    } else {
        width = height;
        x = cam->current_image->location.minx - ((height - width)/2);
        y = cam->current_image->location.miny;

        if (x < 0) {
            x = 0;
        }
        if ((x+width) > cam->imgs.width) {
            x = cam->imgs.width - width;
        }
    }

    roi.x = x;
    roi.y = y;
    roi.width = width;
    roi.height = height;

    /*
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Base %d %d (%dx%d) img(%dx%d)"
        ,cam->current_image->location.minx
        ,cam->current_image->location.miny
        ,cam->current_image->location.width
        ,cam->current_image->location.height
        ,cam->imgs.width
        ,cam->imgs.height);
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Set %d %d %d %d"
        ,x,y,width,height);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Opencv %d %d %d %d"
        ,roi.x,roi.y,roi.width,roi.height);
    */

    mat_dst = mat_src(roi);

}

static void algsec_detect_hog(ctx_cam *cam, ctx_algsec_model &algmdl)
{

    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    Mat mat_dst;

    try {
        if (algmdl.imagetype == "color") {
            /* AFAIK, the detector uses grey so users shouldn't really use this*/
            Mat mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            cvtColor(mat_src, mat_dst, COLOR_YUV2RGB_YV12);

        } else if (algmdl.imagetype == "roi") {
            /*Discard really small and large images */
            if ((cam->current_image->location.width < 64) ||
                (cam->current_image->location.height < 64) ||
               ((cam->current_image->location.width/cam->imgs.width) > 0.7) ||
               ((cam->current_image->location.height/cam->imgs.height) > 0.7)) return;

            Mat mat_src = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            algsec_img_roi(cam, mat_src, mat_dst);

        } else {
            mat_dst = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
        }

        equalizeHist(mat_dst, mat_dst);

        algmdl.hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());

        algmdl.hog.detectMultiScale(mat_dst, detect_pos, detect_weights, 0
            ,Size(algmdl.hog_winstride, algmdl.hog_winstride)
            ,Size(algmdl.hog_padding, algmdl.hog_padding)
            ,algmdl.scalefactor
            ,algmdl.threshold_model
            ,false);

        algsec_img_show(cam, mat_dst, detect_pos, detect_weights, algmdl);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl.method = "none";
    }
}

static void algsec_detect_haar(ctx_cam *cam, ctx_algsec_model &algmdl)
{

    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    std::vector<int> levels;
    Mat mat_dst;

    try {
        if (algmdl.imagetype == "color") {
            /* AFAIK, the detector uses grey so users shouldn't really use this*/
            Mat mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            cvtColor(mat_src, mat_dst, COLOR_YUV2RGB_YV12);

        } else if (algmdl.imagetype == "roi") {
            /*Discard really small and large images */
            if ((cam->current_image->location.width < 64) ||
                (cam->current_image->location.height < 64) ||
               ((cam->current_image->location.width/cam->imgs.width) > 0.7) ||
               ((cam->current_image->location.height/cam->imgs.height) > 0.7)) return;

            Mat mat_src = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            algsec_img_roi(cam, mat_src, mat_dst);

        } else {
            mat_dst = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
        }

        equalizeHist(mat_dst, mat_dst);

        algmdl.haar_cascade.detectMultiScale(
            mat_dst, detect_pos, levels, detect_weights
            ,algmdl.scalefactor, algmdl.haar_minneighbors,algmdl.haar_flags
            , Size(algmdl.haar_minsize,algmdl.haar_minsize)
            , Size(algmdl.haar_maxsize,algmdl.haar_maxsize), true);

        algsec_img_show(cam, mat_dst, detect_pos, detect_weights, algmdl);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl.method = "none";
    }
}

static void algsec_load_haar(ctx_algsec_model &algmdl)
{
    /* If loading fails, reset the method to invalidate detection */
    try {
        if (algmdl.model_file == "") {
            algmdl.method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("No secondary model specified."));
            return;
        }
        if (!algmdl.haar_cascade.load(algmdl.model_file)) {
            /* Loading failed, reset method*/
            algmdl.method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
                ,algmdl.model_file.c_str());
        };
    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s"), algmdl.model_file.c_str());
        algmdl.method = "none";
    }
}

static void algsec_params_log(ctx_algsec_model &algmdl)
{
    int indx;

    for (indx = 0; indx < algmdl.algsec_params->params_count; indx++) {
        motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s %s"
            ,algmdl.algsec_params->params_array[indx].param_name
            ,algmdl.algsec_params->params_array[indx].param_value);
    }

}

static void algsec_params_model(ctx_algsec_model &algmdl)
{
    /* To avoid looping through the parms for each image detection to get
     * the parameters, we put them into variables easily found by the model.
     * As the secondary method processing gets refined, this method will need
     * to be adjusted to be something more efficient.
     */
    int indx;
    for (indx = 0; indx < algmdl.algsec_params->params_count; indx++) {
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"model_file")) {
            algmdl.model_file = algmdl.algsec_params->params_array[indx].param_value;
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"frame_interval")) {
            algmdl.frame_interval = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"imagetype")) {
            algmdl.imagetype = algmdl.algsec_params->params_array[indx].param_value;
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"threshold_motion")) {
            algmdl.threshold_motion = atof(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"threshold_model")) {
            algmdl.threshold_model = atof(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"scalefactor")) {
            algmdl.scalefactor = atof(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"rotate")) {
            algmdl.rotate = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"hog_padding")) {
            algmdl.hog_padding = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"hog_winstride")) {
            algmdl.hog_winstride = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"haar_flags")) {
            algmdl.haar_flags = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"haar_maxsize")) {
            algmdl.haar_maxsize = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"haar_minsize")) {
            algmdl.haar_minsize = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
        if (mystreq(algmdl.algsec_params->params_array[indx].param_name,"haar_minneighbors")) {
            algmdl.haar_minneighbors = atoi(algmdl.algsec_params->params_array[indx].param_value);
        }
    }
}

static void algsec_params_defaults(ctx_algsec_model &algmdl)
{
    util_parms_add_default(algmdl.algsec_params, "model_file", "");

    util_parms_add_default(algmdl.algsec_params, "frame_interval", "5");
    util_parms_add_default(algmdl.algsec_params, "imagetype", "full");

    util_parms_add_default(algmdl.algsec_params, "threshold_motion", "1.1");
    if (algmdl.method == "haar") {
        util_parms_add_default(algmdl.algsec_params, "threshold_model", "1.4");
        util_parms_add_default(algmdl.algsec_params, "scalefactor", "1.1");
    } else {
        util_parms_add_default(algmdl.algsec_params, "threshold_model", "2");
        util_parms_add_default(algmdl.algsec_params, "scalefactor", "1.05");
    }
    util_parms_add_default(algmdl.algsec_params, "rotate", "0");

    util_parms_add_default(algmdl.algsec_params, "hog_padding", "8");
    util_parms_add_default(algmdl.algsec_params, "hog_winstride", "8");

    util_parms_add_default(algmdl.algsec_params, "haar_flags", "0");
    util_parms_add_default(algmdl.algsec_params, "haar_maxsize", "1024");
    util_parms_add_default(algmdl.algsec_params, "haar_minsize", "8");
    util_parms_add_default(algmdl.algsec_params, "haar_minneighbors", "8");
}

static void algsec_params_deinit(ctx_algsec_model &algmdl)
{
    if (algmdl.algsec_params != NULL){
        util_parms_free(algmdl.algsec_params);
        if (algmdl.algsec_params != NULL) {
            free(algmdl.algsec_params);
        }
        algmdl.algsec_params = NULL;
    }
}

static void algsec_params_init(ctx_algsec_model &algmdl)
{
    algmdl.algsec_params = (struct ctx_params*) mymalloc(sizeof(struct ctx_params));
    memset(algmdl.algsec_params, 0, sizeof(struct ctx_params));
    algmdl.algsec_params->params_array = NULL;
    algmdl.algsec_params->params_count = 0;
    algmdl.algsec_params->update_params = true;     /*Set trigger to update parameters */
}

/**Load the parms from the config to algsec struct */
static int algsec_load_params(ctx_cam *cam)
{
    cam->algsec->height = cam->imgs.height;
    cam->algsec->width = cam->imgs.width;
    cam->algsec->models.method = cam->conf->secondary_method;

    cam->algsec->image_norm = (unsigned char*)mymalloc(cam->imgs.size_norm);
    cam->algsec->frame_missed = 0;
    cam->algsec->too_slow = 0;
    cam->algsec->detecting = false;

    /* We need to set the closing to true so that we can
    * know whether to shutdown the handler when we deinit
    */
    cam->algsec->closing = true;

    algsec_params_init(cam->algsec->models);

    util_parms_parse(cam->algsec->models.algsec_params, cam->conf->secondary_params);

    algsec_params_defaults(cam->algsec->models);

    algsec_params_log(cam->algsec->models);

    algsec_params_model(cam->algsec->models);

    cam->algsec->frame_cnt = cam->algsec->models.frame_interval;

    return 0;
}

/**If possible preload the models and initialize them */
static int algsec_load_models(ctx_cam *cam)
{

    if (cam->algsec->models.method == "haar") {
        algsec_load_haar(cam->algsec->models);
    } else if (cam->algsec->models.method == "hog") {
        //algsec_load_hog(cam->algsec->models);
    } else {
        cam->algsec->models.method = "none";
    }

    /* If model fails to load, the method is changed to none*/
    if (cam->algsec->models.method != "none"){
        cam->algsec_inuse = true;
        return 0;
    } else {
        cam->algsec_inuse = false;
        return -1;
    }

}

/**Detection thread processing loop */
static void *algsec_handler(void *arg)
{
    ctx_cam *cam = (ctx_cam*)arg;
    long interval;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Starting."));

    cam->algsec->closing = false;

    interval = 1000000L / cam->conf->framerate;

    while (!cam->algsec->closing){
        if (cam->algsec->detecting){
            if (cam->algsec->models.method == "haar") {
                algsec_detect_haar(cam, cam->algsec->models);
            } else if (cam->algsec->models.method == "hog") {
                algsec_detect_hog(cam, cam->algsec->models);
            }
            cam->algsec->detecting = false;
            /*Set the event based isdetected bool */
            if (cam->algsec->models.isdetected) {
                cam->algsec->isdetected = true;
            }
        } else {
            SLEEP(0,interval)
        }
    }
    cam->algsec->closing = false;
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Exiting."));
    pthread_exit(NULL);

}

/**Start the detection thread*/
static void algsec_start_handler(ctx_cam *cam)
{
    int retcd;
    pthread_attr_t handler_attribute;

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    retcd = pthread_create(&cam->algsec->threadid, &handler_attribute, &algsec_handler, cam);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("Error starting algsec handler thread"));
        cam->algsec->models.method = "none";
    }
    pthread_attr_destroy(&handler_attribute);
    return;
}

#endif

void algsec_init(ctx_cam *cam)
{
    /*
     * This function parses out and initializes the parameters
     * associated with the secondary detection algorithm if a
     * secondary detection method has been requested.
    */
    #ifdef HAVE_OPENCV
        int retcd;

        mythreadname_set("cv",cam->threadnr,cam->conf->camera_name.c_str());

        cam->algsec = new ctx_algsec;

        pthread_mutex_init(&cam->algsec->mutex, NULL);

        retcd = algsec_load_params(cam);
        if (retcd == 0) retcd = algsec_load_models(cam);
        if (retcd == 0) algsec_start_handler(cam);

        mythreadname_set("ml",cam->threadnr,cam->conf->camera_name.c_str());
    #else
        cam->algsec_inuse = false;
    #endif
}

/** Free algsec memory and shutdown thread */
void algsec_deinit(ctx_cam *cam)
{
    #ifdef HAVE_OPENCV
        int waitcnt = 0;

        if (cam->algsec == NULL) {
            return;
        }

        algsec_params_deinit(cam->algsec->models);

        if (!cam->algsec->closing) {
            cam->algsec->closing = true;
            while ((cam->algsec->closing) && (waitcnt <1000)){
                SLEEP(0,100000)
                waitcnt++;
            }
        }
        if (cam->algsec->image_norm != NULL){
            if (cam->algsec->image_norm != NULL) {
                free(cam->algsec->image_norm);
            }
            cam->algsec->image_norm = NULL;
        }

        if (waitcnt == 1000){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Graceful shutdown of secondary detector thread failed"));
        }

        pthread_mutex_destroy(&cam->algsec->mutex);

        delete cam->algsec;

    #else
        (void)cam;
    #endif
}

void algsec_detect(ctx_cam *cam)
{
    /*This function runs on the camera thread */
    #ifdef HAVE_OPENCV

        /* If we have already detected something for this event,
         * we do not need to do further detections
         */
        if (cam->algsec->isdetected) {
            return;
        }

        if (cam->algsec->frame_cnt > 0) {
            cam->algsec->frame_cnt--;
        }

        if (cam->algsec->frame_cnt == 0){

            if (cam->algsec->detecting){
                cam->algsec->frame_missed++;
            } else {
                /*Copy in a new image for processing */
                memcpy(cam->algsec->image_norm, cam->current_image->image_norm, cam->imgs.size_norm);

                /*Set the bool to detect on the new image and reset interval */
                cam->algsec->detecting = true;
                cam->algsec->frame_cnt = cam->algsec->models.frame_interval;
                if (cam->algsec->frame_missed >10){
                    if (cam->algsec->too_slow == 0) {
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Your computer is too slow for these settings."));
                   } else if (cam->algsec->too_slow == 10){
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Missed many frames for secondary detection."));
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Your computer is too slow."));
                    }
                    cam->algsec->too_slow++;
                }
                cam->algsec->frame_missed = 0;

            }
        }
    #else
        (void)cam;
    #endif
}


