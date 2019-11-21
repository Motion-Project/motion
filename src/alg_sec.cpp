/*    alg_sec.cpp
 *
 *    Algorithms for Secondary Detection
 *    Detect changes in a video stream using alternative methods.
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include "motion.hpp"
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

static void myltrim(std::string &vstr){
    vstr.erase(
        vstr.begin()
        , std::find_if( vstr.begin() , vstr.end() , [](int ch)
            { return !std::isspace(ch);}
        )
    );
}
static void myrtrim(std::string &vstr){
    vstr.erase(
        std::find_if(vstr.rbegin(), vstr.rend(), [](int ch)
            {return !std::isspace(ch);}
        ).base()
        , vstr.end()
    );
}
static void mytrim(std::string &vstr){
    myltrim(vstr);
    myrtrim(vstr);
}

static void algsec_img_show(ctx_cam *cam, Mat &mat_src
    , std::vector<Rect> &src_pos, std::vector<double> &src_weights
    , std::string algmethod, ctx_algsec_model &algmdl){

    std::vector<Rect> fltr_pos;
    std::vector<double> fltr_weights;
    std::string testdir;
    std::size_t indx0, indx1;
    std::vector<uchar> buff;    //buffer for coding
    std::vector<int> param(2);
    char wstr[10];
    float min_weight;

    min_weight = algmdl.threshold_motion/100;

    testdir = cam->conf->target_dir;

    imwrite(testdir  + "/src_" + algmethod + ".jpg", mat_src);

    algmdl.isdetected = false;
    for (indx0=0; indx0<src_pos.size(); indx0++) {
        Rect r = src_pos[indx0];
        double w = src_weights[indx0];

        for (indx1=0; indx1<src_pos.size(); indx1++){
            if (indx1 != indx0 && (r & src_pos[indx1])==r) break;
        }
        if ((indx1==src_pos.size()) && (w > min_weight)){
            fltr_pos.push_back(r);
            fltr_weights.push_back(w);
            algmdl.isdetected = true;
        }
    }

    if (algmdl.isdetected){
        for (indx0=0; indx0<fltr_pos.size(); indx0++) {
            Rect r = fltr_pos[indx0];
            r.x += cvRound(r.width*0.1);
            r.width = cvRound(r.width*0.8);
            r.y += cvRound(r.height*0.06);
            r.height = cvRound(r.height*0.9);
            rectangle(mat_src, r.tl(), r.br(), cv::Scalar(0,255,0), 2);
            snprintf(wstr, 10, "%.4f", fltr_weights[indx0]);
            putText(mat_src, wstr, Point(r.x,r.y), FONT_HERSHEY_SIMPLEX, 1, 255, 2);
        }
        imwrite(testdir  + "/detect_" + algmethod + ".jpg", mat_src);
    }

    /* We check the size so that we at least fill in the first image so the
     * web stream will have something to start with.  After feeding in at least
     * the first image, we rely upon the connection count to tell us whether we
     * need to expend the CPU to compress and load the secondary images */
    if ((cam->stream.secondary.cnct_count >0) ||
        (cam->imgs.size_secondary == 0)){
        param[0] = cv::IMWRITE_JPEG_QUALITY;
        param[1] = 75;
        cv::imencode(".jpg", mat_src, buff, param);
        pthread_mutex_lock(&cam->algsec->mutex);
            std::copy(buff.begin(), buff.end(), cam->imgs.image_secondary);
            cam->imgs.size_secondary = (int)buff.size();
        pthread_mutex_unlock(&cam->algsec->mutex);
    }
}

static void algsec_img_roi(ctx_cam *cam, Mat &mat_src, Mat &mat_dst){

    cv::Rect roi;

    /* Set the ROI just a small bit larger than the motion detected area */
    roi.x = cam->current_image->location.minx-32;
    roi.y = cam->current_image->location.miny-32;
    roi.width = cam->current_image->location.width+64;
    roi.height = cam->current_image->location.height+64;
    /*
    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        ,"bbx %d y %d %dx%d %dx%d "
        ,roi.x, roi.y, roi.width, roi.height
        ,cam->current_image->location.width
        ,cam->current_image->location.height);
    */
    if (roi.x < 0) {
        roi.x = 0;
        roi.width =roi.width-16;
    }
    if (roi.y < 0){
        roi.y = 0;
        roi.height = roi.height-16;
    }
    if ((roi.x + roi.width) > cam->imgs.width) {
        roi.width = cam->imgs.width - roi.x;
    }
    if ((roi.y + roi.height) > cam->imgs.height) {
        roi.height = cam->imgs.height - roi.y;
    }

    mat_dst = mat_src(roi);
    /*
    MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
        ,"x %d y %d %dx%d %dx%d "
        ,roi.x, roi.y, roi.width, roi.height
        ,cam->current_image->location.width
        ,cam->current_image->location.height);
    */

}

static void algsec_detect_hog(ctx_cam *cam, ctx_algsec_model &algmdl){

    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    HOGDescriptor hog;
    Mat mat_dst;

    try {
        if (algmdl.imagetype == "color"){
            /* AFAIK, the detector uses grey so users shouldn't really use this*/
            Mat mat_src = Mat(cam->imgs.height*3/2, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            cvtColor(mat_src, mat_dst, COLOR_YUV2RGB_YV12);

        } else if (algmdl.imagetype == "full"){
            mat_dst = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);

        } else {
            /*Discard really small and large images */
            if ((cam->current_image->location.width < 64) ||
                (cam->current_image->location.height < 64) ||
               ((cam->current_image->location.width/cam->imgs.width) > 0.7) ||
               ((cam->current_image->location.height/cam->imgs.height) > 0.7)) return;

            Mat mat_src = Mat(cam->imgs.height, cam->imgs.width
                , CV_8UC1, (void*)cam->algsec->image_norm);
            algsec_img_roi(cam, mat_src, mat_dst);

        }

        equalizeHist(mat_dst, mat_dst);

        hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());

        hog.detectMultiScale(mat_dst, detect_pos, detect_weights, 0
            ,Size(algmdl.hog_winstride, algmdl.hog_winstride)
            ,Size(algmdl.hog_padding, algmdl.hog_padding)
            ,algmdl.scalefactor
            ,algmdl.threshold_model
            ,false);

        algsec_img_show(cam, mat_dst, detect_pos, detect_weights, "hog",algmdl);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl.method = 0;
    }
}

static void algsec_detect_haar(ctx_cam *cam, ctx_algsec_model &algmdl){

    std::vector<double> detect_weights;
    std::vector<Rect> detect_pos;
    std::vector<int> levels;

    try {
        Mat mat_src(cam->imgs.height, cam->imgs.width
            , CV_8UC1, (void*)cam->algsec->image_norm);

        equalizeHist(mat_src, mat_src);

        algmdl.haar_cascade.detectMultiScale(
            mat_src, detect_pos, levels, detect_weights
            ,algmdl.scalefactor, algmdl.haar_minneighbors,algmdl.haar_flags
            , Size(algmdl.haar_minsize,algmdl.haar_minsize)
            , Size(algmdl.haar_maxsize,algmdl.haar_maxsize), true);

        algsec_img_show(cam, mat_src, detect_pos, detect_weights, "haar", algmdl);

    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Disabling secondary detection"));
        algmdl.method = 0;
    }
}

static void algsec_load_haar(ctx_algsec_model &algmdl){

    /* If loading fails, reset the method to invalidate detection */
    try {
        if (!algmdl.haar_cascade.load(algmdl.modelfile)){
            /* Loading failed, reset method*/
            algmdl.method = 0;
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
                ,algmdl.modelfile.c_str());
        };
    } catch ( cv::Exception& e ) {
        const char* err_msg = e.what();
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Error %s"),err_msg);
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO, _("Failed loading model %s")
            ,algmdl.modelfile.c_str());
        algmdl.method = 0;
    }
}

static void algsec_parms_default_hog(ctx_algsec_model &algmdl){
    algmdl.hog_padding = 8;
    algmdl.scalefactor = 1.05;
    algmdl.threshold_model = 1.4;
    algmdl.hog_winstride = 8;
    algmdl.threshold_motion = 50;
}

static void algsec_parms_default_haar(ctx_algsec_model &algmdl){
    algmdl.scalefactor = 1.1;
    algmdl.threshold_model = 2;
    algmdl.haar_flags = 0;
    algmdl.haar_maxsize = 8;
    algmdl.haar_minsize = 8;
    algmdl.haar_minneighbors = 8;
    algmdl.threshold_motion = 50;
}

/**Parse parm based upon colons*/
static void algsec_parms_parse_microdetail(std::string &vin, ctx_algsec_model &algmdl) {
    /* This is a place holder for now. It is thought that we will need some subparameters
     * associated with the configuration in the future...*/
    std::size_t st_colon, en_colon;
    std::string tmp;
    int indx1;

    return;
    if (vin == "") return;

    en_colon = -1;
    for (indx1=0;indx1<10;indx1++){
        st_colon = en_colon+1;
        en_colon = vin.find(':',st_colon);
        tmp = vin.substr(st_colon,en_colon-st_colon);
        //algmdl???? = tmp;
        if (en_colon == std::string::npos) return;
    }

    return;
}

/* modelfile=/home/whatever/model.xml,threshold_motion=50

/**Parse parm based upon equals*/
static void algsec_parms_parse_detail(std::string &vin, ctx_algsec_model &algmdl){

    std::size_t stpos;
    std::string tmpvar, tmpparm;
    int indx1;

    if (vin == "") return;

    stpos = vin.find('=');
    if (stpos != std::string::npos){

        tmpvar = vin.substr(0, stpos-1);
        tmpparm = vin.substr(stpos+1);
        mytrim(tmpvar);
        mytrim(tmpparm);

        if (tmpvar == "modelfile"){
            algmdl.modelfile = tmpparm;
        } else if (tmpvar == "config"){
            algsec_parms_parse_microdetail(tmpparm, algmdl);
        } else if (tmpvar == "imagetype"){
            algmdl.imagetype = tmpparm;
        } else if (tmpvar == "rotate"){
            algmdl.rotate = std::atoi(tmpparm.c_str());
        } else if (tmpvar == "scale"){
            algmdl.scalefactor = std::atof(tmpparm.c_str());
        } else if (tmpvar == "threshold_model"){
            algmdl.threshold_model= std::atoi(tmpparm.c_str());
        } else if (tmpvar == "threshold_motion"){
            algmdl.threshold_motion= std::atoi(tmpparm.c_str());
        }

        /* Hog specific parms below */
        /* These need edits on acceptable values.  Is this full list?*/
        if (tmpvar == "winstride"){
            algmdl.hog_winstride = std::atoi(tmpparm.c_str());
        } else if (tmpvar == "padding"){
            algmdl.hog_padding= std::atoi(tmpparm.c_str());
        }

        /* Haar specific parms below */
        /* These need edits on acceptable values.  Is this full list?*/
        if (tmpvar == "minneighbors"){
            algmdl.haar_minneighbors = std::atoi(tmpparm.c_str());
        } else if (tmpvar == "flags"){
            algmdl.haar_flags = std::atoi(tmpparm.c_str());
        } else if (tmpvar == "minsize"){
            algmdl.haar_minsize = std::atoi(tmpparm.c_str());
        } else if (tmpvar == "maxsize"){
            algmdl.haar_maxsize= std::atoi(tmpparm.c_str());
        }

    }

    return;
}

/**Parse parms based upon commas */
static void algsec_parms_parse(ctx_cam *cam){

    std::size_t st_comma, en_comma;
    std::string tmp;

    cam->algsec->models.method = cam->conf->secondary_method;
    cam->algsec->models.config = cam->conf->secondary_config;

    switch (cam->algsec->models.method) {
    case 1:     //Haar Method
        algsec_parms_default_haar(cam->algsec->models);
        break;
    case 2:     //HoG Method
        algsec_parms_default_hog(cam->algsec->models);
        break;
    default:
        cam->algsec->models.method  = 0;
        break;
    }

    if (cam->algsec->models.config != ""){
        st_comma = 0;
        en_comma = cam->algsec->models.config.find(',', st_comma);
        while (en_comma != std::string::npos){
            tmp = cam->algsec->models.config.substr(st_comma, en_comma - st_comma);
            algsec_parms_parse_detail(tmp, cam->algsec->models);
            st_comma = en_comma + 1;
            en_comma = cam->algsec->models.config.find(',', st_comma);
        }
        tmp = cam->algsec->models.config.substr(st_comma);
        algsec_parms_parse_detail(tmp, cam->algsec->models);
    }

}

/**Load the parms from the config to algsec struct */
static int algsec_load_parms(ctx_cam *cam){

    if (cam->conf->secondary_method == 0){
        cam->algsec->models.method = 0;
        return -1;
    }

    cam->algsec->height = cam->imgs.height;
    cam->algsec->width = cam->imgs.width;

    cam->algsec->frame_interval = cam->conf->secondary_interval;
    cam->algsec->frame_cnt = cam->algsec->frame_interval;
    cam->algsec->image_norm = (unsigned char*)mymalloc(cam->imgs.size_norm);
    cam->algsec->frame_missed = 0;
    cam->algsec->too_slow = 0;
    cam->algsec->detecting = false;

    /* We need to set the closing to true so that we can
    * know whether to shutdown the handler when we deinit
    */
    cam->algsec->closing = true;

    algsec_parms_parse(cam);

    return 0;
}

/**If possible preload the models and initialize them */
static int algsec_load_models(ctx_cam *cam){

    int indx;

    if (cam->algsec->models.method != 0){
        switch (cam->algsec->models.method) {
        case 1:     //Haar Method
            algsec_load_haar(cam->algsec->models);
            break;
        case 2:     //HoG Method
            //algsec_load_hog(cam->algsec->models);
            break;
        default:
            cam->algsec->models.method = 0;
        }
    }

    /* If model fails to load, it sets method to zero*/
    if (cam->algsec->models.method != 0){
        cam->algsec_inuse = TRUE;
        return 0;
    } else {
        cam->algsec_inuse = FALSE;
        return -1;
    }

}

/**Detection thread processing loop */
static void *algsec_handler(void *arg) {
    ctx_cam *cam = (ctx_cam*)arg;
    int indx;
    long interval;

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Starting."));

    cam->algsec->closing = false;

    interval = 1000000L / cam->conf->framerate;

    while (!cam->algsec->closing){
        if (cam->algsec->detecting){
            switch (cam->algsec->models.method) {
            case 1:     //Haar Method
                algsec_detect_haar(cam, cam->algsec->models);
                break;
            case 2:     //HoG Method
                algsec_detect_hog(cam, cam->algsec->models);
                break;
            }
            cam->algsec->detecting = false;
            /*Set the event based isdetected bool */
            if (cam->algsec->models.isdetected) cam->algsec->isdetected = true;
        } else {
            SLEEP(0,interval)
        }
    }
    cam->algsec->closing = false;
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Exiting."));
    pthread_exit(NULL);

}

/**Start the detection thread*/
static void algsec_start_handler(ctx_cam *cam){

    int retcd, indx;
    pthread_attr_t handler_attribute;

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    retcd = pthread_create(&cam->algsec->threadid, &handler_attribute, &algsec_handler, cam);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("Error starting algsec handler thread"));
        cam->algsec->models.method = 0;
    }
    pthread_attr_destroy(&handler_attribute);
    return;

}

#endif

void algsec_init(ctx_cam *cam){
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

        retcd = algsec_load_parms(cam);
        if (retcd == 0) retcd = algsec_load_models(cam);
        if (retcd == 0) algsec_start_handler(cam);

        mythreadname_set("ml",cam->threadnr,cam->conf->camera_name.c_str());
    #else
        (void)cam;
    #endif

}

/** Free algsec memory and shutdown thread */
void algsec_deinit(ctx_cam *cam){
    #ifdef HAVE_OPENCV
        int waitcnt = 0;

        if (!cam->algsec->closing) {
            cam->algsec->closing = true;
            while ((cam->algsec->closing) && (waitcnt <1000)){
                SLEEP(0,100000)
                waitcnt++;
            }
        }
        if (cam->algsec->image_norm != nullptr){
            free(cam->algsec->image_norm);
            cam->algsec->image_norm = nullptr;
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

void algsec_detect(ctx_cam *cam){
    /*This function runs on the camera thread */
    #ifdef HAVE_OPENCV
        if (cam->algsec->frame_cnt > 0) cam->algsec->frame_cnt--;

        if (cam->algsec->frame_cnt == 0){

            if (cam->algsec->detecting){
                cam->algsec->frame_missed++;
            } else {
                /*Get any previous detection results */

                /*Copy in a new image for processing */
                memcpy(cam->algsec->image_norm, cam->current_image->image_norm, cam->imgs.size_norm);

                /*Set the bool to detect on the new image and reset interval */
                cam->algsec->detecting = true;
                cam->algsec->frame_cnt = cam->algsec->frame_interval;
                if (cam->algsec->frame_missed >10){
                    if (cam->algsec->too_slow == 0) {
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Your computer is too slow for these settings."));
                   } else if (cam->algsec->too_slow == 10){
                        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                            ,_("Missed many frames for secondary detection.  Your computer is too slow."));
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


