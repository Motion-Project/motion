/*
 *    video_loopback.c
 *
 *    Video loopback functions for motion.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Copyright 2008 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "translate.h"
#include "motion.h"

#if (defined(HAVE_V4L2)) && (!defined(BSD))

#include "video_loopback.h"
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static int vlp_open_vidpipe(void)
{
    int pipe_fd = -1;
    char pipepath[255];
    char buffer[255];
    DIR *dir;
    struct dirent *dirp;
    const char prefix[] = "/sys/class/video4linux/";
    int fd,tfd;
    int len,min;
    int retcd;

    if ((dir = opendir(prefix)) == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("Failed to open '%s'"), prefix);
        return -1;
    }

    while ((dirp = readdir(dir)) != NULL) {
        if (!strncmp(dirp->d_name, "video", 5)) {

            retcd = snprintf(buffer, sizeof(buffer),"%s%s/name", prefix, dirp->d_name);
            if ((retcd<0) || (retcd >= (int)sizeof(buffer))) {
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
                    ,_("Error specifying buffer: %s"),buffer);
                continue;
            } else {
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Opening buffer: %s"),buffer);
            }

            if ((fd = open(buffer, O_RDONLY|O_CLOEXEC)) >= 0) {
                if ((len = read(fd, buffer, sizeof(buffer)-1)) < 0) {
                    close(fd);
                    continue;
                }
                buffer[len]=0;
                MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO,_("Read buffer: %s"),buffer);
                if (strncmp(buffer, "Loopback video device",21)) { /* weird stuff after minor */
                    close(fd);
                    continue;
                }
                min = atoi(&buffer[21]);

                retcd = snprintf(buffer,sizeof(buffer),"/dev/%s",dirp->d_name);
                if ((retcd < 0) || (retcd >= (int)sizeof(buffer))) {
                    MOTION_LOG(NTC, TYPE_VIDEO, SHOW_ERRNO
                        ,_("Error specifying buffer: %s"),buffer);
                    close(fd);
                    continue;
                } else {
                    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("found video device '%s' %d"), buffer,min);
                }

                if ((tfd = open(buffer, O_RDWR|O_CLOEXEC)) >= 0) {
                    strncpy(pipepath, buffer, sizeof(pipepath));
                    if (pipe_fd >= 0) close(pipe_fd);
                    pipe_fd = tfd;
                    break;
                }
            }
                close(fd);
        }
    }

    closedir(dir);

    if (pipe_fd >= 0)
      MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), pipepath);

    return pipe_fd;
}

typedef struct capent {const char *cap; int code;} capentT;
    capentT cap_list[] ={
        {"V4L2_CAP_VIDEO_CAPTURE"        ,0x00000001 },
        {"V4L2_CAP_VIDEO_CAPTURE_MPLANE" ,0x00001000 },
        {"V4L2_CAP_VIDEO_OUTPUT"         ,0x00000002 },
        {"V4L2_CAP_VIDEO_OUTPUT_MPLANE"  ,0x00002000 },
        {"V4L2_CAP_VIDEO_M2M"            ,0x00004000 },
        {"V4L2_CAP_VIDEO_M2M_MPLANE"     ,0x00008000 },
        {"V4L2_CAP_VIDEO_OVERLAY"        ,0x00000004 },
        {"V4L2_CAP_VBI_CAPTURE"          ,0x00000010 },
        {"V4L2_CAP_VBI_OUTPUT"           ,0x00000020 },
        {"V4L2_CAP_SLICED_VBI_CAPTURE"   ,0x00000040 },
        {"V4L2_CAP_SLICED_VBI_OUTPUT"    ,0x00000080 },
        {"V4L2_CAP_RDS_CAPTURE"          ,0x00000100 },
        {"V4L2_CAP_VIDEO_OUTPUT_OVERLAY" ,0x00000200 },
        {"V4L2_CAP_HW_FREQ_SEEK"         ,0x00000400 },
        {"V4L2_CAP_RDS_OUTPUT"           ,0x00000800 },
        {"V4L2_CAP_TUNER"                ,0x00010000 },
        {"V4L2_CAP_AUDIO"                ,0x00020000 },
        {"V4L2_CAP_RADIO"                ,0x00040000 },
        {"V4L2_CAP_MODULATOR"            ,0x00080000 },
        {"V4L2_CAP_SDR_CAPTURE"          ,0x00100000 },
        {"V4L2_CAP_EXT_PIX_FORMAT"       ,0x00200000 },
        {"V4L2_CAP_SDR_OUTPUT"           ,0x00400000 },
        {"V4L2_CAP_READWRITE"            ,0x01000000 },
        {"V4L2_CAP_ASYNCIO"              ,0x02000000 },
        {"V4L2_CAP_STREAMING"            ,0x04000000 },
        {"V4L2_CAP_DEVICE_CAPS"          ,0x80000000 },
        {"Last",0}
};

static void vlp_show_vcap(struct v4l2_capability *cap) {
    unsigned int vers = cap->version;
    unsigned int c    = cap->capabilities;
    int i;

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Pipe Device");
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.driver:   %s",cap->driver);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %s",cap->card);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.bus_info: %s",cap->bus_info);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "cap.card:     %u.%u.%u",(vers >> 16) & 0xFF,(vers >> 8) & 0xFF,vers & 0xFF);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "Device capabilities");
    for (i=0;cap_list[i].code;i++)
        if (c & cap_list[i].code)
            MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s",cap_list[i].cap);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
}

static void vlp_show_vfmt(struct v4l2_format *v) {
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "type: type:           %d",v->type);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.width:        %d",v->fmt.pix.width);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.height:       %d",v->fmt.pix.height);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.pixelformat:  %d",v->fmt.pix.pixelformat);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.sizeimage:    %d",v->fmt.pix.sizeimage);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.field:        %d",v->fmt.pix.field);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.bytesperline: %d",v->fmt.pix.bytesperline);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "fmt.pix.colorspace:   %d",v->fmt.pix.colorspace);
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "------------------------");
}

int vlp_startpipe(const char *dev_name, int width, int height)
{
    int dev;
    struct v4l2_format v;
    struct v4l2_capability vc;

    if (!strcmp(dev_name, "-")) {
        dev = vlp_open_vidpipe();
    } else {
        dev = open(dev_name, O_RDWR|O_CLOEXEC);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opened %s as pipe output"), dev_name);
    }

    if (dev < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("Opening %s as pipe output failed"), dev_name);
        return -1;
    }


    if (ioctl(dev, VIDIOC_QUERYCAP, &vc) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_QUERYCAP)");
        return -1;
    }

    vlp_show_vcap(&vc);

    memset(&v, 0, sizeof(v));

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(dev, VIDIOC_G_FMT, &v) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_G_FMT)");
        return -1;
    }
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Original pipe specifications"));
    vlp_show_vfmt(&v);

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    v.fmt.pix.width = width;
    v.fmt.pix.height = height;
    v.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    v.fmt.pix.sizeimage = 3 * width * height / 2;
    v.fmt.pix.bytesperline = width;
    v.fmt.pix.field = V4L2_FIELD_NONE;
    v.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Proposed pipe specifications"));
    vlp_show_vfmt(&v);

    if (ioctl(dev,VIDIOC_S_FMT, &v) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "ioctl (VIDIOC_S_FMT)");
        return -1;
    }

    MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("Final pipe specifications"));
    vlp_show_vfmt(&v);

    return dev;
}

int vlp_putpipe(int dev, unsigned char *image, int imgsize)
{
    return write(dev, image, imgsize);
}


#endif /* HAVE_V4L2 && !BSD */
