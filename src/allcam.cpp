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
 *
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "allcam.hpp"
#include "camera.hpp"
#include "jpegutils.hpp"


static void *allcam_handler(void *arg)
{
    ((cls_allcam *)arg)->handler();
    return nullptr;
}

void cls_allcam::getsizes_img(cls_camera *p_cam, int &img_w, int &img_h)
{
    int src_w, src_h;

    src_w = p_cam->all_sizes.width;
    src_h = p_cam->all_sizes.height;

    img_w = ((p_cam->all_loc.scale * src_w) / 100);
    if ((img_w % 16) != 0) {
        img_w = img_w - (img_w % 16) + 16;
    }

    img_h = ((p_cam->all_loc.scale * src_h) / 100);
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

void cls_allcam::getimg(ctx_stream_data *strm_a, std::string imgtyp)
{
    int a_y, a_u, a_v; /* all img y,u,v */
    int c_y, c_u, c_v; /* camera img y,u,v */
    int dst_h, dst_w, dst_sz, src_sz, img_orow, img_ocol;
    int indx, row, indx1;
    int src_w, src_h;
    unsigned char *dst_img, *src_img;
    ctx_stream_data *strm_c;
    ctx_all_sizes *all_sz;
    cls_camera *p_cam;

    getsizes();

    all_sz = &all_sizes;

    a_y = 0;
    a_u = (all_sz->width * all_sz->height);
    a_v = a_u + (a_u / 4);

    memset(strm_a->img_data , 0x80, (size_t)a_u);
    memset(strm_a->img_data  + a_u, 0x80, (size_t)(a_u/2));

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        src_h = p_cam->imgs.height;
        src_w = p_cam->imgs.width;

        getsizes_img(p_cam, dst_w, dst_h);

        dst_sz = (dst_h * dst_w * 3)/2;
        src_sz = (src_h * src_w * 3)/2;
        img_orow = p_cam->all_loc.offset_row;
        img_ocol = p_cam->all_loc.offset_col;
        if (imgtyp == "norm") {
            strm_c = &p_cam->stream.norm;
        } else if (imgtyp == "motion") {
            strm_c = &p_cam->stream.motion;
        } else if (imgtyp == "source") {
            strm_c = &p_cam->stream.source;
        } else if (imgtyp == "secondary") {
            strm_c = &p_cam->stream.secondary;
        } else {
            return;
        }

        dst_img = (unsigned char*) mymalloc((uint)dst_sz);
        src_img = (unsigned char*) mymalloc((uint)src_sz);

        pthread_mutex_lock(&p_cam->stream.mutex);
            indx1=0;
            while (indx1 < 1000) {
                if (strm_c->img_data == NULL) {
                    if (strm_c->all_cnct == 0){
                        strm_c->all_cnct++;
                    }
                    pthread_mutex_unlock(&p_cam->stream.mutex);
                        SLEEP(0, 1000);
                    pthread_mutex_lock(&p_cam->stream.mutex);
                } else {
                    break;
                }
                indx1++;
            }
            if ((p_cam->all_sizes.width != src_w) ||
                (p_cam->all_sizes.height != src_h)) {
                MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
                    , "Image has changed. Device: %d"
                    , p_cam->cfg->device_id);
                memset(src_img, 0x00, (uint)src_sz);
                p_cam->all_sizes.reset = true;
            } else if (strm_c->img_data == NULL) {
                MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                    , "Could not get image for device %d"
                    , p_cam->cfg->device_id);
                memset(src_img, 0x00, (uint)src_sz);
            } else {
                memcpy(src_img, strm_c->img_data, (uint)src_sz);
            }
        pthread_mutex_unlock(&p_cam->stream.mutex);

        util_resize(src_img, src_w, src_h, dst_img, dst_w, dst_h);

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
            memcpy(strm_a->img_data  + a_y, dst_img + c_y, (uint)dst_w);
            a_y += all_sz->width;
            c_y += dst_w;
            if (row % 2) {
                memcpy(strm_a->img_data  + a_u, dst_img + c_u, (uint)dst_w / 2);
                //mymemset(strm_a->img_data  + a_u, 0xFA, dst_w/2);
                a_u += (all_sz->width / 2);
                c_u += (dst_w / 2);
                memcpy(strm_a->img_data  + a_v, dst_img + c_v, (uint)dst_w / 2);
                a_v += (all_sz->width / 2);
                c_v += (dst_w / 2);
            }
        }

        myfree(dst_img);
        myfree(src_img);
    }
    strm_a->jpg_sz = jpgutl_put_yuv420p(
        strm_a->jpg_data, all_sz->img_sz
        ,strm_a->img_data, all_sz->width, all_sz->height
        , 70, NULL,NULL,NULL);
    strm_a->consumed = false;

}

void cls_allcam::stream_free()
{
    int indx;
    ctx_stream_data *strm;

    for (indx=0;indx<5;indx++) {
        if (indx == 0) {
            strm = &stream.norm;
        } else if (indx == 1) {
            strm = &stream.motion;
        } else if (indx == 2) {
            strm = &stream.secondary;
        } else if (indx == 3) {
            strm = &stream.source;
        } else if (indx == 4) {
            strm = &stream.sub;
        }
        myfree(strm->img_data);
        myfree(strm->jpg_data);
        strm->img_data = (unsigned char*)
            mymalloc((size_t)all_sizes.img_sz);
    }

}

void cls_allcam::stream_alloc()
{
    int indx;
    ctx_stream_data *strm;

    for (indx=0;indx<5;indx++) {
        if (indx == 0) {
            strm = &stream.norm;
        } else if (indx == 1) {
            strm = &stream.motion;
        } else if (indx == 2) {
            strm = &stream.secondary;
        } else if (indx == 3) {
            strm = &stream.source;
        } else if (indx == 4) {
            strm = &stream.sub;
        }
        strm->img_data = (unsigned char*)
            mymalloc((size_t)all_sizes.img_sz);
        strm->jpg_data = (unsigned char*)
            mymalloc((size_t)all_sizes.img_sz);
        strm->consumed = true;
    }

}

void cls_allcam::getsizes_scale(int mx_row)
{
    int indx, row;
    int mx_h, img_h, img_w;
    bool dflt_scale;
    cls_camera *p_cam;

    dflt_scale = false;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        if (active_cam[indx]->all_loc.scale == -1) {
            dflt_scale = true;
        }
    }
    if (dflt_scale) {
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            active_cam[indx]->all_loc.scale = 100;
        }
        for (row=1; row<=mx_row; row++) {
            mx_h = 0;
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if (row == p_cam->all_loc.row) {
                    getsizes_img(p_cam, img_w, img_h);
                    if (mx_h < img_h) {
                        mx_h = img_h;
                    }
                }
            }
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if (row == p_cam->all_loc.row) {
                    getsizes_img(p_cam, img_w, img_h);
                    p_cam->all_loc.scale = (int)((float)(mx_h*100 / img_h ));
                }
            }
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                getsizes_img(p_cam, img_w, img_h);
                MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                    , "Device %d Original Size %dx%d Scale %d New Size %dx%d"
                    , p_cam->cfg->device_id
                    , p_cam->imgs.width, p_cam->imgs.height
                    , p_cam->all_loc.scale, img_w, img_h);
            }
        }
    }
}

void cls_allcam::getsizes_alignv(int mx_row, int mx_col)
{
    int indx, row, col;
    int chk_sz;
    int mx_h, img_h, img_w;
    cls_camera *p_cam;

    for (row=1; row<=mx_row; row++) {
        chk_sz = 0;
        mx_h = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                getsizes_img(p_cam, img_w, img_h);
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
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            getsizes_img(p_cam, img_w, img_h);
            if (p_cam->all_loc.row == row) {
                p_cam->all_loc.offset_row =
                    all_sizes.height +
                    ((mx_h - img_h)/2) ;
            }
        }
        all_sizes.height += mx_h;
        if (all_sizes.width < chk_sz) {
            all_sizes.width = chk_sz;
        }
    }
}

void cls_allcam::getsizes_alignh(int mx_col)
{
    int indx, col;
    int chk_sz, chk_w;
    int mx_w, img_h, img_w;
    cls_camera *p_cam;

    /* Align/center horiz. the images within each column area */
    chk_w = 0;
    for (col=1; col<=mx_col; col++) {
        chk_sz = 0;
        mx_w = 0;
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            getsizes_img(p_cam, img_w, img_h);
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
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            getsizes_img(p_cam, img_w, img_h);
            if (p_cam->all_loc.col == col) {
                p_cam->all_loc.offset_col =
                    chk_sz + ((mx_w - img_w) /2) ;
            }
        }
        chk_w = mx_w + chk_sz;
        if (all_sizes.width < chk_w) {
            all_sizes.width = chk_w;
        }
    }
}

void cls_allcam::getsizes()
{
    int indx;
    int chk_sz, mx_col, mx_row;
    int img_h, img_w;
    bool    chk;
    cls_camera *p_cam;

    if (all_sizes.reset == true) {
        chk = true;
    } else {
        chk = false;
    }

    active_cam.clear();
    active_cnt = 0;
    for (indx=0;indx<app->cam_cnt;indx++) {
        p_cam = app->cam_list[indx];
        if (p_cam->device_status == STATUS_OPENED) {
            if (p_cam->all_sizes.reset == true) {
                chk = true;
            }
            active_cnt++;
            active_cam.push_back(p_cam);
            p_cam->all_sizes.width = p_cam->imgs.width;
            p_cam->all_sizes.height = p_cam->imgs.height;
            p_cam->all_sizes.img_sz = ((p_cam->imgs.height * p_cam->imgs.width * 3)/2);
            p_cam->all_sizes.reset = false;
        }
    }

    if (chk == false) {
        return;
    }

    init_cams();

    all_sizes.width = 0;
    all_sizes.height = 0;

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        if (mx_row < p_cam->all_loc.row) {
            mx_row = p_cam->all_loc.row;
        }
        if (mx_col < p_cam->all_loc.col) {
            mx_col = p_cam->all_loc.col;
        }
    }

    getsizes_scale(mx_row);
    getsizes_alignv(mx_row, mx_col);
    getsizes_alignh(mx_col);

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        getsizes_img(p_cam, img_w, img_h);

        chk_sz = p_cam->all_loc.offset_col + p_cam->all_loc.offset_user_col;
        if (chk_sz < 0) {
           MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else if ((chk_sz + img_w) > all_sizes.width) {
           MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else {
            p_cam->all_loc.offset_col = chk_sz;
        }

        chk_sz = p_cam->all_loc.offset_row + p_cam->all_loc.offset_user_row;
        if (chk_sz < 0 ) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else if ((chk_sz + img_h) > all_sizes.height) {
            MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else {
            p_cam->all_loc.offset_row = chk_sz;
        }
    }

    if ((all_sizes.height ==0) ||
        (all_sizes.width == 0)) {
        all_sizes.width = 320;
        all_sizes.height = 240;
    }
    all_sizes.img_sz =((
        all_sizes.height *
        all_sizes.width * 3)/2);
    all_sizes.reset = false;

    stream_free();
    stream_alloc();

    /*
    for (indx=0; indx<active_cnt; indx++) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , "row %d col %d offset row %d offset col %d"
            , active_cam[indx]->all_loc.row
            , active_cam[indx]->all_loc.col
            , active_cam[indx]->all_loc.offset_row
            , active_cam[indx]->all_loc.offset_col);
    }
    */

}

void cls_allcam::init_params()
{
    int indx, indx1;
    ctx_params  *params_loc;
    ctx_params_item *itm;

    memset(&all_sizes, 0, sizeof(ctx_all_sizes));

    params_loc = new ctx_params;

    for (indx=0; indx<app->cam_cnt; indx++) {
        app->cam_list[indx]->all_loc.row = -1;
        app->cam_list[indx]->all_loc.col = -1;
        app->cam_list[indx]->all_loc.offset_user_col = 0;
        app->cam_list[indx]->all_loc.offset_user_row = 0;
        app->cam_list[indx]->all_loc.scale =
            app->cam_list[indx]->cfg->stream_preview_scale;

        util_parms_parse(params_loc
            , "stream_preview_location"
            , app->cam_list[indx]->cfg->stream_preview_location);

        for (indx1=0;indx1<params_loc->params_cnt;indx1++) {
            itm = &params_loc->params_array[indx1];
            if (itm->param_name == "row") {
                app->cam_list[indx]->all_loc.row = mtoi(itm->param_value);
            }
            if (itm->param_name == "col") {
                app->cam_list[indx]->all_loc.col = mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_col") {
                app->cam_list[indx]->all_loc.offset_user_col =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_row") {
                app->cam_list[indx]->all_loc.offset_user_row =
                    mtoi(itm->param_value);
            }
        }
        params_loc->params_array.clear();
    }

    mydelete(params_loc);

}

bool cls_allcam::init_validate()
{
    int indx, indx1;
    int row, col, mx_row, mx_col, col_chk;
    bool cfg_valid, chk;
    std::string cfg_row, cfg_col;

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<app->cam_cnt; indx++) {
        if (mx_col < app->cam_list[indx]->all_loc.col) {
            mx_col = app->cam_list[indx]->all_loc.col;
        }
        if (mx_row < app->cam_list[indx]->all_loc.row) {
            mx_row = app->cam_list[indx]->all_loc.row;
        }
    }

    cfg_valid = true;

    for (indx=0; indx<app->cam_cnt; indx++) {
        if ((app->cam_list[indx]->all_loc.col == -1) ||
            (app->cam_list[indx]->all_loc.row == -1)) {
            cfg_valid = false;
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "No stream_preview_location for cam %d"
                , app->cam_list[indx]->cfg->device_id);
        } else {
            for (indx1=0; indx1<app->cam_cnt; indx1++) {
                if ((app->cam_list[indx]->all_loc.col ==
                    app->cam_list[indx1]->all_loc.col) &&
                    (app->cam_list[indx]->all_loc.row ==
                    app->cam_list[indx1]->all_loc.row) &&
                    (indx != indx1)) {
                    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                        , "Duplicate stream_preview_location "
                        " cam %d, cam %d row %d col %d"
                        , app->cam_list[indx]->cfg->device_id
                        , app->cam_list[indx1]->cfg->device_id
                        , app->cam_list[indx]->all_loc.row
                        , app->cam_list[indx]->all_loc.col);
                    cfg_valid = false;
                }
            }
        }
        if (app->cam_list[indx]->all_loc.row == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location row cam %d, row %d"
                , app->cam_list[indx]->cfg->device_id
                , app->cam_list[indx]->all_loc.row);
            cfg_valid = false;
        }
        if (app->cam_list[indx]->all_loc.col == 0) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location col cam %d, col %d"
                , app->cam_list[indx]->cfg->device_id
                , app->cam_list[indx]->all_loc.col);
            cfg_valid = false;
        }
    }

    for (row=1; row<=mx_row; row++) {
        chk = false;
        for (indx=0; indx<app->cam_cnt; indx++) {
            if (row == app->cam_list[indx]->all_loc.row) {
                chk = true;
            }
        }
        if (chk == false) {
            MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_location combination. "
                " Missing row %d", row);
            cfg_valid = false;
        }
        col_chk = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<app->cam_cnt; indx++) {
                if ((row == app->cam_list[indx]->all_loc.row) &&
                    (col == app->cam_list[indx]->all_loc.col)) {
                    if ((col_chk+1) == col) {
                        col_chk = col;
                    } else {
                        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , "Invalid stream_preview_location combination. "
                            " Missing row %d column %d", row, col_chk+1);
                        cfg_valid = false;
                    }
                }
            }
        }
    }

    return cfg_valid;

}

void cls_allcam::init_cams()
{
    int row, col, indx;

    if (app->cam_cnt < 1) {
        return;
    }

    init_params();

    if (init_validate() == false) {
        MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,"Creating default stream preview values");
        row = 0;
        col = 0;
        for (indx=0; indx<app->cam_cnt; indx++) {
            if (col == 1) {
                col++;
            } else {
                row++;
                col = 1;
            }
            app->cam_list[indx]->all_loc.col = col;
            app->cam_list[indx]->all_loc.row = row;
            app->cam_list[indx]->all_loc.scale = -1;
        }
    }

    for (indx=0; indx<app->cam_cnt; indx++) {
        MOTPLS_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,"stream_preview_location values. Device %d row %d col %d"
            , app->cam_list[indx]->cfg->device_id
            , app->cam_list[indx]->all_loc.row
            , app->cam_list[indx]->all_loc.col);
    }

}

void cls_allcam::timing()
{
    struct timespec ts2;
    int64_t sleeptm;

    if ((restart == true) || (handler_stop == true)) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts2);

    sleeptm = ((1000000L / app->cfg->stream_maxrate) -
          (1000000L * (ts2.tv_sec - curr_ts.tv_sec)) -
          ((ts2.tv_nsec - curr_ts.tv_nsec)/1000))*1000;

    /* If over 1 second, just do one*/
    if (sleeptm > 999999999L) {
        SLEEP(1, 0);
    } else if (sleeptm > 0) {
        SLEEP(0, sleeptm);
    }

    clock_gettime(CLOCK_MONOTONIC, &curr_ts);

}

void cls_allcam::handler()
{
    mythreadname_set("ac", 0, "allcam");

    while (handler_stop == false) {
        if ((stream.norm.all_cnct > 0) &&
            (stream.norm.consumed == true)) {
            getimg(&stream.norm,"norm");
        }
        if ((stream.sub.all_cnct > 0) &&
            (stream.sub.consumed == true)) {
            getimg(&stream.sub,"norm");
        }
        if ((stream.motion.all_cnct > 0) &&
            (stream.motion.consumed == true)) {
            getimg(&stream.motion,"motion");
        }
        if ((stream.source.all_cnct > 0) &&
            (stream.source.consumed == true)) {
            getimg(&stream.source,"source");
        }
        if ((stream.secondary.all_cnct > 0) &&
            (stream.secondary.consumed == true)) {
            getimg(&stream.secondary,"secondary");
        }
        timing();
    }

    MOTPLS_LOG(NTC, TYPE_ALL, NO_ERRNO, _("All camera closed"));

    handler_running = false;
    pthread_exit(NULL);
}

void cls_allcam::handler_startup()
{
    int retcd;
    pthread_attr_t thread_attr;

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        restart = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &allcam_handler, this);
        if (retcd != 0) {
            MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start all camera thread."));
            handler_running = false;
            handler_stop = true;
        }
        pthread_attr_destroy(&thread_attr);
    }
}

void cls_allcam::handler_shutdown()
{
    int waitcnt;

    if (handler_running == true) {
        handler_stop = true;
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < app->cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == app->cfg->watchdog_tmo) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of all camera failed"));
            if (app->cfg->watchdog_kill > 0) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,app->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < app->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == app->cfg->watchdog_kill) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
        watchdog = app->cfg->watchdog_tmo;
    }
}

cls_allcam::cls_allcam(cls_motapp *p_app)
{
    app = p_app;

    watchdog = app->cfg->watchdog_tmo;
    handler_running = false;
    handler_stop = true;
    finish = false;
    memset(&all_sizes, 0, sizeof(ctx_all_sizes));
    memset(&stream, 0, sizeof(ctx_stream));
    all_sizes.reset = true;
    pthread_mutex_init(&stream.mutex, NULL);
    stream.motion.consumed = true;
    stream.norm.consumed = true;
    stream.secondary.consumed = true;
    stream.source.consumed = true;
    stream.sub.consumed = true;
    clock_gettime(CLOCK_MONOTONIC, &curr_ts);
    active_cnt    = 0;
    active_cam.clear();

    handler_startup();
}

cls_allcam::~cls_allcam()
{
    finish = true;
    handler_shutdown();
    pthread_mutex_destroy(&stream.mutex);
    stream_free();
}
