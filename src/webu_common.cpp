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
#include "webu.hpp"
#include "webu_common.hpp"
#include "webu_ans.hpp"
#include "webu_html.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "webu_json.hpp"
#include "webu_post.hpp"
#include "webu_file.hpp"
#include "video_v4l2.hpp"


bool cls_webu_common::check_finish()
{
    if (webu->wb_finish){
        resp_used = 0;
        return true;
    }
    if (webua->cam != NULL) {
        if ((webua->cam->finish_dev == true) ||
            (webua->cam->passflag == false)) {
            resp_used = 0;
            return true;
        }
    }
    return false;
}

void cls_webu_common::set_fps()
{
    if ((webua->cam->detecting_motion == false) &&
        (app->cam_list[webua->camindx]->conf->stream_motion)) {
        stream_fps = 1;
    } else {
        stream_fps = app->cam_list[webua->camindx]->conf->stream_maxrate;
    }
}

/* Sleep required time to get to the user requested framerate for the stream */
void cls_webu_common::delay()
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

void cls_webu_common::img_sizes(ctx_dev *p_cam, int &img_w, int &img_h)
{
    if (((webua->cnct_type == WEBUI_CNCT_JPG_SUB) ||
         (webua->cnct_type == WEBUI_CNCT_TS_SUB)) &&
        (((p_cam->imgs.width  % 16) == 0) &&
         ((p_cam->imgs.height % 16) == 0))) {
        img_w = (p_cam->imgs.width/2);
        img_h = (p_cam->imgs.height/2);
    } else {
        img_w = p_cam->imgs.width;
        img_h = p_cam->imgs.height;
    }
    img_w = ((p_cam->all_loc.scale * img_w) / 100);
    if ((img_w % 16) != 0) {
        img_w = img_w - (img_w % 16) + 16;
    }

    img_h = ((p_cam->all_loc.scale * img_h) / 100);
    if ((img_h % 16) != 0) {
        img_h = img_h - (img_h % 16) + 16;
    }

    if (img_w < 64){
        img_w = 64;
    }
    if (img_h < 64){
        img_h = 64;
    }
}

void cls_webu_common::img_resize(ctx_dev *p_cam
    , uint8_t *src, uint8_t *dst, int dst_w, int dst_h)
{
    int     retcd, img_sz, src_h, src_w;
    char    errstr[128];
    uint8_t *buf;
    AVFrame *frm_in, *frm_out;
    struct SwsContext *swsctx;

    src_h = p_cam->imgs.height;
    src_w = p_cam->imgs.width;

    img_sz = (dst_h * dst_w * 3)/2;
    memset(dst, 0x00, (size_t)img_sz);

    frm_in = av_frame_alloc();
    if (frm_in == NULL) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            , _("Unable to allocate frm_in."));
        return;
    }

    frm_out = av_frame_alloc();
    if (frm_out == NULL) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            , _("Unable to allocate frm_out."));
        av_frame_free(&frm_in);
        return;
    }

    retcd = av_image_fill_arrays(
        frm_in->data, frm_in->linesize
        , src, MY_PIX_FMT_YUV420P
        , src_w, src_h, 1);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            , "Error filling arrays: %s", errstr);
        av_frame_free(&frm_in);
        av_frame_free(&frm_out);
        return;
    }

    buf = (uint8_t *)mymalloc((size_t)img_sz);

    retcd = av_image_fill_arrays(
        frm_out->data, frm_out->linesize
        , buf, MY_PIX_FMT_YUV420P
        , dst_w, dst_h, 1);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            , "Error Filling array 2: %s", errstr);
        free(buf);
        av_frame_free(&frm_in);
        av_frame_free(&frm_out);
        return;
    }

    swsctx = sws_getContext(
            src_w, src_h, MY_PIX_FMT_YUV420P
            ,dst_w, dst_h, MY_PIX_FMT_YUV420P
            ,SWS_BICUBIC, NULL, NULL, NULL);
    if (swsctx == NULL) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            , _("Unable to allocate scaling context."));
        free(buf);
        av_frame_free(&frm_in);
        av_frame_free(&frm_out);
        return;
    }

    retcd = sws_scale(swsctx
        , (const uint8_t* const *)frm_in->data, frm_in->linesize
        , 0, src_h, frm_out->data, frm_out->linesize);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Error resizing/reformatting: %s"), errstr);
        free(buf);
        av_frame_free(&frm_in);
        av_frame_free(&frm_out);
        sws_freeContext(swsctx);
        return;
    }

    retcd = av_image_copy_to_buffer(
        (uint8_t *)dst, img_sz
        , (const uint8_t * const*)frm_out
        , frm_out->linesize
        , MY_PIX_FMT_YUV420P, dst_w, dst_h, 1);

    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Error putting frame into output buffer: %s"), errstr);
        free(buf);
        av_frame_free(&frm_in);
        av_frame_free(&frm_out);
        sws_freeContext(swsctx);
        return;
    }

    free(buf);
    av_frame_free(&frm_in);
    av_frame_free(&frm_out);
    sws_freeContext(swsctx);
}

void cls_webu_common::all_getimg()
{
    int a_y, a_u, a_v; /* all img y,u,v */
    int c_y, c_u, c_v; /* camera img y,u,v */
    int dst_h, dst_w, dst_sz, src_sz, img_orow, img_ocol;
    int indx, row, indx1;
    unsigned char *dst_img, *src_img;
    ctx_stream_data *strm;
    ctx_all_sizes *all_sz;
    ctx_dev *p_cam;

    memset(resp_image, '\0', resp_size);

    all_sz = app->all_sizes;

    a_y = 0;
    a_u = (all_sz->width * all_sz->height);
    a_v = a_u + (a_u / 4);

    memset(all_img_data , 0x80, (size_t)a_u);
    memset(all_img_data  + a_u, 0x80, (size_t)(a_u/2));

    for (indx=0; indx<app->cam_cnt; indx++) {
        p_cam = app->cam_list[indx];
        img_sizes(p_cam, dst_w, dst_h);

        dst_sz = (dst_h * dst_w * 3)/2;
        src_sz = (p_cam->imgs.width * p_cam->imgs.height * 3)/2;
        img_orow = p_cam->all_loc.offset_row;
        img_ocol = p_cam->all_loc.offset_col;

        if ((webua->cnct_type == WEBUI_CNCT_JPG_FULL) ||
            (webua->cnct_type == WEBUI_CNCT_TS_FULL)) {
            strm = &p_cam->stream.norm;
        } else if ((webua->cnct_type == WEBUI_CNCT_JPG_SUB) ||
            (webua->cnct_type == WEBUI_CNCT_TS_SUB)) {
            /* The use of the full size image is is is not an error here.
              For the all_img, we are using a different scaling/resizing method
              and as a result, we need to start with the full size image then
              resize to substream and stream_preview_scale*/
            strm = &p_cam->stream.norm; /* <<==Normal size is correct here*/
        } else if ((webua->cnct_type == WEBUI_CNCT_JPG_MOTION) ||
            (webua->cnct_type == WEBUI_CNCT_TS_MOTION)) {
            strm = &p_cam->stream.motion;
        } else if ((webua->cnct_type == WEBUI_CNCT_JPG_SOURCE)  ||
            (webua->cnct_type == WEBUI_CNCT_TS_SOURCE )) {
            strm = &p_cam->stream.source;
        } else if ((webua->cnct_type == WEBUI_CNCT_JPG_SECONDARY)  ||
            (webua->cnct_type == WEBUI_CNCT_TS_SECONDARY)) {
            strm = &p_cam->stream.secondary;
        } else { /* Should not be possible*/
            return;
        }

        dst_img = (unsigned char*) mymalloc((uint)dst_sz);
        src_img = (unsigned char*) mymalloc((uint)src_sz);

        pthread_mutex_lock(&p_cam->stream.mutex);
            indx1=0;
            while (indx1 < 1000) {
                if (strm->img_data == NULL) {
                    if (strm->all_cnct == 0){
                        strm->all_cnct++;
                    }
                    pthread_mutex_unlock(&p_cam->stream.mutex);
                        SLEEP(0, 1000);
                    pthread_mutex_lock(&p_cam->stream.mutex);
                } else {
                    break;
                }
                indx1++;
            }
            if (strm->img_data == NULL) {
                MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                    , "Could not get image for device %d", p_cam->device_id);
                memset(src_img, 0x00, (uint)src_sz);
            } else {
                memcpy(src_img, strm->img_data, (uint)src_sz);
            }
        pthread_mutex_unlock(&p_cam->stream.mutex);

        img_resize(p_cam, src_img, dst_img, dst_w, dst_h);

        /*
        MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
            , "src w %d h %d dst w %d h %d all w %d h %d "
            , p_cam->imgs.width, p_cam->imgs.height
            , dst_w, dst_h
            , all_sz->width,all_sz->height);
        */
        a_y = (img_orow * all_sz->width) + img_ocol;
        a_u =(all_sz->height * all_sz->width) +
            ((img_orow / 4) * all_sz->width) + (img_ocol / 2) ;
        a_v = a_u + ((all_sz->height * all_sz->width) / 4);

        c_y = 0;
        c_u = (dst_w * dst_h);
        c_v = c_u + (c_u / 4);

        /*
        MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
            , "r %d c %d a %d %d %d h %d w %d"
            , img_orow, img_ocol
            , a_y, a_u, a_v
            , all_sz->height, all_sz->width);
        */

        for (row=0; row<dst_h; row++) {
            memcpy(all_img_data  + a_y, dst_img + c_y, (uint)dst_w);
            a_y += all_sz->width;
            c_y += dst_w;
            if (row % 2) {
                memcpy(all_img_data  + a_u, dst_img + c_u, (uint)dst_w / 2);
                //mymemset(webui->all_img_data  + a_u, 0xFA, dst_w/2);
                a_u += (all_sz->width / 2);
                c_u += (dst_w / 2);
                memcpy(all_img_data  + a_v, dst_img + c_v, (uint)dst_w / 2);
                a_v += (all_sz->width / 2);
                c_v += (dst_w / 2);
            }
        }

        myfree(&dst_img);
        myfree(&src_img);
    }

}

void cls_webu_common::all_sizes()
{
    int indx, row, col;
    int chk_sz, chk_w, mx_col, mx_row;
    int mx_h, mx_w, img_h, img_w;
    bool dflt_scale;
    ctx_dev *p_cam;

    if (app->all_sizes->reset == false) {
        return;
    }

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<app->cam_cnt; indx++) {
        p_cam = app->cam_list[indx];
        if (mx_row < p_cam->all_loc.row) {
            mx_row = p_cam->all_loc.row;
        }
        if (mx_col < p_cam->all_loc.col) {
            mx_col = p_cam->all_loc.col;
        }
    }

    dflt_scale = false;
    for (indx=0; indx<app->cam_cnt; indx++) {
        if (app->cam_list[indx]->all_loc.scale == -1) {
            dflt_scale = true;
        }
    }
    if (dflt_scale) {
        for (indx=0; indx<app->cam_cnt; indx++) {
            app->cam_list[indx]->all_loc.scale = 100;
        }
        for (row=1; row<=mx_row; row++) {
            mx_h = 0;
            for (indx=0; indx<app->cam_cnt; indx++) {
                p_cam = app->cam_list[indx];
                if (row == p_cam->all_loc.row) {
                    img_sizes(p_cam, img_w, img_h);
                    if (mx_h < img_h) {
                        mx_h = img_h;
                    }
                }
            }
            for (indx=0; indx<app->cam_cnt; indx++) {
                p_cam = app->cam_list[indx];
                if (row == p_cam->all_loc.row) {
                    img_sizes(p_cam, img_w, img_h);
                    p_cam->all_loc.scale = (int)((float)(mx_h*100 / img_h ));
                }
            }
            for (indx=0; indx<app->cam_cnt; indx++) {
                p_cam = app->cam_list[indx];
                img_sizes(p_cam, img_w, img_h);
                MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                    , "Device %d Original Size %dx%d Scale %d New Size %dx%d"
                    , p_cam->device_id
                    , p_cam->imgs.width, p_cam->imgs.height
                    , p_cam->all_loc.scale, img_w, img_h);
            }
        }
    }

    app->all_sizes->width = 0;
    app->all_sizes->height = 0;
    for (row=1; row<=mx_row; row++) {
        chk_sz = 0;
        mx_h = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<app->cam_cnt; indx++) {
                p_cam = app->cam_list[indx];
                img_sizes(p_cam, img_w, img_h);
                if ((row == p_cam->all_loc.row) &&
                    (col == p_cam->all_loc.col)) {
                    p_cam->all_loc.offset_col = chk_sz;
                    chk_sz += img_w;
                    if (mx_h < img_h) {
                        mx_h = img_h;
                    }
                }
            }
        }
        /* Align/center vert. the images in each row*/
        for (indx=0; indx<app->cam_cnt; indx++) {
            p_cam = app->cam_list[indx];
            img_sizes(p_cam, img_w, img_h);
            if (p_cam->all_loc.row == row) {
                p_cam->all_loc.offset_row =
                    app->all_sizes->height +
                    ((mx_h - img_h)/2) ;
            }
        }
        app->all_sizes->height += mx_h;
        if (app->all_sizes->width < chk_sz) {
            app->all_sizes->width = chk_sz;
        }
    }

    /* Align/center horiz. the images within each column area */
    chk_w = 0;
    for (col=1; col<=mx_col; col++) {
        chk_sz = 0;
        mx_w = 0;
        for (indx=0; indx<app->cam_cnt; indx++) {
            p_cam = app->cam_list[indx];
            img_sizes(p_cam, img_w, img_h);
            if (p_cam->all_loc.col == col) {
                if (p_cam->all_loc.offset_col < chk_w) {
                    p_cam->all_loc.offset_col = chk_w;
                }
                if (chk_sz < p_cam->all_loc.offset_col) {
                    chk_sz = p_cam->all_loc.offset_col;
                }
                if (mx_w < img_w) {
                    mx_w = img_w;
                }
            }
        }
        for (indx=0; indx<app->cam_cnt; indx++) {
            p_cam = app->cam_list[indx];
            img_sizes(p_cam, img_w, img_h);
            if (p_cam->all_loc.col == col) {
                p_cam->all_loc.offset_col =
                    chk_sz + ((mx_w - img_w) /2) ;
            }
        }
        chk_w = mx_w + chk_sz;
        if (app->all_sizes->width < chk_w) {
            app->all_sizes->width = chk_w;
        }
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        p_cam = app->cam_list[indx];
        img_sizes(p_cam, img_w, img_h);

        chk_sz = p_cam->all_loc.offset_col + p_cam->all_loc.offset_user_col;
        if (chk_sz < 0) {
           MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) less than zero "
                , p_cam->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else if ((chk_sz + img_w) > app->all_sizes->width) {
           MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) over image size"
                , p_cam->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else {
            p_cam->all_loc.offset_col = chk_sz;
        }

        chk_sz = p_cam->all_loc.offset_row + p_cam->all_loc.offset_user_row;
        if (chk_sz < 0 ) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) less than zero "
                , p_cam->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else if ((chk_sz + img_h) > app->all_sizes->height) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) over image size"
                , p_cam->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else {
            p_cam->all_loc.offset_row = chk_sz;
        }
    }

    app->all_sizes->img_sz =((
        app->all_sizes->height *
        app->all_sizes->width * 3)/2);

    /*
    for (indx=0; indx<app->cam_cnt; indx++) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , "row %d col %d offset row %d offset col %d"
            , app->cam_list[indx]->all_loc.row
            , app->cam_list[indx]->all_loc.col
            , app->cam_list[indx]->all_loc.offset_row
            , app->cam_list[indx]->all_loc.offset_col);
    }
    */

}

/* Allocate buffers if needed. */
void cls_webu_common::one_buffer()
{
    if (webua->cam == NULL) {
        return;
    }
    if (resp_size < (size_t)webua->cam->imgs.size_norm) {
        if (resp_image != NULL) {
            myfree(&resp_image);
        }
        resp_image = (unsigned char*) mymalloc((uint)webua->cam->imgs.size_norm);
        memset(resp_image,'\0', (uint)webua->cam->imgs.size_norm);
        resp_size = (uint)webua->cam->imgs.size_norm;
        resp_used = 0;
    }
}

void cls_webu_common::all_buffer()
{
    if (resp_size < (size_t)app->all_sizes->img_sz) {
        if (resp_image != nullptr) {
            myfree(&resp_image);
        }
        resp_size = (uint)app->all_sizes->img_sz;
        resp_image = (unsigned char*) mymalloc(resp_size);
        memset(resp_image, '\0', resp_size);
        resp_used = 0;
    }
    if ((all_img_data  == nullptr) &&
        (app->all_sizes->img_sz >0)) {
        all_img_data  = (unsigned char*)
            mymalloc((size_t)app->all_sizes->img_sz);
    }
}

cls_webu_common::cls_webu_common(cls_webu_ans *p_webua)
{
    app       = p_webua->app;
    webu      = p_webua->webu;
    webua     = p_webua;

    all_img_data  = nullptr;
    resp_image = nullptr;
    resp_size     = 0;                           /* The size of the resp_page buffer.  May get adjusted */
    resp_used     = 0;                           /* How many bytes used so far in resp_page*/
    stream_fps    = 1;                           /* Stream rate */
}

cls_webu_common::~cls_webu_common()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
    myfree(&resp_image);
    myfree(&all_img_data);
}
