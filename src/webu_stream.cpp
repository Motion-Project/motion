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
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "alg_sec.hpp"
#include "jpegutils.hpp"

/* Allocate buffers if needed */
void webu_stream_checkbuffers(ctx_webui *webui)
{
    if (webui->cam == NULL) {
        return;
    }
    if (webui->resp_size < (size_t)webui->cam->imgs.size_norm) {
        if (webui->resp_image != NULL) {
            myfree(&webui->resp_image);
        }
        webui->resp_image = (unsigned char*) mymalloc(webui->cam->imgs.size_norm);
        memset(webui->resp_image,'\0',webui->cam->imgs.size_norm);
        webui->resp_size = webui->cam->imgs.size_norm;
        webui->resp_used = 0;
    }
}

void webu_stream_allsize(ctx_webui *webui)
{
    int indx, row, col;
    int chk_sz, chk_w, mx_col, mx_row;
    int mx_h, mx_w;

    /* Calculate total image size*/
    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
        if (mx_row < webui->motapp->cam_list[indx]->all_loc.row) {
            mx_row = webui->motapp->cam_list[indx]->all_loc.row;
        }
        if (mx_col < webui->motapp->cam_list[indx]->all_loc.col) {
            mx_col = webui->motapp->cam_list[indx]->all_loc.col;
        }
    }

    webui->motapp->all_img->width = 0;
    webui->motapp->all_img->height = 0;
    for (row=1; row<=mx_row; row++) {
        chk_sz = 0;
        mx_h = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
                if ((row == webui->motapp->cam_list[indx]->all_loc.row) &&
                    (col == webui->motapp->cam_list[indx]->all_loc.col)) {
                    webui->motapp->cam_list[indx]->all_loc.offset_col = chk_sz;
                    chk_sz += webui->motapp->cam_list[indx]->imgs.width;
                    if (mx_h < webui->motapp->cam_list[indx]->imgs.height) {
                        mx_h = webui->motapp->cam_list[indx]->imgs.height;
                    }
                }
            }
        }
        /* Align/center vert. the images in each row*/
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            if (webui->motapp->cam_list[indx]->all_loc.row == row) {
                webui->motapp->cam_list[indx]->all_loc.offset_row =
                    webui->motapp->all_img->height +
                    ((mx_h -webui->motapp->cam_list[indx]->imgs.height)/2) ;
            }
        }
        webui->motapp->all_img->height += mx_h;
        if (webui->motapp->all_img->width < chk_sz) {
            webui->motapp->all_img->width = chk_sz;
        }
    }

    /* Align/center horiz. the images within each column area */
    chk_w = 0;
    for (col=1; col<=mx_col; col++) {
        chk_sz = 0;
        mx_w = 0;
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            if (webui->motapp->cam_list[indx]->all_loc.col == col) {
                if (webui->motapp->cam_list[indx]->all_loc.offset_col < chk_w) {
                    webui->motapp->cam_list[indx]->all_loc.offset_col = chk_w;
                }
                if (chk_sz < webui->motapp->cam_list[indx]->all_loc.offset_col) {
                    chk_sz = webui->motapp->cam_list[indx]->all_loc.offset_col;
                }
                if (mx_w < webui->motapp->cam_list[indx]->imgs.width) {
                    mx_w = webui->motapp->cam_list[indx]->imgs.width;
                }
            }
        }
        for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
            if (webui->motapp->cam_list[indx]->all_loc.col == col) {
                webui->motapp->cam_list[indx]->all_loc.offset_col = chk_sz +
                    ((mx_w - webui->motapp->cam_list[indx]->imgs.width) /2) ;
            }
        }
        chk_w = mx_w + chk_sz;
        if (webui->motapp->all_img->width < chk_w) {
            webui->motapp->all_img->width = chk_w;
        }
    }


    webui->motapp->all_img->imgsz =((
        webui->motapp->all_img->height *
        webui->motapp->all_img->width * 3)/2);

    /*
    for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , "row %d col %d offset row %d offset col %d"
            , webui->motapp->cam_list[indx]->all_loc.row
            , webui->motapp->cam_list[indx]->all_loc.col
            , webui->motapp->cam_list[indx]->all_loc.offset_row
            , webui->motapp->cam_list[indx]->all_loc.offset_col);
    }
    */

}

void webu_stream_allbuffer(ctx_webui *webui)
{
    if (webui->resp_size < (size_t)webui->motapp->all_img->imgsz) {
        if (webui->resp_image != NULL) {
            myfree(&webui->resp_image);
        }
        webui->resp_size = webui->motapp->all_img->imgsz;
        webui->resp_image = (unsigned char*) mymalloc(webui->resp_size);
        memset(webui->resp_image, '\0', webui->resp_size);
        webui->resp_used = 0;
    }
}

static bool webu_stream_check_finish(ctx_webui *webui)
{
    if (webui->motapp->webcontrol_finish){
        webui->resp_used = 0;
        return true;
    }
    if (webui->cam != NULL) {
        if ((webui->cam->finish_dev == true) ||
            (webui->cam->passflag == false)) {
            webui->resp_used = 0;
            return true;
        }
    }
    return false;
}

/* Sleep required time to get to the user requested framerate for the stream */
void webu_stream_delay(ctx_webui *webui)
{
    long   stream_rate;
    struct timespec time_curr;
    long   stream_delay;

    if (webu_stream_check_finish(webui)) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &time_curr);

    /* The stream rate MUST be less than 1000000000 otherwise undefined behaviour
     * will occur with the SLEEP function.
     */
    stream_delay = ((time_curr.tv_nsec - webui->time_last.tv_nsec)) +
        ((time_curr.tv_sec - webui->time_last.tv_sec)*1000000000);
    if (stream_delay < 0)  {
        stream_delay = 0;
    }
    if (stream_delay > 1000000000 ) {
        stream_delay = 1000000000;
    }

    if (webui->stream_fps >= 1) {
        stream_rate = ( (1000000000 / webui->stream_fps) - stream_delay);
        if ((stream_rate > 0) && (stream_rate < 1000000000)) {
            SLEEP(0,stream_rate);
        } else if (stream_rate == 1000000000) {
            SLEEP(1,0);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

}

static void webu_stream_get_allimg(ctx_webui *webui)
{
    int a_y, a_u, a_v; /* all img y,u,v */
    int c_y, c_u, c_v; /* camera img y,u,v */
    char resp_head[80];
    int  header_len, indx, row;

    memset(webui->resp_image, '\0', webui->resp_size);

    myfree(&webui->motapp->all_img->image);
    myfree(&webui->motapp->all_img->jpeg_data);

    a_y = 0;
    a_u = (webui->motapp->all_img->width *webui->motapp->all_img->height);
    a_v = a_u + (a_u / 4);

    webui->motapp->all_img->image = (unsigned char*) mymalloc(
        (size_t)webui->motapp->all_img->imgsz);
    webui->motapp->all_img->jpeg_data = (unsigned char*) mymalloc(
        (size_t)webui->motapp->all_img->imgsz);

    memset(webui->motapp->all_img->image, 0x00, (size_t)a_u);
    memset(webui->motapp->all_img->image + a_u, 0x10, (size_t)(a_u/2));

    for (indx=0; indx<webui->motapp->cam_cnt; indx++) {
        a_y = (webui->motapp->cam_list[indx]->all_loc.offset_row *
            webui->motapp->all_img->width) +
            webui->motapp->cam_list[indx]->all_loc.offset_col;
        a_u =
            (webui->motapp->all_img->height * webui->motapp->all_img->width) +
            ((webui->motapp->cam_list[indx]->all_loc.offset_row/4) *
            webui->motapp->all_img->width) +
            (webui->motapp->cam_list[indx]->all_loc.offset_col / 2) ;

        a_v = a_u + ((webui->motapp->all_img->height *
            webui->motapp->all_img->width) / 4);

        /*
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , "r %d c %d a %d %d %d w %d h %d"
            , webui->motapp->cam_list[indx]->all_loc.offset_row
            , webui->motapp->cam_list[indx]->all_loc.offset_col
            , a_y, a_u, a_v
            , webui->motapp->all_img->width
            , webui->motapp->all_img->height);
        */
        c_y = 0;
        c_u = (webui->motapp->cam_list[indx]->imgs.width *
            webui->motapp->cam_list[indx]->imgs.height);
        c_v = c_u + (c_u / 4);

        for (row=0; row<webui->motapp->cam_list[indx]->imgs.height; row++) {
            memcpy(webui->motapp->all_img->image + a_y
                , webui->motapp->cam_list[indx]->current_image->image_norm + c_y
                , webui->motapp->cam_list[indx]->imgs.width);
            a_y += webui->motapp->all_img->width;
            c_y += webui->motapp->cam_list[indx]->imgs.width;
            if (row % 2) {
                memcpy(webui->motapp->all_img->image + a_u
                    , webui->motapp->cam_list[indx]->current_image->image_norm + c_u
                    , (webui->motapp->cam_list[indx]->imgs.width/2));
                //memset(webui->motapp->all_img->image + a_u, 0xFA
                //    , (webui->motapp->cam_list[indx]->imgs.width/2));
                a_u += (webui->motapp->all_img->width/2);
                c_u += (webui->motapp->cam_list[indx]->imgs.width/2);
                memcpy(webui->motapp->all_img->image + a_v
                    , webui->motapp->cam_list[indx]->current_image->image_norm + c_v
                    , (webui->motapp->cam_list[indx]->imgs.width/2));
                a_v += (webui->motapp->all_img->width/2);
                c_v += (webui->motapp->cam_list[indx]->imgs.width/2);
            }
        }
    }

    webui->motapp->all_img->jpeg_size = jpgutl_put_yuv420p(
        webui->motapp->all_img->jpeg_data
        , webui->motapp->all_img->imgsz
        , webui->motapp->all_img->image
        , webui->motapp->all_img->width
        , webui->motapp->all_img->height
        , 70, NULL,NULL,NULL);

    webui->stream_fps = 1;

    header_len = snprintf(resp_head, 80
        ,"--BoundaryString\r\n"
        "Content-type: image/jpeg\r\n"
        "Content-Length: %9ld\r\n\r\n"
        ,webui->motapp->all_img->jpeg_size);
    memcpy(webui->resp_image, resp_head, header_len);
    memcpy(webui->resp_image + header_len
        , webui->motapp->all_img->jpeg_data
        , webui->motapp->all_img->jpeg_size);
    /* Copy in the terminator after the jpg data at the end*/
    memcpy(webui->resp_image + header_len +
        webui->motapp->all_img->jpeg_size,"\r\n",2);
    webui->resp_used = header_len + webui->motapp->all_img->jpeg_size + 2;

    myfree(&webui->motapp->all_img->image);
    myfree(&webui->motapp->all_img->jpeg_data);

}

static void webu_stream_mjpeg_allimg(ctx_webui *webui)
{
    if (webu_stream_check_finish(webui)) {
        return;
    }
    webu_stream_allsize(webui);
    webu_stream_allbuffer(webui);
    webu_stream_get_allimg(webui);
}

static void webu_stream_mjpeg_getimg(ctx_webui *webui)
{
    long jpeg_size;
    char resp_head[80];
    int  header_len;
    ctx_stream_data *strm;

    if (webu_stream_check_finish(webui)) {
        return;
    }

    memset(webui->resp_image, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cam == NULL) {
        return;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webui->cam->stream.norm;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webui->cam->stream.secondary;
    } else {
        return;
    }

    /* Copy jpg from the motion loop thread */
    pthread_mutex_lock(&webui->cam->stream.mutex);
        if ((webui->cam->detecting_motion == false) &&
            (webui->motapp->cam_list[webui->camindx]->conf->stream_motion)) {
            webui->stream_fps = 1;
        } else {
            webui->stream_fps = webui->motapp->cam_list[webui->camindx]->conf->stream_maxrate;
        }
        if (strm->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return;
        }
        jpeg_size = strm->jpeg_size;
        header_len = snprintf(resp_head, 80
            ,"--BoundaryString\r\n"
            "Content-type: image/jpeg\r\n"
            "Content-Length: %9ld\r\n\r\n"
            ,jpeg_size);
        memcpy(webui->resp_image, resp_head, header_len);
        memcpy(webui->resp_image + header_len
            ,strm->jpeg_data
            ,jpeg_size);
        /* Copy in the terminator after the jpg data at the end*/
        memcpy(webui->resp_image + header_len + jpeg_size,"\r\n",2);
        webui->resp_used = header_len + jpeg_size + 2;
        strm->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

}

/* Callback function for mhd to get stream */
static ssize_t webu_stream_mjpeg_response (void *cls, uint64_t pos, char *buf, size_t max)
{
    ctx_webui *webui =(ctx_webui *)cls;
    size_t sent_bytes;
    (void)pos;

    if (webu_stream_check_finish(webui)) {
        return -1;
    }

    if ((webui->stream_pos == 0) || (webui->resp_used == 0)) {

        webu_stream_delay(webui);

        webui->stream_pos = 0;
        webui->resp_used = 0;

        if (webui->device_id == 0) {
            webu_stream_mjpeg_allimg(webui);
        } else {
            webu_stream_mjpeg_getimg(webui);
        }

        if (webui->resp_used == 0) {
            return 0;
        }
    }

    if ((webui->resp_used - webui->stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - webui->stream_pos;
    }

    memcpy(buf, webui->resp_image + webui->stream_pos, sent_bytes);

    webui->stream_pos = webui->stream_pos + sent_bytes;
    if (webui->stream_pos >= webui->resp_used) {
        webui->stream_pos = 0;
    }

    return sent_bytes;

}

/* Increment the all camera stream counters */
static void webu_stream_all_cnct(ctx_webui *webui)
{
    ctx_stream_data *strm;
    int indx_cam;

    for (indx_cam=0; indx_cam<webui->motapp->cam_cnt; indx_cam++) {
        if (webui->cnct_type == WEBUI_CNCT_JPG_SUB) {
            strm = &webui->motapp->cam_list[indx_cam]->stream.sub;
        } else if (webui->cnct_type == WEBUI_CNCT_JPG_MOTION) {
            strm = &webui->motapp->cam_list[indx_cam]->stream.motion;
        } else if (webui->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
            strm = &webui->motapp->cam_list[indx_cam]->stream.source;
        } else if (webui->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
            strm = &webui->motapp->cam_list[indx_cam]->stream.secondary;
        } else {
            strm = &webui->motapp->cam_list[indx_cam]->stream.norm;
        }
        pthread_mutex_lock(&webui->motapp->cam_list[indx_cam]->stream.mutex);
            strm->all_cnct++;
        pthread_mutex_unlock(&webui->motapp->cam_list[indx_cam]->stream.mutex);
    }
}

/* Obtain the current image for the camera.*/
static void webu_stream_static_allimg(ctx_webui *webui)
{
    ctx_stream_data *strm;

    webu_stream_all_cnct(webui);

    webui->resp_used = 0;

    memset(webui->resp_image, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cam == NULL) {
        return;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webui->cam->stream.norm;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webui->cam->stream.secondary;
    } else {
        return;
    }

    pthread_mutex_lock(&webui->cam->stream.mutex);
        if (strm->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return;
        }
        memcpy(webui->resp_image
            ,strm->jpeg_data
            ,strm->jpeg_size);
        webui->resp_used =strm->jpeg_size;
        strm->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

}

/* Increment the jpg stream counters */
static void webu_stream_jpg_cnct(ctx_webui *webui)
{
    ctx_stream_data *strm;

    if (webui->cam == NULL) {
        return;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webui->cam->stream.secondary;
    } else {
        strm = &webui->cam->stream.norm;
    }

    pthread_mutex_lock(&webui->cam->stream.mutex);
        strm->jpg_cnct++;
    pthread_mutex_unlock(&webui->cam->stream.mutex);


    if (strm->jpg_cnct == 1) {
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }

}

/* Obtain the current image for the camera.*/
static void webu_stream_static_getimg(ctx_webui *webui)
{
    ctx_stream_data *strm;

    webu_stream_jpg_cnct(webui);

    webu_stream_checkbuffers(webui);

    webui->resp_used = 0;
    memset(webui->resp_image, '\0', webui->resp_size);

    /* Assign to a local pointer the stream we want */
    if (webui->cam == NULL) {
        return;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_FULL) {
        strm = &webui->cam->stream.norm;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SUB) {
        strm = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_MOTION) {
        strm = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SOURCE) {
        strm = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_JPG_SECONDARY) {
        strm = &webui->cam->stream.secondary;
    } else {
        return;
    }

    pthread_mutex_lock(&webui->cam->stream.mutex);
        if (strm->jpeg_data == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return;
        }
        memcpy(webui->resp_image
            ,strm->jpeg_data
            ,strm->jpeg_size);
        webui->resp_used =strm->jpeg_size;
        strm->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

}

/* Determine whether the user specified a valid URL for the particular port */
static int webu_stream_checks(ctx_webui *webui)
{
    pthread_mutex_lock(&webui->motapp->mutex_camlst);
        if (webui->device_id < 0) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), webui->url.c_str());
            pthread_mutex_unlock(&webui->motapp->mutex_camlst);
            return -1;
        }
        if ((webui->device_id > 0) && (webui->cam == NULL)) {
            MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
                , _("Invalid camera specified: %s"), webui->url.c_str());
            pthread_mutex_unlock(&webui->motapp->mutex_camlst);
            return -1;
        }
        if (webu_stream_check_finish(webui)) {
            return -1;
        }
    pthread_mutex_unlock(&webui->motapp->mutex_camlst);

    return 0;
}

/* Increment the transport stream counters */
static void webu_stream_ts_cnct(ctx_webui *webui)
{
    ctx_stream_data *strm;

    if (webui->cam == NULL) {
        return;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SUB) {
        strm = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_MOTION) {
        strm = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SOURCE) {
        strm = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SECONDARY) {
        strm = &webui->cam->stream.secondary;
    } else {
        strm = &webui->cam->stream.norm;
    }
    pthread_mutex_lock(&webui->cam->stream.mutex);
        strm->ts_cnct++;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

    if (strm->ts_cnct == 1) {
        /* This is the first connection so we need to wait half a sec
         * so that the motion loop on the other thread can update image
         */
        SLEEP(0,500000000L);
    }
}

/* Assign the type of stream that is being answered*/
static void webu_stream_type(ctx_webui *webui)
{
    if (webui->uri_cmd1 == "mpegts") {
        if (webui->uri_cmd2 == "stream") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else if (webui->uri_cmd2 == "substream") {
            webui->cnct_type = WEBUI_CNCT_TS_SUB;
        } else if (webui->uri_cmd2 == "motion") {
            webui->cnct_type = WEBUI_CNCT_TS_MOTION;
        } else if (webui->uri_cmd2 == "source") {
            webui->cnct_type = WEBUI_CNCT_TS_SOURCE;
        } else if (webui->uri_cmd2 == "secondary") {
            if (webui->cam == NULL) {
                webui->cnct_type = WEBUI_CNCT_UNKNOWN;
            } else {
                if (webui->cam->algsec_inuse) {
                    webui->cnct_type = WEBUI_CNCT_TS_SECONDARY;
                } else {
                    webui->cnct_type = WEBUI_CNCT_UNKNOWN;
                }
            }
        } else if (webui->uri_cmd2 == "") {
            webui->cnct_type = WEBUI_CNCT_TS_FULL;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        }
    } else {
        if (webui->uri_cmd2 == "stream") {
            webui->cnct_type = WEBUI_CNCT_JPG_FULL;
        } else if (webui->uri_cmd2 == "substream") {
            webui->cnct_type = WEBUI_CNCT_JPG_SUB;
        } else if (webui->uri_cmd2 == "motion") {
            webui->cnct_type = WEBUI_CNCT_JPG_MOTION;
        } else if (webui->uri_cmd2 == "source") {
            webui->cnct_type = WEBUI_CNCT_JPG_SOURCE;
        } else if (webui->uri_cmd2 == "secondary") {
            if (webui->cam == NULL) {
                webui->cnct_type = WEBUI_CNCT_UNKNOWN;
            } else {
                if (webui->cam->algsec_inuse) {
                    webui->cnct_type = WEBUI_CNCT_JPG_SECONDARY;
                } else {
                    webui->cnct_type = WEBUI_CNCT_UNKNOWN;
                }
            }
        } else if (webui->uri_cmd2 == "") {
            webui->cnct_type = WEBUI_CNCT_JPG_FULL;
        } else {
            webui->cnct_type = WEBUI_CNCT_UNKNOWN;
        }
    }
}

static mhdrslt webu_stream_mjpeg(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 1024
        ,&webu_stream_mjpeg_response, webui, NULL);
    if (response == NULL) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->motapp->webcontrol_headers->params_count > 0) {
        for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
            MHD_add_response_header (response
                , webui->motapp->webcontrol_headers->params_array[indx].param_name
                , webui->motapp->webcontrol_headers->params_array[indx].param_value
            );
        }
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE
        , "multipart/x-mixed-replace; boundary=BoundaryString");

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Create the response for the static image request*/
static mhdrslt webu_stream_static(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    char resp_used[20];
    int indx;

    if (webui->resp_used == 0) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Could not get image to stream."));
        return MHD_NO;
    }

    response = MHD_create_response_from_buffer (webui->resp_size
        ,(void *)webui->resp_image, MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->motapp->webcontrol_headers->params_count > 0) {
        for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
            MHD_add_response_header (response
                , webui->motapp->webcontrol_headers->params_array[indx].param_name
                , webui->motapp->webcontrol_headers->params_array[indx].param_value
            );
        }
    }

    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "image/jpeg");
    snprintf(resp_used, 20, "%9ld\r\n\r\n",(long)webui->resp_used);
    MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_LENGTH, resp_used);

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;
}

/* Entry point for answering stream*/
mhdrslt webu_stream_main(ctx_webui *webui)
{
    mhdrslt retcd;

    if (webu_stream_check_finish(webui)) {
        return MHD_NO;
    }

    if (webui->cam != NULL) {
        if ((webui->cam->passflag == false) || (webui->cam->finish_dev)) {
            return MHD_NO;
        }
    }

    webu_stream_type(webui);

    if (webu_stream_checks(webui) == -1) {
        return MHD_NO;
    }

    if (webui->uri_cmd1 == "static") {
        if (webui->device_id > 0) {
            webu_stream_static_getimg(webui);
        } else {
            webu_stream_static_allimg(webui);
        }
        retcd = webu_stream_static(webui);
    } else if (webui->uri_cmd1 == "mjpg") {
        if (webui->device_id > 0) {
            webu_stream_jpg_cnct(webui);
            webu_stream_checkbuffers(webui);
        } else {
            webu_stream_all_cnct(webui);
        }
        retcd = webu_stream_mjpeg(webui);
    } else if (webui->uri_cmd1 == "mpegts") {
        if (webui->device_id > 0) {
            webu_stream_ts_cnct(webui);
            retcd = webu_mpegts_main(webui);
        }
    } else {
        retcd = MHD_NO;
    }

    return retcd;
}

/* Initial the stream context items for the camera */
void webu_stream_init(ctx_dev *cam)
{
    /* NOTE:  This runs on the motion_loop thread. */

    pthread_mutex_init(&cam->stream.mutex, NULL);

    cam->imgs.image_substream = NULL;

    cam->stream.norm.jpeg_size = 0;
    cam->stream.norm.jpeg_data = NULL;
    cam->stream.norm.jpg_cnct = 0;
    cam->stream.norm.ts_cnct = 0;
    cam->stream.norm.all_cnct = 0;
    cam->stream.norm.consumed = true;
    cam->stream.norm.image = NULL;

    cam->stream.sub.jpeg_size = 0;
    cam->stream.sub.jpeg_data = NULL;
    cam->stream.sub.jpg_cnct = 0;
    cam->stream.sub.ts_cnct = 0;
    cam->stream.sub.all_cnct = 0;
    cam->stream.sub.consumed = true;
    cam->stream.sub.image = NULL;

    cam->stream.motion.jpeg_size = 0;
    cam->stream.motion.jpeg_data = NULL;
    cam->stream.motion.jpg_cnct = 0;
    cam->stream.motion.ts_cnct = 0;
    cam->stream.motion.all_cnct = 0;
    cam->stream.motion.consumed = true;
    cam->stream.motion.image = NULL;

    cam->stream.source.jpeg_size = 0;
    cam->stream.source.jpeg_data = NULL;
    cam->stream.source.jpg_cnct = 0;
    cam->stream.source.ts_cnct = 0;
    cam->stream.source.all_cnct = 0;
    cam->stream.source.consumed = true;
    cam->stream.source.image = NULL;

    cam->stream.secondary.jpeg_size = 0;
    cam->stream.secondary.jpeg_data = NULL;
    cam->stream.secondary.jpg_cnct = 0;
    cam->stream.secondary.ts_cnct = 0;
    cam->stream.secondary.all_cnct = 0;
    cam->stream.secondary.consumed = true;
    cam->stream.secondary.image = NULL;

}

/* Free the stream buffers and mutex for shutdown */
void webu_stream_deinit(ctx_dev *cam)
{
    /* NOTE:  This runs on the motion_loop thread. */

    pthread_mutex_destroy(&cam->stream.mutex);

    myfree(&cam->imgs.image_substream);

    myfree(&cam->stream.norm.jpeg_data);
    myfree(&cam->stream.sub.jpeg_data);
    myfree(&cam->stream.motion.jpeg_data);
    myfree(&cam->stream.source.jpeg_data);
    myfree(&cam->stream.secondary.jpeg_data);

    myfree(&cam->stream.norm.image);
    myfree(&cam->stream.sub.image);
    myfree(&cam->stream.motion.image);
    myfree(&cam->stream.source.image);
    myfree(&cam->stream.secondary.image);

}

/* Get a normal image from the motion loop and compress it*/
static void webu_stream_getimg_norm(ctx_dev *cam)
{
    if ((cam->stream.norm.jpg_cnct == 0) &&
        (cam->stream.norm.ts_cnct == 0) &&
        (cam->stream.norm.all_cnct == 0)) {
        return;
    }

    if (cam->stream.norm.jpg_cnct > 0) {
        if (cam->stream.norm.jpeg_data == NULL) {
            cam->stream.norm.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        if (cam->current_image->image_norm != NULL && cam->stream.norm.consumed) {
            cam->stream.norm.jpeg_size = pic_put_memory(cam
                ,cam->stream.norm.jpeg_data
                ,cam->imgs.size_norm
                ,cam->current_image->image_norm
                ,cam->conf->stream_quality
                ,cam->imgs.width
                ,cam->imgs.height);
            cam->stream.norm.consumed = false;
        }
    }
    if ((cam->stream.norm.ts_cnct > 0) || (cam->stream.norm.all_cnct > 0)) {
        if (cam->stream.norm.image == NULL) {
            cam->stream.norm.image =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        memcpy(cam->stream.norm.image, cam->current_image->image_norm, cam->imgs.size_norm);
    }
}

/* Get a substream image from the motion loop and compress it*/
static void webu_stream_getimg_sub(ctx_dev *cam)
{
    int subsize;

    if ((cam->stream.sub.jpg_cnct == 0) &&
        (cam->stream.sub.ts_cnct == 0) &&
        (cam->stream.sub.all_cnct == 0)) {
        return;
    }

    if (cam->stream.sub.jpg_cnct > 0) {
        if (cam->stream.sub.jpeg_data == NULL) {
            cam->stream.sub.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        if (cam->current_image->image_norm != NULL && cam->stream.sub.consumed) {
            /* Resulting substream image must be multiple of 8 */
            if (((cam->imgs.width  % 16) == 0)  &&
                ((cam->imgs.height % 16) == 0)) {

                subsize = ((cam->imgs.width / 2) * (cam->imgs.height / 2) * 3 / 2);
                if (cam->imgs.image_substream == NULL) {
                    cam->imgs.image_substream =(unsigned char*)mymalloc(subsize);
                }
                pic_scale_img(cam->imgs.width
                    ,cam->imgs.height
                    ,cam->current_image->image_norm
                    ,cam->imgs.image_substream);
                cam->stream.sub.jpeg_size = pic_put_memory(cam
                    ,cam->stream.sub.jpeg_data
                    ,subsize
                    ,cam->imgs.image_substream
                    ,cam->conf->stream_quality
                    ,(cam->imgs.width / 2)
                    ,(cam->imgs.height / 2));
            } else {
                /* Substream was not multiple of 8 so send full image*/
                cam->stream.sub.jpeg_size = pic_put_memory(cam
                    ,cam->stream.sub.jpeg_data
                    ,cam->imgs.size_norm
                    ,cam->current_image->image_norm
                    ,cam->conf->stream_quality
                    ,cam->imgs.width
                    ,cam->imgs.height);
            }
            cam->stream.sub.consumed = false;
        }
    }

    if ((cam->stream.sub.ts_cnct > 0) || (cam->stream.sub.all_cnct > 0)) {
        if (cam->stream.sub.image == NULL) {
            cam->stream.sub.image =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        if (((cam->imgs.width  % 16) == 0)  &&
            ((cam->imgs.height % 16) == 0)) {
            subsize = ((cam->imgs.width / 2) * (cam->imgs.height / 2) * 3 / 2);
            if (cam->imgs.image_substream == NULL) {
                cam->imgs.image_substream =(unsigned char*)mymalloc(subsize);
            }
            pic_scale_img(cam->imgs.width
                ,cam->imgs.height
                ,cam->current_image->image_norm
                ,cam->imgs.image_substream);
            memcpy(cam->stream.sub.image, cam->imgs.image_substream, subsize);
        } else {
            memcpy(cam->stream.sub.image, cam->current_image->image_norm, cam->imgs.size_norm);
        }
    }

}

/* Get a motion image from the motion loop and compress it*/
static void webu_stream_getimg_motion(ctx_dev *cam)
{
    if ((cam->stream.motion.jpg_cnct == 0) &&
        (cam->stream.motion.ts_cnct == 0) &&
        (cam->stream.motion.all_cnct == 0)) {
        return;
    }

    if (cam->stream.motion.jpg_cnct > 0) {
        if (cam->stream.motion.jpeg_data == NULL) {
            cam->stream.motion.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        if (cam->imgs.image_motion.image_norm != NULL  && cam->stream.motion.consumed) {
            cam->stream.motion.jpeg_size = pic_put_memory(cam
                ,cam->stream.motion.jpeg_data
                ,cam->imgs.size_norm
                ,cam->imgs.image_motion.image_norm
                ,cam->conf->stream_quality
                ,cam->imgs.width
                ,cam->imgs.height);
            cam->stream.motion.consumed = false;
        }
    }
    if ((cam->stream.motion.ts_cnct > 0) || (cam->stream.motion.all_cnct > 0)) {
        if (cam->stream.motion.image == NULL) {
            cam->stream.motion.image =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        memcpy(cam->stream.motion.image
            , cam->imgs.image_motion.image_norm
            , cam->imgs.size_norm);
    }
}

/* Get a source image from the motion loop and compress it*/
static void webu_stream_getimg_source(ctx_dev *cam)
{
    if ((cam->stream.source.jpg_cnct == 0) &&
        (cam->stream.source.ts_cnct == 0) &&
        (cam->stream.source.all_cnct == 0)) {
        return;
    }

    if (cam->stream.source.jpg_cnct > 0) {
        if (cam->stream.source.jpeg_data == NULL) {
            cam->stream.source.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        if (cam->imgs.image_virgin != NULL && cam->stream.source.consumed) {
            cam->stream.source.jpeg_size = pic_put_memory(cam
                ,cam->stream.source.jpeg_data
                ,cam->imgs.size_norm
                ,cam->imgs.image_virgin
                ,cam->conf->stream_quality
                ,cam->imgs.width
                ,cam->imgs.height);
            cam->stream.source.consumed = false;
        }
    }
    if ((cam->stream.source.ts_cnct > 0) || (cam->stream.source.all_cnct > 0)) {
        if (cam->stream.source.image == NULL) {
            cam->stream.source.image =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        memcpy(cam->stream.source.image
            , cam->imgs.image_virgin
            , cam->imgs.size_norm);
    }
}

/* Get a secondary image from the motion loop and compress it*/
static void webu_stream_getimg_secondary(ctx_dev *cam)
{
     if ((cam->stream.secondary.jpg_cnct == 0) &&
         (cam->stream.secondary.ts_cnct == 0) &&
         (cam->stream.secondary.all_cnct == 0)) {
        return;
    }

    if (cam->stream.secondary.jpg_cnct > 0) {
        if (cam->imgs.size_secondary>0) {
            pthread_mutex_lock(&cam->algsec->mutex);
                if (cam->stream.secondary.jpeg_data == NULL) {
                    cam->stream.secondary.jpeg_data =(unsigned char*)mymalloc(cam->imgs.size_norm);
                }

                memcpy(cam->stream.secondary.jpeg_data,cam->imgs.image_secondary,cam->imgs.size_secondary);
                cam->stream.secondary.jpeg_size = cam->imgs.size_secondary;
            pthread_mutex_unlock(&cam->algsec->mutex);
        } else {
            myfree(&cam->stream.secondary.jpeg_data);
        }
    }
    if ((cam->stream.secondary.ts_cnct > 0) || (cam->stream.secondary.all_cnct > 0)) {
        if (cam->stream.secondary.image == NULL) {
            cam->stream.secondary.image =(unsigned char*)mymalloc(cam->imgs.size_norm);
        }
        memcpy(cam->stream.secondary.image
            , cam->current_image->image_norm, cam->imgs.size_norm);
    }

}

/* Get image from the motion loop and compress it*/
void webu_stream_getimg(ctx_dev *cam)
{
    /*This is on the motion_loop thread */
    pthread_mutex_lock(&cam->stream.mutex);
        webu_stream_getimg_norm(cam);
        webu_stream_getimg_sub(cam);
        webu_stream_getimg_motion(cam);
        webu_stream_getimg_source(cam);
        webu_stream_getimg_secondary(cam);
    pthread_mutex_unlock(&cam->stream.mutex);
}
