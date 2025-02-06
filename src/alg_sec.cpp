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

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "alg_sec.hpp"

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

using namespace cv;
using namespace dnn;

static void *algsec_handler(void *arg)
{
    ((cls_algsec *)arg)->handler();
    return nullptr;
}

void cls_algsec::debug_notice(Mat &mat_dst, bool isdetect)
{
    if (handler_stop == true) {
        return;
    }

    if (cfg_log_level >= DBG) {
        if (first_pass == true) {
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                , "Secondary detect and debug enabled.");
            MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
                , "Saving source and detected images to %s"
                , cfg_target_dir.c_str());
            first_pass = false;
        }
        if (isdetect == true) {
            imwrite(cfg_target_dir  + "/detect_" + method + ".jpg"
                , mat_dst);
        } else {
            imwrite(cfg_target_dir  + "/src_" + method + ".jpg"
                , mat_dst);
        }
    }
}

void cls_algsec::image_show(Mat &mat_dst)
{
    std::vector<uchar> buff;
    std::vector<int> param(2);

    if (handler_stop == true) {
        return;
    }

    /* We check the size so that we at least fill in the first image so the
     * web stream will have something to start with.  After feeding in at least
     * the first image, we rely upon the connection count to tell us whether we
     * need to expend the CPU to compress and load the secondary images */
    if ((cam->stream.secondary.jpg_cnct >0) ||
        (cam->imgs.size_secondary == 0) ||
        (cfg_log_level >= DBG)) {

        debug_notice(mat_dst, detected);

        param[0] = cv::IMWRITE_JPEG_QUALITY;
        param[1] = 75;
        cv::imencode(".jpg", mat_dst, buff, param);
        pthread_mutex_lock(&mutex);
            std::copy(buff.begin(), buff.end(), cam->imgs.image_secondary);
            cam->imgs.size_secondary = (int)buff.size();
        pthread_mutex_unlock(&mutex);
    }

}

void cls_algsec::label_image(Mat &mat_dst
    , std::vector<Rect> &src_pos, std::vector<double> &src_weights)
{
    std::vector<Rect> fltr_pos;
    std::vector<double> fltr_weights;
    std::string testdir;
    std::size_t indx0, indx1;
    std::vector<uchar> buff;
    std::vector<int> param(2);
    char wstr[10];

    try {
        detected = false;

        debug_notice(mat_dst, detected);

        for (indx0=0; indx0<src_pos.size(); indx0++) {
            Rect r = src_pos[indx0];
            double w = src_weights[indx0];

            for (indx1=0; indx1<src_pos.size(); indx1++) {
                if (indx1 != indx0 && (r & src_pos[indx1])==r) {
                    break;
                }
            }
            if ((indx1==src_pos.size()) && (w > threshold)) {
                fltr_pos.push_back(r);
                fltr_weights.push_back(w);
                detected = true;
            }
        }

        if (detected) {
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

        image_show(mat_dst);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        method = "none";
    }

}

void cls_algsec::label_image(Mat &mat_dst, double confidence, Point classIdPoint)
{
    std::string label;

    try {
        detected = false;
        debug_notice(mat_dst, detected);

        if (confidence < threshold) {
            return;
        }

        detected = true;
        label = format("%s: %.4f"
            , (dnn_classes.empty() ?
                format("Class #%d", classIdPoint.x).c_str() :
                dnn_classes[(uint)classIdPoint.x].c_str())
            , confidence);

        putText(mat_dst , label, Point(0, 15)
            , FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));

        image_show(mat_dst);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        method = "none";
    }

}

void cls_algsec::get_image_roi(Mat &mat_src, Mat &mat_dst)
{
    cv::Rect roi;

    roi.x = cam->current_image->location.minx;
    roi.y = cam->current_image->location.miny;
    roi.width = cam->current_image->location.width;
    roi.height = cam->current_image->location.height;

    /* Images smaller than 100 cause seg faults.  112 is the nearest
     multiple of 16 greater than 100*/
    if (roi.height < 112) {
        roi.height = 112;
    }
    if ((roi.y + roi.height) > (height-112)) {
        roi.y = height - roi.height;
    } else if ((roi.y + roi.height) > height) {
        roi.height = height - roi.y;
    }

    if (roi.width < 112) {
        roi.width = 112;
    }
    if ((roi.x + roi.width) > (width-112)) {
        roi.x = width - roi.width;
    } else {
        roi.width = width - roi.x;
    }

    /*
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Base %d %d (%dx%d) img(%dx%d)"
        ,cam->current_image->location.minx
        ,cam->current_image->location.miny
        ,cam->current_image->location.width
        ,cam->current_image->location.height
        ,width,height);
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Opencv %d %d %d %d"
        ,roi.x,roi.y,roi.width,roi.height);
    */
    mat_dst = mat_src(roi);

}

void cls_algsec::get_image(Mat &mat_dst)
{
    Mat mat_src;

    if (image_type == "grey") {
        mat_dst = Mat(cam->imgs.height, cam->imgs.width
            , CV_8UC1, (void*)image_norm);
    } else if ((image_type == "roi") || (image_type == "greyroi")) {
        /*Discard really small and large images */
        if ((cam->current_image->location.width < 64) ||
            (cam->current_image->location.height < 64) ||
            ((cam->current_image->location.width/cam->imgs.width) > 0.7) ||
            ((cam->current_image->location.height/cam->imgs.height) > 0.7)) {
            return;
        }
        if (image_type == "roi") {
            mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
                , CV_8UC1, (void*)image_norm);
            cvtColor(mat_src, mat_src, COLOR_YUV2RGB_YV12);
        } else {
            mat_src = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)image_norm);
        }
        get_image_roi(mat_src, mat_dst);
    } else {
        mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
            , CV_8UC1, (void*)image_norm);
        cvtColor(mat_src, mat_dst, COLOR_YUV2RGB_YV12);
    }
}

void cls_algsec::detect_hog()
{
    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    Mat mat_dst;

    try {
        get_image(mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }
        equalizeHist(mat_dst, mat_dst);

        hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());

        hog.detectMultiScale(mat_dst, detect_pos, detect_weights, 0
            ,Size(hog_winstride, hog_winstride)
            ,Size(hog_padding, hog_padding)
            ,scalefactor
            ,hog_threshold_model
            ,false);

        label_image(mat_dst, detect_pos, detect_weights);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        method = "none";
    }
}

void cls_algsec::detect_haar()
{
    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    std::vector<int> levels;
    Mat mat_dst;

    try {
        get_image(mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }
        equalizeHist(mat_dst, mat_dst);

        haar_cascade.detectMultiScale(
            mat_dst, detect_pos, levels, detect_weights
            ,scalefactor, haar_minneighbors,haar_flags
            , Size(haar_minsize,haar_minsize)
            , Size(haar_maxsize,haar_maxsize), true);

        label_image(mat_dst, detect_pos, detect_weights);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        method = "none";
    }
}

void cls_algsec::detect_dnn()
{
    Mat mat_dst, softmaxProb;
    double confidence;
    float maxProb = 0.0, sum = 0.0;
    Point classIdPoint;

    try {
        get_image(mat_dst);
        if (mat_dst.empty() == true) {
            return;
        }

        Mat blob = blobFromImage(mat_dst
            , dnn_scale
            , Size(dnn_width, dnn_height)
            , Scalar());
        net.setInput(blob);
        Mat prob = net.forward();

        maxProb = *std::max_element(prob.begin<float>(), prob.end<float>());
        cv::exp(prob-maxProb, softmaxProb);
        sum = (float)cv::sum(softmaxProb)[0];
        softmaxProb /= sum;
        cv::minMaxLoc(softmaxProb.reshape(1, 1), 0, &confidence, 0, &classIdPoint);

        label_image(mat_dst, confidence, classIdPoint);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        method = "none";
    }
}

void cls_algsec::load_haar()
{
    try {
        if (model_file == "") {
            method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("No secondary model specified."));
            return;
        }
        if (haar_cascade.load(model_file) == false) {
            /* Loading failed, reset method*/
            method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
                ,model_file.c_str());
        }
    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
            , model_file.c_str());
        method = "none";
    }
}

void cls_algsec::load_hog()
{
    if (image_type == "roi") {
        image_type = "greyroi";
    } else if (
        (image_type != "grey") &&
        (image_type != "greyroi")) {
        image_type = "grey";
    }
}

void cls_algsec::load_dnn()
{
    std::string line;
    std::ifstream ifs;

    try {
        if (model_file == "") {
            method = "none";
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("No secondary model specified."));
            return;
        }
        net = readNet(
            model_file
            , dnn_config
            , dnn_framework);
        net.setPreferableBackend(dnn_backend);
        net.setPreferableTarget(dnn_target);

        ifs.open(dnn_classes_file.c_str());
            if (ifs.is_open() == false) {
                method = "none";
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("Classes file not found: %s")
                    ,dnn_classes_file.c_str());
                return;
            }
            while (std::getline(ifs, line)) {
                dnn_classes.push_back(line);
            }
        ifs.close();

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
            , model_file.c_str());
        method = "none";
    }
}

void cls_algsec::params_log()
{
    ctx_params_item *itm;
    int indx;

    if (method != "none") {
        for (indx=0;indx<params->params_cnt;indx++) {
            itm = &params->params_array[indx];
            MOTION_SHT(INF, TYPE_ALL, NO_ERRNO, "%-25s %s"
                ,itm->param_name.c_str(),itm->param_value.c_str());
        }
    }
}

void cls_algsec::params_model()
{
    ctx_params_item *itm;
    int indx;

    for (indx=0;indx<params->params_cnt;indx++) {
        itm = &params->params_array[indx];
        if (itm->param_name == "model_file") {
            model_file = itm->param_value;
        } else if (itm->param_name == "frame_interval") {
            frame_interval = mtoi(itm->param_value);
        } else if (itm->param_name == "image_type") {
            image_type = itm->param_value;
        } else if (itm->param_name == "threshold") {
            threshold = mtof(itm->param_value);
        } else if (itm->param_name == "scalefactor") {
            scalefactor = mtof(itm->param_value);
        } else if (itm->param_name == "rotate") {
            rotate = mtoi(itm->param_value);
        }

        if (method == "hog") {
            if (itm->param_name =="padding") {
                hog_padding = mtoi(itm->param_value);
            } else if (itm->param_name =="threshold_model") {
                hog_threshold_model = mtof(itm->param_value);
            } else if (itm->param_name =="winstride") {
                hog_winstride = mtoi(itm->param_value);
            }
        } else if (method == "haar") {
            if (itm->param_name =="flags") {
                haar_flags = mtoi(itm->param_value);
            } else if (itm->param_name =="maxsize") {
                haar_maxsize = mtoi(itm->param_value);
            } else if (itm->param_name =="minsize") {
                haar_minsize = mtoi(itm->param_value);
            } else if (itm->param_name =="minneighbors") {
                haar_minneighbors = mtoi(itm->param_value);
            }
        } else if (method == "dnn") {
            if (itm->param_name == "config") {
                dnn_config = itm->param_value;
            } else if (itm->param_name == "classes_file") {
                dnn_classes_file = itm->param_value;
            } else if (itm->param_name =="framework") {
                dnn_framework = itm->param_value;
            } else if (itm->param_name =="backend") {
                dnn_backend = mtoi(itm->param_value);
            } else if (itm->param_name =="target") {
                dnn_target = mtoi(itm->param_value);
            } else if (itm->param_name =="scale") {
                dnn_scale = mtof(itm->param_value);
            } else if (itm->param_name =="width") {
                dnn_width = mtoi(itm->param_value);
            } else if (itm->param_name =="height") {
                dnn_height = mtoi(itm->param_value);
            }
        }
    }
}

void cls_algsec::params_defaults()
{
    util_parms_add_default(params, "model_file", "");
    util_parms_add_default(params, "frame_interval", "5");
    util_parms_add_default(params, "image_type", "full");
    util_parms_add_default(params, "rotate", "0");

    if (method == "haar") {
        util_parms_add_default(params, "threshold", "1.1");
        util_parms_add_default(params, "scalefactor", "1.1");
        util_parms_add_default(params, "flags", "0");
        util_parms_add_default(params, "maxsize", "1024");
        util_parms_add_default(params, "minsize", "8");
        util_parms_add_default(params, "minneighbors", "8");
    } else if (method == "hog") {
        util_parms_add_default(params, "threshold", "1.1");
        util_parms_add_default(params, "threshold_model", "2");
        util_parms_add_default(params, "scalefactor", "1.05");
        util_parms_add_default(params, "padding", "8");
        util_parms_add_default(params, "winstride", "8");
    } else if (method == "dnn") {
        util_parms_add_default(params, "backend", DNN_BACKEND_DEFAULT);
        util_parms_add_default(params, "target", DNN_TARGET_CPU);
        util_parms_add_default(params, "threshold", "0.75");
        util_parms_add_default(params, "width", cam->imgs.width);
        util_parms_add_default(params, "height", cam->imgs.height);
        util_parms_add_default(params, "scale", "1.0");
    }
}

/**Load the parms from the config to algsec struct */
void cls_algsec::load_params()
{
    method = cam->cfg->secondary_method;
    image_norm = nullptr;
    params = nullptr;
    detected = false;
    height = cam->imgs.height;
    width = cam->imgs.width;
    frame_missed = 0;
    frame_cnt = 0;
    too_slow = 0;
    in_process = false;
    first_pass = true;
    handler_stop = false;

    cfg_framerate = cam->cfg->framerate;
    cfg_log_level = cam->app->cfg->log_level;
    cfg_target_dir = cam->cfg->target_dir;

    if (method == "none") {
        return;
    }

    image_norm = (u_char*)mymalloc((size_t)cam->imgs.size_norm);

    params = new ctx_params;
    util_parms_parse(params, "secondary_params", cam->cfg->secondary_params);

    params_defaults();

    params_log();

    params_model();

    frame_cnt = frame_interval;

    if (method == "haar") {
        load_haar();
    } else if (method == "hog") {
        load_hog();
    } else if (method == "dnn") {
        load_dnn();
    } else {
        method = "none";
    }

}

/**Detection thread processing loop */
void cls_algsec::handler()
{
    long interval;

    mythreadname_set("cv",cam->cfg->device_id, cam->cfg->device_name.c_str());

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Secondary detection starting."));

    handler_running = true;
    handler_stop = false;

    load_params();

    interval = 1000000L / cfg_framerate;
    is_started = true;
    while ((handler_stop == false) && (method != "none")) {
        if (in_process){
            if (method == "haar") {
                detect_haar();
            } else if (method == "hog") {
                detect_hog();
            } else if (method == "dnn") {
                detect_dnn();
            }
            in_process = false;
        } else {
            SLEEP(0,interval)
        }
    }
    is_started = false;
    handler_stop = false;
    handler_running = false;
    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,_("Secondary detection stopped."));

    pthread_exit(nullptr);
}

void cls_algsec::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    if (cam->cfg->secondary_method == "none") {
        return;
    }

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &algsec_handler, this);
        if (retcd != 0) {
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start secondary detection"));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_algsec::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < cam->cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == cam->cfg->watchdog_tmo) {
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of camera failed"));
            if (cam->cfg->watchdog_kill > 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,cam->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < cam->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == cam->cfg->watchdog_kill) {
                    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
    }

    myfree(image_norm);
    mydelete(params);

}

#endif


/*Invoke the secondary detetction method*/
void cls_algsec::detect()
{
    #ifdef HAVE_OPENCV
        if (method == "none") {
            return;
        }
        if (is_started == false) {
            return;
        }

        if (frame_cnt > 0) {
            frame_cnt--;
        }

        if (frame_cnt == 0){
            if (in_process){
                frame_missed++;
            } else {
                memcpy(image_norm
                    , cam->imgs.image_virgin
                    , (uint)cam->imgs.size_norm);

                /*Set the bool to detect on the new image and reset interval */
                in_process = true;
                frame_cnt = frame_interval;
                if (frame_missed >10){
                    if (too_slow == 0) {
                        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                            ,_("Your computer is too slow for these settings."));
                    } else if (too_slow == 10){
                        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                            ,_("Missed many frames for secondary detection."));
                        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
                            ,_("Your computer is too slow."));
                    }
                    too_slow++;
                }
                frame_missed = 0;
            }
        }

        /* If the method was changed to none, an error occurred*/
        if (method == "none") {
            handler_shutdown();
        }

    #endif
}

cls_algsec::cls_algsec(cls_camera *p_cam)
{
    #ifdef HAVE_OPENCV
        cam = p_cam;
        handler_running = false;
        handler_stop = true;
        image_norm = nullptr;
        params = nullptr;
        method = "none";
        is_started = false;
        pthread_mutex_init(&mutex, NULL);
        handler_startup();
    #else
        (void)p_cam;
    #endif
}

cls_algsec::~cls_algsec()
{
    #ifdef HAVE_OPENCV
        handler_shutdown();
        pthread_mutex_destroy(&mutex);
    #endif
}

