/*
 *    video2.c
 *
 *    V4L2 interface with basically JPEG decompression support and even more ...
 *    Copyright 2006 Krzysztof Blaszkowski (kb@sysmikro.com.pl)
 *              2007 Angel Carpintero (motiondevelop@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
*/
#include "rotate.h"    /* Already includes motion.h */
#include "video_common.h"
#include "video_v4l2.h"
#include <sys/mman.h>


#ifdef HAVE_V4L2

#include <linux/videodev2.h>

/* video4linux stuff */
#define NORM_DEFAULT    0
#define NORM_PAL        0
#define NORM_NTSC       1
#define NORM_SECAM      2
#define NORM_PAL_NC     3

#define IN_TV           0
#define IN_COMPOSITE    1
#define IN_COMPOSITE2   2
#define IN_SVIDEO       3

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define s32 signed int

#define MMAP_BUFFERS 4
#define MIN_MMAP_BUFFERS 2

#ifndef V4L2_PIX_FMT_SBGGR8
#define V4L2_PIX_FMT_SBGGR8  v4l2_fourcc('B','A','8','1')  /*  8  BGBG.. GRGR.. */
#endif

#ifndef V4L2_PIX_FMT_MJPEG
#define V4L2_PIX_FMT_MJPEG   v4l2_fourcc('M','J','P','G')  /* Motion-JPEG   */
#endif

#ifndef V4L2_PIX_FMT_SN9C10X
#define V4L2_PIX_FMT_SN9C10X v4l2_fourcc('S','9','1','0')  /* SN9C10x compression */
#endif

#ifndef V4L2_PIX_FMT_SGBRG8
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /*  8  GBGB.. RGRG.. */
#endif

#ifndef V4L2_PIX_FMT_SGRBG8
#define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */
#endif

#ifndef V4L2_PIX_FMT_SBGGR16
#define V4L2_PIX_FMT_SBGGR16 v4l2_fourcc('B', 'Y', 'R', '2') /* 16  BGBG.. GRGR.. */
#endif

#ifndef V4L2_PIX_FMT_SPCA561
#define V4L2_PIX_FMT_SPCA561 v4l2_fourcc('S', '5', '6', '1') /* compressed GBRG bayer */
#endif

#ifndef V4L2_PIX_FMT_PJPG
#define V4L2_PIX_FMT_PJPG    v4l2_fourcc('P', 'J', 'P', 'G') /* Pixart 73xx JPEG */
#endif

#ifndef V4L2_PIX_FMT_PAC207
#define V4L2_PIX_FMT_PAC207  v4l2_fourcc('P', '2', '0', '7') /* compressed BGGR bayer */
#endif

#ifndef V4L2_PIX_FMT_SPCA501
#define V4L2_PIX_FMT_SPCA501 v4l2_fourcc('S', '5', '0', '1') /*  YUYV per line */
#endif

#ifndef V4L2_PIX_FMT_SPCA505
#define V4L2_PIX_FMT_SPCA505 v4l2_fourcc('S', '5', '0', '5') /* YYUV per line  */
#endif

#ifndef V4L2_PIX_FMT_SPCA508
#define V4L2_PIX_FMT_SPCA508 v4l2_fourcc('S', '5', '0', '8') /* YUVY per line  */
#endif

#ifndef V4L2_PIX_FMT_Y10
#define V4L2_PIX_FMT_Y10     v4l2_fourcc('Y', '1', '0', ' ') /* 10  Greyscale     */
#endif

#ifndef V4L2_PIX_FMT_Y12
#define V4L2_PIX_FMT_Y12     v4l2_fourcc('Y', '1', '2', ' ') /* 12  Greyscale     */
#endif

#ifndef V4L2_PIX_FMT_GREY
#define V4L2_PIX_FMT_GREY     v4l2_fourcc('G', 'R', 'E', 'Y') /* 8 Greyscale     */
#endif

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264     */
#endif

#define ZC301_V4L2_CID_DAC_MAGN       V4L2_CID_PRIVATE_BASE
#define ZC301_V4L2_CID_GREEN_BALANCE  (V4L2_CID_PRIVATE_BASE+1)

static pthread_mutex_t v4l2_mutex;

static struct video_dev *viddevs = NULL;

typedef struct video_image_buff {
    unsigned char *ptr;
    int content_length;
    size_t size;                    /* total allocated size */
    size_t used;                    /* bytes already used */
    struct timeval image_time;      /* time this image was received */
} video_buff;

static const u32 queried_ctrls[] = {
    V4L2_CID_BRIGHTNESS,
    V4L2_CID_CONTRAST,
    V4L2_CID_SATURATION,
    V4L2_CID_HUE,
    V4L2_CID_POWER_LINE_FREQUENCY,
    V4L2_CID_RED_BALANCE,
    V4L2_CID_BLUE_BALANCE,
    V4L2_CID_GAMMA,
    V4L2_CID_EXPOSURE,
    V4L2_CID_AUTOGAIN,
    V4L2_CID_GAIN,

    ZC301_V4L2_CID_DAC_MAGN,
    ZC301_V4L2_CID_GREEN_BALANCE,
    0
};

typedef struct {
    int fd_device;
    u32 fps;

    struct v4l2_capability cap;
    struct v4l2_format src_fmt;
    struct v4l2_format dst_fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    video_buff *buffers;

    s32 pframe;

    u32 ctrl_flags;
    struct v4l2_queryctrl *controls;
    volatile unsigned int *finish;      /* End the thread */

} src_v4l2_t;

/**
 * xioctl
 */
#if defined (__OpenBSD__) || defined (__FreeBSD__)
static int xioctl(src_v4l2_t *vid_source, unsigned long request, void *arg)
#else
static int xioctl(src_v4l2_t *vid_source, int request, void *arg)
#endif
{
    int ret;

    do
        ret = ioctl(vid_source->fd_device, request, arg);
    while (-1 == ret && EINTR == errno && !vid_source->finish);

    return ret;
}

static int v4l2_get_capability(src_v4l2_t * vid_source)
{
    if (xioctl(vid_source, VIDIOC_QUERYCAP, &vid_source->cap) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Not a V4L2 device?");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: \n------------------------\n"
               "cap.driver: \"%s\"\n"
               "cap.card: \"%s\"\n"
               "cap.bus_info: \"%s\"\n"
               "cap.capabilities=0x%08X\n------------------------",
               vid_source->cap.driver, vid_source->cap.card, vid_source->cap.bus_info,
               vid_source->cap.capabilities);

    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - VIDEO_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - VIDEO_OUTPUT");
    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - VIDEO_OVERLAY");
    if (vid_source->cap.capabilities & V4L2_CAP_VBI_CAPTURE)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - VBI_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_VBI_OUTPUT)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - VBI_OUTPUT");
    if (vid_source->cap.capabilities & V4L2_CAP_RDS_CAPTURE)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - RDS_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_TUNER)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - TUNER");
    if (vid_source->cap.capabilities & V4L2_CAP_AUDIO)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - AUDIO");
    if (vid_source->cap.capabilities & V4L2_CAP_READWRITE)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - READWRITE");
    if (vid_source->cap.capabilities & V4L2_CAP_ASYNCIO)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - ASYNCIO");
    if (vid_source->cap.capabilities & V4L2_CAP_STREAMING)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - STREAMING");
    if (vid_source->cap.capabilities & V4L2_CAP_TIMEPERFRAME)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - TIMEPERFRAME");

    if (!(vid_source->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Device does not support capturing.");
        return -1;
    }

    return 0;
}

static int v4l2_select_input(struct config *conf, struct video_dev *viddev,
                             src_v4l2_t * vid_source, int in, int norm,
                             unsigned long freq_, int tuner_number ATTRIBUTE_UNUSED)
{
    struct v4l2_input input;
    struct v4l2_standard standard;
    v4l2_std_id std_id;

    /* Set the input. */
    memset(&input, 0, sizeof (input));
    if (in == DEF_INPUT)
        input.index = IN_TV;
    else input.index = in;

    if (xioctl(vid_source, VIDIOC_ENUMINPUT, &input) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Unable to query input %d."
                   " VIDIOC_ENUMINPUT, if you use a WEBCAM change input value in conf by -1",
                   input.index);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: name = \"%s\", type 0x%08X,"
               " status %08x", input.name, input.type, input.status);

    if (input.type & V4L2_INPUT_TYPE_TUNER)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - TUNER");

    if (input.type & V4L2_INPUT_TYPE_CAMERA)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - CAMERA");

    if (xioctl(vid_source, VIDIOC_S_INPUT, &input.index) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error selecting input %d"
                   " VIDIOC_S_INPUT", input.index);
        return -1;
    }

    viddev->input = conf->input = in;

    /*
     * Set video standard usually webcams doesn't support the ioctl or
     * return V4L2_STD_UNKNOWN
     */
    if (xioctl(vid_source, VIDIOC_G_STD, &std_id) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Device doesn't support specifying PAL/NTSC norm");
        norm = std_id = 0;    // V4L2_STD_UNKNOWN = 0
    }

    if (std_id) {
        memset(&standard, 0, sizeof(standard));
        standard.index = 0;

        while (xioctl(vid_source, VIDIOC_ENUMSTD, &standard) == 0) {
            if (standard.id & std_id)
                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: - video standard %s",
                           standard.name);

            standard.index++;
        }

        switch (norm) {
        case 1:
            std_id = V4L2_STD_NTSC;
            break;
        case 2:
            std_id = V4L2_STD_SECAM;
            break;
        default:
            std_id = V4L2_STD_PAL;
        }

        if (xioctl(vid_source, VIDIOC_S_STD, &std_id) == -1)
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error selecting standard"
                       " method %d VIDIOC_S_STD", (int)std_id);

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set standard method %d",
                   (int)std_id);
    }

    viddev->norm = conf->norm = norm;

    /* If this input is attached to a tuner, set the frequency. */
    if (input.type & V4L2_INPUT_TYPE_TUNER) {
        struct v4l2_tuner tuner;
        struct v4l2_frequency freq;

        /* Query the tuners capabilities. */

        memset(&tuner, 0, sizeof(struct v4l2_tuner));
        tuner.index = input.tuner;

        if (xioctl(vid_source, VIDIOC_G_TUNER, &tuner) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: tuner %d VIDIOC_G_TUNER",
                       tuner.index);
            return 0;
        }

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set tuner %d",
                   tuner.index);

        /* Set the frequency. */
        memset(&freq, 0, sizeof(struct v4l2_frequency));
        freq.tuner = input.tuner;
        freq.type = V4L2_TUNER_ANALOG_TV;
        freq.frequency = (freq_ / 1000) * 16;

        if (xioctl(vid_source, VIDIOC_S_FREQUENCY, &freq) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: freq %ul VIDIOC_S_FREQUENCY",
                       freq.frequency);
            return 0;
        }

        viddev->freq = conf->frequency = freq_;

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Set Frequency to %ul",
                   freq.frequency);
    } else {
        viddev->freq = conf->frequency = 0;
    }

    return 0;
}

/* *
 * v4l2_do_set_pix_format
 *
 *          This routine does the actual request to the driver
 *
 * Returns:  0  Ok
 *          -1  Problems setting palette or not supported
 *
 * Our algorithm for setting the picture format for the data which the
 * driver returns to us will be as follows:
 *
 * First, we request that the format be set to whatever is in the config
 * file (which is either the motion default, or a value chosen by the user).
 * If that request is successful, we are finished.
 *
 * If the driver responds that our request is not accepted, we then enumerate
 * the formats which the driver claims to be able to supply.  From this list,
 * we choose whichever format is "most efficient" for motion.  The enumerated
 * list is also printed to the motion log so that the user can consider
 * choosing a different value for the config file.
 *
 * We then request the driver to set the format we have chosen.  That request
 * should never fail, so if it does we log the fact and give up.
 */
static int v4l2_do_set_pix_format(u32 pixformat, src_v4l2_t * vid_source,
                                  int *width, int *height)
{
    CLEAR(vid_source->dst_fmt);
    vid_source->dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_source->dst_fmt.fmt.pix.width = *width;
    vid_source->dst_fmt.fmt.pix.height = *height;
    vid_source->dst_fmt.fmt.pix.pixelformat = pixformat;
    vid_source->dst_fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(vid_source, VIDIOC_TRY_FMT, &vid_source->dst_fmt) != -1 &&
        vid_source->dst_fmt.fmt.pix.pixelformat == pixformat) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Testing palette %c%c%c%c (%dx%d)",
                   pixformat >> 0, pixformat >> 8,
                   pixformat >> 16, pixformat >> 24, *width, *height);

        if (vid_source->dst_fmt.fmt.pix.width != (unsigned int) *width ||
            vid_source->dst_fmt.fmt.pix.height != (unsigned int) *height) {

            MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: Adjusting resolution "
                       "from %ix%i to %ix%i.",
                       *width, *height, vid_source->dst_fmt.fmt.pix.width,
                       vid_source->dst_fmt.fmt.pix.height);

            *width = vid_source->dst_fmt.fmt.pix.width;
            *height = vid_source->dst_fmt.fmt.pix.height;
        }

        if (xioctl(vid_source, VIDIOC_S_FMT, &vid_source->dst_fmt) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error setting pixel "
                       "format.\nVIDIOC_S_FMT: ");
            return -1;
        }

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using palette %c%c%c%c (%dx%d)"
                   " bytesperlines %d sizeimage %d colorspace %08x", pixformat >> 0,
                   pixformat >> 8, pixformat >> 16, pixformat >> 24, *width,
                   *height, vid_source->dst_fmt.fmt.pix.bytesperline,
                   vid_source->dst_fmt.fmt.pix.sizeimage,
                   vid_source->dst_fmt.fmt.pix.colorspace);

        return 0;
    }

    return -1;
}

static int v4l2_set_pix_format(struct context *cnt, src_v4l2_t * vid_source,
                               int *width, int *height)
{
    struct v4l2_fmtdesc fmtd;
    int v4l2_pal;

    /*
     * Note that this array MUST exactly match the config file list.
     * A higher index means better chance to be used
     * Special note on the H264:  It must be kept within this
     * list to keep the numbering correct even though it is not a
     * valid format for the v4l2 components.  We edit conf option in
     * the v4l2_start function so that it is impossible that this
     * routine will ever get sent the H264.
     */
    static const u32 supported_formats[] = {
        V4L2_PIX_FMT_SN9C10X,
        V4L2_PIX_FMT_SBGGR16,
        V4L2_PIX_FMT_SBGGR8,
        V4L2_PIX_FMT_SPCA561,
        V4L2_PIX_FMT_SGBRG8,
        V4L2_PIX_FMT_SGRBG8,
        V4L2_PIX_FMT_PAC207,
        V4L2_PIX_FMT_PJPG,
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_JPEG,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_SPCA501,
        V4L2_PIX_FMT_SPCA505,
        V4L2_PIX_FMT_SPCA508,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_YUV422P,
        V4L2_PIX_FMT_YUV420, /* most efficient for motion */
        V4L2_PIX_FMT_Y10,
        V4L2_PIX_FMT_Y12,
        V4L2_PIX_FMT_GREY,
        V4L2_PIX_FMT_H264
    };

    int array_size = sizeof(supported_formats) / sizeof(supported_formats[0]);
    int index_format = -1; /* -1 says not yet chosen */
    CLEAR(fmtd);
    fmtd.index = v4l2_pal = 0;
    fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* First we try a shortcut of just setting the config file value */
    if (cnt->conf.v4l2_palette >= 0) {
        char name[5] = {supported_formats[cnt->conf.v4l2_palette] >>  0,
                        supported_formats[cnt->conf.v4l2_palette] >>  8,
                        supported_formats[cnt->conf.v4l2_palette] >>  16,
                        supported_formats[cnt->conf.v4l2_palette] >>  24, 0};

        if (v4l2_do_set_pix_format(supported_formats[cnt->conf.v4l2_palette],
                                   vid_source, width, height) >= 0)
            return 0;

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Config palette index %d (%s)"
                   " doesn't work.", cnt->conf.v4l2_palette, name);
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Supported palettes:");

    while (xioctl(vid_source, VIDIOC_ENUM_FMT, &fmtd) != -1) {

        int i;

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: (%i) %c%c%c%c (%s)",
                   v4l2_pal, fmtd.pixelformat >> 0,
                   fmtd.pixelformat >> 8, fmtd.pixelformat >> 16,
                   fmtd.pixelformat >> 24, fmtd.description);

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: %d - %s (compressed : %d) (%#x)",
                   fmtd.index, fmtd.description, fmtd.flags, fmtd.pixelformat);

         /* Adjust index_format if larger value found */
        for (i = index_format + 1; i < array_size; i++)
            if (supported_formats[i] == fmtd.pixelformat)
                index_format = i;

        CLEAR(fmtd);
        fmtd.index = ++v4l2_pal;
        fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

    if (index_format >= 0) {
        char name[5] = {supported_formats[index_format] >>  0,
                        supported_formats[index_format] >>  8,
                        supported_formats[index_format] >>  16,
                        supported_formats[index_format] >>  24, 0};

        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s Selected palette %s", name);

        if (v4l2_do_set_pix_format(supported_formats[index_format],
                                   vid_source, width, height) >= 0)
            return 0;

        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "VIDIOC_TRY_FMT failed for "
                   "format %s", name);
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Unable to find a compatible"
               " palette format.");

    return -1;
}

static int v4l2_set_mmap(src_v4l2_t * vid_source)
{
    enum v4l2_buf_type type;
    u32 buffer_index;

    /* Does the device support streaming? */
    if (!(vid_source->cap.capabilities & V4L2_CAP_STREAMING))
        return -1;

    memset(&vid_source->req, 0, sizeof(struct v4l2_requestbuffers));

    vid_source->req.count = MMAP_BUFFERS;
    vid_source->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_source->req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(vid_source, VIDIOC_REQBUFS, &vid_source->req) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error requesting buffers"
                   " %d for memory map. VIDIOC_REQBUFS",
                   vid_source->req.count);
        return -1;
    }

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: mmap information: frames=%d",
               vid_source->req.count);

    if (vid_source->req.count < MIN_MMAP_BUFFERS) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Insufficient buffer memory"
                   " %d < MIN_MMAP_BUFFERS.", vid_source->req.count);
        return -1;
    }

    vid_source->buffers = calloc(vid_source->req.count, sizeof(video_buff));

    if (!vid_source->buffers) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Out of memory.");
        return -1;
    }

    for (buffer_index = 0; buffer_index < vid_source->req.count; buffer_index++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(struct v4l2_buffer));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer_index;

        if (xioctl(vid_source, VIDIOC_QUERYBUF, &buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error querying buffer"
                       " %i\nVIDIOC_QUERYBUF: ", buffer_index);
            free(vid_source->buffers);
            return -1;
        }

        vid_source->buffers[buffer_index].size = buf.length;
        vid_source->buffers[buffer_index].ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                     MAP_SHARED, vid_source->fd_device, buf.m.offset);

        if (vid_source->buffers[buffer_index].ptr == MAP_FAILED) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error mapping buffer %i mmap",
                       buffer_index);
            free(vid_source->buffers);
            return -1;
        }

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: %i length=%d Address (%x)",
                   buffer_index, buf.length, vid_source->buffers[buffer_index].ptr);
    }

    for (buffer_index = 0; buffer_index < vid_source->req.count; buffer_index++) {
        memset(&vid_source->buf, 0, sizeof(struct v4l2_buffer));

        vid_source->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vid_source->buf.memory = V4L2_MEMORY_MMAP;
        vid_source->buf.index = buffer_index;

        if (xioctl(vid_source, VIDIOC_QBUF, &vid_source->buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: VIDIOC_QBUF");
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(vid_source, VIDIOC_STREAMON, &type) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error starting stream."
                   " VIDIOC_STREAMON");
        return -1;
    }

    return 0;
}

static int v4l2_scan_controls(src_v4l2_t * vid_source)
{
    int count, i;
    struct v4l2_queryctrl queryctrl;

    memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));

    for (i = 0, count = 0; queried_ctrls[i]; i++) {
        queryctrl.id = queried_ctrls[i];
        if (xioctl(vid_source, VIDIOC_QUERYCTRL, &queryctrl))
            continue;

        count++;
        vid_source->ctrl_flags |= 1 << i;
    }

    if (count) {
        struct v4l2_queryctrl *ctrl = vid_source->controls
                                    = calloc(count, sizeof(struct v4l2_queryctrl));

        if (!ctrl) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Insufficient buffer memory.");
            return -1;
        }

        for (i = 0; queried_ctrls[i]; i++) {
            if (vid_source->ctrl_flags & (1 << i)) {
                struct v4l2_control control;

                queryctrl.id = queried_ctrls[i];
                if (xioctl(vid_source, VIDIOC_QUERYCTRL, &queryctrl))
                    continue;

                memcpy(ctrl, &queryctrl, sizeof(struct v4l2_queryctrl));

                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: found control 0x%08x, \"%s\","
                           " range %d,%d %s",
                           ctrl->id, ctrl->name, ctrl->minimum, ctrl->maximum,
                           ctrl->flags & V4L2_CTRL_FLAG_DISABLED ? "!DISABLED!" : "");

                memset(&control, 0, sizeof (control));
                control.id = queried_ctrls[i];
                xioctl(vid_source, VIDIOC_G_CTRL, &control);
                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: \t\"%s\", default %d, current %d",
                           ctrl->name, ctrl->default_value, control.value);

                ctrl++;
            }
        }
    }

    return 0;
}

static int v4l2_set_control(src_v4l2_t * vid_source, u32 cid, int value)
{
    int i, count;

    if (!vid_source->controls)
        return -1;

    for (i = 0, count = 0; queried_ctrls[i]; i++) {
        if (vid_source->ctrl_flags & (1 << i)) {
            if (cid == queried_ctrls[i]) {
                struct v4l2_queryctrl *ctrl = vid_source->controls + count;
                struct v4l2_control control;
                int ret;

                memset(&control, 0, sizeof (control));
                control.id = queried_ctrls[i];

                switch (ctrl->type) {
                case V4L2_CTRL_TYPE_INTEGER:
                    value = control.value =
                            (value * (ctrl->maximum - ctrl->minimum) / 256) + ctrl->minimum;
                    ret = xioctl(vid_source, VIDIOC_S_CTRL, &control);
                    break;

                case V4L2_CTRL_TYPE_BOOLEAN:
                    value = control.value = value ? 1 : 0;
                    ret = xioctl(vid_source, VIDIOC_S_CTRL, &control);
                    break;

                case V4L2_CTRL_TYPE_MENU:
                    /* set as is, no adjustments */
                    control.value = value;
                    ret = xioctl(vid_source, VIDIOC_S_CTRL, &control);
                    break;


                default:
                    MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: control type not supported yet");
                    return -1;
                }

                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: setting control \"%s\" to %d"
                           " (ret %d %s) %s", ctrl->name, value, ret, ret ? strerror(errno) : "",
                           ctrl->flags & V4L2_CTRL_FLAG_DISABLED ? "Control is DISABLED!" : "");

                return 0;
            }
            count++;
        }
    }

    return -1;
}

static void v4l2_picture_controls(struct context *cnt, struct video_dev *viddev)
{
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;

    if (cnt->conf.contrast && cnt->conf.contrast != viddev->contrast) {
        viddev->contrast = cnt->conf.contrast;
        v4l2_set_control(vid_source, V4L2_CID_CONTRAST, viddev->contrast);
    }

    if (cnt->conf.saturation && cnt->conf.saturation != viddev->saturation) {
        viddev->saturation = cnt->conf.saturation;
        v4l2_set_control(vid_source, V4L2_CID_SATURATION, viddev->saturation);
    }

    if (cnt->conf.hue && cnt->conf.hue != viddev->hue) {
        viddev->hue = cnt->conf.hue;
        v4l2_set_control(vid_source, V4L2_CID_HUE, viddev->hue);
    }

    /* -1 is don't modify as 0 is an option to disable the power line filter */
    if (cnt->conf.power_line_frequency != -1 && cnt->conf.power_line_frequency != viddev->power_line_frequency) {
        viddev->power_line_frequency = cnt->conf.power_line_frequency;
        v4l2_set_control(vid_source, V4L2_CID_POWER_LINE_FREQUENCY, viddev->power_line_frequency);
    }

    if (cnt->conf.autobright) {
        if (vid_do_autobright(cnt, viddev)) {
            if (v4l2_set_control(vid_source, V4L2_CID_BRIGHTNESS, viddev->brightness))
                v4l2_set_control(vid_source, V4L2_CID_GAIN, viddev->brightness);
        }
    } else {
        if (cnt->conf.brightness && cnt->conf.brightness != viddev->brightness) {
            viddev->brightness = cnt->conf.brightness;
            if (v4l2_set_control(vid_source, V4L2_CID_BRIGHTNESS, viddev->brightness))
                v4l2_set_control(vid_source, V4L2_CID_GAIN, viddev->brightness);
        }
    }

}

static unsigned char *v4l2_device_init(struct context *cnt, struct video_dev *viddev, int width, int height,
              int input, int norm, unsigned long freq, int tuner_number)
{
    src_v4l2_t *vid_source;

    /* Allocate memory for the state structure. */
    if (!(vid_source = calloc(sizeof(src_v4l2_t), 1))) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Out of memory.");
        goto err;
    }

    viddev->v4l2_private = vid_source;
    vid_source->fd_device = viddev->fd_device;
    vid_source->fps = cnt->conf.frame_limit;
    vid_source->pframe = -1;
    vid_source->finish = &cnt->finish;
    struct config *conf = &cnt->conf;

    if (v4l2_get_capability(vid_source))
        goto err;

    if (v4l2_select_input(conf, viddev, vid_source, input, norm, freq, tuner_number))
        goto err;

    if (v4l2_set_pix_format(cnt, vid_source, &width, &height))
        goto err;

    if (v4l2_scan_controls(vid_source))
        goto err;

    if (v4l2_set_mmap(vid_source))
        goto err;

    viddev->size_map = 0;
    viddev->v4l_buffers[0] = NULL;
    viddev->v4l_maxbuffer = 1;
    viddev->v4l_curbuffer = 0;

    viddev->v4l_fmt = VIDEO_PALETTE_YUV420P;
    viddev->v4l_bufsize = (width * height * 3) / 2;


    /* Update width and height with supported values from camera driver */
    viddev->width = width;
    viddev->height = height;

    return (void *) 1;

err:
    free(vid_source);

    viddev->v4l2_private = NULL;
    viddev->v4l2 = 0;
    return NULL;
}

static int v4l2_capture(struct context *cnt, struct video_dev *viddev, unsigned char *map,
              int width, int height)
{
    sigset_t set, old;
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;
    int shift = 0;

    if (viddev->v4l_fmt != VIDEO_PALETTE_YUV420P)
        return V4L2_FATAL_ERROR;

    /* Block signals during IOCTL */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, &old);

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: 1) vid_source->pframe %i",
               vid_source->pframe);

    if (vid_source->pframe >= 0) {
        if (xioctl(vid_source, VIDIOC_QBUF, &vid_source->buf) == -1) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: VIDIOC_QBUF");
            pthread_sigmask(SIG_UNBLOCK, &old, NULL);
            return -1;
        }
    }

    memset(&vid_source->buf, 0, sizeof(struct v4l2_buffer));

    vid_source->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_source->buf.memory = V4L2_MEMORY_MMAP;
    vid_source->buf.bytesused = 0;

    if (xioctl(vid_source, VIDIOC_DQBUF, &vid_source->buf) == -1) {
        int ret;
        /*
         * Some drivers return EIO when there is no signal,
         * driver might dequeue an (empty) buffer despite
         * returning an error, or even stop capturing.
         */
        if (errno == EIO) {
            vid_source->pframe++;

            if ((u32)vid_source->pframe >= vid_source->req.count)
                vid_source->pframe = 0;

             vid_source->buf.index = vid_source->pframe;
             MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: VIDIOC_DQBUF: EIO "
                        "(vid_source->pframe %d)", vid_source->pframe);
             ret = 1;
        } else if (errno == EAGAIN) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: VIDIOC_DQBUF: EAGAIN"
                       " (vid_source->pframe %d)", vid_source->pframe);
            ret = 1;
        } else {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: VIDIOC_DQBUF");
            ret = -1;
        }

        pthread_sigmask(SIG_UNBLOCK, &old, NULL);
        return ret;
    }

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: 2) vid_source->pframe %i",
               vid_source->pframe);

    vid_source->pframe = vid_source->buf.index;
    vid_source->buffers[vid_source->buf.index].used = vid_source->buf.bytesused;
    vid_source->buffers[vid_source->buf.index].content_length = vid_source->buf.bytesused;

    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: 3) vid_source->pframe %i "
               "vid_source->buf.index %i", vid_source->pframe, vid_source->buf.index);
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: vid_source->buf.bytesused %i",
               vid_source->buf.bytesused);

    pthread_sigmask(SIG_UNBLOCK, &old, NULL);    /*undo the signal blocking */

    {
        video_buff *the_buffer = &vid_source->buffers[vid_source->buf.index];

        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: the_buffer index %d Address (%x)",
                   vid_source->buf.index, the_buffer->ptr);

        switch (vid_source->dst_fmt.fmt.pix.pixelformat) {
        case V4L2_PIX_FMT_RGB24:
            vid_rgb24toyuv420p(map, the_buffer->ptr, width, height);
            return 0;

        case V4L2_PIX_FMT_UYVY:
            vid_uyvyto420p(map, the_buffer->ptr, (unsigned)width, (unsigned)height);
            return 0;

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YUV422P:
            vid_yuv422to420p(map, the_buffer->ptr, width, height);
            return 0;

        case V4L2_PIX_FMT_YUV420:
            memcpy(map, the_buffer->ptr, viddev->v4l_bufsize);
            return 0;

        case V4L2_PIX_FMT_PJPG:
        case V4L2_PIX_FMT_JPEG:
        case V4L2_PIX_FMT_MJPEG:
            return vid_mjpegtoyuv420p(map, the_buffer->ptr, width, height,
                                  vid_source->buffers[vid_source->buf.index].content_length);

        /* FIXME: quick hack to allow work all bayer formats */
        case V4L2_PIX_FMT_SBGGR16:
        case V4L2_PIX_FMT_SGBRG8:
        case V4L2_PIX_FMT_SGRBG8:
        /* case V4L2_PIX_FMT_SPCA561: */
        case V4L2_PIX_FMT_SBGGR8:    /* bayer */
            vid_bayer2rgb24(cnt->imgs.common_buffer, the_buffer->ptr, width, height);
            vid_rgb24toyuv420p(map, cnt->imgs.common_buffer, width, height);
            return 0;

        case V4L2_PIX_FMT_SPCA561:
        case V4L2_PIX_FMT_SN9C10X:
            vid_sonix_decompress(map, the_buffer->ptr, width, height);
            vid_bayer2rgb24(cnt->imgs.common_buffer, map, width, height);
            vid_rgb24toyuv420p(map, cnt->imgs.common_buffer, width, height);
            return 0;
        case V4L2_PIX_FMT_Y12:
            shift += 2;
        case V4L2_PIX_FMT_Y10:
            shift += 2;
            vid_y10torgb24(cnt->imgs.common_buffer, the_buffer->ptr, width, height, shift);
            vid_rgb24toyuv420p(map, cnt->imgs.common_buffer, width, height);
            return 0;
        case V4L2_PIX_FMT_GREY:
            vid_greytoyuv420p(map, the_buffer->ptr, width, height);
            return 0;

        }
    }

    return 1;
}

static void v4l2_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map,
                    int width, int height, struct config *conf)
{
    int input = conf->input;
    int norm = conf->norm;
    unsigned long freq = conf->frequency;
    int tuner_number = conf->tuner_number;

    if (input != viddev->input || width != viddev->width || height != viddev->height ||
        freq != viddev->freq || tuner_number != viddev->tuner_number || norm != viddev->norm) {

        unsigned int i;
        struct timeval switchTime;
        unsigned int skip = conf->roundrobin_skip;

        if (conf->roundrobin_skip < 0)
            skip = 1;

        v4l2_select_input(conf, viddev, (src_v4l2_t *) viddev->v4l2_private,
                          input, norm, freq, tuner_number);

        gettimeofday(&switchTime, NULL);

        v4l2_picture_controls(cnt, viddev);

        viddev->width = width;
        viddev->height = height;

        /*
        viddev->input = input;
        viddev->norm = norm;
        viddev->width = width;
        viddev->height = height;
        viddev->freq = freq;
        viddev->tuner_number = tuner_number;
        */

        /* Skip all frames captured before switchtime, capture 1 after switchtime */
        {
            src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;
            unsigned int counter = 0;

            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: set_input_skip_frame "
                       "switch_time=%ld.%06ld", switchTime.tv_sec, switchTime.tv_usec);

            /* Avoid hang using the number of mmap buffers */
            while(counter < vid_source->req.count) {
                counter++;
                if (v4l2_capture(cnt, viddev, map, width, height))
                    break;

                if (vid_source->buf.timestamp.tv_sec > switchTime.tv_sec ||
                   (vid_source->buf.timestamp.tv_sec == switchTime.tv_sec &&
                    vid_source->buf.timestamp.tv_usec > switchTime.tv_usec))
                    break;

                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: got frame before "
                           "switch timestamp=%ld.%06ld",
                           vid_source->buf.timestamp.tv_sec,
                           vid_source->buf.timestamp.tv_usec);
            }
        }

        /* skip a few frames if needed */
        for (i = 1; i < skip; i++)
            v4l2_capture(cnt, viddev, map, width, height);
    } else {
        /* No round robin - we only adjust picture controls */
        v4l2_picture_controls(cnt, viddev);
    }
}

static void v4l2_device_close(struct video_dev *viddev)
{
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(vid_source, VIDIOC_STREAMOFF, &type);
    close(vid_source->fd_device);
    vid_source->fd_device = -1;
}

static void v4l2_device_cleanup(struct video_dev *viddev)
{
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;

    if (vid_source->buffers) {
        unsigned int i;

        for (i = 0; i < vid_source->req.count; i++)
            munmap(vid_source->buffers[i].ptr, vid_source->buffers[i].size);

        free(vid_source->buffers);
        vid_source->buffers = NULL;
    }

    free(vid_source->controls);
    vid_source->controls = NULL;

    free(vid_source);
    viddev->v4l2_private = NULL;
}

#endif /* HAVE_V4L2 */

void v4l2_mutex_init(void)
{
    int chk_v4l2;
#ifdef HAVE_V4L2
    pthread_mutex_init(&v4l2_mutex, NULL);
    chk_v4l2 = 0;
#else
    chk_v4l2 = 1;
#endif // HAVE_V4L2
    if (chk_v4l2 == 1) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: V4L2 is not enabled");
}

void v4l2_mutex_destroy(void)
{
    int chk_v4l2;
#ifdef HAVE_V4L2
    pthread_mutex_destroy(&v4l2_mutex);
    chk_v4l2 = 0;
#else
    chk_v4l2 = 1;
#endif // HAVE_V4L2
    if (chk_v4l2 == 1) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: V4L2 is not enabled");
}

int v4l2_start(struct context *cnt)
{
#ifdef HAVE_V4L2

    struct config *conf = &cnt->conf;
    int fd_device = -1;
    struct video_dev *dev;

    int width, height, input, norm, tuner_number;
    unsigned long frequency;

    /*
     * We use width and height from conf in this function. They will be assigned
     * to width and height in imgs here, and cap_width and cap_height in
     * rotate_data won't be set until in rotate_init.
     * Motion requires that width and height is a multiple of 8 so we check
     * for this first.
     */
    if (conf->width % 8) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: config image width (%d) is not modulo 8", conf->width);
        return -2;
    }

    if (conf->height % 8) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: config image height (%d) is not modulo 8", conf->height);
        return -2;
    }
    // The H264(21) is not supported via this module so change it to yuv420(17)
    if (conf->v4l2_palette == 21 ) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: H264(21) format not supported via videodevice.  Changing to default palette");
        conf->v4l2_palette = 17;
    }

    width = conf->width;
    height = conf->height;
    input = conf->input;
    norm = conf->norm;
    frequency = conf->frequency;
    tuner_number = conf->tuner_number;

    pthread_mutex_lock(&v4l2_mutex);

    /*
     * Transfer width and height from conf to imgs. The imgs values are the ones
     * that is used internally in Motion. That way, setting width and height via
     * http remote control won't screw things up.
     */
    cnt->imgs.width = width;
    cnt->imgs.height = height;

    /*
     * First we walk through the already discovered video devices to see
     * if we have already setup the same device before. If this is the case
     * the device is a Round Robin device and we set the basic settings
     * and return the file descriptor.
     */
    dev = viddevs;
    while (dev) {
        if (!strcmp(conf->video_device, dev->video_device)) {
            dev->usage_count++;
            cnt->imgs.type = dev->v4l_fmt;
            switch (cnt->imgs.type) {
            case VIDEO_PALETTE_GREY:
                cnt->imgs.motionsize = width * height;
                cnt->imgs.size = width * height;
                break;
            case VIDEO_PALETTE_YUYV:
            case VIDEO_PALETTE_RGB24:
            case VIDEO_PALETTE_YUV422:
                cnt->imgs.type = VIDEO_PALETTE_YUV420P;
            case VIDEO_PALETTE_YUV420P:
                cnt->imgs.motionsize = width * height;
                cnt->imgs.size = (width * height * 3) / 2;
                break;
            }
            pthread_mutex_unlock(&v4l2_mutex);
            return dev->fd_device;
        }
        dev = dev->next;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using videodevice %s and input %d",
               conf->video_device, conf->input);

    dev = mymalloc(sizeof(struct video_dev));

    dev->video_device = conf->video_device;

    fd_device = open(dev->video_device, O_RDWR);

    if (fd_device < 0) {
        MOTION_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed to open video device %s",
                   conf->video_device);
        free(dev);
        pthread_mutex_unlock(&v4l2_mutex);
        return -1;
    }

    pthread_mutexattr_init(&dev->attr);
    pthread_mutex_init(&dev->mutex, &dev->attr);

    dev->usage_count = 1;
    dev->fd_device = fd_device;
    dev->input = input;
    dev->norm = norm;
    dev->height = height;
    dev->width = width;
    dev->freq = frequency;
    dev->tuner_number = tuner_number;

    /*
     * We set brightness, contrast, saturation and hue = 0 so that they only get
     * set if the config is not zero.
     */
    dev->brightness = 0;
    dev->contrast = 0;
    dev->saturation = 0;
    dev->hue = 0;
    /* -1 is don't modify, (0 is a valid value) */
    dev->power_line_frequency = -1;
    dev->owner = -1;
    dev->v4l_fmt = VIDEO_PALETTE_YUV420P;
    dev->fps = 0;

    dev->v4l2 = 1;
    if (!v4l2_device_init(cnt, dev, width, height, input, norm, frequency, tuner_number)) {
        /*
         * Restore width & height before test with v4l
         * because could be changed in v4l2_device_init().
         */
        dev->width = width;
        dev->height = height;
        dev->v4l2 = 0;
    }

    if (dev->v4l2 != 0) {
        /* Update width & height because could be changed in v4l2_device_init(). */
        width = dev->width;
        height = dev->height;
        cnt->imgs.width = width;
        cnt->imgs.height = height;
    }

    cnt->imgs.type = dev->v4l_fmt;

    switch (cnt->imgs.type) {
    case VIDEO_PALETTE_GREY:
        cnt->imgs.size = width * height;
        cnt->imgs.motionsize = width * height;
        break;
    case VIDEO_PALETTE_YUYV:
    case VIDEO_PALETTE_RGB24:
    case VIDEO_PALETTE_YUV422:
        cnt->imgs.type = VIDEO_PALETTE_YUV420P;
    case VIDEO_PALETTE_YUV420P:
        cnt->imgs.size = (width * height * 3) / 2;
        cnt->imgs.motionsize = width * height;
        break;
    }

    /* Insert into linked list. */
    dev->next = viddevs;
    viddevs = dev;

    pthread_mutex_unlock(&v4l2_mutex);

    return fd_device;
#else
    if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: V4L2 is not enabled.");
    return -1;
#endif // HAVE_V4l2
}

void v4l2_cleanup(struct context *cnt)
{
#ifdef HAVE_V4L2

    struct video_dev *dev = viddevs;
    struct video_dev *prev = NULL;

        /* Cleanup the v4l2 part */
    pthread_mutex_lock(&v4l2_mutex);
    while (dev) {
        if (dev->fd_device == cnt->video_dev)
            break;
        prev = dev;
        dev = dev->next;
    }
    pthread_mutex_unlock(&v4l2_mutex);

    /* Set it as closed in thread context. */
    cnt->video_dev = -1;

    if (dev == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: Unable to find video device");
        return;
    }

    if (--dev->usage_count == 0) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Closing video device %s",
                   dev->video_device);
        if (dev->v4l2) {
            v4l2_device_close(dev);
            v4l2_device_cleanup(dev);
        } else {
            close(dev->fd_device);
            munmap(viddevs->v4l_buffers[0], dev->size_map);
        }

        dev->fd_device = -1;
        pthread_mutex_lock(&v4l2_mutex);
        /* Remove from list */
        if (prev == NULL)
            viddevs = dev->next;
        else
            prev->next = dev->next;
        pthread_mutex_unlock(&v4l2_mutex);

        pthread_mutexattr_destroy(&dev->attr);
        pthread_mutex_destroy(&dev->mutex);
        free(dev);
    } else {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Still %d users of video device %s, so we don't close it now",
                   dev->usage_count, dev->video_device);
        /*
         * There is still at least one thread using this device
         * If we own it, release it.
         */
        if (dev->owner == cnt->threadnr) {
            dev->frames = 0;
            dev->owner = -1;
            pthread_mutex_unlock(&dev->mutex);
        }
    }
#else
    if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: V4L2 is not enabled.");
#endif // HAVE_V4L2
}

int v4l2_next(struct context *cnt, unsigned char *map)
{
#ifdef HAVE_V4L2
    int ret = -2;
    struct config *conf = &cnt->conf;
    struct video_dev *dev;
    int width, height;

    /* NOTE: Since this is a capture, we need to use capture dimensions. */
    width = cnt->rotate_data.cap_width;
    height = cnt->rotate_data.cap_height;

    pthread_mutex_lock(&v4l2_mutex);
    dev = viddevs;
    while (dev) {
        if (dev->fd_device == cnt->video_dev)
            break;
        dev = dev->next;
    }
    pthread_mutex_unlock(&v4l2_mutex);

    if (dev == NULL)
        return V4L2_FATAL_ERROR;

    if (dev->owner != cnt->threadnr) {
        pthread_mutex_lock(&dev->mutex);
        dev->owner = cnt->threadnr;
        dev->frames = conf->roundrobin_frames;
    }

    if (dev->v4l2) {
        v4l2_set_input(cnt, dev, map, width, height, conf);
        ret = v4l2_capture(cnt, dev, map, width, height);
    }

    if (--dev->frames <= 0) {
        dev->owner = -1;
        dev->frames = 0;
        pthread_mutex_unlock(&dev->mutex);
    }

    /* Rotate the image as specified. */
    if (cnt->rotate_data.degrees > 0 || cnt->rotate_data.axis != FLIP_TYPE_NONE)
        rotate_map(cnt, map);

    return ret;
#else
    if (!cnt || !map) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, "%s: V4L2 is not enabled.");
    return -1;
#endif // HAVE_V4L2

}

