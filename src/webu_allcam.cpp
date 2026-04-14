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
#include "webu.hpp"
#include "webu_allcam.hpp"
#include "camera.hpp"
#include "jpegutils.hpp"


static void *allcam_handler(void *arg)
{
    ((cls_allcam *)arg)->handler();
    return nullptr;
}

void cls_allcam::getsizes_img(int indx)
{
    int src_w, src_h, dst_w, dst_h;

    src_w = active_cam[indx].src_w;
    src_h = active_cam[indx].src_h;

    dst_w = ((active_cam[indx].scale * src_w) / 100);
    if ((dst_w % 8) != 0) {
        dst_w = dst_w - (dst_w % 8) + 8;
    }
    if (dst_w < 64){
        dst_w = 64;
    }
    active_cam[indx].dst_w = dst_w;

    dst_h = ((active_cam[indx].scale * src_h) / 100);
    if ((dst_h % 8) != 0) {
        dst_h = dst_h - (dst_h % 8) + 8;
    }
    if (dst_h < 64) {
        dst_h = 64;
    }
    active_cam[indx].dst_h = dst_h;
    active_cam[indx].dst_sz = (dst_w * dst_h * 3)/2;
}

void cls_allcam::getimg_src(cls_camera *p_cam
    , std::string imgtyp, u_char *dst_img, u_char *src_img, int indx_act)
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
        if ((p_cam->imgs.height != active_cam[indx_act].src_h) ||
            (p_cam->imgs.width  != active_cam[indx_act].src_w)) {
            MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
                , "Image has changed. Device: %d %dx%d to %dx%d"
                , p_cam->cfg->device_id
                , active_cam[indx_act].src_w
                , active_cam[indx_act].src_h
                , p_cam->imgs.width
                , p_cam->imgs.height
            );
            memset(src_img, 0x00, (uint)active_cam[indx_act].src_sz);
            reset = true;
        } else if (strm_c->img_data == nullptr) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Could not get image for device %d"
                , p_cam->cfg->device_id);
            memset(src_img, 0x00, (uint)active_cam[indx_act].src_sz);
        } else {
            memcpy(src_img, strm_c->img_data, (uint)active_cam[indx_act].src_sz);
        }
    pthread_mutex_unlock(&p_cam->stream.mutex);

    util_resize(src_img
        , active_cam[indx_act].src_w
        , active_cam[indx_act].src_h
        , dst_img
        , active_cam[indx_act].dst_w
        , active_cam[indx_act].dst_h);

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
    a_u = (info.src_w * info.src_h);
    a_v = a_u + (a_u / 4);

    all_img = (unsigned char*) mymalloc((uint)info.src_sz);
    memset(all_img , 0x80, (size_t)a_u);
    memset(all_img  + a_u, 0x80, (size_t)(a_u/2));

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        dst_w = active_cam[indx].dst_w;
        dst_h = active_cam[indx].dst_h;

        img_orow = active_cam[indx].offset_row;
        img_ocol = active_cam[indx].offset_col;

        dst_img = (unsigned char*) mymalloc((uint)active_cam[indx].dst_sz);
        src_img = (unsigned char*) mymalloc((uint)active_cam[indx].src_sz);

        getimg_src(p_cam, imgtyp, dst_img, src_img, indx);

        a_y = (img_orow * info.src_w) + img_ocol;
        a_u =(info.src_h * info.src_w) +
            ((img_orow / 4) * info.src_w) + (img_ocol / 2) ;
        a_v = a_u + ((info.src_h * info.src_w) / 4);

        c_y = 0;
        c_u = (dst_w * dst_h);
        c_v = c_u + (c_u / 4);

        for (row=0; row<dst_h; row++) {
            memcpy(all_img  + a_y, dst_img + c_y, (uint)dst_w);
            a_y += info.src_w;
            c_y += dst_w;
            if (row % 2) {
                memcpy(all_img  + a_u, dst_img + c_u, (uint)dst_w / 2);
                //mymemset(all_img  + a_u, 0xFA, dst_w/2);
                a_u += (info.src_w / 2);
                c_u += (dst_w / 2);
                memcpy(all_img  + a_v, dst_img + c_v, (uint)dst_w / 2);
                a_v += (info.src_w / 2);
                c_v += (dst_w / 2);
            }
        }

        myfree(dst_img);
        myfree(src_img);
    }

    pthread_mutex_lock(&stream.mutex);
        memset(strm_a->img_data, 0x80, (size_t)info.dst_sz);
        util_resize(all_img, info.src_w, info.src_h
            , strm_a->img_data, info.dst_w, info.dst_h);
        myfree(all_img);

        strm_a->jpg_sz = jpgutl_put_yuv420p(
            strm_a->jpg_data, info.dst_sz, strm_a->img_data
            , info.dst_w, info.dst_h
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
            mymalloc((size_t)info.dst_sz);
        strm->jpg_data = (unsigned char*)
            mymalloc((size_t)info.dst_sz);
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
        p_cam = active_cam[indx].cam;
        if (active_cam[indx].scale == -1) {
            dflt_scale = true;
        }
    }
    if (dflt_scale) {
        for (row=1; row<=max_row; row++) {
            mx_h = 0;
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx].cam;
                if (row == active_cam[indx].row) {
                    if (mx_h < active_cam[indx].src_h) {
                        mx_h = active_cam[indx].src_h;
                    }
                }
            }
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx].cam;
                if (row == active_cam[indx].row) {
                    active_cam[indx].scale = (int)((float)(mx_h*100 / active_cam[indx].src_h));
                }
            }
        }
    }

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        getsizes_img(indx);
        MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
            , "Device %d Original Size %dx%d Scale %d New Size %dx%d"
            , p_cam->cfg->device_id
            , active_cam[indx].src_w, active_cam[indx].src_h
            , active_cam[indx].scale
            , active_cam[indx].dst_w, active_cam[indx].dst_h);
    }
}

void cls_allcam::getsizes_alignv()
{
    int indx, row, col;
    int chk_sz;
    int mx_h;

    for (row=1; row<=max_row; row++) {
        chk_sz = 0;
        mx_h = 0;
        for (col=1; col<=max_col; col++) {
            for (indx=0; indx<active_cnt; indx++) {
                if ((row == active_cam[indx].row) &&
                    (col == active_cam[indx].col)) {
                    active_cam[indx].offset_col = chk_sz;
                    chk_sz += active_cam[indx].dst_w;
                    if (mx_h < active_cam[indx].dst_h) {
                        mx_h = active_cam[indx].dst_h;
                    }
                }
            }
        }
        /* Align/center vert. the images in each row*/
        for (indx=0; indx<active_cnt; indx++) {
            if (active_cam[indx].row == row) {
                active_cam[indx].offset_row =
                    info.src_h +
                    ((mx_h - active_cam[indx].dst_h)/2) ;
            }
        }
        info.src_h += mx_h;
        if (info.src_w < chk_sz) {
            info.src_w = chk_sz;
        }
    }
}

void cls_allcam::getsizes_alignh()
{
    int indx, col;
    int chk_sz, chk_w, mx_w;

    /* Align/center horiz. the images within each column area */
    chk_w = 0;
    for (col=1; col<=max_col; col++) {
        chk_sz = 0;
        mx_w = 0;
        for (indx=0; indx<active_cnt; indx++) {
            if (active_cam[indx].col == col) {
                if (active_cam[indx].offset_col < chk_w) {
                    active_cam[indx].offset_col = chk_w;
                }
                if (chk_sz < active_cam[indx].offset_col) {
                    chk_sz = active_cam[indx].offset_col;
                }
                if (mx_w < active_cam[indx].dst_w) {
                    mx_w = active_cam[indx].dst_w;
                }
            }
        }
        for (indx=0; indx<active_cnt; indx++) {
            if (active_cam[indx].col == col) {
                active_cam[indx].offset_col =
                    chk_sz + ((mx_w - active_cam[indx].dst_w) /2) ;
            }
        }
        chk_w = mx_w + chk_sz;
        if (info.src_w < chk_w) {
            info.src_w = chk_w;
        }
    }
}

void cls_allcam::getsizes_offset_user()
{
    int indx, chk_sz;
    cls_camera *p_cam;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;

        chk_sz = active_cam[indx].offset_col + active_cam[indx].offset_user_col;
        if (chk_sz < 0) {
           MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , active_cam[indx].offset_col
                , active_cam[indx].offset_user_col);
         } else if ((chk_sz + active_cam[indx].dst_w) > info.src_w) {
           MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image column offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , active_cam[indx].offset_col
                , active_cam[indx].offset_user_col);
         } else {
            active_cam[indx].offset_col = chk_sz;
        }

        chk_sz = active_cam[indx].offset_row + active_cam[indx].offset_user_row;
        if (chk_sz < 0 ) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) less than zero "
                , p_cam->cfg->device_id
                , active_cam[indx].offset_row
                , active_cam[indx].offset_user_row);
        } else if ((chk_sz + active_cam[indx].dst_h) > info.src_h) {
            MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
                , "Device %d invalid image row offset. (%d + %d) over image size"
                , p_cam->cfg->device_id
                , active_cam[indx].offset_row
                , active_cam[indx].offset_user_row);
        } else {
            active_cam[indx].offset_row = chk_sz;
        }
    }

}

void cls_allcam::getsizes_pct()
{
    int indx, dst_w, dst_h, dst_scale;

    if ((info.src_h ==0) || (info.src_w == 0)) {
        info.src_w = 320;
        info.src_h = 240;
    }
    info.src_sz =((info.src_h * info.src_w * 3)/2);
    reset = false;

    for (indx=0; indx<active_cnt; indx++) {
        active_cam[indx].xpct_st = ((active_cam[indx].offset_col * 100) /info.src_w);
        active_cam[indx].xpct_en =
            (((active_cam[indx].offset_col+active_cam[indx].dst_w) * 100) /info.src_w);
        active_cam[indx].ypct_st = ((active_cam[indx].offset_row * 100) /info.src_h);
        active_cam[indx].ypct_en =
            (((active_cam[indx].offset_row+active_cam[indx].dst_h) * 100) /info.src_h);
    }

    dst_scale = app->cfg->stream_preview_scale;

    dst_w = ((dst_scale * info.src_w) / 100);
    if ((dst_w % 8) != 0) {
        dst_w = dst_w - (dst_w % 8) + 8;
    }
    if (dst_w < 64){
        dst_w = 64;
    }
    info.dst_w = dst_w;

    dst_h = ((dst_scale * info.src_h) / 100);
    if ((dst_h % 8) != 0) {
        dst_h = dst_h - (dst_h % 8) + 8;
    }
    if (dst_h < 64) {
        dst_h = 64;
    }
    info.dst_h = dst_h;
    info.dst_sz = (dst_w * dst_h * 3)/2;

    MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
        , "Combined Image Original Size %dx%d Scale %d New Size %dx%d"
        , info.src_w, info.src_h
        , dst_scale
        , info.dst_w, info.dst_h);

}

void cls_allcam::init_params()
{
    int indx, indx1;
    std::string portnbr;
    ctx_params  *params;
    ctx_params_item *itm;
    cls_camera *p_cam;

    portnbr = std::to_string(webu->cfg->webcontrol_port);

    memset(&info, 0, sizeof(ctx_allcam_info));

    params = new ctx_params;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        active_cam[indx].row = -1;
        active_cam[indx].col = -1;
        active_cam[indx].offset_user_col = 0;
        active_cam[indx].offset_user_row = 0;
        active_cam[indx].scale = p_cam->cfg->stream_preview_scale;

        util_parms_parse(params
            , "stream_allcam_params"
            , p_cam->cfg->stream_allcam_params);

        for (indx1=0;indx1<params->params_cnt;indx1++) {
            itm = &params->params_array[indx1];
            if (itm->param_name == "row") {
                active_cam[indx].row = mtoi(itm->param_value);
            }
            if (itm->param_name == "col") {
                active_cam[indx].col = mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_col") {
                active_cam[indx].offset_user_col =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_row") {
                active_cam[indx].offset_user_row =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "scale") {
                active_cam[indx].scale =
                    mtoi(itm->param_value);
            }
        }

        for (indx1=0;indx1<params->params_cnt;indx1++) {
            itm = &params->params_array[indx1];
            if (itm->param_name == "row"+portnbr) {
                active_cam[indx].row = mtoi(itm->param_value);
            }
            if (itm->param_name == "col"+portnbr) {
                active_cam[indx].col = mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_col"+portnbr) {
                active_cam[indx].offset_user_col =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "offset_row"+portnbr) {
                active_cam[indx].offset_user_row =
                    mtoi(itm->param_value);
            }
            if (itm->param_name == "scale"+portnbr) {
                active_cam[indx].scale =
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
        p_cam = active_cam[indx].cam;
        if (mx_col < active_cam[indx].col) {
            mx_col = active_cam[indx].col;
        }
        if (mx_row < active_cam[indx].row) {
            mx_row = active_cam[indx].row;
        }
    }

    cfg_valid = true;

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        if ((active_cam[indx].col == -1) ||
            (active_cam[indx].row == -1)) {
            cfg_valid = false;
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "No stream_allcam_params for cam %d"
                , p_cam->cfg->device_id);
        } else {
            for (indx1=0; indx1<active_cnt; indx1++) {
                p_cam1 = active_cam[indx1].cam;
                if ((active_cam[indx].col == active_cam[indx1].col) &&
                    (active_cam[indx].row == active_cam[indx1].row) &&
                    (indx != indx1)) {
                    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                        , "Duplicate stream_allcam_params "
                        " cam %d, cam %d row %d col %d"
                        , p_cam->cfg->device_id
                        , p_cam1->cfg->device_id
                        , active_cam[indx].row
                        , active_cam[indx].col);
                    cfg_valid = false;
                }
            }
        }
        if (active_cam[indx].row == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_allcam_params row cam %d, row %d"
                , p_cam->cfg->device_id
                , active_cam[indx].row);
            cfg_valid = false;
        }
        if (active_cam[indx].col == 0) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_allcam_params col cam %d, col %d"
                , p_cam->cfg->device_id
                , active_cam[indx].col);
            cfg_valid = false;
        }
    }

    for (row=1; row<=mx_row; row++) {
        chk = false;
        for (indx=0; indx<active_cnt; indx++) {
            p_cam = active_cam[indx].cam;
            if (row == active_cam[indx].row) {
                chk = true;
            }
        }
        if (chk == false) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                , "Invalid stream_allcam_params combination. "
                " Missing row %d", row);
            cfg_valid = false;
        }
        col_chk = 0;
        for (col=1; col<=mx_col; col++) {
            for (indx=0; indx<active_cnt; indx++) {
                p_cam = active_cam[indx].cam;
                if ((row == active_cam[indx].row) &&
                    (col == active_cam[indx].col)) {
                    if ((col_chk+1) == col) {
                        col_chk = col;
                    } else {
                        MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO
                            , "Invalid stream_allcam_params combination. "
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
            p_cam = active_cam[indx].cam;
            if (col == 1) {
                col++;
            } else {
                row++;
                col = 1;
            }
            active_cam[indx].col = col;
            active_cam[indx].row = row;
        }
    }

}

void cls_allcam::init_active()
{
    int indx;
    cls_camera *p_cam;
    ctx_allcam_info p_info;

    active_cam.clear();
    active_cnt = 0;
    memset(&p_info,0,sizeof(p_info));
    for (indx=0;indx<webu->cam_cnt;indx++) {
        p_cam = webu->cam_list[indx];
        if (p_cam->device_status == STATUS_OPENED) {
            p_info.cam = app->cam_list[indx];
            active_cam.push_back(p_info);
            active_cnt++;
        }
    }
}

void cls_allcam::init_cams()
{
    int indx;
    cls_camera *p_cam;

    init_active();
    init_params();
    init_validate();

    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO
            ,"stream_allcam_params values. Device %d row %d col %d"
            , p_cam->cfg->device_id
            , active_cam[indx].row
            , active_cam[indx].col);
    }

    info.src_w = 0;
    info.src_h = 0;
    info.src_sz = 0;

    max_row = 1;
    max_col = 1;
    for (indx=0; indx<active_cnt; indx++) {
        p_cam = active_cam[indx].cam;
        active_cam[indx].src_w = p_cam->imgs.width;
        active_cam[indx].src_h = p_cam->imgs.height;
        active_cam[indx].src_sz = ((p_cam->imgs.height * p_cam->imgs.width * 3)/2);
        if (max_row < active_cam[indx].row) {
            max_row = active_cam[indx].row;
        }
        if (max_col < active_cam[indx].col) {
            max_col = active_cam[indx].col;
        }
    }
}

void cls_allcam::getsizes()
{
    if (reset == false) {
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
    mythreadname_set("ac", webuindx, "");

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

cls_allcam::cls_allcam(cls_webu *p_webu, int p_indx)
{
    webu = p_webu;
    app = p_webu->app;

    watchdog = app->cfg->watchdog_tmo;
    handler_running = false;
    handler_stop = true;
    finish = false;
    memset(&info, 0, sizeof(ctx_allcam_info));
    memset(&stream, 0, sizeof(ctx_stream));
    reset = true;
    pthread_mutex_init(&stream.mutex, NULL);
    stream.motion.consumed = true;
    stream.norm.consumed = true;
    stream.secondary.consumed = true;
    stream.source.consumed = true;
    stream.sub.consumed = true;
    clock_gettime(CLOCK_MONOTONIC, &curr_ts);
    active_cnt    = 0;
    active_cam.clear();
    webuindx = p_indx;

    handler_startup();
}

cls_allcam::~cls_allcam()
{
    finish = true;
    handler_shutdown();
    pthread_mutex_destroy(&stream.mutex);
    stream_free();
}
