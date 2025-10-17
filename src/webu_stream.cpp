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
#include "allcam.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "alg_sec.hpp"
#include "jpegutils.hpp"

static ssize_t webu_mjpeg_response (void *cls, uint64_t pos, char *buf, size_t max)
{
    cls_webu_stream *webu_stream = (cls_webu_stream *)cls;
    (void)pos;
    return webu_stream->mjpeg_response(buf, max);
}

void cls_webu_stream::set_fps()
{
    if (webua->device_id == 0) {
        stream_fps = app->cfg->stream_maxrate;
        return;
    }
    if (webua->camindx >= app->cam_list.size()) {
        stream_fps = 1;
    } else if ((webua->cam->detecting_motion == false) &&
        (app->cam_list[webua->camindx]->cfg->stream_motion)) {
        stream_fps = 1;
    } else {
        stream_fps = app->cam_list[webua->camindx]->cfg->stream_maxrate;
    }
}

/* Sleep required time to get to the user requested framerate for the stream */
void cls_webu_stream::delay()
{
    long   stream_rate;
    struct timespec time_curr;
    long   stream_delay;

    if (check_finish()) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &time_curr);

    /* The stream rate MUST be less than 1000000000 otherwise undefined behaviour
     * will occur with the SLEEP function.
     */
    stream_delay = ((time_curr.tv_nsec - time_last.tv_nsec)) +
        ((time_curr.tv_sec - time_last.tv_sec)*1000000000);
    if (stream_delay < 0)  {
        stream_delay = 0;
    }
    if (stream_delay > 1000000000 ) {
        stream_delay = 1000000000;
    }

    if (stream_fps >= 1) {
        stream_rate = ( (1000000000 / stream_fps) - stream_delay);
        if ((stream_rate > 0) && (stream_rate < 1000000000)) {
            SLEEP(0,stream_rate);
        } else if (stream_rate == 1000000000) {
            SLEEP(1,0);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &time_last);
}

void cls_webu_stream::one_buffer()
{
    if (webua->cam == NULL) {
        return;
    }
    if (resp_size < (size_t)webua->cam->imgs.size_norm) {
        if (resp_image != NULL) {
            myfree(resp_image);
        }
        resp_image = (unsigned char*) mymalloc((uint)webua->cam->imgs.size_norm);
        memset(resp_image,'\0', (uint)webua->cam->imgs.size_norm);
        resp_size = (uint)webua->cam->imgs.size_norm;
        resp_used = 0;
    }
}

void cls_webu_stream::all_buffer()
{
    if (resp_size < (size_t)app->allcam->all_sizes.dst_sz) {
        if (resp_image != nullptr) {
            myfree(resp_image);
        }
        resp_size = (uint)app->allcam->all_sizes.dst_sz;
        resp_image = (unsigned char*) mymalloc(resp_size);
        memset(resp_image, '\0', resp_size);
        resp_used = 0;
    }
}

bool cls_webu_stream::check_finish()
{
    if (webu->finish){
        resp_used = 0;
        return true;
    }
    if (webua->cam != NULL) {
        if ((webua->cam->finish == true) ||
            (webua->cam->passflag == false)) {
            resp_used = 0;
            return true;
        }
    }
    return false;
}

bool cls_webu_stream::all_ready()
{
    int indx, indx1;
    cls_camera *p_cam;

    for (indx=0; indx<app->cam_cnt; indx++) {
        p_cam = app->cam_list[indx];
        if ((p_cam->device_status == STATUS_OPENED) &&
            (p_cam->passflag == false)) {
            indx1 = 0;
            while (indx1 < 1000) {
                SLEEP(0, 1000);
                if (p_cam->passflag) {
                    break;
                }
                indx1++;
            }
            if (p_cam->passflag == false) {
                MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                    , "Camera %d not ready", p_cam->cfg->device_id);
                return false;
            }
        }
    }
    if ((webua->app->allcam->all_sizes.dst_h == 0) ||
        (webua->app->allcam->all_sizes.dst_w == 0)) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO, "All cameras not ready");
            return false;
    }

    return true;
}

void cls_webu_stream::mjpeg_all_img()
{
    char resp_head[80];
    int  header_len;
    ctx_stream_data *strm;

    if (check_finish()) {
        return;
    }

    if (all_ready() == false) {
        return;
    }

    all_buffer();

    memset(resp_image, '\0', resp_size);

    /* Assign to a local pointer the stream we want */
    if (webua->app == NULL) {
        return;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webua->app->allcam->stream.norm;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webua->app->allcam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webua->app->allcam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webua->app->allcam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webua->app->allcam->stream.secondary;
    } else {
        return;
    }

    /* Copy jpg from the motion loop thread */
    pthread_mutex_lock(&webua->app->allcam->stream.mutex);
        set_fps();
        if (strm->jpg_data == NULL) {
            pthread_mutex_unlock(&webua->app->allcam->stream.mutex);
            return;
        }
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9d\r\n\r\n"
            ,strm->jpg_sz);
        memcpy(resp_image, resp_head, (uint)header_len);
        memcpy(resp_image + header_len
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(resp_image + header_len + strm->jpg_sz,"\r\n",2);
        resp_used =(uint)(header_len + strm->jpg_sz + 2);
        strm->consumed = true;
    pthread_mutex_unlock(&webua->app->allcam->stream.mutex);

}

void cls_webu_stream::mjpeg_one_img()
{
    char resp_head[80];
    int  header_len;
    ctx_stream_data *strm;

    if (check_finish()) {
        return;
    }

    memset(resp_image, '\0', resp_size);

    /* Assign to a local pointer the stream we want */
    if (webua->cam == NULL) {
        return;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webua->cam->stream.norm;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webua->cam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webua->cam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webua->cam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webua->cam->stream.secondary;
    } else {
        return;
    }

    /* Copy jpg from the motion loop thread */
    pthread_mutex_lock(&webua->cam->stream.mutex);
        set_fps();
        if (strm->jpg_data == NULL) {
            pthread_mutex_unlock(&webua->cam->stream.mutex);
            return;
        }
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9d\r\n\r\n"
            ,strm->jpg_sz);
        memcpy(resp_image, resp_head, (uint)header_len);
        memcpy(resp_image + header_len
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(resp_image + header_len + strm->jpg_sz,"\r\n",2);
        resp_used =(uint)(header_len + strm->jpg_sz + 2);
        strm->consumed = true;
    pthread_mutex_unlock(&webua->cam->stream.mutex);

}

ssize_t cls_webu_stream::mjpeg_response (char *buf, size_t max)
{
    size_t sent_bytes;

    if (check_finish()) {
        return -1;
    }

    if ((stream_pos == 0) || (resp_used == 0)) {

        delay();

        stream_pos = 0;
        resp_used = 0;

        if (webua->device_id == 0) {
            mjpeg_all_img();
        } else {
            mjpeg_one_img();
        }

        if (resp_used == 0) {
            return 0;
        }
    }

    if ((resp_used - stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = resp_used - stream_pos;
    }

    memcpy(buf, resp_image + stream_pos, sent_bytes);

    stream_pos = stream_pos + sent_bytes;
    if (stream_pos >= resp_used) {
        stream_pos = 0;
    }

    return (ssize_t)sent_bytes;
}

/* Increment the all camera stream counters */
void cls_webu_stream::all_cnct()
{
    ctx_stream_data *strm;
    int indx_cam;

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
            strm = &app->cam_list[indx_cam]->stream.sub;
        } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
            strm = &app->cam_list[indx_cam]->stream.motion;
        } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
            strm = &app->cam_list[indx_cam]->stream.source;
        } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
            strm = &app->cam_list[indx_cam]->stream.secondary;
        } else {
            strm = &app->cam_list[indx_cam]->stream.norm;
        }
        pthread_mutex_lock(&app->cam_list[indx_cam]->stream.mutex);
            strm->all_cnct++;
        pthread_mutex_unlock(&app->cam_list[indx_cam]->stream.mutex);
    }
    if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &app->allcam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &app->allcam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &app->allcam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &app->allcam->stream.secondary;
    } else {
        strm = &app->allcam->stream.norm;
    }
    pthread_mutex_lock(&app->allcam->stream.mutex);
        strm->all_cnct++;
    pthread_mutex_unlock(&app->allcam->stream.mutex);

}

/* Obtain the current image for the camera.*/
void cls_webu_stream::static_all_img()
{
    ctx_stream_data *strm;

    if (check_finish()) {
        return;
    }

    if (all_ready() == false) {
        return;
    }

    all_buffer();

    resp_used = 0;
    memset(resp_image, '\0', resp_size);

    /* Assign to a local pointer the stream we want */
    if (webua->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webua->app->allcam->stream.norm;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webua->app->allcam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webua->app->allcam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webua->app->allcam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webua->app->allcam->stream.secondary;
    } else {
        return;
    }

    pthread_mutex_lock(&webua->app->allcam->stream.mutex);
        if (strm->jpg_data == NULL) {
            pthread_mutex_unlock(&webua->app->allcam->stream.mutex);
            return;
        }
        memcpy(resp_image
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        resp_used =(uint)strm->jpg_sz;
        strm->consumed = true;
    pthread_mutex_unlock(&webua->app->allcam->stream.mutex);

}

/* Increment the jpg stream counters */
void cls_webu_stream::jpg_cnct()
{
    ctx_stream_data *strm;

    if (webua->cam == NULL) {
        return;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webua->cam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webua->cam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webua->cam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webua->cam->stream.secondary;
    } else {
        strm = &webua->cam->stream.norm;
    }

    pthread_mutex_lock(&webua->cam->stream.mutex);
        strm->jpg_cnct++;
    pthread_mutex_unlock(&webua->cam->stream.mutex);


    if (strm->jpg_cnct == 1) {
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }

}

/* Obtain the current image for the camera.*/
void cls_webu_stream::static_one_img()
{
    ctx_stream_data *strm;

    one_buffer();

    resp_used = 0;
    memset(resp_image, '\0', resp_size);

    /* Assign to a local pointer the stream we want */
    if (webua->cam == NULL) {
        return;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webua->cam->stream.norm;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webua->cam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webua->cam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webua->cam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webua->cam->stream.secondary;
    } else {
        return;
    }

    pthread_mutex_lock(&webua->cam->stream.mutex);
        if (strm->jpg_data == NULL) {
            pthread_mutex_unlock(&webua->cam->stream.mutex);
            return;
        }
        memcpy(resp_image
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        resp_used =(uint)strm->jpg_sz;
        strm->consumed = true;
    pthread_mutex_unlock(&webua->cam->stream.mutex);

}

/* Increment the transport stream counters */
void cls_webu_stream::ts_cnct()
{
    ctx_stream_data *strm;

    if (webua->cam == NULL) {
        return;
    } else if (webua->cnct_type == WEBUI_CNCT_TS_SUB) {
        strm = &webua->cam->stream.sub;
    } else if (webua->cnct_type == WEBUI_CNCT_TS_MOTION) {
        strm = &webua->cam->stream.motion;
    } else if (webua->cnct_type == WEBUI_CNCT_TS_SOURCE) {
        strm = &webua->cam->stream.source;
    } else if (webua->cnct_type == WEBUI_CNCT_TS_SECONDARY) {
        strm = &webua->cam->stream.secondary;
    } else {
        strm = &webua->cam->stream.norm;
    }
    pthread_mutex_lock(&webua->cam->stream.mutex);
        strm->ts_cnct++;
    pthread_mutex_unlock(&webua->cam->stream.mutex);

    if (strm->ts_cnct == 1) {
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }
}

/* Assign the type of stream that is being answered*/
void cls_webu_stream::set_cnct_type()
{
    if (webua->uri_cmd1 == "mpegts") {
        if (webua->uri_cmd2 == "stream") {
            webua->cnct_type = WEBUI_CNCT_TS_FULL;
        } else if (webua->uri_cmd2 == "substream") {
            webua->cnct_type = WEBUI_CNCT_TS_SUB;
        } else if (webua->uri_cmd2 == "motion") {
            webua->cnct_type = WEBUI_CNCT_TS_MOTION;
        } else if (webua->uri_cmd2 == "source") {
            webua->cnct_type = WEBUI_CNCT_TS_SOURCE;
        } else if (webua->uri_cmd2 == "secondary") {
            if (webua->cam == NULL) {
                webua->cnct_type = WEBUI_CNCT_UNKNOWN;
            } else {
                if (webua->cam->algsec->method != "none") {
                    webua->cnct_type = WEBUI_CNCT_TS_SECONDARY;
                } else {
                    webua->cnct_type = WEBUI_CNCT_UNKNOWN;
                }
            }
        } else if (webua->uri_cmd2 == "") {
            webua->cnct_type = WEBUI_CNCT_TS_FULL;
        } else {
            webua->cnct_type = WEBUI_CNCT_UNKNOWN;
        }
    } else {
        if (webua->uri_cmd2 == "stream") {
            webua->cnct_type = WEBUI_CNCT_JPG_FULL;
        } else if (webua->uri_cmd2 == "substream") {
            webua->cnct_type = WEBUI_CNCT_JPG_SUB;
        } else if (webua->uri_cmd2 == "motion") {
            webua->cnct_type = WEBUI_CNCT_JPG_MOTION;
        } else if (webua->uri_cmd2 == "source") {
            webua->cnct_type = WEBUI_CNCT_JPG_SOURCE;
        } else if (webua->uri_cmd2 == "secondary") {
            if (webua->cam == NULL) {
                webua->cnct_type = WEBUI_CNCT_UNKNOWN;
            } else {
                if (webua->cam->algsec->method != "none") {
                    webua->cnct_type = WEBUI_CNCT_JPG_SECONDARY;
                } else {
                    webua->cnct_type = WEBUI_CNCT_UNKNOWN;
                }
            }
        } else if (webua->uri_cmd2 == "") {
            webua->cnct_type = WEBUI_CNCT_JPG_FULL;
        } else {
            webua->cnct_type = WEBUI_CNCT_UNKNOWN;
        }
    }
}

mhdrslt cls_webu_stream::stream_mjpeg()
{
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    clock_gettime(CLOCK_MONOTONIC, &time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        , &webu_mjpeg_response, (void *)this, NULL);
    if (response == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webu->wb_headers->params_cnt > 0) {
        for (indx=0;indx<webu->wb_headers->params_cnt;indx++) {
            MHD_add_response_header (response
                , webu->wb_headers->params_array[indx].param_name.c_str()
                , webu->wb_headers->params_array[indx].param_value.c_str());
        }
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE
        , "multipart/x-mixed-replace; boundary=BoundaryString");

    retcd = MHD_queue_response(webua->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Create the response for the static image request*/
mhdrslt cls_webu_stream::stream_static()
{
    mhdrslt retcd;
    struct MHD_Response *response;
    char resp_head[20];
    int indx;

    if (resp_used == 0) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Could not get image to stream."));
        return MHD_NO;
    }

    response = MHD_create_response_from_buffer (
            resp_size,(void *)resp_image
            , MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webu->wb_headers->params_cnt > 0) {
        for (indx=0;indx<webu->wb_headers->params_cnt;indx++) {
            MHD_add_response_header (response
                , webu->wb_headers->params_array[indx].param_name.c_str()
                , webu->wb_headers->params_array[indx].param_value.c_str());
        }
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg");
    snprintf(resp_head, 20, "%9ld\r\n\r\n",(long)resp_used);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_head);

    retcd = MHD_queue_response (webua->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Entry point for answering stream*/
void cls_webu_stream::main()
{
    mhdrslt retcd = MHD_NO;

    if (check_finish()) {
        return;
    }

    set_cnct_type();

    if (webua->uri_cmd1 == "static") {
        if (webua->device_id > 0) {
            jpg_cnct();
            static_one_img();
        } else {
            all_cnct();
            static_all_img();
        }
        retcd = stream_static();
    } else if (webua->uri_cmd1 == "mjpg") {
        if (webua->device_id > 0) {
            jpg_cnct();
            one_buffer();
        } else {
            all_cnct();
            all_buffer();
        }
        retcd = stream_mjpeg();
    } else if (webua->uri_cmd1 == "mpegts") {
        if (webua->device_id > 0) {
            ts_cnct();
        } else {
            all_cnct();
        }
        if (webu_mpegts == nullptr){
            webu_mpegts = new cls_webu_mpegts(webua, this);
        }
        retcd = webu_mpegts->main();
        if (retcd == MHD_NO) {
            mydelete(webu_mpegts);
        }
    }

    if (retcd == MHD_NO) {
        webua->bad_request();
    }

}

cls_webu_stream::cls_webu_stream(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webua  = p_webua;
    webu_mpegts = nullptr;

    resp_image    = nullptr;
    resp_size     = 0;
    resp_used     = 0;

    stream_pos = 0;
    stream_fps = 1;

}

cls_webu_stream::~cls_webu_stream()
{
    mydelete(webu_mpegts);

    myfree(resp_image);

}
