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

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_common.hpp"
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

bool cls_webu_stream::all_ready()
{
    int indx, indx1;
    ctx_dev *p_cam;

    for (indx=0; indx<app->cam_cnt; indx++) {
        p_cam = app->cam_list[indx];
        if (p_cam->passflag == false) {
            app->all_sizes->reset = true;
            indx1 = 0;
            while (indx1 < 1000) {
                SLEEP(0, 1000);
                if (p_cam->passflag) {
                    break;
                }
                indx1++;
            }
            if (p_cam->passflag == false) {
                return false;
            }
        }
    }
    return true;
}

void cls_webu_stream::mjpeg_all_img()
{
    int header_len, jpg_sz;
    char resp_head[80];
    ctx_all_sizes *all_sz;
    unsigned char *jpg_data;

    if (webuc->check_finish()) {
        return;
    }

    if (all_ready() == false) {
        return;
    }

    webuc->all_sizes();
    webuc->all_buffer();
    webuc->all_getimg();

    all_sz = app->all_sizes;

    jpg_data = (unsigned char*) mymalloc((uint)all_sz->img_sz);

    jpg_sz = jpgutl_put_yuv420p(jpg_data, all_sz->img_sz
        , webuc->all_img_data, all_sz->width, all_sz->height
        , 70, NULL,NULL,NULL);
    webuc->stream_fps = 1;

    header_len = snprintf(resp_head, 80
        ,"--BoundaryString\r\n"
        "Content-type: image/jpeg\r\n"
        "Content-Length: %9d\r\n\r\n"
        ,jpg_sz);
    memcpy(webuc->resp_image, resp_head, (uint)header_len);
    memcpy(webuc->resp_image + header_len, jpg_data, (uint)jpg_sz);
    memcpy(webuc->resp_image + header_len + jpg_sz,"\r\n",(uint)2);
    webuc->resp_used =(uint)(header_len + jpg_sz + 2);
    myfree(jpg_data);

}

void cls_webu_stream::mjpeg_one_img()
{
    char resp_head[80];
    int  header_len;
    ctx_stream_data *strm;

    if (webuc->check_finish()) {
        return;
    }

    memset(webuc->resp_image, '\0', webuc->resp_size);

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
        webuc->set_fps();
        if (strm->jpg_data == NULL) {
            pthread_mutex_unlock(&webua->cam->stream.mutex);
            return;
        }
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9d\r\n\r\n"
            ,strm->jpg_sz);
        memcpy(webuc->resp_image, resp_head, (uint)header_len);
        memcpy(webuc->resp_image + header_len
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(webuc->resp_image + header_len + strm->jpg_sz,"\r\n",2);
        webuc->resp_used =(uint)(header_len + strm->jpg_sz + 2);
        strm->consumed = true;
    pthread_mutex_unlock(&webua->cam->stream.mutex);

}

ssize_t cls_webu_stream::mjpeg_response (char *buf, size_t max)
{
    size_t sent_bytes;

    if (webuc->check_finish()) {
        return -1;
    }

    if ((stream_pos == 0) || (webuc->resp_used == 0)) {

        webuc->delay();

        stream_pos = 0;
        webuc->resp_used = 0;

        if (webua->device_id == 0) {
            mjpeg_all_img();
        } else {
            mjpeg_one_img();
        }

        if (webuc->resp_used == 0) {
            return 0;
        }
    }

    if ((webuc->resp_used - stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webuc->resp_used - stream_pos;
    }

    memcpy(buf, webuc->resp_image + stream_pos, sent_bytes);

    stream_pos = stream_pos + sent_bytes;
    if (stream_pos >= webuc->resp_used) {
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
}

/* Obtain the current image for the camera.*/
void cls_webu_stream::static_all_img()
{
    ctx_all_sizes *all_sz;
    unsigned char *jpg_data;

    if (webuc->check_finish()) {
        return;
    }

    if (all_ready() == false) {
        return;
    }
    webuc->all_sizes();
    webuc->all_buffer();
    webuc->all_getimg();

    all_sz = app->all_sizes;
    jpg_data = (unsigned char*)mymalloc((uint)all_sz->img_sz);

    webuc->resp_used = (uint)jpgutl_put_yuv420p(jpg_data
        , all_sz->img_sz, webuc->all_img_data, all_sz->width
        , all_sz->height, 70, NULL,NULL,NULL);
    memcpy(webuc->resp_image, jpg_data, webuc->resp_used);
    myfree(jpg_data);
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

    webuc->one_buffer();

    webuc->resp_used = 0;
    memset(webuc->resp_image, '\0', webuc->resp_size);

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
        memcpy(webuc->resp_image
            , strm->jpg_data
            , (uint)strm->jpg_sz);
        webuc->resp_used =(uint)strm->jpg_sz;
        strm->consumed = true;
    pthread_mutex_unlock(&webua->cam->stream.mutex);

}

/* Determine whether the user specified a valid URL for the particular port */
int cls_webu_stream::checks()
{
    pthread_mutex_lock(&app->mutex_camlst);
        if (webua->device_id < 0) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), webua->url.c_str());
            pthread_mutex_unlock(&app->mutex_camlst);
            return -1;
        }
        if ((webua->device_id > 0) && (webua->cam == NULL)) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), webua->url.c_str());
            pthread_mutex_unlock(&app->mutex_camlst);
            return -1;
        }
        if (webuc->check_finish()) {
            return -1;
        }
    pthread_mutex_unlock(&app->mutex_camlst);

    return 0;
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
    p_lst *lst = &webu->wb_headers->params_array;
    p_it it;

    clock_gettime(CLOCK_MONOTONIC, &webuc->time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        , &webu_mjpeg_response, (void *)this, NULL);
    if (response == NULL) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webu->wb_headers->params_count > 0) {
        for (it = lst->begin(); it != lst->end(); it++) {
            MHD_add_response_header (response
                , it->param_name.c_str(), it->param_value.c_str());
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
    char resp_used[20];
    p_lst *lst = &webu->wb_headers->params_array;
    p_it it;

    if (webuc->resp_used == 0) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Could not get image to stream."));
        return MHD_NO;
    }

    response = MHD_create_response_from_buffer (
            webuc->resp_size,(void *)webuc->resp_image
            , MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webu->wb_headers->params_count > 0) {
        for (it = lst->begin(); it != lst->end(); it++) {
            MHD_add_response_header (response
                , it->param_name.c_str(), it->param_value.c_str());
        }
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg");
    snprintf(resp_used, 20, "%9ld\r\n\r\n",(long)webuc->resp_used);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_used);

    retcd = MHD_queue_response (webua->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Entry point for answering stream*/
mhdrslt cls_webu_stream::main()
{
    mhdrslt retcd;

    if (webuc->check_finish()) {
        return MHD_NO;
    }

    if (webua->cam != NULL) {
        if ((webua->cam->passflag == false) || (webua->cam->finish_dev)) {
            return MHD_NO;
        }
    }

    set_cnct_type();

    if (checks() == -1) {
        webua->bad_request();
        return MHD_NO;
    }

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
            webuc->one_buffer();
        } else {
            all_cnct();
            webuc->all_buffer();
        }
        retcd = stream_mjpeg();
    } else if (webua->uri_cmd1 == "mpegts") {
        if (webua->device_id > 0) {
            ts_cnct();
        } else {
            all_cnct();
        }
        if (webu_mpegts == nullptr){
            webu_mpegts = new cls_webu_mpegts(webua);
        }
        retcd = webu_mpegts->main();
        if (retcd == MHD_NO) {
            delete webu_mpegts;
            webu_mpegts = nullptr;
        }

    } else {
        retcd = MHD_NO;
    }

    return retcd;
}

cls_webu_stream::cls_webu_stream(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webuc  = new cls_webu_common(p_webua);
    webua  = p_webua;
    webu_mpegts = nullptr;
    stream_pos = 0;
}

cls_webu_stream::~cls_webu_stream()
{
    if (webu_mpegts != nullptr){
        delete webu_mpegts;
    }
    if (webuc != nullptr){
        delete webuc;
    }
}
