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
 *
*/

#include "motion.hpp"
#include "util.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "allcam.hpp"
#include "camera.hpp"
#include "jpegutils.hpp"


static void *allcam_handler(void *arg)
{
    ((cls_allcam *)arg)->handler();
    return nullptr;
}

void cls_allcam::getsizes_img(cls_camera *p_cam)
{
    int src_w, src_h, dst_w, dst_h;

    src_w = p_cam->all_sizes.src_w;
    src_h = p_cam->all_sizes.src_h;

    dst_w = ((p_cam->all_loc.scale * src_w) / 100);
    if ((dst_w % 8) != 0) {
        dst_w = dst_w - (dst_w % 8) + 8;
    }
    if (dst_w < 64){
        dst_w = 64;
    }
    p_cam->all_sizes.dst_w = dst_w;

    dst_h = ((p_cam->all_loc.scale * src_h) / 100);
    if ((dst_h % 8) != 0) {
        dst_h = dst_h - (dst_h % 8) + 8;
    }
    if (dst_h < 64) {
        dst_h = 64;
    }
    p_cam->all_sizes.dst_h = dst_h;
    p_cam->all_sizes.dst_sz = (dst_w * dst_h * 3)/2;
}

void cls_allcam::getimg_src(cls_camera *p_cam, std::string imgtyp, u_char *dst_img, u_char *src_img)
{
    int indx;
    ctx_stream_data *strm_c;

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

    pthread_mutex_lock(&p_cam->stream.mutex);
        indx=0;
        while (indx < 1000) {
            if (strm_c->img_data == nullptr) {
                if (strm_c->all_cnct == 0){
                    strm_c->all_cnct++;
                }
                pthread_mutex_unlock(&p_cam->stream.mutex);
                    SLEEP(0, 1000);
                pthread_mutex_lock(&p_cam->stream.mutex);
            } else {
                break;
            }
            indx++;
        }
        if ((p_cam->imgs.height != p_cam->all_sizes.src_h) ||
            (p_cam->imgs.width  != p_cam->all_sizes.src_w)) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , "Image has changed. Device: %d"
                , p_cam->cfg->device_id);
            memset(src_img, 0x00, (uint)p_cam->all_sizes.src_sz);
            p_cam->all_sizes.reset = true;
        } else if (strm_c->img_data == nullptr) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Could not get image for device %d"
                , p_cam->cfg->device_id);
            memset(src_img, 0x00, (uint)p_cam->all_sizes.src_sz);
        } else {
            memcpy(src_img, strm_c->img_data, (uint)p_cam->all_sizes.src_sz);
        }
    pthread_mutex_unlock(&p_cam->stream.mutex);

    util_resize(src_img, p_cam->all_sizes.src_w, p_cam->all_sizes.src_h
        , dst_img, p_cam->all_sizes.dst_w, p_cam->all_sizes.dst_h);

}

void cls_allcam::getimg(ctx_stream_data *strm_a, std::string imgtyp)
{
    int a_y, a_u, a_v; /* all img y,u,v */
    int c_y, c_u, c_v; /* camera img y,u,v */
    int img_orow, img_ocol;
    int indx, row, dst_w, dst_h;
    u_char *dst_img, *src_img, *all_img;
    cls_camera *p_cam;

    getsizes();

    a_y = 0;
    a_u = (all_sizes.src_w * all_sizes.src_h);
    a_v = a_u + (a_u / 4);

    all_img = (unsigned char*) mymalloc((uint)all_sizes.src_sz);
    memset(all_img , 0x80, (size_t)a_u);
    memset(all_img  + a_u, 0x80, (size_t)(a_u/2));

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        dst_w = p_cam->all_sizes.dst_w;
        dst_h = p_cam->all_sizes.dst_h;

        img_orow = p_cam->all_loc.offset_row;
        img_ocol = p_cam->all_loc.offset_col;

        dst_img = (unsigned char*) mymalloc((uint)p_cam->all_sizes.dst_sz);
        src_img = (unsigned char*) mymalloc((uint)p_cam->all_sizes.src_sz);

        getimg_src(p_cam, imgtyp, dst_img, src_img);

        a_y = (img_orow * all_sizes.src_w) + img_ocol;
        a_u =(all_sizes.src_h * all_sizes.src_w) +
            ((img_orow / 4) * all_sizes.src_w) + (img_ocol / 2) ;
        a_v = a_u + ((all_sizes.src_h * all_sizes.src_w) / 4);

        c_y = 0;
        c_u = (dst_w * dst_h);
        c_v = c_u + (c_u / 4);

        for (row=0; row<dst_h; row++) {
            memcpy(all_img  + a_y, dst_img + c_y, (uint)dst_w);
            a_y += all_sizes.src_w;
            c_y += dst_w;
            if (row % 2) {
                memcpy(all_img  + a_u, dst_img + c_u, (uint)dst_w / 2);
                //mymemset(all_img  + a_u, 0xFA, dst_w/2);
                a_u += (all_sizes.src_w / 2);
                c_u += (dst_w / 2);
                memcpy(all_img  + a_v, dst_img + c_v, (uint)dst_w / 2);
                a_v += (all_sizes.src_w / 2);
                c_v += (dst_w / 2);
            }
        }

        myfree(dst_img);
        myfree(src_img);
    }

    pthread_mutex_lock(&stream.mutex);
        memset(strm_a->img_data, 0x80, (size_t)all_sizes.dst_sz);
        util_resize(all_img, all_sizes.src_w, all_sizes.src_h
            , strm_a->img_data, all_sizes.dst_w, all_sizes.dst_h);
        myfree(all_img);

        strm_a->jpg_sz = jpgutl_put_yuv420p(
            strm_a->jpg_data, all_sizes.dst_sz, strm_a->img_data
            , all_sizes.dst_w, all_sizes.dst_h
            , 70, NULL,NULL,NULL);

        strm_a->consumed = false;
    pthread_mutex_unlock(&stream.mutex);

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
            mymalloc((size_t)all_sizes.dst_sz);
        strm->jpg_data = (unsigned char*)
            mymalloc((size_t)all_sizes.dst_sz);
        strm->consumed = true;
    }

}

void cls_allcam::getsizes_scale()
{
    int indx, row, mx_h;
    bool dflt_scale;
    cls_camera *p_cam;

    dflt_scale = false;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        if (p_cam->all_loc.scale == -1) {
            dflt_scale = true;
        }
    }
    if (dflt_scale) {
        for (row=1; row<=max_row; row++) {
            mx_h = 0;
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if (row == p_cam->all_loc.row) {
                    if (mx_h < p_cam->all_sizes.src_h) {
                        mx_h = p_cam->all_sizes.src_h;
                    }
                }
            }
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if (row == p_cam->all_loc.row) {
                    p_cam->all_loc.scale = (int)((float)(mx_h*100 / p_cam->all_sizes.src_h));
                }
            }
        }
    }

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        getsizes_img(p_cam);
        MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
            , "Device %d Original Size %dx%d Scale %d New Size %dx%d"
            , p_cam->cfg->device_id
            , p_cam->all_sizes.src_w, p_cam->all_sizes.src_h
            , p_cam->all_loc.scale
            , p_cam->all_sizes.dst_w, p_cam->all_sizes.dst_h);
    }
}

void cls_allcam::getsizes_alignv()
{
    int indx, row, col;
    int chk_sz;
    int mx_h;
    cls_camera *p_cam;

    for (row=1; row<=max_row; row++) {
        chk_sz = 0;
        mx_h = 0;
        for (col=1; col<=max_col; col++) {
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if ((row == p_cam->all_loc.row) &&
                    (col == p_cam->all_loc.col)) {
                    p_cam->all_loc.offset_col = chk_sz;
                    chk_sz += p_cam->all_sizes.dst_w;
                    if (mx_h < p_cam->all_sizes.dst_h) {
                        mx_h = p_cam->all_sizes.dst_h;
                    }
                }
            }
        }
        /* Align/center vert. the images in each row*/
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            if (p_cam->all_loc.row == row) {
                p_cam->all_loc.offset_row =
                    all_sizes.src_h +
                    ((mx_h - p_cam->all_sizes.dst_h)/2) ;
            }
        }
        all_sizes.src_h += mx_h;
        if (all_sizes.src_w < chk_sz) {
            all_sizes.src_w = chk_sz;
        }
    }
}

void cls_allcam::getsizes_alignh()
{
    int indx, col;
    int chk_sz, chk_w, mx_w;
    cls_camera *p_cam;

    /* Align/center horiz. the images within each column area */
    chk_w = 0;
    for (col=1; col<=max_col; col++) {
        chk_sz = 0;
        mx_w = 0;
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            if (p_cam->all_loc.col == col) {
                if (p_cam->all_loc.offset_col < chk_w) {
                    p_cam->all_loc.offset_col = chk_w;
                }
                if (chk_sz < p_cam->all_loc.offset_col) {
                    chk_sz = p_cam->all_loc.offset_col;
                }
                if (mx_w < p_cam->all_sizes.dst_w) {
                    mx_w = p_cam->all_sizes.dst_w;
                }
            }
        }
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];

            if (p_cam->all_loc.col == col) {
                p_cam->all_loc.offset_col =
                    chk_sz + ((mx_w - p_cam->all_sizes.dst_w) /2) ;
            }
        }
        chk_w = mx_w + chk_sz;
        if (all_sizes.src_w < chk_w) {
            all_sizes.src_w = chk_w;
        }
    }
}

void cls_allcam::getsizes_offset_user()
{
    int indx, chk_sz;
    cls_camera *p_cam;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];

        chk_sz = p_cam->all_loc.offset_col + p_cam->all_loc.offset_user_col;
        if (chk_sz < 0) {
           MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else if ((chk_sz + p_cam->all_sizes.dst_w) > all_sizes.src_w) {
           MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_col
                , p_cam->all_loc.offset_user_col);
         } else {
            p_cam->all_loc.offset_col = chk_sz;
        }

        chk_sz = p_cam->all_loc.offset_row + p_cam->all_loc.offset_user_row;
        if (chk_sz < 0 ) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else if ((chk_sz + p_cam->all_sizes.dst_h) > all_sizes.src_h) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , p_cam->all_loc.offset_row
                , p_cam->all_loc.offset_user_row);
        } else {
            p_cam->all_loc.offset_row = chk_sz;
        }
    }

}

bool cls_allcam::getsizes_reset()
{
    int indx;
    bool reset;
    cls_camera *p_cam;

    if (all_sizes.reset == true) {
        reset = true;
    } else {
        reset = false;
    }

    active_cam.clear();
    active_cnt = 0;
    for (indx=0;indx<app->cam_cnt;indx++) {
        p_cam = app->cam_list[indx];
        if (p_cam->device_status == STATUS_OPENED) {
            if (p_cam->all_sizes.reset == true) {
                reset = true;
                p_cam->all_sizes.reset = false;
            }
            active_cnt++;
            active_cam.push_back(p_cam);
        }
    }
    return reset;
}

void cls_allcam::getsizes_pct()
{
    int indx, dst_w, dst_h, dst_scale;
    cls_camera *p_cam;

    if ((all_sizes.src_h ==0) || (all_sizes.src_w == 0)) {
        all_sizes.src_w = 320;
        all_sizes.src_h = 240;
    }
    all_sizes.src_sz =((all_sizes.src_h * all_sizes.src_w * 3)/2);
    all_sizes.reset = false;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        p_cam->all_loc.xpct_st = ((p_cam->all_loc.offset_col * 100) /all_sizes.src_w);
        p_cam->all_loc.xpct_en =
            (((p_cam->all_loc.offset_col+p_cam->all_sizes.dst_w) * 100) /all_sizes.src_w);
        p_cam->all_loc.ypct_st = ((p_cam->all_loc.offset_row * 100) /all_sizes.src_h);
        p_cam->all_loc.ypct_en =
            (((p_cam->all_loc.offset_row+p_cam->all_sizes.dst_h) * 100) /all_sizes.src_h);
    }

    dst_scale = app->cfg->stream_preview_scale;

    dst_w = ((dst_scale * all_sizes.src_w) / 100);
    if ((dst_w % 8) != 0) {
        dst_w = dst_w - (dst_w % 8) + 8;
    }
    if (dst_w < 64){
        dst_w = 64;
    }
    all_sizes.dst_w = dst_w;

    dst_h = ((dst_scale * all_sizes.src_h) / 100);
    if ((dst_h % 8) != 0) {
        dst_h = dst_h - (dst_h % 8) + 8;
    }
    if (dst_h < 64) {
        dst_h = 64;
    }
    all_sizes.dst_h = dst_h;
    all_sizes.dst_sz = (dst_w * dst_h * 3)/2;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "Combined Image Original Size %dx%d Scale %d New Size %dx%d"
        , all_sizes.src_w, all_sizes.src_h
        , dst_scale
        , all_sizes.dst_w, all_sizes.dst_h);

}

void cls_allcam::init_params()
{
    int indx, indx1;
    ctx_params  *params;
    ctx_params_item *itm;
    cls_camera *p_cam;

    memset(&all_sizes, 0, sizeof(ctx_all_sizes));

    params = new ctx_params;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        p_cam->all_loc.row = -1;
        p_cam->all_loc.col = -1;
        p_cam->all_loc.offset_user_col = 0;
        p_cam->all_loc.offset_user_row = 0;
        p_cam->all_loc.scale = p_cam->cfg->stream_preview_scale;

        util_parms_parse(params
            , "stream_preview_params"
            , p_cam->cfg->stream_preview_params);

        for (indx1=0;indx1<params->params_cnt;indx1++) {
            itm = &params->params_array[indx1];
            if (itm->param_name == "row") {
                p_cam->all_loc.row = mtoi(itm->param_value);
            }
            if (itm->param_name == "col") {
                p_cam->all_loc.col = mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_col") {
                p_cam->all_loc.offset_user_col =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_row") {
                p_cam->all_loc.offset_user_row =
                    mtoi(itm->param_value);
            }
        }
        params->params_array.clear();
    }

    mydelete(params);

}

void cls_allcam::init_validate()
{
    int indx, indx1;
    int row, col, mx_row, mx_col, col_chk;
    bool cfg_valid, chk;
    std::string cfg_row, cfg_col;
    cls_camera *p_cam, *p_cam1;

    mx_row = 0;
    mx_col = 0;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        if (mx_col < p_cam->all_loc.col) {
            mx_col = p_cam->all_loc.col;
        }
        if (mx_row < p_cam->all_loc.row) {
            mx_row = p_cam->all_loc.row;
        }
    }

    cfg_valid = true;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        if ((p_cam->all_loc.col == -1) ||
            (p_cam->all_loc.row == -1)) {
            cfg_valid = false;
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "No stream_preview_params for cam %d"
                , p_cam->cfg->device_id);
        } else {
            for (indx1=0; indx1<active_cnt; indx1++) {
                p_cam1 = active_cam[indx1];
                if ((p_cam->all_loc.col == p_cam1->all_loc.col) &&
                    (p_cam->all_loc.row == p_cam1->all_loc.row) &&
                    (indx != indx1)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        , "Duplicate stream_preview_params "
                        " cam %d, cam %d row %d col %d"
                        , p_cam->cfg->device_id
                        , p_cam1->cfg->device_id
                        , p_cam->all_loc.row
                        , p_cam->all_loc.col);
                    cfg_valid = false;
                }
            }
        }
        if (p_cam->all_loc.row == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_params row cam %d, row %d"
                , p_cam->cfg->device_id
                , p_cam->all_loc.row);
            cfg_valid = false;
        }
        if (p_cam->all_loc.col == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_params col cam %d, col %d"
                , p_cam->cfg->device_id
                , p_cam->all_loc.col);
            cfg_valid = false;
        }
    }

    for (row=1; row<=mx_row; row++) {
        chk = false;
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            if (row == p_cam->all_loc.row) {
                chk = true;
            }
        }
        if (chk == false) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_preview_params combination. "
                " Missing row %d", row);
            cfg_valid = false;
        }
        col_chk = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx];
                if ((row == p_cam->all_loc.row) &&
                    (col == p_cam->all_loc.col)) {
                    if ((col_chk+1) == col) {
                        col_chk = col;
                    } else {
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , "Invalid stream_preview_params combination. "
                            " Missing row %d column %d", row, col_chk+1);
                        cfg_valid = false;
                    }
                }
            }
        }
    }

    if (cfg_valid == false) {
        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
            ,"Creating default stream preview values");
        row = 0;
        col = 0;
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx];
            if (col == 1) {
                col++;
            } else {
                row++;
                col = 1;
            }
            p_cam->all_loc.col = col;
            p_cam->all_loc.row = row;
        }
    }

}

void cls_allcam::init_cams()
{
    int indx;
    cls_camera *p_cam;

    init_params();
    init_validate();

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,"stream_preview_params values. Device %d row %d col %d"
            , p_cam->cfg->device_id
            , p_cam->all_loc.row
            , p_cam->all_loc.col);
    }

    all_sizes.src_w = 0;
    all_sizes.src_h = 0;
    all_sizes.src_sz = 0;

    max_row = 1;
    max_col = 1;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx];
        p_cam->all_sizes.src_w = p_cam->imgs.width;
        p_cam->all_sizes.src_h = p_cam->imgs.height;
        p_cam->all_sizes.src_sz = ((p_cam->imgs.height * p_cam->imgs.width * 3)/2);
        if (max_row < p_cam->all_loc.row) {
            max_row = p_cam->all_loc.row;
        }
        if (max_col < p_cam->all_loc.col) {
            max_col = p_cam->all_loc.col;
        }
    }
}

void cls_allcam::getsizes()
{
    if (getsizes_reset() == false) {
        return;
    }
    init_cams();
    getsizes_scale();
    getsizes_alignv();
    getsizes_alignh();
    getsizes_offset_user();
    getsizes_pct();
    stream_free();
    stream_alloc();

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

    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("All camera closed"));

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
            MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start all camera thread."));
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
            MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of all camera failed"));
            if (app->cfg->watchdog_kill > 0) {
                MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,app->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < app->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == app->cfg->watchdog_kill) {
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
