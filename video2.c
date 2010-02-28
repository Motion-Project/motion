/*
 *    video2.c
 *
 *    V4L2 interface with basically JPEG decompression support and even more ...
 *    Copyright 2006 Krzysztof Blaszkowski (kb@sysmikro.com.pl)
 *              2007 Angel Carpintero (ack@telefonica.net)
 
 * Supported features and TODO
   - preferred palette is JPEG which seems to be very popular for many 640x480 usb cams
   - other supported palettes (NOT TESTED)
       V4L2_PIX_FMT_SN9C10X   (sonix)
       V4L2_PIX_FMT_SBGGR16,
       V4L2_PIX_FMT_SBGGR8,   (sonix)
       V4L2_PIX_FMT_SPCA561,
       V4L2_PIX_FMT_SGBRG8,
       V4L2_PIX_FMT_SGRBG8,
       V4L2_PIX_FMT_PAC207,
       V4L2_PIX_FMT_PJPG,
       V4L2_PIX_FMT_MJPEG,    (tested)
       V4L2_PIX_FMT_JPEG,     (tested)
       V4L2_PIX_FMT_RGB24,
       V4L2_PIX_FMT_SPCA501,
       V4L2_PIX_FMT_SPCA505,
       V4L2_PIX_FMT_SPCA508,
       V4L2_PIX_FMT_UYVY,     (tested)
       V4L2_PIX_FMT_YUV422P,
       V4L2_PIX_FMT_YUV420,   (tested)
       V4L2_PIX_FMT_YUYV      (tested)

 *  - setting tuner - NOT TESTED 
 *  - access to V4L2 device controls is missing. Partially added but requires some improvements likely.
 *  - changing resolution at run-time may not work. 
 *  - ucvideo svn r75 or above to work with MJPEG ( i.ex Logitech 5000 pro )
 
 * This work is inspired by fswebcam and current design of motion.
 * This interface has been tested with ZC0301 driver from kernel 2.6.17.3 and Labtec's usb camera (PAS202 sensor)
 
 * I'm very pleased by achieved image quality and cpu usage comparing to junky v4l1 spca5xx driver with 
 * it nonsensical kernel messy jpeg decompressor.
 * Default sensor settings used by ZC0301 driver are very reasonable choosen.
 * apparently brigthness should be controlled automatically by motion still for light compensation.
 * it can be done by adjusting ADC gain and also exposure time.

 * Kernel 2.6.27
    
 V4L2_PIX_FMT_SPCA501 v4l2_fourcc('S', '5', '0', '1')  YUYV per line 
 V4L2_PIX_FMT_SPCA505 v4l2_fourcc('S', '5', '0', '5')  YYUV per line 
 V4L2_PIX_FMT_SPCA508 v4l2_fourcc('S', '5', '0', '8')  YUVY per line 
 V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G')   8  GBGB.. RGRG.. 
 V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G')   8  GRGR.. BGBG..
 V4L2_PIX_FMT_SBGGR16 v4l2_fourcc('B', 'Y', 'R', '2')  16  BGBG.. GRGR.. 
 V4L2_PIX_FMT_SPCA561 v4l2_fourcc('S', '5', '6', '1')  compressed GBRG bayer 
 V4L2_PIX_FMT_PJPG    v4l2_fourcc('P', 'J', 'P', 'G')  Pixart 73xx JPEG
 V4L2_PIX_FMT_PAC207  v4l2_fourcc('P', '2', '0', '7')  compressed BGGR bayer 


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

 let's go :)
*/

#if !defined(WITHOUT_V4L) && defined(MOTION_V4L2)

#include "motion.h"
#include "video.h"

#ifdef MOTION_V4L2_OLD
// Seems that is needed for some system
#include <linux/time.h>
#include <linux/videodev2.h>
#endif

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define s32 signed int

#define MMAP_BUFFERS 4
#define MIN_MMAP_BUFFERS 2

#ifndef V4L2_PIX_FMT_SBGGR8
/* see http://www.siliconimaging.com/RGB%20Bayer.htm */
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



#define ZC301_V4L2_CID_DAC_MAGN       V4L2_CID_PRIVATE_BASE
#define ZC301_V4L2_CID_GREEN_BALANCE  (V4L2_CID_PRIVATE_BASE+1)

static const u32 queried_ctrls[] = {
    V4L2_CID_BRIGHTNESS,
    V4L2_CID_CONTRAST,
    V4L2_CID_SATURATION,
    V4L2_CID_HUE,

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
    int fd;
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

} src_v4l2_t;

static int xioctl(int fd, int request, void *arg) 
{
    int ret;

    do
        ret = ioctl(fd, request, arg);
    while (-1 == ret && EINTR == errno);

    return ret;
}

static int v4l2_get_capability(src_v4l2_t * vid_source) 
{
    if (xioctl(vid_source->fd, VIDIOC_QUERYCAP, &vid_source->cap) < 0) {
        motion_log(LOG_ERR, 0, "%s: Not a V4L2 device?", __FUNCTION__);
        return -1;
    }

    motion_log(LOG_INFO, 0, "%s: \n------------------------\ncap.driver: \"%s\"\n"
               "cap.card: \"%s\"\n"
               "cap.bus_info: \"%s\"\n"
               "cap.capabilities=0x%08X\n------------------------", __FUNCTION__,           
               vid_source->cap.driver, vid_source->cap.card, vid_source->cap.bus_info, 
               vid_source->cap.capabilities);

    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        motion_log(LOG_INFO, 0, "- VIDEO_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
        motion_log(LOG_INFO, 0, "- VIDEO_OUTPUT");
    if (vid_source->cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)
        motion_log(LOG_INFO, 0, "- VIDEO_OVERLAY");
    if (vid_source->cap.capabilities & V4L2_CAP_VBI_CAPTURE)
        motion_log(LOG_INFO, 0, "- VBI_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_VBI_OUTPUT)
        motion_log(LOG_INFO, 0, "- VBI_OUTPUT");
    if (vid_source->cap.capabilities & V4L2_CAP_RDS_CAPTURE)
        motion_log(LOG_INFO, 0, "- RDS_CAPTURE");
    if (vid_source->cap.capabilities & V4L2_CAP_TUNER)
        motion_log(LOG_INFO, 0, "- TUNER");
    if (vid_source->cap.capabilities & V4L2_CAP_AUDIO)
        motion_log(LOG_INFO, 0, "- AUDIO");
    if (vid_source->cap.capabilities & V4L2_CAP_READWRITE)
        motion_log(LOG_INFO, 0, "- READWRITE");
    if (vid_source->cap.capabilities & V4L2_CAP_ASYNCIO)
        motion_log(LOG_INFO, 0, "- ASYNCIO");
    if (vid_source->cap.capabilities & V4L2_CAP_STREAMING)
        motion_log(LOG_INFO, 0, "- STREAMING");
    if (vid_source->cap.capabilities & V4L2_CAP_TIMEPERFRAME)
        motion_log(LOG_INFO, 0, "- TIMEPERFRAME");

    if (!vid_source->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        motion_log(LOG_ERR, 0, "%s: Device does not support capturing.", __FUNCTION__);
        return -1;
    }

    return 0;
}

static int v4l2_select_input(struct config *conf, struct video_dev *viddev, src_v4l2_t * vid_source, 
                             int in, int norm, unsigned long freq_, int tuner_number ATTRIBUTE_UNUSED)
{
    struct v4l2_input input;
    struct v4l2_standard standard;
    v4l2_std_id std_id;

    /*if (in == IN_DEFAULT)
        in = IN_TV;
    */

    /* Set the input. */
    memset(&input, 0, sizeof (input));
    if (in == IN_DEFAULT)
        input.index = IN_TV;
    else input.index = in;

    if (xioctl(vid_source->fd, VIDIOC_ENUMINPUT, &input) == -1) {
        motion_log(LOG_ERR, 1, "%s: Unable to query input %d. VIDIOC_ENUMINPUT", 
                   __FUNCTION__, input.index);
        return -1;
    }

    if (debug_level >= CAMERA_VIDEO)
        motion_log(LOG_INFO, 0, "%s: name = \"%s\", type 0x%08X, status %08x", __FUNCTION__, 
                   input.name, input.type, input.status);

    if ((input.type & V4L2_INPUT_TYPE_TUNER) && (debug_level >= CAMERA_VIDEO))
        motion_log(LOG_INFO, 0, "- TUNER");

    if ((input.type & V4L2_INPUT_TYPE_CAMERA) && (debug_level >= CAMERA_VIDEO)) 
        motion_log(LOG_INFO, 0, "- CAMERA");

    if (xioctl(vid_source->fd, VIDIOC_S_INPUT, &input.index) == -1) {
        motion_log(LOG_ERR, 1, "%s: Error selecting input %d VIDIOC_S_INPUT", 
                   __FUNCTION__, input.index);
        return -1;
    }

    viddev->input = conf->input = in;
    
    /* Set video standard usually webcams doesn't support the ioctl or return V4L2_STD_UNKNOWN */
    if (xioctl(vid_source->fd, VIDIOC_G_STD, &std_id) == -1) {
        if (debug_level >= CAMERA_VIDEO)
            motion_log(LOG_INFO, 0, "%s: Device doesn't support VIDIOC_G_STD", __FUNCTION__);
        norm = std_id = 0;    // V4L2_STD_UNKNOWN = 0
    }

    if (std_id) {
        memset(&standard, 0, sizeof(standard));
        standard.index = 0;

        while (xioctl(vid_source->fd, VIDIOC_ENUMSTD, &standard) == 0) {
            if ((standard.id & std_id)  && (debug_level >= CAMERA_VIDEO)) 
                motion_log(LOG_INFO, 0, "%s: - video standard %s", __FUNCTION__, standard.name);
            
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

        if (xioctl(vid_source->fd, VIDIOC_S_STD, &std_id) == -1) {
            motion_log(LOG_ERR, 1, "%s: Error selecting standard method %d VIDIOC_S_STD", 
                       __FUNCTION__, (int)std_id);
        }    
    
        if (debug_level >= CAMERA_VIDEO)
            motion_log(LOG_DEBUG, 0, "%s: Set standard method %d", __FUNCTION__, (int)std_id);
                
    }

    viddev->norm = conf->norm = norm;

    /* If this input is attached to a tuner, set the frequency. */
    if (input.type & V4L2_INPUT_TYPE_TUNER) {
        struct v4l2_tuner tuner;
        struct v4l2_frequency freq;

        /* Query the tuners capabilities. */

        memset(&tuner, 0, sizeof(struct v4l2_tuner));
        tuner.index = input.tuner;

        if (xioctl(vid_source->fd, VIDIOC_G_TUNER, &tuner) == -1) {
            motion_log(LOG_ERR, 1, "%s: tuner %d VIDIOC_G_TUNER", __FUNCTION__, tuner.index);    
            return 0;
        }

        if (debug_level >= CAMERA_VIDEO)
            motion_log(LOG_DEBUG, 0, "%s: Set tuner %d", __FUNCTION__, tuner.index);

        /* Set the frequency. */
        memset(&freq, 0, sizeof(struct v4l2_frequency));
        freq.tuner = input.tuner;
        freq.type = V4L2_TUNER_ANALOG_TV;
        freq.frequency = (freq_ / 1000) * 16;

        if (xioctl(vid_source->fd, VIDIOC_S_FREQUENCY, &freq) == -1) {
            motion_log(LOG_ERR, 1, "%s: freq %ul VIDIOC_S_FREQUENCY", __FUNCTION__, freq.frequency);
            return 0;
        }

        viddev->freq = conf->frequency = freq_;

        if (debug_level >= CAMERA_VIDEO)
            motion_log(LOG_DEBUG, 0, "%s: Set Frequency to %ul", __FUNCTION__, freq.frequency);
    } else {
        viddev->freq = conf->frequency = 0;
    }

    return 0;
}

static int v4l2_set_pix_format(struct context *cnt, src_v4l2_t * vid_source, int *width, int *height)
{
    struct v4l2_fmtdesc fmtd;
    int v4l2_pal;

    static const u32 supported_formats[] = {    /* higher index means better chance to be used */
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
        V4L2_PIX_FMT_YUV420,
        0
    };

    int index_format = -1;
    CLEAR(fmtd);
    fmtd.index = v4l2_pal = 0;
    fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    motion_log(LOG_INFO, 0, "%s: Supported palettes:", __FUNCTION__);

    while (xioctl(vid_source->fd, VIDIOC_ENUM_FMT, &fmtd) != -1) {

        int i;

        motion_log(LOG_INFO, 0, "%i: %c%c%c%c (%s)", v4l2_pal, fmtd.pixelformat >> 0, 
                   fmtd.pixelformat >> 8, fmtd.pixelformat >> 16, fmtd.pixelformat >> 24, fmtd.description);

        motion_log(LOG_INFO, 0, "%s: %d - %s (compressed : %d) (%#x)",
                   __FUNCTION__, fmtd.index, fmtd.description, fmtd.flags, fmtd.pixelformat);

        for (i = 0; supported_formats[i]; i++)
            if (supported_formats[i] == fmtd.pixelformat) {
                if (cnt->conf.v4l2_palette == i) {
                    index_format = cnt->conf.v4l2_palette;
                    motion_log(LOG_INFO, 0, "Selected palette %c%c%c%c", fmtd.pixelformat >> 0, 
                               fmtd.pixelformat >> 8, fmtd.pixelformat >> 16, fmtd.pixelformat >> 24);
                    i = sizeof(supported_formats)/sizeof(u32);
                    break;
                }
                index_format = i;
            }

        /* Chosen our selected palette, break from while */
        if (index_format == cnt->conf.v4l2_palette && index_format >= 0)
            break;
        
        CLEAR(fmtd);
        fmtd.index = ++v4l2_pal;
        fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

    if (index_format >= 0) {
        
        u32 pixformat = supported_formats[index_format];

        CLEAR(vid_source->dst_fmt);
        CLEAR(vid_source->src_fmt);
        vid_source->dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vid_source->dst_fmt.fmt.pix.width = *width;
        vid_source->dst_fmt.fmt.pix.height = *height;
        vid_source->dst_fmt.fmt.pix.pixelformat = pixformat;
        vid_source->dst_fmt.fmt.pix.field = V4L2_FIELD_ANY;

        if (xioctl(vid_source->fd, VIDIOC_TRY_FMT, &vid_source->dst_fmt) != -1 && 
            vid_source->dst_fmt.fmt.pix.pixelformat == pixformat) {
            motion_log(LOG_INFO, 0, "%s: index_format %d Testing palette %c%c%c%c (%dx%d)", 
                       __FUNCTION__, index_format, pixformat >> 0, pixformat >> 8, 
                       pixformat >> 16, pixformat >> 24, *width, *height);

            if (vid_source->dst_fmt.fmt.pix.width != (unsigned int) *width || 
                vid_source->dst_fmt.fmt.pix.height != (unsigned int) *height) {

                motion_log(LOG_INFO, 0, "%s: Adjusting resolution from %ix%i to %ix%i.", 
                           __FUNCTION__, *width, *height, vid_source->dst_fmt.fmt.pix.width, 
                           vid_source->dst_fmt.fmt.pix.height);

                *width = vid_source->dst_fmt.fmt.pix.width;
                *height = vid_source->dst_fmt.fmt.pix.height;
            }

            if (xioctl(vid_source->fd, VIDIOC_S_FMT, &vid_source->dst_fmt) == -1) {
                motion_log(LOG_ERR, 1, "%s: Error setting pixel format.\nVIDIOC_S_FMT: ",
                           __FUNCTION__);
                return -1;
            }

            motion_log(LOG_INFO, 0, "%s: Using palette %c%c%c%c (%dx%d) bytesperlines %d sizeimage "
                       "%d colorspace %08x", __FUNCTION__, pixformat >> 0, pixformat >> 8, pixformat >> 16, 
                       pixformat >> 24, *width, *height, vid_source->dst_fmt.fmt.pix.bytesperline, 
                       vid_source->dst_fmt.fmt.pix.sizeimage, vid_source->dst_fmt.fmt.pix.colorspace);

            return 0;
        }

        motion_log(LOG_ERR, 1, "%s: VIDIOC_TRY_FMT failed for format %c%c%c%c ", __FUNCTION__,
                   pixformat >> 0, pixformat >> 8, pixformat >> 16, pixformat >> 24);

        return -1;
    }

    motion_log(LOG_ERR, 0, "%s: Unable to find a compatible palette format.", __FUNCTION__);
    
    return -1;
}

#if 0
static void v4l2_set_fps(src_v4l2_t * vid_source) {
    struct v4l2_streamparm* setfps;

    setfps = (struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
    memset(setfps, 0, sizeof(struct v4l2_streamparm));
    setfpvid_source->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfpvid_source->parm.capture.timeperframe.numerator = 1;
    setfpvid_source->parm.capture.timeperframe.denominator = vid_source->fps;

    if (xioctl(vid_source->fd, VIDIOC_S_PARM, setfps) == -1) 
        motion_log(LOG_ERR, 1, "%s: v4l2_set_fps VIDIOC_S_PARM", __FUNCTION__);
    

}
#endif

static int v4l2_set_mmap(src_v4l2_t * vid_source)
{
    enum v4l2_buf_type type;
    u32 buffer_index;

    /* Does the device support streaming? */
    if (!vid_source->cap.capabilities && V4L2_CAP_STREAMING)
        return -1;

    memset(&vid_source->req, 0, sizeof(struct v4l2_requestbuffers));

    vid_source->req.count = MMAP_BUFFERS;
    vid_source->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_source->req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(vid_source->fd, VIDIOC_REQBUFS, &vid_source->req) == -1) {
        motion_log(LOG_ERR, 1, "%s: Error requesting buffers %d for memory map. VIDIOC_REQBUFS",
                   __FUNCTION__, vid_source->req.count);
        return -1;
    }

    motion_log(LOG_DEBUG, 0, "%s: mmap information: frames=%d", __FUNCTION__, vid_source->req.count);

    if (vid_source->req.count < MIN_MMAP_BUFFERS) {
        motion_log(LOG_ERR, 1, "%s: Insufficient buffer memory %d < MIN_MMAP_BUFFERS.",
                   __FUNCTION__, vid_source->req.count);
        return -1;
    }

    vid_source->buffers = calloc(vid_source->req.count, sizeof(video_buff));

    if (!vid_source->buffers) {
        motion_log(LOG_ERR, 1, "%s: Out of memory.", __FUNCTION__);
        return -1;
    }

    for (buffer_index = 0; buffer_index < vid_source->req.count; buffer_index++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(struct v4l2_buffer));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buffer_index;

        if (xioctl(vid_source->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            motion_log(LOG_ERR, 1, "%s: Error querying buffer %i\nVIDIOC_QUERYBUF: ",
                       __FUNCTION__, buffer_index);
            free(vid_source->buffers);
            return -1;
        }

        vid_source->buffers[buffer_index].size = buf.length;
        vid_source->buffers[buffer_index].ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
                                                     MAP_SHARED, vid_source->fd, buf.m.offset);

        if (vid_source->buffers[buffer_index].ptr == MAP_FAILED) {
            motion_log(LOG_ERR, 1, "%s: Error mapping buffer %i mmap", 
                       __FUNCTION__, buffer_index);
            free(vid_source->buffers);
            return -1;
        }

        motion_log(LOG_DEBUG, 0, "%s: %i length=%d", __FUNCTION__, buffer_index, buf.length);
    }

    for (buffer_index = 0; buffer_index < vid_source->req.count; buffer_index++) {
        memset(&vid_source->buf, 0, sizeof(struct v4l2_buffer));

        vid_source->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vid_source->buf.memory = V4L2_MEMORY_MMAP;
        vid_source->buf.index = buffer_index;

        if (xioctl(vid_source->fd, VIDIOC_QBUF, &vid_source->buf) == -1) {
            motion_log(LOG_ERR, 1, "%s: VIDIOC_QBUF", __FUNCTION__);
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(vid_source->fd, VIDIOC_STREAMON, &type) == -1) {
        motion_log(LOG_ERR, 1, "%s: Error starting stream. VIDIOC_STREAMON", 
                   __FUNCTION__);
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
        if (xioctl(vid_source->fd, VIDIOC_QUERYCTRL, &queryctrl))
            continue;

        count++;
        vid_source->ctrl_flags |= 1 << i;
    }

    if (count) {
        struct v4l2_queryctrl *ctrl = vid_source->controls = calloc(count, sizeof(struct v4l2_queryctrl));

        if (!ctrl) {
            motion_log(LOG_ERR, 1, "%s: Insufficient buffer memory.", __FUNCTION__);
            return -1;
        }

        for (i = 0; queried_ctrls[i]; i++) {
            if (vid_source->ctrl_flags & (1 << i)) {
                struct v4l2_control control;

                queryctrl.id = queried_ctrls[i];
                if (xioctl(vid_source->fd, VIDIOC_QUERYCTRL, &queryctrl))
                    continue;

                memcpy(ctrl, &queryctrl, sizeof(struct v4l2_queryctrl));

                motion_log(LOG_INFO, 0, "%s: found control 0x%08x, \"%s\", range %d,%d %s", 
                           __FUNCTION__,ctrl->id, ctrl->name, ctrl->minimum, ctrl->maximum,
                           ctrl->flags & V4L2_CTRL_FLAG_DISABLED ? "!DISABLED!" : "");

                memset(&control, 0, sizeof (control));
                control.id = queried_ctrls[i];
                xioctl(vid_source->fd, VIDIOC_G_CTRL, &control);
                motion_log(LOG_INFO, 0, "%s: \t\"%s\", default %d, current %d", __FUNCTION__,
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
                    ret = xioctl(vid_source->fd, VIDIOC_S_CTRL, &control);
                    break;

                case V4L2_CTRL_TYPE_BOOLEAN:
                    value = control.value = value ? 1 : 0;
                    ret = xioctl(vid_source->fd, VIDIOC_S_CTRL, &control);
                    break;

                default:
                    motion_log(LOG_ERR, 0, "%s: control type not supported yet", __FUNCTION__);
                    return -1;
                }

                if (debug_level >= CAMERA_VIDEO)
                    motion_log(LOG_INFO, 0, "%s: setting control \"%s\" to %d (ret %d %s) %s", 
                               __FUNCTION__, ctrl->name, value, ret, ret ? strerror(errno) : "",
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

/* public functions */

unsigned char *v4l2_start(struct context *cnt, struct video_dev *viddev, int width, int height,
              int input, int norm, unsigned long freq, int tuner_number)
{
    src_v4l2_t *vid_source;

    /* Allocate memory for the state structure. */
    if (!(vid_source = calloc(sizeof(src_v4l2_t), 1))) {
        motion_log(LOG_ERR, 1, "%s: Out of memory.", __FUNCTION__);
        goto err;
    }

    viddev->v4l2_private = vid_source;
    vid_source->fd = viddev->fd;
    vid_source->fps = cnt->conf.frame_limit;
    vid_source->pframe = -1;
    struct config *conf = &cnt->conf;

    if (v4l2_get_capability(vid_source)) 
        goto err;
    
    if (v4l2_select_input(conf, viddev, vid_source, input, norm, freq, tuner_number)) 
        goto err;
    
    if (v4l2_set_pix_format(cnt, vid_source, &width, &height)) 
        goto err;
    
    if (v4l2_scan_controls(vid_source)) 
        goto err;

#if 0
    v4l2_set_fps(vid_source);
#endif
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
    if (vid_source) 
        free(vid_source);

    viddev->v4l2_private = NULL;
    viddev->v4l2 = 0;
    return NULL;
}

void v4l2_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, 
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

        v4l2_select_input(conf, viddev, (src_v4l2_t *) viddev->v4l2_private, input, norm, freq, tuner_number);

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

            if (debug_level >= CAMERA_VIDEO)
                motion_log(LOG_DEBUG, 0, "%s: set_input_skip_frame switch_time=%ld:%ld", 
                           __FUNCTION__, switchTime.tv_sec, switchTime.tv_usec);

            /* Avoid hang using the number of mmap buffers */
            while(counter < vid_source->req.count) {
                counter++;
                if (v4l2_next(cnt, viddev, map, width, height))
                    break;

                if (vid_source->buf.timestamp.tv_sec > switchTime.tv_sec || 
                   (vid_source->buf.timestamp.tv_sec == switchTime.tv_sec && 
                    vid_source->buf.timestamp.tv_usec > switchTime.tv_usec))
                    break;

                if (debug_level >= CAMERA_VIDEO)
                    motion_log(LOG_DEBUG, 0, "%s: got frame before switch timestamp=%ld:%ld", 
                               __FUNCTION__, vid_source->buf.timestamp.tv_sec, 
                               vid_source->buf.timestamp.tv_usec);
            }
        }

        /* skip a few frames if needed */
        for (i = 1; i < skip; i++)
            v4l2_next(cnt, viddev, map, width, height);
    } else {
        /* No round robin - we only adjust picture controls */
        v4l2_picture_controls(cnt, viddev);
    }
}

int v4l2_next(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height)
{
    sigset_t set, old;
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;

    if (viddev->v4l_fmt != VIDEO_PALETTE_YUV420P) 
        return V4L_FATAL_ERROR;
    
    /* Block signals during IOCTL */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, &old);

    if (debug_level == CAMERA_VIDEO) {
        motion_log(LOG_DEBUG, 0, "%s: 1) vid_source->pframe %i", 
                   __FUNCTION__, vid_source->pframe);
    }

    if (vid_source->pframe >= 0) {
        if (xioctl(vid_source->fd, VIDIOC_QBUF, &vid_source->buf) == -1) {
            motion_log(LOG_ERR, 1, "%s: VIDIOC_QBUF", __FUNCTION__);
            pthread_sigmask(SIG_UNBLOCK, &old, NULL);
            return -1;
        }
    }

    memset(&vid_source->buf, 0, sizeof(struct v4l2_buffer));

    vid_source->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_source->buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(vid_source->fd, VIDIOC_DQBUF, &vid_source->buf) == -1) {
        int ret;
        /* some drivers return EIO when there is no signal, 
           driver might dequeue an (empty) buffer despite
           returning an error, or even stop capturing.
        */
        if (errno == EIO) {
            vid_source->pframe++; 

            if ((u32)vid_source->pframe >= vid_source->req.count) 
                vid_source->pframe = 0;

             vid_source->buf.index = vid_source->pframe;
             motion_log(LOG_ERR, 1, "%s: VIDIOC_DQBUF: EIO (vid_source->pframe %d)", 
                        __FUNCTION__, vid_source->pframe);
             ret = 1;
        } else if (errno == EAGAIN) {
            motion_log(LOG_INFO, 1, "%s: VIDIOC_DQBUF: EAGAIN (vid_source->pframe %d)",
                       __FUNCTION__, vid_source->pframe);
            ret = 1;
        } else {
            motion_log(LOG_ERR, 1, "%s: VIDIOC_DQBUF", __FUNCTION__);
            ret = -1;
        }

        pthread_sigmask(SIG_UNBLOCK, &old, NULL);
        return ret;
    }

    if (debug_level == CAMERA_VIDEO) {
        motion_log(LOG_DEBUG, 0, "%s: 2) vid_source->pframe %i", 
                   __FUNCTION__, vid_source->pframe);
    }

    vid_source->pframe = vid_source->buf.index;
    vid_source->buffers[vid_source->buf.index].used = vid_source->buf.bytesused;
    vid_source->buffers[vid_source->buf.index].content_length = vid_source->buf.bytesused;

    if (debug_level == CAMERA_VIDEO) {
        motion_log(LOG_DEBUG, 0, "%s: 3) vid_source->pframe %i vid_source->buf.index %i", 
                   __FUNCTION__, vid_source->pframe, vid_source->buf.index);
        motion_log(LOG_DEBUG, 0, "%s: vid_source->buf.bytesused %i", 
                   __FUNCTION__, vid_source->buf.bytesused);
    }

    pthread_sigmask(SIG_UNBLOCK, &old, NULL);    /*undo the signal blocking */

    {
        video_buff *the_buffer = &vid_source->buffers[vid_source->buf.index];
        
        switch (vid_source->dst_fmt.fmt.pix.pixelformat) {
        case V4L2_PIX_FMT_RGB24:
            conv_rgb24toyuv420p(map, the_buffer->ptr, width, height);
            return 0;

        case V4L2_PIX_FMT_UYVY:
            conv_uyvyto420p(map, the_buffer->ptr, (unsigned)width, (unsigned)height);
            return 0;

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YUV422P:
            conv_yuv422to420p(map, the_buffer->ptr, width, height);
            return 0;

        case V4L2_PIX_FMT_YUV420:
            memcpy(map, the_buffer->ptr, viddev->v4l_bufsize);
            return 0;

        case V4L2_PIX_FMT_PJPG:            
        case V4L2_PIX_FMT_JPEG:            
        case V4L2_PIX_FMT_MJPEG:
            return mjpegtoyuv420p(map, the_buffer->ptr, width, height, 
                                  vid_source->buffers[vid_source->buf.index].content_length);
        
        /* FIXME: quick hack to allow work all bayer formats */            
        case V4L2_PIX_FMT_SBGGR16:            
        case V4L2_PIX_FMT_SGBRG8: 
        case V4L2_PIX_FMT_SGRBG8:           
        /* case V4L2_PIX_FMT_SPCA561: */            
        case V4L2_PIX_FMT_SBGGR8:    /* bayer */
            bayer2rgb24(cnt->imgs.common_buffer, the_buffer->ptr, width, height);
            conv_rgb24toyuv420p(map, cnt->imgs.common_buffer, width, height);
            return 0;

        case V4L2_PIX_FMT_SPCA561:            
        case V4L2_PIX_FMT_SN9C10X:
            sonix_decompress(map, the_buffer->ptr, width, height);
            bayer2rgb24(cnt->imgs.common_buffer, map, width, height);
            conv_rgb24toyuv420p(map, cnt->imgs.common_buffer, width, height);
            return 0;
        }
    }

    return 1;
}

void v4l2_close(struct video_dev *viddev)
{
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(vid_source->fd, VIDIOC_STREAMOFF, &type);
    close(vid_source->fd);
    vid_source->fd = -1;
}

void v4l2_cleanup(struct video_dev *viddev)
{
    src_v4l2_t *vid_source = (src_v4l2_t *) viddev->v4l2_private;

    if (vid_source->buffers) {
        unsigned int i;

        for (i = 0; i < vid_source->req.count; i++)
            munmap(vid_source->buffers[i].ptr, vid_source->buffers[i].size);

        free(vid_source->buffers);
        vid_source->buffers = NULL;
    }

    if (vid_source->controls) {
        free(vid_source->controls);
        vid_source->controls = NULL;
    }

    free(vid_source);
    viddev->v4l2_private = NULL;
}

#endif /* !WITHOUT_V4L && MOTION_V4L2 */ 
