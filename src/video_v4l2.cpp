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
 *    Copyright 2020 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "video_common.hpp"
#include "video_v4l2.hpp"
#include <sys/mman.h>


#define MMAP_BUFFERS            4
#define MIN_MMAP_BUFFERS        2
#define V4L2_PALETTE_COUNT_MAX 21

#ifdef HAVE_V4L2

static void v4l2_palette_init(palette_item *palette_array)
{
    int indx;

    /* When adding here, update the max defined as V4L2_PALETTE_COUNT_MAX above */
    palette_array[0].v4l2id = V4L2_PIX_FMT_SN9C10X;
    palette_array[1].v4l2id = V4L2_PIX_FMT_SBGGR16;
    palette_array[2].v4l2id = V4L2_PIX_FMT_SBGGR8;
    palette_array[3].v4l2id = V4L2_PIX_FMT_SPCA561;
    palette_array[4].v4l2id = V4L2_PIX_FMT_SGBRG8;
    palette_array[5].v4l2id = V4L2_PIX_FMT_SGRBG8;
    palette_array[6].v4l2id = V4L2_PIX_FMT_PAC207;
    palette_array[7].v4l2id = V4L2_PIX_FMT_PJPG;
    palette_array[8].v4l2id = V4L2_PIX_FMT_MJPEG;
    palette_array[9].v4l2id = V4L2_PIX_FMT_JPEG;
    palette_array[10].v4l2id = V4L2_PIX_FMT_RGB24;
    palette_array[11].v4l2id = V4L2_PIX_FMT_SPCA501;
    palette_array[12].v4l2id = V4L2_PIX_FMT_SPCA505;
    palette_array[13].v4l2id = V4L2_PIX_FMT_SPCA508;
    palette_array[14].v4l2id = V4L2_PIX_FMT_UYVY;
    palette_array[15].v4l2id = V4L2_PIX_FMT_YUYV;
    palette_array[16].v4l2id = V4L2_PIX_FMT_YUV422P;
    palette_array[17].v4l2id = V4L2_PIX_FMT_YUV420; /* most efficient for motion */
    palette_array[18].v4l2id = V4L2_PIX_FMT_Y10;
    palette_array[19].v4l2id = V4L2_PIX_FMT_Y12;
    palette_array[20].v4l2id = V4L2_PIX_FMT_GREY;
    palette_array[21].v4l2id = V4L2_PIX_FMT_H264;

    for (indx = 0; indx <= V4L2_PALETTE_COUNT_MAX; indx++) {
        sprintf(palette_array[indx].fourcc ,"%c%c%c%c"
                ,palette_array[indx].v4l2id >> 0
                ,palette_array[indx].v4l2id >> 8
                ,palette_array[indx].v4l2id >> 16
                ,palette_array[indx].v4l2id >> 24);
    }

}

/* Execute the request to the device */
static int xioctl(ctx_v4l2cam *v4l2cam, unsigned long request, void *arg)
{
    int retcd;

    if (v4l2cam->fd_device < 0) {
        return -1;
    }

    do {
        retcd = ioctl(v4l2cam->fd_device, request, arg);
    } while (-1 == retcd && EINTR == errno && !v4l2cam->finish);

    return retcd;
}

/* Get the count of how many controls and menu items the device supports */
static int v4l2_ctrls_count(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_queryctrl       vid_ctrl;
    struct v4l2_querymenu       vid_menu;
    int indx;

    v4l2cam->devctrl_count = 0;

    memset(&vid_ctrl, 0, sizeof(struct v4l2_queryctrl));
    vid_ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (xioctl (v4l2cam, VIDIOC_QUERYCTRL, &vid_ctrl) == 0) {
        if (vid_ctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
            vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }
        v4l2cam->devctrl_count++;
        if (vid_ctrl.type == V4L2_CTRL_TYPE_MENU) {
            for (indx = vid_ctrl.minimum; indx <= vid_ctrl.maximum; indx++) {
                memset(&vid_menu, 0, sizeof(struct v4l2_querymenu));
                vid_menu.id = vid_ctrl.id;
                vid_menu.index = indx;
                if (xioctl(v4l2cam, VIDIOC_QUERYMENU, &vid_menu) == 0) {
                    v4l2cam->devctrl_count++;
                }
            }
        }
        vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    return 0;
}

/* Print the device controls to the log */
static void v4l2_ctrls_log(ctx_v4l2cam *v4l2cam)
{
    int indx;

    if (v4l2cam->devctrl_count != 0 ) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("---------Controls---------"));
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("  V4L2 ID   Name and Range"));
        for (indx = 0; indx < v4l2cam->devctrl_count; indx++) {
            if (v4l2cam->devctrl_array[indx].ctrl_menuitem) {
                MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "  %s %s"
                    ,v4l2cam->devctrl_array[indx].ctrl_iddesc
                    ,v4l2cam->devctrl_array[indx].ctrl_name);
            } else {
                MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s %s, %d to %d"
                    ,v4l2cam->devctrl_array[indx].ctrl_iddesc
                    ,v4l2cam->devctrl_array[indx].ctrl_name
                    ,v4l2cam->devctrl_array[indx].ctrl_minimum
                    ,v4l2cam->devctrl_array[indx].ctrl_maximum);
            }
        }
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "--------------------------");
    }

}

/* Get names of the controls and menu items the device supports */
static void v4l2_ctrls_list(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_queryctrl       vid_ctrl;
    struct v4l2_querymenu       vid_menu;
    int indx, indx_ctrl;

    v4l2cam->devctrl_array = NULL;
    if (v4l2cam->devctrl_count == 0) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, _("No Controls found for device"));
        return;
    }

    v4l2cam->devctrl_array =(ctx_v4l2cam_ctrl*) malloc(v4l2cam->devctrl_count * sizeof(ctx_v4l2cam_ctrl));

    memset(&vid_ctrl, 0, sizeof(struct v4l2_queryctrl));
    vid_ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    indx_ctrl = 0;
    while (xioctl (v4l2cam, VIDIOC_QUERYCTRL, &vid_ctrl) == 0) {
        if (vid_ctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
            vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        v4l2cam->devctrl_array[indx_ctrl].ctrl_id = vid_ctrl.id;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_type = vid_ctrl.type;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_default = vid_ctrl.default_value;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_currval = vid_ctrl.default_value;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_newval = vid_ctrl.default_value;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_menuitem = FALSE;

        v4l2cam->devctrl_array[indx_ctrl].ctrl_name =(char*) malloc(32);
        sprintf(v4l2cam->devctrl_array[indx_ctrl].ctrl_name,"%s",vid_ctrl.name);

        v4l2cam->devctrl_array[indx_ctrl].ctrl_iddesc =(char*) malloc(15);
        sprintf(v4l2cam->devctrl_array[indx_ctrl].ctrl_iddesc,"ID%08d",vid_ctrl.id);

        v4l2cam->devctrl_array[indx_ctrl].ctrl_minimum = vid_ctrl.minimum;
        v4l2cam->devctrl_array[indx_ctrl].ctrl_maximum = vid_ctrl.maximum;

        if (vid_ctrl.type == V4L2_CTRL_TYPE_MENU) {
            for (indx = vid_ctrl.minimum; indx <= vid_ctrl.maximum; indx++) {
                memset(&vid_menu, 0, sizeof(struct v4l2_querymenu));
                vid_menu.id = vid_ctrl.id;
                vid_menu.index = indx;

                if (xioctl(v4l2cam, VIDIOC_QUERYMENU, &vid_menu) == 0) {
                    indx_ctrl++;
                    v4l2cam->devctrl_array[indx_ctrl].ctrl_id = vid_ctrl.id;
                    v4l2cam->devctrl_array[indx_ctrl].ctrl_type = 0;
                    v4l2cam->devctrl_array[indx_ctrl].ctrl_menuitem = TRUE;

                    v4l2cam->devctrl_array[indx_ctrl].ctrl_name =(char*) malloc(32);
                    sprintf(v4l2cam->devctrl_array[indx_ctrl].ctrl_name,"%s",vid_menu.name);

                    v4l2cam->devctrl_array[indx_ctrl].ctrl_iddesc =(char*) malloc(40);
                    sprintf(v4l2cam->devctrl_array[indx_ctrl].ctrl_iddesc,"menu item: Value %d",indx);

                    v4l2cam->devctrl_array[indx_ctrl].ctrl_minimum = 0;
                    v4l2cam->devctrl_array[indx_ctrl].ctrl_maximum = 0;
                }
           }
        }
        indx_ctrl++;
        vid_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    v4l2_ctrls_log(v4l2cam);

    return;

}

/* Set the control array items to the device */
static void v4l2_ctrls_set(ctx_v4l2cam *v4l2cam)
{
    struct ctx_v4l2cam_ctrl *devitem;
    struct v4l2_control     vid_ctrl;
    int indx_dev, retcd;

    for (indx_dev = 0; indx_dev < v4l2cam->devctrl_count; indx_dev++) {
        devitem=&v4l2cam->devctrl_array[indx_dev];
        if (!devitem->ctrl_menuitem) {
            if (devitem->ctrl_currval != devitem->ctrl_newval) {
                memset(&vid_ctrl, 0, sizeof (struct v4l2_control));
                vid_ctrl.id = devitem->ctrl_id;
                vid_ctrl.value = devitem->ctrl_newval;
                retcd = xioctl(v4l2cam, VIDIOC_S_CTRL, &vid_ctrl);
                if (retcd < 0) {
                    MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO
                        ,_("setting control %s \"%s\" to %d failed with return code %d")
                        ,devitem->ctrl_iddesc, devitem->ctrl_name
                        ,devitem->ctrl_newval,retcd);
                } else {
                    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
                        ,_("Set control \"%s\" to value %d")
                        ,devitem->ctrl_name, devitem->ctrl_newval);
                   devitem->ctrl_currval = devitem->ctrl_newval;
                }
            }
        }
     }

    return;
}


static int v4l2_parms_set(ctx_v4l2cam *v4l2cam)
{
    struct ctx_v4l2cam_ctrl  *devitem;
    struct ctx_params_item *usritem;
    int indx_dev, indx_user;

    if (v4l2cam->devctrl_count == 0) {
        v4l2cam->params->update_params = FALSE;
        return 0;
    }

    for (indx_dev = 0; indx_dev < v4l2cam->devctrl_count; indx_dev++) {

        devitem=&v4l2cam->devctrl_array[indx_dev];
        devitem->ctrl_newval = devitem->ctrl_default;

        for (indx_user = 0; indx_user < v4l2cam->params->params_count; indx_user++){

            usritem=&v4l2cam->params->params_array[indx_user];

            if ((mystrceq(devitem->ctrl_iddesc,usritem->param_name)) ||
                (mystrceq(devitem->ctrl_name  ,usritem->param_name))) {
                switch (devitem->ctrl_type) {
                case V4L2_CTRL_TYPE_MENU:
                    /*FALLTHROUGH*/
                case V4L2_CTRL_TYPE_INTEGER:
                    if (atoi(usritem->param_value) < devitem->ctrl_minimum) {
                        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                            ,_("%s control option value %s is below minimum.  Skipping...")
                            ,devitem->ctrl_name, usritem->param_value, devitem->ctrl_minimum);
                    } else if (atoi(usritem->param_value) > devitem->ctrl_maximum) {
                        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                            ,_("%s control option value %s is above maximum.  Skipping...")
                            ,devitem->ctrl_name, usritem->param_value, devitem->ctrl_maximum);
                    } else {
                        devitem->ctrl_newval = atoi(usritem->param_value);
                    }
                    break;
                case V4L2_CTRL_TYPE_BOOLEAN:
                    devitem->ctrl_newval = usritem->param_value ? 1 : 0;
                    break;
                default:
                    MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                        ,_("control type not supported yet"));
                }
            }
        }
    }

    return 0;

}

/* Set the device to the input number requested by user */
static int v4l2_set_input(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_input    input;

    memset(&input, 0, sizeof (struct v4l2_input));
    if (v4l2cam->input == -1) {
        input.index = 0;
    } else {
        input.index = v4l2cam->input;
    }

    if (xioctl(v4l2cam, VIDIOC_ENUMINPUT, &input) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Unable to query input %d."
            " VIDIOC_ENUMINPUT, if you use a WEBCAM change input value in conf by -1")
            ,input.index);
        return -1;
    }

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
        ,_("Name = \"%s\", type 0x%08X, status %08x")
        ,input.name, input.type, input.status);

    if (input.type & V4L2_INPUT_TYPE_TUNER) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Name = \"%s\",- TUNER"), input.name);
    }

    if (input.type & V4L2_INPUT_TYPE_CAMERA) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Name = \"%s\"- CAMERA"), input.name);
    }

    if (xioctl(v4l2cam, VIDIOC_S_INPUT, &input.index) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            , _("Error selecting input %d VIDIOC_S_INPUT"), input.index);
        return -1;
    }

    v4l2cam->device_type  = input.type;
    v4l2cam->device_tuner = input.tuner;

    return 0;
}

/* Set the video standard(norm) for the device to the user requested value*/
static void v4l2_set_norm(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_standard standard;
    v4l2_std_id std_id;

    if (xioctl(v4l2cam, VIDIOC_G_STD, &std_id) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Device does not support specifying PAL/NTSC norm"));
        v4l2cam->norm = std_id = 0;    // V4L2_STD_UNKNOWN = 0
    }

    if (std_id) {
        memset(&standard, 0, sizeof(struct v4l2_standard));
        standard.index = 0;

        while (xioctl(v4l2cam, VIDIOC_ENUMSTD, &standard) == 0) {
            if (standard.id & std_id) {
                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                    ,_("- video standard %s"), standard.name);
            }
            standard.index++;
        }

        switch (v4l2cam->norm) {
        case 1:
            std_id = V4L2_STD_NTSC;
            break;
        case 2:
            std_id = V4L2_STD_SECAM;
            break;
        default:
            std_id = V4L2_STD_PAL;
        }

        if (xioctl(v4l2cam, VIDIOC_S_STD, &std_id) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error selecting standard method %d VIDIOC_S_STD")
                ,(int)std_id);
        }

        if (std_id == V4L2_STD_NTSC) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to NTSC"));
        } else if (std_id == V4L2_STD_SECAM) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to SECAM"));
        } else {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Video standard set to PAL"));
        }
    }

     return;
}

/* Set the frequency on the device to the user requested value */
static void v4l2_set_frequency(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_tuner     tuner;
    struct v4l2_frequency freq;

    /* If this input is attached to a tuner, set the frequency. */
    if (v4l2cam->device_type & V4L2_INPUT_TYPE_TUNER) {
        /* Query the tuners capabilities. */
        memset(&tuner, 0, sizeof(struct v4l2_tuner));
        tuner.index = v4l2cam->device_tuner;

        if (xioctl(v4l2cam, VIDIOC_G_TUNER, &tuner) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("tuner %d VIDIOC_G_TUNER"), tuner.index);
            return;
        }

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Set tuner %d"), tuner.index);

        /* Set the frequency. */
        memset(&freq, 0, sizeof(struct v4l2_frequency));
        freq.tuner = v4l2cam->device_tuner;
        freq.type = V4L2_TUNER_ANALOG_TV;
        freq.frequency = (v4l2cam->frequency / 1000) * 16;

        if (xioctl(v4l2cam, VIDIOC_S_FREQUENCY, &freq) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("freq %ul VIDIOC_S_FREQUENCY"), freq.frequency);
            return;
        }

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Set Frequency to %ul"), freq.frequency);
    }

    return;
}

/* Set the pixel format on the device */
static int v4l2_set_pixfmt(ctx_v4l2cam *v4l2cam, unsigned int pixformat)
{

    memset(&v4l2cam->dst_fmt, 0, sizeof(struct v4l2_format));

    v4l2cam->dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2cam->dst_fmt.fmt.pix.width = v4l2cam->width;
    v4l2cam->dst_fmt.fmt.pix.height = v4l2cam->height;
    v4l2cam->dst_fmt.fmt.pix.pixelformat = pixformat;
    v4l2cam->dst_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(v4l2cam, VIDIOC_TRY_FMT, &v4l2cam->dst_fmt) != -1 &&
        v4l2cam->dst_fmt.fmt.pix.pixelformat == pixformat) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Testing palette %c%c%c%c (%dx%d)")
            ,pixformat >> 0, pixformat >> 8
            ,pixformat >> 16, pixformat >> 24
            ,v4l2cam->width, v4l2cam->height);

        if (v4l2cam->dst_fmt.fmt.pix.width != (unsigned int) v4l2cam->width ||
            v4l2cam->dst_fmt.fmt.pix.height != (unsigned int) v4l2cam->height) {

            MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                ,_("Adjusting resolution from %ix%i to %ix%i.")
                ,v4l2cam->width, v4l2cam->height
                ,v4l2cam->dst_fmt.fmt.pix.width
                ,v4l2cam->dst_fmt.fmt.pix.height);

            v4l2cam->width = v4l2cam->dst_fmt.fmt.pix.width;
            v4l2cam->height = v4l2cam->dst_fmt.fmt.pix.height;

            if ((v4l2cam->width % 8) || (v4l2cam->height % 8)) {
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                    ,_("Adjusted resolution not modulo 8."));
                MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
                    ,_("Specify different palette or width/height in config file."));
                return -1;
            }

        }

        if (xioctl(v4l2cam, VIDIOC_S_FMT, &v4l2cam->dst_fmt) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error setting pixel format.\nVIDIOC_S_FMT: "));
            return -1;
        }

        v4l2cam->pixfmt_src = pixformat;

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Using palette %c%c%c%c (%dx%d)")
            ,pixformat >> 0 , pixformat >> 8
            ,pixformat >> 16, pixformat >> 24
            ,v4l2cam->width, v4l2cam->height);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("Bytesperlines %d sizeimage %d colorspace %08x")
            ,v4l2cam->dst_fmt.fmt.pix.bytesperline
            ,v4l2cam->dst_fmt.fmt.pix.sizeimage
            ,v4l2cam->dst_fmt.fmt.pix.colorspace);

        return 0;
    }

    return -1;
}

/* If needed adjust sizes to be modulo 8 */
static void v4l2_validate_sizes(ctx_v4l2cam *v4l2cam)
{
    if (v4l2cam->width % 8) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("config image width (%d) is not modulo 8"), v4l2cam->width);
        v4l2cam->width = v4l2cam->width - (v4l2cam->width % 8) + 8;
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            , _("Adjusting to width (%d)"), v4l2cam->width);
    }

    if (v4l2cam->height % 8) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("config image height (%d) is not modulo 8"), v4l2cam->height);
        v4l2cam->height = v4l2cam->height - (v4l2cam->height % 8) + 8;
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Adjusting to height (%d)"), v4l2cam->height);
    }

}

/* Find and select the pixel format for camera*/
static int v4l2_set_palette(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_fmtdesc fmtd;
    int v4l2_pal, indx_palette, indx, retcd;
    palette_item *palette_array;

    palette_array =(palette_item*) malloc(sizeof(palette_item) * (V4L2_PALETTE_COUNT_MAX+1));
    v4l2_palette_init(palette_array);

    v4l2_validate_sizes(v4l2cam);

    if (v4l2cam->palette == 21) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("H264(21) format not supported via v4l2_device.  Changing to default palette"));
        v4l2cam->palette = 17;
    }

    /* First we try setting the config file value */
    indx_palette = v4l2cam->palette;
    if ((indx_palette >= 0) && (indx_palette <= V4L2_PALETTE_COUNT_MAX)) {
        retcd = v4l2_set_pixfmt(v4l2cam, palette_array[indx_palette].v4l2id);
        if (retcd >= 0) {
            free(palette_array);
            return 0;
        }
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Configuration palette index %d (%s) for %dx%d doesn't work.")
            , indx_palette, palette_array[indx_palette].fourcc
            ,v4l2cam->width, v4l2cam->height);
    }

    memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
    fmtd.index = v4l2_pal = 0;
    fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    indx_palette = -1; /* -1 says not yet selected */
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, _("Supported palettes:"));

    while (xioctl(v4l2cam, VIDIOC_ENUM_FMT, &fmtd) != -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "(%i) %c%c%c%c (%s)",
            v4l2_pal, fmtd.pixelformat >> 0,
            fmtd.pixelformat >> 8, fmtd.pixelformat >> 16,
            fmtd.pixelformat >> 24, fmtd.description);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("%d - %s (compressed : %d) (%#x)")
            ,fmtd.index, fmtd.description, fmtd.flags, fmtd.pixelformat);

        /* Adjust indx_palette if larger value found */
        /* Prevent the selection of H264 since this module does not support it */
        for (indx = 0; indx <= V4L2_PALETTE_COUNT_MAX; indx++) {
            if ((palette_array[indx].v4l2id == fmtd.pixelformat) &&
                (palette_array[indx].v4l2id != V4L2_PIX_FMT_H264)) {
                indx_palette = indx;
            }
        }

        memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
        fmtd.index = ++v4l2_pal;
        fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

    if (indx_palette >= 0) {
        retcd = v4l2_set_pixfmt(v4l2cam, palette_array[indx_palette].v4l2id);
        if (retcd >= 0) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Selected palette %s")
                ,palette_array[indx_palette].fourcc);
            free(palette_array);
            return 0;
        }
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Palette selection failed for format %s")
            , palette_array[indx_palette].fourcc);
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
        ,_("Unable to find a compatible palette format."));

    free(palette_array);

    return -1;

}

/* Set the memory mapping from device to Motion*/
static int v4l2_set_mmap(ctx_v4l2cam *v4l2cam)
{
    enum v4l2_buf_type type;
    int buffer_index;

    /* Does the device support streaming? */
    if (!(v4l2cam->cap.capabilities & V4L2_CAP_STREAMING)) {
        return -1;
    }

    memset(&v4l2cam->req, 0, sizeof(struct v4l2_requestbuffers));

    v4l2cam->req.count = MMAP_BUFFERS;
    v4l2cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2cam->req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(v4l2cam, VIDIOC_REQBUFS, &v4l2cam->req) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error requesting buffers %d for memory map. VIDIOC_REQBUFS")
            ,v4l2cam->req.count);
        return -1;
    }
    v4l2cam->buffer_count = v4l2cam->req.count;

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
        ,_("mmap information: frames=%d"), v4l2cam->buffer_count);

    if (v4l2cam->buffer_count < MIN_MMAP_BUFFERS) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Insufficient buffer memory %d < MIN_MMAP_BUFFERS.")
            ,v4l2cam->buffer_count);
        return -1;
    }

    v4l2cam->buffers =(video_buff*) calloc(v4l2cam->buffer_count, sizeof(video_buff));
    if (!v4l2cam->buffers) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, _("Out of memory."));
        v4l2cam->buffers = NULL;
        return -1;
    }

    for (buffer_index = 0; buffer_index < v4l2cam->buffer_count; buffer_index++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(struct v4l2_buffer));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer_index;
        if (xioctl(v4l2cam, VIDIOC_QUERYBUF, &buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error querying buffer %i\nVIDIOC_QUERYBUF: ")
                ,buffer_index);
            free(v4l2cam->buffers);
            v4l2cam->buffers = NULL;
            return -1;
        }

        v4l2cam->buffers[buffer_index].size = buf.length;
        v4l2cam->buffers[buffer_index].ptr =(unsigned char*) mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                     MAP_SHARED, v4l2cam->fd_device, buf.m.offset);

        if (v4l2cam->buffers[buffer_index].ptr == MAP_FAILED) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("Error mapping buffer %i mmap"), buffer_index);
            free(v4l2cam->buffers);
            v4l2cam->buffers = NULL;
            return -1;
        }

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            ,_("%i length=%d Address (%x)")
            ,buffer_index, buf.length, v4l2cam->buffers[buffer_index].ptr);
    }

    for (buffer_index = 0; buffer_index < v4l2cam->buffer_count; buffer_index++) {
        memset(&v4l2cam->buf, 0, sizeof(struct v4l2_buffer));

        v4l2cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2cam->buf.memory = V4L2_MEMORY_MMAP;
        v4l2cam->buf.index = buffer_index;

        if (xioctl(v4l2cam, VIDIOC_QBUF, &v4l2cam->buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_QBUF");
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(v4l2cam, VIDIOC_STREAMON, &type) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error starting stream. VIDIOC_STREAMON"));
        return -1;
    }

    return 0;
}

/* Assign the resulting sizes to the camera context items */
static void v4l2_set_imgs(ctx_cam *cam)
{
    cam->imgs.width = cam->v4l2cam->width;
    cam->imgs.height = cam->v4l2cam->height;
    cam->imgs.motionsize = cam->imgs.width * cam->imgs.height;
    cam->imgs.size_norm = (cam->imgs.motionsize * 3) / 2;
    cam->conf->width = cam->v4l2cam->width;
    cam->conf->height = cam->v4l2cam->height;

}

/* Capture the image into the buffer */
static int v4l2_capture_buffer(ctx_v4l2cam *v4l2cam)
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

    if (v4l2cam->pframe >= 0) {
        if (xioctl(v4l2cam, VIDIOC_QBUF, &v4l2cam->buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_QBUF");
            pthread_sigmask(SIG_UNBLOCK, &old, NULL);
            return -1;
        }
    }

    memset(&v4l2cam->buf, 0, sizeof(struct v4l2_buffer));

    v4l2cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2cam->buf.memory = V4L2_MEMORY_MMAP;
    v4l2cam->buf.bytesused = 0;

    if (xioctl(v4l2cam, VIDIOC_DQBUF, &v4l2cam->buf) == -1) {
        /*
         * Some drivers return EIO when there is no signal,
         * driver might dequeue an (empty) buffer despite
         * returning an error, or even stop capturing.
         */
        if (errno == EIO) {
            v4l2cam->pframe++;

            if (v4l2cam->pframe >= (int)v4l2cam->req.count) {
                v4l2cam->pframe = 0;
            }

            v4l2cam->buf.index = v4l2cam->pframe;
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,"VIDIOC_DQBUF: EIO "
                "(vid_source->pframe %ud)", v4l2cam->pframe);
            retcd = 1;
        } else if (errno == EAGAIN) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_DQBUF: EAGAIN"
                       " (vid_source->pframe %ud)", v4l2cam->pframe);
            retcd = 1;
        } else {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_DQBUF");
            retcd = -1;
        }

        pthread_sigmask(SIG_UNBLOCK, &old, NULL);
        return retcd;
    }

    v4l2cam->pframe = v4l2cam->buf.index;
    v4l2cam->buffers[v4l2cam->buf.index].used = v4l2cam->buf.bytesused;
    v4l2cam->buffers[v4l2cam->buf.index].content_length = v4l2cam->buf.bytesused;

    pthread_sigmask(SIG_UNBLOCK, &old, NULL);    /*undo the signal blocking */

    return 0;

}

/* Convert captured image to the standard motion pixel format*/
static int v4l2_capture_convert(ctx_cam *cam, ctx_v4l2cam *v4l2cam, unsigned char *img_norm)
{
    int shift;
    video_buff *the_buffer = &v4l2cam->buffers[v4l2cam->buf.index];

    shift = 0;
    /*The FALLTHROUGH is a special comment required by compiler. */
    switch (v4l2cam->pixfmt_src) {
    case V4L2_PIX_FMT_RGB24:
        vid_rgb24toyuv420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_UYVY:
        vid_uyvyto420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_YUYV:
        vid_yuv422to420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_YUV422P:
        vid_yuv422pto420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_YUV420:
        memcpy(img_norm, the_buffer->ptr, the_buffer->content_length);
        return 0;

    case V4L2_PIX_FMT_PJPG:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_JPEG:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_MJPEG:
        return vid_mjpegtoyuv420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height
                                    ,the_buffer->content_length);

    case V4L2_PIX_FMT_SBGGR16:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_SGBRG8:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_SGRBG8:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_SBGGR8:    /* bayer */
        vid_bayer2rgb24(cam->imgs.common_buffer, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        vid_rgb24toyuv420p(img_norm, cam->imgs.common_buffer, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_SPCA561:
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_SN9C10X:
        vid_sonix_decompress(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        vid_bayer2rgb24(cam->imgs.common_buffer, img_norm, v4l2cam->width, v4l2cam->height);
        vid_rgb24toyuv420p(img_norm, cam->imgs.common_buffer, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_Y12:
        shift += 2;
        /*FALLTHROUGH*/
    case V4L2_PIX_FMT_Y10:
        shift += 2;
        vid_y10torgb24(cam->imgs.common_buffer, the_buffer->ptr, v4l2cam->width, v4l2cam->height, shift);
        vid_rgb24toyuv420p(img_norm, cam->imgs.common_buffer, v4l2cam->width, v4l2cam->height);
        return 0;

    case V4L2_PIX_FMT_GREY:
        vid_greytoyuv420p(img_norm, the_buffer->ptr, v4l2cam->width, v4l2cam->height);
        return 0;
    }

    return 1;

}

static int v4l2_device_init(ctx_cam *cam)
{
    int indx;

    cam->v4l2cam = (struct ctx_v4l2cam*)mymalloc(sizeof(struct ctx_v4l2cam));
    cam->v4l2cam->devctrl_array = NULL;
    cam->v4l2cam->devctrl_count = 0;
    cam->v4l2cam->buffer_count= 0;
    cam->v4l2cam->pframe = -1;
    cam->v4l2cam->finish = &cam->finish_cam;
    cam->v4l2cam->buffers = NULL;

    cam->v4l2cam->params =(struct ctx_params*) mymalloc(sizeof(struct ctx_params));
    memset(cam->v4l2cam->params, 0, sizeof(struct ctx_params));
    cam->v4l2cam->params->params_array = NULL;
    cam->v4l2cam->params->params_count = 0;
    cam->v4l2cam->params->update_params = TRUE;     /*Set trigger to update the params */

    util_parms_parse(cam->v4l2cam->params, cam->conf->v4l2_params);

    util_parms_add_default(cam->v4l2cam->params, "input", "-1");
    util_parms_add_default(cam->v4l2cam->params, "palette", "17");
    util_parms_add_default(cam->v4l2cam->params, "norm", "0");
    util_parms_add_default(cam->v4l2cam->params, "frequency", "0");


    for (indx = 0; indx < cam->v4l2cam->params->params_count; indx++) {
        if (mystreq(cam->v4l2cam->params->params_array[indx].param_name,"input")) {
            cam->v4l2cam->input =  atoi(cam->v4l2cam->params->params_array[indx].param_value);
        }
        if (mystreq(cam->v4l2cam->params->params_array[indx].param_name,"palette")) {
            cam->v4l2cam->palette =  atoi(cam->v4l2cam->params->params_array[indx].param_value);
        }
        if (mystreq(cam->v4l2cam->params->params_array[indx].param_name,"norm")) {
            cam->v4l2cam->norm =  atoi(cam->v4l2cam->params->params_array[indx].param_value);
        }
        if (mystreq(cam->v4l2cam->params->params_array[indx].param_name,"frequency")) {
            cam->v4l2cam->frequency =  atol(cam->v4l2cam->params->params_array[indx].param_value);
        }
    }

    cam->v4l2cam->height = cam->conf->height;
    cam->v4l2cam->width = cam->conf->width;
    cam->v4l2cam->fps =cam->conf->framerate;

    return 0;
}

/* Update and set user params if needed */
static void v4l2_device_select(ctx_cam *cam)
{
    int retcd;

    if (cam->v4l2cam->params->update_params == TRUE) {

        util_parms_parse(cam->v4l2cam->params, cam->conf->v4l2_params);

        retcd = v4l2_parms_set(cam->v4l2cam);
        if (retcd < 0 ) {
            MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Error setting device controls"));
            return;
        }

        v4l2_ctrls_set(cam->v4l2cam);
    }
}

/* Open the device */
static int v4l2_device_open(ctx_cam *cam)
{

    int fd_device;

    cam->v4l2cam->fd_device = -1;
    fd_device = -1;

    cam->watchdog = 60;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        , _("Opening video device %s")
        , cam->conf->v4l2_device.c_str());

    fd_device = open(cam->conf->v4l2_device.c_str(), O_RDWR|O_CLOEXEC);
    if (fd_device > 0) {
        cam->v4l2cam->fd_device = fd_device;
    } else {
        MOTION_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO
            , _("Failed to open video device %s")
            , cam->conf->v4l2_device.c_str());
        return -1;
    }

    if (xioctl(cam->v4l2cam, VIDIOC_QUERYCAP, &cam->v4l2cam->cap) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("Not a V4L2 device?"));
        return -1;
    }

    if (!(cam->v4l2cam->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, _("Device does not support capturing."));
        return -1;
    }

    return 0;

}

static void v4l2_log_types(ctx_v4l2cam *v4l2cam)
{

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "------------------------");
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.driver: \"%s\"",v4l2cam->cap.driver);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.card: \"%s\"",v4l2cam->cap.card);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.bus_info: \"%s\"",v4l2cam->cap.bus_info);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "cap.capabilities=0x%08X",v4l2cam->cap.capabilities);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "------------------------");

    if (v4l2cam->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_CAPTURE");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_OUTPUT");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VIDEO_OVERLAY");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_VBI_CAPTURE) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VBI_CAPTURE");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_VBI_OUTPUT) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- VBI_OUTPUT");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_RDS_CAPTURE) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- RDS_CAPTURE");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_TUNER) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- TUNER");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_AUDIO) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- AUDIO");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_READWRITE) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- READWRITE");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_ASYNCIO) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- ASYNCIO");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_STREAMING) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- STREAMING");
    }
    if (v4l2cam->cap.capabilities & V4L2_CAP_TIMEPERFRAME) {
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "- TIMEPERFRAME");
    }

    return;
}

static void v4l2_log_formats(ctx_v4l2cam *v4l2cam)
{
        palette_item *palette_array;
        struct v4l2_fmtdesc         dev_format;
        struct v4l2_frmsizeenum     dev_sizes;
        struct v4l2_frmivalenum     dev_frameint;
        int indx_format, indx_sizes, indx_frameint;

        palette_array = (palette_item *)malloc(sizeof(palette_item) * (V4L2_PALETTE_COUNT_MAX+1));

        v4l2_palette_init(palette_array);

        memset(&dev_format, 0, sizeof(struct v4l2_fmtdesc));
        dev_format.index = indx_format = 0;
        dev_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while (xioctl(v4l2cam, VIDIOC_ENUM_FMT, &dev_format) != -1) {
            MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                ,_("Supported palette %s (%c%c%c%c)")
                ,dev_format.description
                ,dev_format.pixelformat >> 0
                ,dev_format.pixelformat >> 8
                ,dev_format.pixelformat >> 16
                ,dev_format.pixelformat >> 24);

            memset(&dev_sizes, 0, sizeof(struct v4l2_frmsizeenum));
            dev_sizes.index = indx_sizes = 0;
            dev_sizes.pixel_format = dev_format.pixelformat;
            while (xioctl(v4l2cam, VIDIOC_ENUM_FRAMESIZES, &dev_sizes) != -1) {
                MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                    ,_("  Width: %d, Height %d")
                    ,dev_sizes.discrete.width
                    ,dev_sizes.discrete.height);

                memset(&dev_frameint, 0, sizeof(struct v4l2_frmivalenum));
                dev_frameint.index = indx_frameint = 0;
                dev_frameint.pixel_format = dev_format.pixelformat;
                dev_frameint.width = dev_sizes.discrete.width;
                dev_frameint.height = dev_sizes.discrete.height;
                while (xioctl(v4l2cam, VIDIOC_ENUM_FRAMEINTERVALS, &dev_frameint) != -1) {
                    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                        ,_("    Framerate %d/%d")
                        ,dev_frameint.discrete.numerator
                        ,dev_frameint.discrete.denominator);
                    memset(&dev_frameint, 0, sizeof(struct v4l2_frmivalenum));
                    dev_frameint.index = ++indx_frameint;
                    dev_frameint.pixel_format = dev_format.pixelformat;
                    dev_frameint.width = dev_sizes.discrete.width;
                    dev_frameint.height = dev_sizes.discrete.height;
                }
                memset(&dev_sizes, 0, sizeof(struct v4l2_frmsizeenum));
                dev_sizes.index = ++indx_sizes;
                dev_sizes.pixel_format = dev_format.pixelformat;
            }
            memset(&dev_format, 0, sizeof(struct v4l2_fmtdesc));
            dev_format.index = ++indx_format;
            dev_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }

        free(palette_array);

        return;
}

static void v4l2_set_fps(ctx_v4l2cam *v4l2cam)
{
    struct v4l2_streamparm setfps;
    int retcd;

    memset(&setfps, 0, sizeof(struct v4l2_streamparm));

    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = v4l2cam->fps;

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
        , _("Trying to set fps to %d")
        , setfps.parm.capture.timeperframe.denominator);

    retcd = xioctl(v4l2cam, VIDIOC_S_PARM, &setfps);
    if (retcd != 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
            ,_("Error setting fps. Return code %d"), retcd);
    }

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
        , _("Device set fps to %d")
        , setfps.parm.capture.timeperframe.denominator);

    return;
}

#endif /* HAVE_V4L2 */

void v4l2_cleanup(ctx_cam *cam)
{
    #ifdef HAVE_V4L2

        enum v4l2_buf_type type;
        int indx;

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Closing video device %s"), cam->conf->v4l2_device.c_str());

        if (cam->v4l2cam == NULL) {
            return;
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (cam->v4l2cam->fd_device != -1) {
            xioctl(cam->v4l2cam, VIDIOC_STREAMOFF, &type);

            close(cam->v4l2cam->fd_device);
            cam->v4l2cam->fd_device = -1;
        }

        if (cam->v4l2cam->buffers != NULL) {
            for (indx = 0; indx < (int)cam->v4l2cam->req.count; indx++){
                munmap(cam->v4l2cam->buffers[indx].ptr, cam->v4l2cam->buffers[indx].size);
            }
            free(cam->v4l2cam->buffers);
            cam->v4l2cam->buffers = NULL;
        }

        if (cam->v4l2cam->devctrl_count != 0) {
            for (indx = 0; indx < cam->v4l2cam->devctrl_count; indx++){
                free(cam->v4l2cam->devctrl_array[indx].ctrl_iddesc);
                free(cam->v4l2cam->devctrl_array[indx].ctrl_name);
                cam->v4l2cam->devctrl_array[indx].ctrl_iddesc = NULL;
                cam->v4l2cam->devctrl_array[indx].ctrl_name = NULL;
            }
            free(cam->v4l2cam->devctrl_array);
            cam->v4l2cam->devctrl_array = NULL;
        }
        cam->v4l2cam->devctrl_count=0;

        util_parms_free(cam->v4l2cam->params);

        free(cam->v4l2cam->params);

        free(cam->v4l2cam);
        cam->v4l2cam = NULL;
        cam->running_cam = FALSE;
    #else
        (void)cam;
    #endif // HAVE_V4L2
}


int v4l2_start(ctx_cam *cam)
{
    #ifdef HAVE_V4L2

        int retcd;

        retcd = v4l2_device_init(cam);
        if (retcd == 0) retcd = v4l2_device_open(cam);
        if (retcd == 0) v4l2_log_types(cam->v4l2cam);
        if (retcd == 0) v4l2_log_formats(cam->v4l2cam);
        if (retcd == 0) retcd = v4l2_set_input(cam->v4l2cam);
        if (retcd == 0) v4l2_set_norm(cam->v4l2cam);
        if (retcd == 0) v4l2_set_frequency(cam->v4l2cam);
        if (retcd == 0) retcd = v4l2_set_palette(cam->v4l2cam);
        if (retcd == 0) v4l2_set_fps(cam->v4l2cam);
        if (retcd == 0) retcd = v4l2_ctrls_count(cam->v4l2cam);
        if (retcd == 0) v4l2_ctrls_list(cam->v4l2cam);
        if (retcd == 0) v4l2_ctrls_set(cam->v4l2cam);
        if (retcd == 0) retcd = v4l2_set_mmap(cam->v4l2cam);
        if (retcd == 0) v4l2_set_imgs(cam);
        if (retcd < 0) {
            v4l2_cleanup(cam);
            return retcd;
        }

        return cam->v4l2cam->fd_device;
    #else
        (void)cam;
        return -1;
    #endif // HAVE_V4l2
}

int v4l2_next(ctx_cam *cam, ctx_image_data *img_data)
{
    #ifdef HAVE_V4L2
        int retcd;

        v4l2_device_select(cam);

        retcd = v4l2_capture_buffer(cam->v4l2cam);
        if (retcd != 0) {
            return retcd;
        }

        retcd = v4l2_capture_convert(cam, cam->v4l2cam, img_data->image_norm);
        if (retcd != 0) {
            return retcd;
        }

        /* Rotate the image as specified. */
        rotate_map(cam, img_data);

        return retcd;
    #else
        (void)cam;
        (void)img_data;
        return -1;
    #endif // HAVE_V4L2
}


