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
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "rotate.hpp"
#include "video_convert.hpp"
#include "video_v4l2.hpp"
#include <sys/mman.h>

#define MMAP_BUFFERS            4
#define MIN_MMAP_BUFFERS        2

#ifdef HAVE_V4L2

void cls_v4l2cam::palette_add(uint p_v4l2id)
{
    char    tmp4cc[5];
    ctx_palette_item    p_itm;

    sprintf(tmp4cc,"%c%c%c%c"
        , p_v4l2id >> 0 , p_v4l2id >> 8
        , p_v4l2id >> 16, p_v4l2id >> 24);
    p_itm.v4l2id = p_v4l2id;
    p_itm.fourcc = tmp4cc;
    palette.push_back(p_itm);
}

void cls_v4l2cam::palette_init()
{
    palette.clear();

    palette_add(V4L2_PIX_FMT_SN9C10X);
    palette_add(V4L2_PIX_FMT_SBGGR16);
    palette_add(V4L2_PIX_FMT_SBGGR8);
    palette_add(V4L2_PIX_FMT_SPCA561);
    palette_add(V4L2_PIX_FMT_SGBRG8);
    palette_add(V4L2_PIX_FMT_SGRBG8);
    palette_add(V4L2_PIX_FMT_PAC207);
    palette_add(V4L2_PIX_FMT_PJPG);
    palette_add(V4L2_PIX_FMT_MJPEG);
    palette_add(V4L2_PIX_FMT_JPEG);
    palette_add(V4L2_PIX_FMT_RGB24);
    palette_add(V4L2_PIX_FMT_SPCA501);
    palette_add(V4L2_PIX_FMT_SPCA505);
    palette_add(V4L2_PIX_FMT_SPCA508);
    palette_add(V4L2_PIX_FMT_UYVY);
    palette_add(V4L2_PIX_FMT_YUYV);
    palette_add(V4L2_PIX_FMT_YUV422P);
    palette_add(V4L2_PIX_FMT_YUV420);
    palette_add(V4L2_PIX_FMT_Y10);
    palette_add(V4L2_PIX_FMT_Y12);
    palette_add(V4L2_PIX_FMT_GREY);
    palette_add(V4L2_PIX_FMT_SRGGB8);
}

/* Execute the request to the device */
int cls_v4l2cam::xioctl(unsigned long request, void *arg)
{
    int retcd;

    if (fd_device < 0) {
        return -1;
    }
    cam->watchdog = cam->cfg->watchdog_tmo;
    do {
        retcd = ioctl(fd_device, request, arg);
    } while (-1 == retcd && EINTR == errno);

    return retcd;
}

void cls_v4l2cam::device_close()
{
    close(fd_device);
    fd_device = -1;
}

/* Print the device controls to the log */
void cls_v4l2cam::ctrls_log()
{
    int indx;
    ctx_v4l2ctrl_item *itm;

    if (device_ctrls.size() >0) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("---------Controls---------"));
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("  V4L2 ID :  Name : Range"));
        for (indx=0;indx<device_ctrls.size();indx++){
            itm = &device_ctrls[indx];
            if (itm->ctrl_menuitem) {
                MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "  %s : %s"
                    ,itm->ctrl_iddesc.c_str()
                    ,itm->ctrl_name.c_str());
            } else {
                MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s : %s : %d to %d"
                    ,itm->ctrl_iddesc.c_str()
                    ,itm->ctrl_name.c_str()
                    ,itm->ctrl_minimum
                    ,itm->ctrl_maximum);
            }
        }
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, "--------------------------");
    }
}

/* Get names of the controls and menu items the device supports */
void cls_v4l2cam::ctrls_list()
{
    int indx;
    v4l2_queryctrl      vid_ctrl;
    v4l2_querymenu      vid_menu;
    ctx_v4l2ctrl_item   vid_item;
    char tmp_desc[40];

    if (fd_device == -1) {
        return;
    }

    device_ctrls.clear();
    memset(&vid_ctrl, 0, sizeof(struct v4l2_queryctrl));
    vid_ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (xioctl(VIDIOC_QUERYCTRL, &vid_ctrl) == 0) {
        if (vid_ctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
            vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }
        vid_item.ctrl_id = vid_ctrl.id;
        vid_item.ctrl_type = vid_ctrl.type;
        vid_item.ctrl_default = vid_ctrl.default_value;
        vid_item.ctrl_currval = vid_ctrl.default_value;
        vid_item.ctrl_newval = vid_ctrl.default_value;
        vid_item.ctrl_menuitem = false;
        vid_item.ctrl_name =(char*)vid_ctrl.name;
        sprintf(tmp_desc,"ID%08d",vid_ctrl.id);
        vid_item.ctrl_iddesc = tmp_desc;
        vid_item.ctrl_minimum = vid_ctrl.minimum;
        vid_item.ctrl_maximum = vid_ctrl.maximum;
        device_ctrls.push_back(vid_item);

        if (vid_ctrl.type == V4L2_CTRL_TYPE_MENU) {
            for (indx = vid_ctrl.minimum; indx <= vid_ctrl.maximum; indx++) {
                memset(&vid_menu, 0, sizeof(struct v4l2_querymenu));
                vid_menu.id = vid_ctrl.id;
                vid_menu.index = (uint)indx;
                if (xioctl(VIDIOC_QUERYMENU, &vid_menu) == 0) {
                    vid_item.ctrl_id = vid_ctrl.id;
                    vid_item.ctrl_type = 0;
                    vid_item.ctrl_menuitem = true;
                    vid_item.ctrl_name =(char*)vid_menu.name;
                    sprintf(tmp_desc,"menu item: Value %d",indx);
                    vid_item.ctrl_iddesc = tmp_desc;
                    vid_item.ctrl_minimum = 0;
                    vid_item.ctrl_maximum = 0;
                    device_ctrls.push_back(vid_item);
                }
           }
        }
        vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (device_ctrls.size() == 0) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("No Controls found for device"));
        return;
    }

    ctrls_log();

}

/* Set the control array items to the device */
void cls_v4l2cam::ctrls_set()
{
    int retcd, indx;
    struct v4l2_control vid_ctrl;
    ctx_v4l2ctrl_item *itm;

    if (fd_device == -1) {
        return;
    }

    for (indx=0;indx<device_ctrls.size();indx++) {
        itm = &device_ctrls[indx];
        if (itm->ctrl_menuitem == false) {
            if (itm->ctrl_currval != itm->ctrl_newval) {
                memset(&vid_ctrl, 0, sizeof (struct v4l2_control));
                vid_ctrl.id = itm->ctrl_id;
                vid_ctrl.value = itm->ctrl_newval;
                retcd = xioctl(VIDIOC_S_CTRL, &vid_ctrl);
                if (retcd < 0) {
                    MOTPLS_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO
                        ,_("setting control %s \"%s\" to %d failed with return code %d")
                        ,itm->ctrl_iddesc.c_str(), itm->ctrl_name.c_str()
                        ,itm->ctrl_newval, retcd);
                } else {
                    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
                        ,_("Set control \"%s\" to value %d")
                        ,itm->ctrl_name.c_str(), itm->ctrl_newval);
                   itm->ctrl_currval = itm->ctrl_newval;
                }
            }
        }
     }
}

void cls_v4l2cam::parms_set()
{
    int indx_p, indx_d;
    ctx_params_item *itm_p;
    ctx_v4l2ctrl_item *itm_d;

    if (device_ctrls.size() == 0) {
        return;
    }

    for (indx_d = 0;indx_d< device_ctrls.size(); indx_d++) {
        itm_d = &device_ctrls[indx_d];
        itm_d->ctrl_newval = itm_d->ctrl_default;
        for (indx_p=0;indx_p<params->params_cnt;indx_p++) {
            itm_p = &params->params_array[indx_p];
            if ((itm_d->ctrl_iddesc == itm_p->param_name) ||
                (itm_d->ctrl_name == itm_p->param_name)) {
                switch (itm_d->ctrl_type) {
                case V4L2_CTRL_TYPE_MENU:
                    /*FALLTHROUGH*/
                case V4L2_CTRL_TYPE_INTEGER:
                    if (mtoi(itm_p->param_value.c_str()) < itm_d->ctrl_minimum) {
                        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                            ,_("%s control option value %s is below minimum.  Skipping...")
                            , itm_d->ctrl_name.c_str()
                            , itm_p->param_value.c_str()
                            , itm_d->ctrl_minimum);
                    } else if (mtoi(itm_p->param_value.c_str()) > itm_d->ctrl_maximum) {
                        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                            ,_("%s control option value %s is above maximum.  Skipping...")
                            , itm_d->ctrl_name.c_str()
                            , itm_p->param_value.c_str()
                            , itm_d->ctrl_maximum);
                    } else {
                        itm_d->ctrl_newval = mtoi(itm_p->param_value.c_str());
                    }
                    break;
                case V4L2_CTRL_TYPE_BOOLEAN:
                    itm_d->ctrl_newval = mtob(itm_p->param_value.c_str()) ? 1 : 0;
                    break;
                default:
                    MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                        ,_("control type not supported"));
                }
            }
        }
    }

}

/* Set the device to the input number requested by user */
void cls_v4l2cam::set_input()
{
    int spec, indx;
    struct v4l2_input    input;

    if (fd_device == -1) {
        return;
    }

    spec = -1;
    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "input") {
            spec =  mtoi(params->params_array[indx].param_value);
            break;
        }
    }

    memset(&input, 0, sizeof (struct v4l2_input));
    if (spec == -1) {
        input.index = 0;
    } else {
        input.index = (uint)spec;
    }

    if (xioctl(VIDIOC_ENUMINPUT, &input) == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Unable to query input %d."
            " VIDIOC_ENUMINPUT, if you use a WEBCAM change input value in conf by -1")
            ,input.index);
        device_close();
        return;
    }

    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
        ,_("Name = \"%s\", type 0x%08X, status %08x")
        ,input.name, input.type, input.status);

    if (input.type & V4L2_INPUT_TYPE_TUNER) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Name = \"%s\",- TUNER"), input.name);
    }

    if (input.type & V4L2_INPUT_TYPE_CAMERA) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Name = \"%s\"- CAMERA"), input.name);
    }

    if (xioctl(VIDIOC_S_INPUT, &input.index) == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            , _("Error selecting input %d VIDIOC_S_INPUT"), input.index);
        device_close();
        return;
    }

    device_type  = (int)input.type;
    device_tuner = (int)input.tuner;

    return;
}

/* Set the video standard(norm) for the device to the user requested value*/
void cls_v4l2cam::set_norm()
{
    int spec, indx;
    struct v4l2_standard standard;
    v4l2_std_id std_id;

    if (fd_device == -1) {
        return;
    }

    spec = 1;
    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "norm") {
            spec =  mtoi(params->params_array[indx].param_value);
            break;
        }
    }

    if (xioctl(VIDIOC_G_STD, &std_id) == -1) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("Device does not support specifying PAL/NTSC norm"));
        return;
    }

    if (std_id) {
        memset(&standard, 0, sizeof(struct v4l2_standard));
        standard.index = 0;

        while (xioctl(VIDIOC_ENUMSTD, &standard) == 0) {
            if (standard.id & std_id) {
                MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                    ,_("- video standard %s"), standard.name);
            }
            standard.index++;
        }

        switch (spec) {
        case 1:
            std_id = V4L2_STD_NTSC;
            break;
        case 2:
            std_id = V4L2_STD_SECAM;
            break;
        default:
            std_id = V4L2_STD_PAL;
        }

        if (xioctl(VIDIOC_S_STD, &std_id) == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error selecting standard method %d VIDIOC_S_STD")
                ,(int)std_id);
        }

        if (std_id == V4L2_STD_NTSC) {
            MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to NTSC"));
        } else if (std_id == V4L2_STD_SECAM) {
            MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to SECAM"));
        } else {
            MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to PAL"));
        }
    }

     return;
}

/* Set the frequency on the device to the user requested value */
void cls_v4l2cam::set_frequency()
{
    long spec, indx;
    struct v4l2_tuner     tuner;
    struct v4l2_frequency freq;

    if (fd_device == -1) {
        return;
    }

    spec = 0;
    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "frequency") {
            spec =  mtol(params->params_array[indx].param_value);
            break;
        }
    }

    /* If this input is attached to a tuner, set the frequency. */
    if (device_type & V4L2_INPUT_TYPE_TUNER) {
        /* Query the tuners capabilities. */
        memset(&tuner, 0, sizeof(struct v4l2_tuner));
        tuner.index = (uint)device_tuner;

        if (xioctl(VIDIOC_G_TUNER, &tuner) == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("tuner %d VIDIOC_G_TUNER"), tuner.index);
            return;
        }

        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Set tuner %d"), tuner.index);

        /* Set the frequency. */
        memset(&freq, 0, sizeof(struct v4l2_frequency));
        freq.tuner = (uint)device_tuner;
        freq.type = V4L2_TUNER_ANALOG_TV;
        freq.frequency = (uint)((spec / 1000) * 16);

        if (xioctl(VIDIOC_S_FREQUENCY, &freq) == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("freq %ul VIDIOC_S_FREQUENCY"), freq.frequency);
            return;
        }

        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Set Frequency to %ul"), freq.frequency);
    }

    return;
}

int cls_v4l2cam::pixfmt_try(uint pixformat)
{
    int retcd;

    memset(&vidfmt, 0, sizeof(struct v4l2_format));

    vidfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vidfmt.fmt.pix.width = (uint)cam->cfg->width;
    vidfmt.fmt.pix.height = (uint)cam->cfg->height;
    vidfmt.fmt.pix.pixelformat = pixformat;
    vidfmt.fmt.pix.field = V4L2_FIELD_ANY;

    retcd = xioctl(VIDIOC_TRY_FMT, &vidfmt);
    if ((retcd == -1) || (vidfmt.fmt.pix.pixelformat != pixformat)) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Unable to use palette %c%c%c%c (%dx%d)")
            ,pixformat >> 0, pixformat >> 8
            ,pixformat >> 16, pixformat >> 24
            ,cam->cfg->width, cam->cfg->height);
        return -1;
    }

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Testing palette %c%c%c%c (%dx%d)")
        ,pixformat >> 0, pixformat >> 8
        ,pixformat >> 16, pixformat >> 24
        ,cam->cfg->width, cam->cfg->height);

    return 0;
}

int cls_v4l2cam::pixfmt_stride()
{
    int wd, bpl, wps;

    cam->cfg->width = (int)vidfmt.fmt.pix.width;
    cam->cfg->height = (int)vidfmt.fmt.pix.height;

    bpl = (int)vidfmt.fmt.pix.bytesperline;
    wd = cam->cfg->width;

    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
        , _("Checking image size %dx%d with stride %d")
        , cam->cfg->width, cam->cfg->height, bpl);

    if (bpl == 0) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , _("No stride value provided from device."));
        return 0;
    }

    if (wd > bpl) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , _("Width(%d) must be less than stride(%d)"), wd, bpl);
        return -1;
    }

    /* For perfect multiples of width and stride, no adjustment needed */
    if ((wd == bpl) || ((bpl % wd) == 0)) {
        return 0;
    }

    MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
        , _("The image width(%d) is not multiple of the stride(%d)")
        , wd, bpl);

    /* Width per stride */
    wps = bpl / wd;
    if (wps < 1) {
        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            , _("Impossible condition: Width(%d), Stride(%d), Per stride(%d)")
            , wd, bpl, wps);
    }

    MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
        , _("Image width will be padded %d bytes"), ((bpl % wd)/wps));

    cam->cfg->width = wd + ((bpl % wd)/wps);

    return 0;

}

int cls_v4l2cam::pixfmt_adjust()
{
    if ((vidfmt.fmt.pix.width != (uint)cam->cfg->width) ||
        (vidfmt.fmt.pix.height != (uint)cam->cfg->height)) {

        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Adjusting resolution from %ix%i to %ix%i.")
            ,cam->cfg->width, cam->cfg->height
            ,vidfmt.fmt.pix.width
            ,vidfmt.fmt.pix.height);

        cam->cfg->width = (int)vidfmt.fmt.pix.width;
        cam->cfg->height = (int)vidfmt.fmt.pix.height;

        if ((cam->cfg->width % 8) || (cam->cfg->height % 8)) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Adjusted resolution not modulo 8."));
            MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                ,_("Specify different palette or width/height in config file."));
            return -1;
        }
    }

    return 0;
}

/* Set the pixel format on the device */
int cls_v4l2cam::pixfmt_set(uint pixformat)
{
    int retcd;

    retcd = pixfmt_try(pixformat);
    if (retcd != 0) {
        return -1;
    }
    retcd = pixfmt_stride();
    if (retcd != 0) {
        return -1;
    }
    retcd = pixfmt_adjust();
    if (retcd != 0) {
        return -1;
    }
    retcd = xioctl(VIDIOC_S_FMT, &vidfmt);
    if (retcd == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error setting pixel format."));
        return -1;
    }

    pixfmt_src = (int)pixformat;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Using palette %c%c%c%c (%dx%d)")
        ,pixformat >> 0 , pixformat >> 8
        ,pixformat >> 16, pixformat >> 24
        ,cam->cfg->width, cam->cfg->height);

    return 0;
}

void cls_v4l2cam::params_check()
{
    int spec, indx;

    if (cam->cfg->width % 8) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("config image width (%d) is not modulo 8"), cam->cfg->width);
        cam->cfg->width = cam->cfg->width - (cam->cfg->width % 8) + 8;
        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            , _("Adjusting to width (%d)"), cam->cfg->width);
    }

    if (cam->cfg->height % 8) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("config image height (%d) is not modulo 8"), cam->cfg->height);
        cam->cfg->height = cam->cfg->height - (cam->cfg->height % 8) + 8;
        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Adjusting to height (%d)"), cam->cfg->height);
    }

    spec = 17;
    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "palette") {
            spec =  mtoi(params->params_array[indx].param_value);
            break;
        }
    }

    if ((spec < 0) || (spec > (int)palette.size())) {
        MOTPLS_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Invalid palette.  Changing to default"));
        util_parms_add(params,"palette","17");
    }

}

/*List camera palettes and return index of one that Motionplus supports*/
int cls_v4l2cam::pixfmt_list()
{
    int v4l2_pal, indx_palette, indx;
    struct v4l2_fmtdesc fmtd;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Supported palettes:"));

    v4l2_pal = 0;
    memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
    fmtd.index = (uint)v4l2_pal;
    fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    indx_palette = -1; /* -1 says not yet selected */

    while (xioctl(VIDIOC_ENUM_FMT, &fmtd) != -1) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            , "(%i) %c%c%c%c (%s)", v4l2_pal
            , fmtd.pixelformat >> 0, fmtd.pixelformat >> 8
            , fmtd.pixelformat >> 16, fmtd.pixelformat >> 24
            , fmtd.description);
        for (indx = 0; indx < (int)palette.size(); indx++) {
            if (palette[(uint)indx].v4l2id == fmtd.pixelformat) {
                indx_palette = indx;
            }
        }

        v4l2_pal++;
        memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
        fmtd.index = (uint)v4l2_pal;
        fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

    return indx_palette;
}

/* Find and select the pixel format for camera*/
void cls_v4l2cam::palette_set()
{
    int indxp, retcd, indx;

    if (fd_device == -1) {
        return;
    }

    params_check();

    indxp = 17;
    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "palette") {
            indxp =  mtoi(params->params_array[indx].param_value);
            break;
        }
    }

    retcd = pixfmt_set(palette[(uint)indxp].v4l2id);
    if (retcd == 0) {
        return;
    }

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Configuration palette index %d (%s) for %dx%d doesn't work.")
        , indxp, palette[(uint)indxp].fourcc.c_str()
        ,cam->cfg->width, cam->cfg->height);

    indxp = pixfmt_list();
    if (indxp < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Unable to find a compatible palette format."));
        device_close();
        return;
    }

    retcd = pixfmt_set(palette[(uint)indxp].v4l2id);
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            , _("Palette selection failed for format %s")
            , palette[(uint)indxp].fourcc.c_str());
        device_close();
        return;
    }

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Selected palette index %d (%s)")
        ,indxp, palette[(uint)indxp].fourcc.c_str());

}

/* Set the memory mapping from device to Motion*/
void cls_v4l2cam::set_mmap()
{
    enum v4l2_buf_type type;
    int buffer_index;

    if (fd_device == -1) {
        return;
    }

    if (!(vidcap.capabilities & V4L2_CAP_STREAMING)) {
        device_close();
        return;
    }

    memset(&vidreq, 0, sizeof(struct v4l2_requestbuffers));

    vidreq.count = MMAP_BUFFERS;
    vidreq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vidreq.memory = V4L2_MEMORY_MMAP;
    if (xioctl(VIDIOC_REQBUFS, &vidreq) == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error requesting buffers %d for memory map. VIDIOC_REQBUFS")
            ,vidreq.count);
        device_close();
        return;
    }
    buffer_count = (int)vidreq.count;

    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
        ,_("mmap information: frames=%d"), buffer_count);

    if (buffer_count < MIN_MMAP_BUFFERS) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Insufficient buffer memory %d < MIN_MMAP_BUFFERS.")
            ,buffer_count);
        device_close();
        return;
    }

    buffers =(video_buff*) calloc((uint)buffer_count, sizeof(video_buff));
    if (buffers == nullptr) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, _("Out of memory."));
        device_close();
        return;
    }

    for (buffer_index = 0; buffer_index < buffer_count; buffer_index++) {
        struct v4l2_buffer p_buf;

        memset(&p_buf, 0, sizeof(struct v4l2_buffer));

        p_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        p_buf.memory = V4L2_MEMORY_MMAP;
        p_buf.index = (uint)buffer_index;
        if (xioctl(VIDIOC_QUERYBUF, &p_buf) == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error querying buffer %i\nVIDIOC_QUERYBUF: ")
                ,buffer_index);
            myfree(buffers);
            device_close();
            return;
        }

        buffers[buffer_index].size = p_buf.length;
        buffers[buffer_index].ptr =(unsigned char*) mmap(
            nullptr, p_buf.length, PROT_READ | PROT_WRITE
            , MAP_SHARED, fd_device, p_buf.m.offset);

        if (buffers[buffer_index].ptr == MAP_FAILED) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error mapping buffer %i mmap"), buffer_index);
            myfree(buffers);
            device_close();
            return;
        }

        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("%i length=%d Address (%x)")
            ,buffer_index, p_buf.length, buffers[buffer_index].ptr);
    }

    for (buffer_index = 0; buffer_index < buffer_count; buffer_index++) {
        memset(&vidbuf, 0, sizeof(struct v4l2_buffer));

        vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vidbuf.memory = V4L2_MEMORY_MMAP;
        vidbuf.index = (uint)buffer_index;

        if (xioctl(VIDIOC_QBUF, &vidbuf) == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_QBUF");
            device_close();
            return;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(VIDIOC_STREAMON, &type) == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error starting stream. VIDIOC_STREAMON"));
        device_close();
        return;
    }

}

/* Assign the resulting sizes to the camera context items */
void cls_v4l2cam::set_imgs()
{
    if (fd_device == -1) {
        return;
    }
    cam->imgs.width = cam->cfg->width;
    cam->imgs.height = cam->cfg->height;
    cam->imgs.motionsize = cam->imgs.width * cam->imgs.height;
    cam->imgs.size_norm = (cam->imgs.motionsize * 3) / 2;

    convert = new cls_convert(cam, pixfmt_src, cam->cfg->width, cam->cfg->height);

}

/* Capture the image into the buffer */
int cls_v4l2cam::capture()
{
    int retcd;
    sigset_t set, old;

    /* Block signals during IOCTL */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);

    pthread_sigmask(SIG_BLOCK, &set, &old);

    if (pframe >= 0) {
        retcd = xioctl(VIDIOC_QBUF, &vidbuf);
        if (retcd == -1) {
            MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_QBUF");
            pthread_sigmask(SIG_UNBLOCK, &old, nullptr);
            return -1;
        }
    }

    memset(&vidbuf, 0, sizeof(struct v4l2_buffer));

    vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vidbuf.memory = V4L2_MEMORY_MMAP;
    vidbuf.bytesused = 0;

    retcd = xioctl(VIDIOC_DQBUF, &vidbuf);
    if (retcd == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_DQBUF");
        pthread_sigmask(SIG_UNBLOCK, &old, nullptr);
        return -1;
    }

    pframe = (int)vidbuf.index;
    buffers[vidbuf.index].used = vidbuf.bytesused;
    buffers[vidbuf.index].content_length = (int)vidbuf.bytesused;
    pthread_sigmask(SIG_UNBLOCK, &old, nullptr);    /*undo the signal blocking */

    return 0;

}

void cls_v4l2cam::init_vars()
{
    buffer_count= 0;
    pframe = -1;
    buffers = nullptr;
    convert = nullptr;

    params = new ctx_params;
    params->params_cnt = 0;
    util_parms_parse(params, "v4l2_params", cam->cfg->v4l2_params);
    util_parms_add_default(params, "input", "-1");
    util_parms_add_default(params, "palette", "17");
    util_parms_add_default(params, "norm", "0");
    util_parms_add_default(params, "frequency", "0");

    palette_init();
}

/* Open the device */
void cls_v4l2cam::device_open()
{
    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        , _("Opening video device %s")
        , cam->cfg->v4l2_device.c_str());

    cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbritrary multiplier to give open function more time*/
    fd_device = open(cam->cfg->v4l2_device.c_str(), O_RDWR|O_CLOEXEC);
    if (fd_device <= 0) {
        MOTPLS_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO
            , _("Failed to open video device %s")
            , cam->cfg->v4l2_device.c_str());
        fd_device = -1;
        return;
    }

    if (xioctl(VIDIOC_QUERYCAP, &vidcap) < 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("Not a V4L2 device?"));
        device_close();
        return;
    }

    if (!(vidcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("Device does not support capturing."));
        device_close();
        return;
    }
}

void cls_v4l2cam::log_types()
{
    if (fd_device == -1) {
        return;
    }

    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "------------------------");
    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.driver: \"%s\"",vidcap.driver);
    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.card: \"%s\"",vidcap.card);
    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.bus_info: \"%s\"",vidcap.bus_info);
    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.capabilities=0x%08X",vidcap.capabilities);
    MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "------------------------");

    if (vidcap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_CAPTURE");
    }
    if (vidcap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_OUTPUT");
    }
    if (vidcap.capabilities & V4L2_CAP_VIDEO_OVERLAY) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_OVERLAY");
    }
    if (vidcap.capabilities & V4L2_CAP_VBI_CAPTURE) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VBI_CAPTURE");
    }
    if (vidcap.capabilities & V4L2_CAP_VBI_OUTPUT) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VBI_OUTPUT");
    }
    if (vidcap.capabilities & V4L2_CAP_RDS_CAPTURE) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- RDS_CAPTURE");
    }
    if (vidcap.capabilities & V4L2_CAP_TUNER) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- TUNER");
    }
    if (vidcap.capabilities & V4L2_CAP_AUDIO) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- AUDIO");
    }
    if (vidcap.capabilities & V4L2_CAP_READWRITE) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- READWRITE");
    }
    if (vidcap.capabilities & V4L2_CAP_ASYNCIO) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- ASYNCIO");
    }
    if (vidcap.capabilities & V4L2_CAP_STREAMING) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- STREAMING");
    }
    if (vidcap.capabilities & V4L2_CAP_TIMEPERFRAME) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- TIMEPERFRAME");
    }

}

void cls_v4l2cam::log_formats()
{
    struct v4l2_fmtdesc         dev_format;
    struct v4l2_frmsizeenum     dev_sizes;
    struct v4l2_frmivalenum     dev_frameint;
    int indx_format, indx_sizes, indx_frameint;

    if (fd_device == -1) {
        return;
    }

    memset(&dev_format, 0, sizeof(struct v4l2_fmtdesc));
    dev_format.index = indx_format = 0;
    dev_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (xioctl(VIDIOC_ENUM_FMT, &dev_format) != -1) {
        MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("Supported palette %s (%c%c%c%c)")
            ,dev_format.description
            ,dev_format.pixelformat >> 0
            ,dev_format.pixelformat >> 8
            ,dev_format.pixelformat >> 16
            ,dev_format.pixelformat >> 24);

        memset(&dev_sizes, 0, sizeof(struct v4l2_frmsizeenum));
        dev_sizes.index = indx_sizes = 0;
        dev_sizes.pixel_format = dev_format.pixelformat;
        while (xioctl(VIDIOC_ENUM_FRAMESIZES, &dev_sizes) != -1) {
            MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                ,_("  Width: %d, Height %d")
                ,dev_sizes.discrete.width
                ,dev_sizes.discrete.height);

            memset(&dev_frameint, 0, sizeof(struct v4l2_frmivalenum));
            dev_frameint.index = indx_frameint = 0;
            dev_frameint.pixel_format = dev_format.pixelformat;
            dev_frameint.width = dev_sizes.discrete.width;
            dev_frameint.height = dev_sizes.discrete.height;
            while (xioctl(VIDIOC_ENUM_FRAMEINTERVALS, &dev_frameint) != -1) {
                MOTPLS_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                    ,_("    Framerate %d/%d")
                    ,dev_frameint.discrete.numerator
                    ,dev_frameint.discrete.denominator);
                memset(&dev_frameint, 0, sizeof(struct v4l2_frmivalenum));
                dev_frameint.index = (uint)(++indx_frameint);
                dev_frameint.pixel_format = dev_format.pixelformat;
                dev_frameint.width = dev_sizes.discrete.width;
                dev_frameint.height = dev_sizes.discrete.height;
            }
            memset(&dev_sizes, 0, sizeof(struct v4l2_frmsizeenum));
            dev_sizes.index = (uint)(++indx_sizes);
            dev_sizes.pixel_format = dev_format.pixelformat;
        }
        memset(&dev_format, 0, sizeof(struct v4l2_fmtdesc));
        dev_format.index = (uint)(++indx_format);
        dev_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

}

void cls_v4l2cam::set_fps()
{
    int retcd;

    struct v4l2_streamparm setfps;

    if (fd_device == -1) {
        return;
    }

    memset(&setfps, 0, sizeof(struct v4l2_streamparm));

    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = (uint)cam->cfg->framerate;

    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
        , _("Trying to set fps to %d")
        , setfps.parm.capture.timeperframe.denominator);

    retcd = xioctl(VIDIOC_S_PARM, &setfps);
    if (retcd != 0) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Error setting fps. Return code %d"), retcd);
    }

    MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO
        , _("Device set fps to %d")
        , setfps.parm.capture.timeperframe.denominator);

}

void cls_v4l2cam::stop_cam()
{
    enum v4l2_buf_type p_type;
    int indx;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        ,_("Closing video device %s"), cam->cfg->v4l2_device.c_str());

    p_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (fd_device != -1) {
        xioctl(VIDIOC_STREAMOFF, &p_type);
        device_close();
    }

    if (buffers != nullptr) {
        for (indx = 0; indx < (int)vidreq.count; indx++){
            munmap(buffers[indx].ptr, buffers[indx].size);
        }
        myfree(buffers);
    }

    if (convert != nullptr) {
        mydelete(convert);
    }

    mydelete(params);
}

void cls_v4l2cam::start_cam()
{
    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening V4L2 device"));
    init_vars();
    device_open();
    log_types();
    log_formats();
    set_input();
    set_norm();
    set_frequency();
    palette_set();
    set_fps();
    ctrls_list();
    ctrls_set();
    set_mmap();
    set_imgs();
    if (fd_device == -1) {
        MOTPLS_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("V4L2 device failed to open"));
        stop_cam();
        return;
    }
    cam->device_status = STATUS_OPENED;

}

#endif /* HAVE_V4L2 */

void cls_v4l2cam::noimage()
{
    #ifdef HAVE_V4L2
        int slp_dur;

        if (reconnect_count < 100) {
            reconnect_count++;
        } else {
            if (reconnect_count >= 500) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 2 hours."));
                slp_dur = 7200;
            } else if (reconnect_count >= 200) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 10 minutes."));
                reconnect_count++;
                slp_dur = 600;
            } else {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 30 seconds."));
                reconnect_count++;
                slp_dur = 30;
            }
            cam->watchdog = slp_dur + (cam->cfg->watchdog_tmo * 3);
            SLEEP(slp_dur,0);
            stop_cam();
            start_cam();
        }
    #endif
}

int cls_v4l2cam::next(ctx_image_data *img_data)
{
    #ifdef HAVE_V4L2
        int retcd;

        cam->watchdog = cam->cfg->watchdog_tmo;
        retcd = capture();
        if (retcd != 0) {
            return CAPTURE_FAILURE;
        }

        retcd = convert->process(
            img_data->image_norm
            , buffers[vidbuf.index].ptr
            , buffers[vidbuf.index].content_length);
        if (retcd != 0) {
            return CAPTURE_FAILURE;
        }

        cam->rotate->process(img_data);

        return CAPTURE_SUCCESS;
    #else
        (void)img_data;
        return CAPTURE_FAILURE;
    #endif // HAVE_V4L2
}

cls_v4l2cam::cls_v4l2cam(cls_camera *p_cam)
{
    cam = p_cam;
    #ifdef HAVE_V4L2
        cam->watchdog = cam->cfg->watchdog_tmo * 3;
        start_cam();
    #else
        cam->device_status = STATUS_CLOSED;
    #endif // HAVE_V4l2
}

cls_v4l2cam::~cls_v4l2cam()
{
    #ifdef HAVE_V4L2
        stop_cam();
    #endif
    cam->device_status = STATUS_CLOSED;
}
