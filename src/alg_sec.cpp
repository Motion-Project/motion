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

#include "motionplus.hpp"
#include "conf.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "alg_sec.hpp"

#ifdef HAVE_OPENCV

#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>

using namespace cv;
using namespace dnn;

static void algsec_image_show(ctx_dev *cam, Mat &mat_dst)
{
    //std::string testdir;
    std::vector<uchar> buff;    //buffer for coding
    std::vector<int> param(2);
    ctx_algsec_model *algmdl = &cam->algsec->models;


    /* We check the size so that we at least fill in the first image so the
     * web stream will have something to start with.  After feeding in at least
     * the first image, we rely upon the connection count to tell us whether we
     * need to expend the CPU to compress and load the secondary images */
    if ((cam->stream.secondary.cnct_count >0) ||
        (cam->imgs.size_secondary == 0) ||
        (cam->motapp->log_level >= DBG)) {

        if ((cam->motapp->log_level >= DBG) &&
            (algmdl->isdetected == true)) {
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Saved detected image: %s%s%s%s"
                , cam->conf->target_dir.c_str()
                ,  "/detect_"
                , algmdl->method.c_str()
                , ".jpg");
            imwrite(cam->conf->target_dir  + "/detect_" + algmdl->method + ".jpg"
                , mat_dst);
        }

        param[0] = cv::IMWRITE_JPEG_QUALITY;
        param[1] = 75;
        cv::imencode(".jpg", mat_dst, buff, param);
        pthread_mutex_lock(&cam->algsec->mutex);
            std::copy(buff.begin(), buff.end(), cam->imgs.image_secondary);
            cam->imgs.size_secondary = (int)buff.size();
        pthread_mutex_unlock(&cam->algsec->mutex);
    }

}

static void algsec_image_label(ctx_dev *cam, Mat &mat_dst
    , std::vector<Rect> &src_pos, std::vector<double> &src_weights)
{
    std::vector<Rect> fltr_pos;
    std::vector<double> fltr_weights;
    std::string testdir;
    std::size_t indx0, indx1;
    std::vector<uchar> buff;    //buffer for coding
    std::vector<int> param(2);
    char wstr[10];
    ctx_algsec_model *algmdl = &cam->algsec->models;

    try {
        algmdl->isdetected = false;

        if (cam->motapp->log_level >= DBG) {
            imwrite(cam->conf->target_dir  + "/src_" + algmdl->method + ".jpg"
                , mat_dst);
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Saved source image: %s%s%s%s"
                , cam->conf->target_dir.c_str()
                ,  "/src_"
                , algmdl->method.c_str()
                , ".jpg");
        }

        for (indx0=0; indx0<src_pos.size(); indx0++) {
            Rect r = src_pos[indx0];
            double w = src_weights[indx0];

            for (indx1=0; indx1<src_pos.size(); indx1++) {
                if (indx1 != indx0 && (r & src_pos[indx1])==r) {
                    break;
                }
            }
            if ((indx1==src_pos.size()) && (w > algmdl->threshold)) {
                fltr_pos.push_back(r);
                fltr_weights.push_back(w);
                algmdl->isdetected = true;
            }
        }

        if (algmdl->isdetected) {
            for (indx0=0; indx0<fltr_pos.size(); indx0++) {
                Rect r = fltr_pos[indx0];
                r.x += cvRound(r.width*0.1);
                r.width = cvRound(r.width*0.8);
                r.y += cvRound(r.height*0.06);
                r.height = cvRound(r.height*0.9);
                rectangle(mat_dst, r.tl(), r.br(), cv::Scalar(0,255,0), 2);
                snprintf(wstr, 10, "%.4f", fltr_weights[indx0]);
                putText(mat_dst, wstr, Point(r.x,r.y), FONT_HERSHEY_PLAIN, 1, 255, 1);
            }
        }

        algsec_image_show(cam, mat_dst);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl->method = "none";
    }

}

static void algsec_image_label(ctx_dev *cam, Mat &mat_dst
    , double confidence, Point classIdPoint)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    std::string label;

    try {
        algmdl->isdetected = false;

        if (cam->motapp->log_level >= DBG) {
            imwrite(cam->conf->target_dir  + "/src_" + algmdl->method + ".jpg"
                , mat_dst);
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Saved source image: %s%s%s%s"
                , cam->conf->target_dir.c_str()
                ,  "/src_"
                , algmdl->method.c_str()
                , ".jpg");
        }

        if (confidence < algmdl->threshold) {
            return;
        }

        algmdl->isdetected = true;
        label = format("%s: %.4f"
            , (algmdl->dnn_classes.empty() ?
                format("Class #%d", classIdPoint.x).c_str() :
                algmdl->dnn_classes[classIdPoint.x].c_str())
            , confidence);

        putText(mat_dst , label, Point(0, 15)
            , FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));

        algsec_image_show(cam, mat_dst);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl->method = "none";
    }

}

static void algsec_image_roi(ctx_dev *cam, Mat &mat_src, Mat &mat_dst)
{
    cv::Rect roi;
    int width, height, x, y;

    x = cam->current_image->location.minx;
    y = cam->current_image->location.miny;
    width = cam->current_image->location.width;
    height = cam->current_image->location.height;

    if ((y + height) > cam->imgs.height) {
        height = cam->imgs.height - y;
    }
    if ((x + width) > cam->imgs.width) {
        width = cam->imgs.width - x;
    }

    roi.x = x;
    roi.y = y;
    roi.width = width;
    roi.height = height;

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

    mat_dst = mat_src(roi);

}

static void algsec_image_type(ctx_dev *cam, Mat &mat_dst)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;

    if ((algmdl->image_type == "gray") || (algmdl->image_type == "grey")) {
        mat_dst = Mat(cam->imgs.height, cam->imgs.width
            , CV_8UC1, (void*)cam->algsec->image_norm);
    } else if (algmdl->image_type == "roi") {
        /*Discard really small and large images */
        if ((cam->current_image->location.width < 64) ||
            (cam->current_image->location.height < 64) ||
            ((cam->current_image->location.width/cam->imgs.width) > 0.7) ||
            ((cam->current_image->location.height/cam->imgs.height) > 0.7)) {
            return;
        }
        Mat mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
            , CV_8UC1, (void*)cam->algsec->image_norm);
        cvtColor(mat_src, mat_src, COLOR_YUV2RGB_YV12);
        algsec_image_roi(cam, mat_src, mat_dst);
    } else {
        Mat mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
            , CV_8UC1, (void*)cam->algsec->image_norm);
        cvtColor(mat_src, mat_dst, COLOR_YUV2RGB_YV12);
    }

}

static void algsec_detect_hog(ctx_dev *cam)
{
    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    Mat mat_dst;
    ctx_algsec_model *algmdl = &cam->algsec->models;

    try {
        algsec_image_type(cam, mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }
        equalizeHist(mat_dst, mat_dst);

        algmdl->hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());

        algmdl->hog.detectMultiScale(mat_dst, detect_pos, detect_weights, 0
            ,Size(algmdl->hog_winstride, algmdl->hog_winstride)
            ,Size(algmdl->hog_padding, algmdl->hog_padding)
            ,algmdl->scalefactor
            ,algmdl->hog_threshold_model
            ,false);

        algsec_image_label(cam, mat_dst, detect_pos, detect_weights);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl->method = "none";
    }
}

static void algsec_detect_haar(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    std::vector<int> levels;
    Mat mat_dst;

    try {
        algsec_image_type(cam, mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }
        equalizeHist(mat_dst, mat_dst);

        algmdl->haar_cascade.detectMultiScale(
            mat_dst, detect_pos, levels, detect_weights
            ,algmdl->scalefactor, algmdl->haar_minneighbors,algmdl->haar_flags
            , Size(algmdl->haar_minsize,algmdl->haar_minsize)
            , Size(algmdl->haar_maxsize,algmdl->haar_maxsize), true);

        algsec_image_label(cam, mat_dst, detect_pos, detect_weights);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl->method = "none";
    }
}

static void algsec_detect_dnn(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    Mat mat_dst, softmaxProb;
    double confidence;
    float maxProb = 0.0, sum = 0.0;
    Point classIdPoint;

    try {
        algsec_image_type(cam, mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }

        Mat blob = blobFromImage(mat_dst
            , algmdl->dnn_scale
            , Size(algmdl->dnn_width, algmdl->dnn_height)
            , Scalar());
        algmdl->net.setInput(blob);
        Mat prob = algmdl->net.forward();

        maxProb = *std::max_element(prob.begin<float>(), prob.end<float>());
        cv::exp(prob-maxProb, softmaxProb);
        sum = (float)cv::sum(softmaxProb)[0];
        softmaxProb /= sum;
        minMaxLoc(softmaxProb.reshape(1, 1), 0, &confidence, 0, &classIdPoint);

        algsec_image_label(cam, mat_dst, confidence, classIdPoint);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl->method = "none";
    }
}

static void algsec_load_haar(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    try {
        if (algmdl->model_file == "") {
            algmdl->method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("No secondary model specified."));
            return;
        }
        if (algmdl->haar_cascade.load(algmdl->model_file) == false) {
            /* Loading failed, reset method*/
            algmdl->method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
                ,algmdl->model_file.c_str());
        };
    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
            , algmdl->model_file.c_str());
        algmdl->method = "none";
    }
}

static void algsec_load_dnn(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    std::string line;
    std::ifstream ifs;

    try {
        if (algmdl->model_file == "") {
            algmdl->method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("No secondary model specified."));
            return;
        }
        algmdl->net = readNet(
            algmdl->model_file
            , algmdl->dnn_config
            , algmdl->dnn_framework);
        algmdl->net.setPreferableBackend(algmdl->dnn_backend);
        algmdl->net.setPreferableTarget(algmdl->dnn_target);

        ifs.open(algmdl->dnn_classes_file.c_str());
            if (ifs.is_open() == false) {
                algmdl->method = "none";
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Classes file not found: %s")
                    ,algmdl->dnn_classes_file.c_str());
                return;
            }
            while (std::getline(ifs, line)) {
                algmdl->dnn_classes.push_back(line);
            }
        ifs.close();

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
            , algmdl->model_file.c_str());
        algmdl->method = "none";
    }
}

static void algsec_params_log(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    int indx;

    if (algmdl->method != "none") {
        for (indx = 0; indx < algmdl->algsec_params->params_count; indx++) {
            motion_log(INF, TYPE_ALL, NO_ERRNO,0, "%-25s %s"
                ,algmdl->algsec_params->params_array[indx].param_name
                ,algmdl->algsec_params->params_array[indx].param_value);
        }
    }
}

static void algsec_params_model(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;
    int indx;
    char *param_nm, *param_vl;

    for (indx = 0; indx < algmdl->algsec_params->params_count; indx++) {
        param_nm = algmdl->algsec_params->params_array[indx].param_name;
        param_vl = algmdl->algsec_params->params_array[indx].param_value;

        if (mystreq(param_nm, "model_file")) {
            algmdl->model_file = param_vl;
        } else if (mystreq(param_nm,"frame_interval")) {
            algmdl->frame_interval = atoi(param_vl);
        } else if (mystreq(param_nm,"image_type")) {
            algmdl->image_type = param_vl;
        } else if (mystreq(param_nm,"threshold")) {
            algmdl->threshold = atof(param_vl);
        } else if (mystreq(param_nm,"scalefactor")) {
            algmdl->scalefactor = atof(param_vl);
        } else if (mystreq(param_nm,"rotate")) {
            algmdl->rotate = atoi(param_vl);
        }

        if (algmdl->method == "hog") {
            if (mystreq(param_nm,"padding")) {
                algmdl->hog_padding = atoi(param_vl);
            } else if (mystreq(param_nm,"threshold_model")) {
                algmdl->hog_threshold_model = atof(param_vl);
            } else if (mystreq(param_nm,"winstride")) {
                algmdl->hog_winstride = atoi(param_vl);
            }
        } else if (algmdl->method == "haar") {
            if (mystreq(param_nm,"flags")) {
                algmdl->haar_flags = atoi(param_vl);
            } else if (mystreq(param_nm,"maxsize")) {
                algmdl->haar_maxsize = atoi(param_vl);
            } else if (mystreq(param_nm,"minsize")) {
                algmdl->haar_minsize = atoi(param_vl);
            } else if (mystreq(param_nm,"minneighbors")) {
                algmdl->haar_minneighbors = atoi(param_vl);
            }
        } else if (algmdl->method == "dnn") {
            if (mystreq(param_nm, "config")) {
                algmdl->dnn_config = param_vl;
            } else if (mystreq(param_nm, "classes_file")) {
                algmdl->dnn_classes_file = param_vl;
            } else if (mystreq(param_nm,"framework")) {
                algmdl->dnn_framework = param_vl;
            } else if (mystreq(param_nm,"backend")) {
                algmdl->dnn_backend = atoi(param_vl);
            } else if (mystreq(param_nm,"target")) {
                algmdl->dnn_target = atoi(param_vl);
            } else if (mystreq(param_nm,"scale")) {
                algmdl->dnn_scale = atof(param_vl);
            } else if (mystreq(param_nm,"width")) {
                algmdl->dnn_width = atoi(param_vl);
            } else if (mystreq(param_nm,"height")) {
                algmdl->dnn_height = atoi(param_vl);
            }
        }
    }
}

static void algsec_params_defaults(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;

    util_parms_add_default(algmdl->algsec_params, "model_file", "");
    util_parms_add_default(algmdl->algsec_params, "frame_interval", "5");
    util_parms_add_default(algmdl->algsec_params, "image_type", "full");
    util_parms_add_default(algmdl->algsec_params, "rotate", "0");

    if (algmdl->method == "haar") {
        util_parms_add_default(algmdl->algsec_params, "threshold", "1.1");
        util_parms_add_default(algmdl->algsec_params, "scalefactor", "1.1");
        util_parms_add_default(algmdl->algsec_params, "flags", "0");
        util_parms_add_default(algmdl->algsec_params, "maxsize", "1024");
        util_parms_add_default(algmdl->algsec_params, "minsize", "8");
        util_parms_add_default(algmdl->algsec_params, "minneighbors", "8");
    } else if (algmdl->method == "hog") {
        util_parms_add_default(algmdl->algsec_params, "threshold", "1.1");
        util_parms_add_default(algmdl->algsec_params, "threshold_model", "2");
        util_parms_add_default(algmdl->algsec_params, "scalefactor", "1.05");
        util_parms_add_default(algmdl->algsec_params, "padding", "8");
        util_parms_add_default(algmdl->algsec_params, "winstride", "8");
    } else if (algmdl->method == "dnn") {
        util_parms_add_default(algmdl->algsec_params, "backend", DNN_BACKEND_DEFAULT);
        util_parms_add_default(algmdl->algsec_params, "target", DNN_TARGET_CPU);
        util_parms_add_default(algmdl->algsec_params, "threshold", "0.75");
        util_parms_add_default(algmdl->algsec_params, "width", cam->imgs.width);
        util_parms_add_default(algmdl->algsec_params, "height", cam->imgs.height);
        util_parms_add_default(algmdl->algsec_params, "scale", "1.0");
    }

}

static void algsec_params_deinit(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;

    if (algmdl->algsec_params != NULL){
        util_parms_free(algmdl->algsec_params);
        myfree(&algmdl->algsec_params);
    }
}

static void algsec_params_init(ctx_dev *cam)
{
    ctx_algsec_model *algmdl = &cam->algsec->models;

    algmdl->algsec_params = (ctx_params*) mymalloc(sizeof(ctx_params));
    memset(algmdl->algsec_params, 0, sizeof(ctx_params));
    algmdl->algsec_params->params_array = NULL;
    algmdl->algsec_params->params_count = 0;
    algmdl->algsec_params->update_params = true;     /*Set trigger to update parameters */
}

/**Load the parms from the config to algsec struct */
static void algsec_load_params(ctx_dev *cam)
{
    pthread_mutex_init(&cam->algsec->mutex, NULL);

    cam->algsec->isdetected = false;
    cam->algsec->height = cam->imgs.height;
    cam->algsec->width = cam->imgs.width;
    cam->algsec->models.method = cam->conf->secondary_method;
    cam->algsec->image_norm = (unsigned char*)mymalloc(cam->imgs.size_norm);
    cam->algsec->frame_missed = 0;
    cam->algsec->too_slow = 0;
    cam->algsec->detecting = false;
    cam->algsec->closing = false;
    cam->algsec->thread_running = false;

    algsec_params_init(cam);

    util_parms_parse(cam->algsec->models.algsec_params, cam->conf->secondary_params);

    algsec_params_defaults(cam);

    algsec_params_log(cam);

    algsec_params_model(cam);

    cam->algsec->frame_cnt = cam->algsec->models.frame_interval;

}

/**Preload the models and initialize them */
static void algsec_load_models(ctx_dev *cam)
{
    if (cam->algsec->models.method == "haar") {
        algsec_load_haar(cam);
    } else if (cam->algsec->models.method == "hog") {
        //algsec_load_hog(cam->algsec->models);
    } else if (cam->algsec->models.method == "dnn") {
        algsec_load_dnn(cam);
    } else {
        cam->algsec->models.method = "none";
    }

    /* If model fails to load, the method is changed to none*/
    if ((cam->algsec->models.method == "haar") ||
        (cam->algsec->models.method == "hog") ||
        (cam->algsec->models.method == "dnn")) {
        cam->algsec_inuse = true;
    } else {
        cam->algsec_inuse = false;
    }

}

/**Detection thread processing loop */
static void *algsec_handler(void *arg)
{
    ctx_dev *cam = (ctx_dev*)arg;
    long interval;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Starting."));

    cam->algsec->closing = false;
    cam->algsec->thread_running = true;

    interval = 1000000L / cam->conf->framerate;

    while (cam->algsec->closing == false) {
        if (cam->algsec->detecting){
            if (cam->algsec->models.method == "haar") {
                algsec_detect_haar(cam);
            } else if (cam->algsec->models.method == "hog") {
                algsec_detect_hog(cam);
            } else if (cam->algsec->models.method == "dnn") {
                algsec_detect_dnn(cam);
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
    cam->algsec->thread_running = false;
    pthread_exit(NULL);

}

/**Start the detection thread*/
static void algsec_start_handler(ctx_dev *cam)
{
    int retcd;
    pthread_attr_t handler_attribute;

    if (cam->algsec->models.method == "none") {
        return;
    }

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);
    retcd = pthread_create(&cam->algsec->threadid, &handler_attribute, &algsec_handler, cam);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("Error starting algsec handler thread"));
        cam->algsec->models.method = "none";
    }
    pthread_attr_destroy(&handler_attribute);

}

#endif

/** Initialize the secondary processes and parameters */
void algsec_init(ctx_dev *cam)
{
    cam->algsec_inuse = false;

    #ifdef HAVE_OPENCV
        mythreadname_set("cv",cam->threadnr,cam->conf->camera_name.c_str());
            cam->algsec = new ctx_algsec;
            algsec_load_params(cam);
            algsec_load_models(cam);
            algsec_start_handler(cam);
        mythreadname_set("ml",cam->threadnr,cam->conf->camera_name.c_str());
    #endif
}

/** Shut down the secondary detection components */
void algsec_deinit(ctx_dev *cam)
{
    #ifdef HAVE_OPENCV
        int waitcnt = 0;

        if (cam->algsec == NULL) {
            return;
        }

        if (cam->algsec->thread_running == true) {
            if (cam->algsec->closing == false) {
                cam->algsec->closing = true;
                while ((cam->algsec->closing) && (waitcnt <1000)){
                    SLEEP(0,1000000)
                    waitcnt++;
                }
            }
            if (waitcnt == 1000){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("Graceful shutdown of secondary detector thread failed"));
            }
        }

        algsec_params_deinit(cam);

        myfree(&cam->algsec->image_norm);

        pthread_mutex_destroy(&cam->algsec->mutex);

        delete cam->algsec;
        cam->algsec = NULL;
        cam->algsec_inuse = false;

    #else
        (void)cam;
    #endif
}

/*Invoke the secondary detetction method*/
void algsec_detect(ctx_dev *cam)
{
    #ifdef HAVE_OPENCV
        if (cam->algsec_inuse == false){
            return;
        }

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
                memcpy(cam->algsec->image_norm
                    , cam->imgs.image_virgin
                    , cam->imgs.size_norm);

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

        /* If the method was changed to none, then an error occurred*/
        if (cam->algsec->models.method == "none") {
            algsec_deinit(cam);
        }

    #else
        (void)cam;
    #endif
}
