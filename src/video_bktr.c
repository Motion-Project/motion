/*    video_bktr.c
 *
 *    BSD Video stream functions for motion.
 *    Copyright 2004 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

/* For rotation */
#include "translate.h"
#include "rotate.h"     /* Already includes motion.h */
#include "video_common.h"
#include "video_bktr.h"

#ifdef HAVE_BKTR

#include <sys/mman.h>

#if defined(HAVE_DEV_IC_BT8XX_H)
    #include <dev/ic/bt8xx.h>
#endif

#if defined(HAVE_DEV_BKTR_IOCTL_METEOR_H)
    #include <dev/bktr/ioctl_meteor.h>
#endif

#if defined(HAVE_DEV_BKTR_IOCTL_BT848_H)
    #include <dev/bktr/ioctl_bt848.h>
#endif

#define array_elem(x) (sizeof(x) / sizeof((x)[0]))

#define BKTR_PAL                   0
#define BKTR_NTSC                  1
#define BKTR_SECAM                 2
#define BKTR_PAL_NC                3

#define BKTR_PAL_HEIGHT          576
#define BKTR_SECAM_HEIGHT        576
#define BKTR_NTSC_HEIGHT         480

#define BKTR_IN_COMPOSITE          0
#define BKTR_IN_TV                 1
#define BKTR_IN_COMPOSITE2         2
#define BKTR_IN_SVIDEO             3

#define BKTR_NORM_DEFAULT      BT848_IFORM_F_AUTO
#define BKTR_NORM_PAL          BT848_IFORM_F_PALBDGHI
#define BKTR_NORM_NTSC         BT848_IFORM_F_NTSCM
#define BKTR_NORM_SECAM        BT848_IFORM_F_SECAM


volatile sig_atomic_t bktr_frame_waiting;

static pthread_mutex_t bktr_mutex;

static struct video_dev *viddevs = NULL;

static void catchsignal(int sig ATTRIBUTE_UNUSED)
{
    bktr_frame_waiting++;
}

static int bktr_set_hue(int viddev, int new_hue) {
    signed char ioctlval = new_hue;

    if (ioctl(viddev, METEORSHUE, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORSHUE Error setting hue [%d]"),new_hue);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    return ioctlval;
}

static int bktr_get_hue(int viddev , int *hue) {
    signed char ioctlval;

    if (ioctl(viddev, METEORGHUE, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("METEORGHUE Error getting hue"));
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    *hue = ioctlval;
    return ioctlval;
}

static int bktr_set_saturation(int viddev, int new_saturation) {
    unsigned char ioctlval= new_saturation;

    if (ioctl(viddev, METEORSCSAT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORSCSAT Error setting saturation [%d]"), new_saturation);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    return ioctlval;
}

static int bktr_get_saturation(int viddev , int *saturation) {
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGCSAT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORGCSAT Error getting saturation"));
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    *saturation = ioctlval;
    return ioctlval;
}

static int bktr_set_contrast(int viddev, int new_contrast) {
    unsigned char ioctlval = new_contrast;

    if (ioctl(viddev, METEORSCONT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORSCONT Error setting contrast [%d]"), new_contrast);
        return 0;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    return ioctlval;
}

static int bktr_get_contrast(int viddev, int *contrast) {
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGCONT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORGCONT Error getting contrast"));
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    *contrast = ioctlval;
    return ioctlval;
}

static int bktr_set_brightness(int viddev, int new_bright)
{
    unsigned char ioctlval = new_bright;

    if (ioctl(viddev, METEORSBRIG, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORSBRIG  brightness [%d]"), new_bright);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    return ioctlval;
}

static int bktr_get_brightness(int viddev, int *brightness)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGBRIG, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("METEORGBRIG  getting brightness"));
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d]"), ioctlval);

    *brightness = ioctlval;
    return ioctlval;
}

static int bktr_set_freq(struct video_dev *viddev, unsigned long freq)
{
    int tuner_fd = viddev->bktr_fdtuner;
    int old_audio;

    MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO,_("Not implemented"));
    return 0;

    /* HACK maybe not need it , but seems that is needed to mute before changing frequency */
    if (ioctl(tuner_fd, BT848_GAUDIO, &old_audio) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "BT848_GAUDIO");
        return -1;
    }

    if (ioctl(tuner_fd, TVTUNER_SETFREQ, &freq) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "Tuning (TVTUNER_SETFREQ) failed, ",
                   "freq [%lu]", freq);
        return -1;
    }

    old_audio &= AUDIO_MUTE;
    if (old_audio) {
        old_audio = AUDIO_MUTE;
        if (ioctl(tuner_fd , BT848_SAUDIO, &old_audio) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "BT848_SAUDIO %i",
                       old_audio);
            return -1;
        }
    }

    return 0;
}

static int bktr_set_input_device(struct video_dev *viddev, unsigned input)
{
    int actport;
    int portdata[] = { METEOR_INPUT_DEV0, METEOR_INPUT_DEV1,
                       METEOR_INPUT_DEV2, METEOR_INPUT_DEV3,
                       METEOR_INPUT_DEV_SVIDEO  };

    if (input >= array_elem(portdata)) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,_("Device Input %d out of range (0-4)"), input);
        return -1;
    }

    actport = portdata[ input ];
    if (ioctl(viddev->fd_device, METEORSINPUT, &actport) < 0) {
        if (input != BKTR_IN_COMPOSITE) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("METEORSINPUT %d invalid -Trying composite %d")
                , input, BKTR_IN_COMPOSITE);
            input = BKTR_IN_COMPOSITE;
            actport = portdata[ input ];
            if (ioctl(viddev->fd_device, METEORSINPUT, &actport) < 0) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "METEORSINPUT %d init", input);
                return -1;
            }
        } else {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "METEORSINPUT %d init",
                       input);
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "to [%d]", input);

    return input;
}

static int bktr_set_input_format(struct video_dev *viddev, unsigned newformat)
{
    int input_format[] = { BKTR_NORM_PAL, BKTR_NORM_NTSC, BKTR_NORM_SECAM, BKTR_NORM_DEFAULT};
    int format;

    if (newformat >= array_elem(input_format)) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "Input format %d out of range (0-2)",
                   newformat);
        return -1;
    }

    format = input_format[newformat];

    if (ioctl(viddev->fd_device, BT848SFMT, &format) < 0) {
        MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO
            ,_("BT848SFMT, Couldn't set the input format, try again with default"));
        format = BKTR_NORM_DEFAULT;
        newformat = 3;

        if (ioctl(viddev->fd_device, BT848SFMT, &format) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("BT848SFMT, Couldn't set the input format either default"));
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "to %d", newformat);

    return newformat;
}

static int bktr_set_geometry(struct video_dev *viddev, int width, int height)
{
    struct meteor_geomet geom;
    int h_max;

    geom.columns = width;
    geom.rows = height;
    geom.oformat = METEOR_GEO_YUV_422 | METEOR_GEO_YUV_12;

    /* FIXME:  These seem to be problematic on whether they work the same on bktr...*/
    /* FIXME:  Best result in testing was only a BW picture with no color.*/
    /*
    The following are supported pixel format types:
    METEOR_GEO_RGB16        16-bit RGB
    METEOR_GEO_RGB24        24-bit RGB in 32 bits
    METEOR_GEO_YUV_PACKED   16-bit 4:2:2 YUV
    METEOR_GEO_YUV_PLANAR   16-bit 4:2:2 YUV
    METEOR_GEO_YUV_UNSIGNED unsigned UV
    METEOR_GEO_YUV_422
    METEOR_GEO_YUV_12
    METEOR_GEO_YUV_9
    */
    viddev->pixfmt_src = geom.oformat;

    switch (viddev->norm) {
    case BKTR_PAL:
        h_max = BKTR_PAL_HEIGHT;
        break;
    case BKTR_NTSC:
        h_max = BKTR_NTSC_HEIGHT;
        break;
    case BKTR_SECAM:
        h_max = BKTR_SECAM_HEIGHT;
        break;
    default:
        h_max = BKTR_PAL_HEIGHT;
    }

    if (height <= h_max / 2)
        geom.oformat |= METEOR_GEO_EVEN_ONLY;

    geom.frames = 1;

    if (ioctl(viddev->fd_device, METEORSETGEO, &geom) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("Couldn't set the geometry"));
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("to [%d/%d] Norm %d")
        ,width, height, viddev->norm);

    return 0;
}

static void bktr_picture_controls(struct context *cnt, struct video_dev *viddev) {

    int dev = viddev->fd_device;
    int indx_user, retcd;
    struct vdev_usrctrl_ctx *usritem;

    if (!cnt->vdev->update_parms) return;

    retcd = vid_parms_parse(cnt);
    if (retcd < 0) return;

    for (indx_user=0; indx_user<cnt->vdev->usrctrl_count; indx_user++){
        usritem=&cnt->vdev->usrctrl_array[indx_user];
        if (!strcasecmp(usritem->ctrl_name,"contrast")) bktr_set_contrast(dev,usritem->ctrl_value);
        if (!strcasecmp(usritem->ctrl_name,"hue")) bktr_set_hue(dev,usritem->ctrl_value);
        if (!strcasecmp(usritem->ctrl_name,"brightness")) bktr_set_brightness(dev,usritem->ctrl_value);
        if (!strcasecmp(usritem->ctrl_name,"saturation")) bktr_set_saturation(dev,usritem->ctrl_value);
    }

    cnt->vdev->update_parms = FALSE;

}

static unsigned char *bktr_device_init(struct video_dev *viddev, int width, int height,
                                unsigned input, unsigned norm, unsigned long freq)
{
    int dev_bktr = viddev->fd_device;
    struct sigaction act, old;
    //int dev_tunner = viddev->bktr_fdtuner;
    /* to ensure that all device will be support the capture mode
      _TODO_ : Autodected the best capture mode .
    */
    int dummy = 1;
    //    int pixelformat = BSD_VIDFMT_I420;

    void *map;

    /* If we have choose the tuner is needed to setup the frequency. */
    if ((viddev->bktr_tuner != NULL) && (input == BKTR_IN_TV)) {
        if (!freq) {
            MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
                ,_("Not valid Frequency [%lu] for Source input [%i]"), freq, input);
            return NULL;
        } else if (bktr_set_freq(viddev, freq) == -1) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Frequency [%lu] Source input [%i]"), freq, input);
            return NULL;
        }
    }

    /* FIXME if we set as input tuner , we need to set option for tuner not for bktr */
    if ((dummy = bktr_set_input_device(viddev, input)) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("set input [%d]"), input);
        return NULL;
    }

    viddev->input = dummy;

    if ((dummy = bktr_set_input_format(viddev, norm)) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("set input format [%d]"), norm);
        return NULL;
    }

    viddev->norm = dummy;

    if (bktr_set_geometry(viddev, width, height) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("set geometry [%d]x[%d]"), width, height);
        return NULL;
    }

    if (freq) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO
            ,_("Frequency set (no implemented yet"));
    }

    /*
     * Set capture mode and capture buffers
     * That is the buffer size for capture images ,
     * so is dependent of color space of input format / FIXME
     */
     /* FIXME:  These seem to be problematic on whether they work the same on bktr...*/
     /* FIXME:  Spec says some of these should be 3/2 but testing indicates it is too small.*/
    switch (viddev->pixfmt_src) {
    case METEOR_GEO_RGB16:
        /*FALLTHROUGH*/
    case METEOR_GEO_RGB24:
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Sizing buffer to 3x"));
        viddev->bktr_bufsize = ((width * height * 3) * sizeof(unsigned char));
    case METEOR_GEO_YUV_PACKED:
        /*FALLTHROUGH*/
    case METEOR_GEO_YUV_PLANAR:
        /*FALLTHROUGH*/
    case METEOR_GEO_YUV_422:
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Sizing buffer to 3/2x"));
        viddev->bktr_bufsize = (((width * height * 3 / 2)) * sizeof(unsigned char));
        break;
    case METEOR_GEO_YUV_9:
        /*FALLTHROUGH*/
    case METEOR_GEO_YUV_12:
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Sizing buffer to 3x"));
        viddev->bktr_bufsize = ((width * height * 3) * sizeof(unsigned char));
        break;
    default:
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Sizing buffer to 3/2x"));
        viddev->bktr_bufsize = (((width * height * 3 / 2)) * sizeof(unsigned char));
    }

    map = mmap((caddr_t)0, viddev->bktr_bufsize, PROT_READ|PROT_WRITE, MAP_SHARED,
               dev_bktr, (off_t)0);

    if (map == MAP_FAILED) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO,_("mmap failed"));
        return NULL;
    }

    /* FIXME double buffer */
    if (0) {
        viddev->bktr_maxbuffer = 2;
        viddev->bktr_buffers[0] = map;
        viddev->bktr_buffers[1] = (unsigned char *)map + 0; /* 0 is not valid just a test */
        //viddev->bktr_buffers[1] = map+vid_buf.offsets[1];
    } else {
        viddev->bktr_buffers[0] = map;
        viddev->bktr_maxbuffer = 1;
    }

    viddev->bktr_curbuffer = 0;

    /* Clear the buffer */
    if (ioctl(dev_bktr, BT848SCBUF, &dummy) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "BT848SCBUF");
        return NULL;
    }

    /* Signal handler to know when data is ready to be read() */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = catchsignal;
    sigaction(SIGUSR2, &act, &old);

    dummy = SIGUSR2;

    //viddev->bktr_method = METEOR_CAP_CONTINOUS;
    //viddev->bktr_method = METEOR_CAP_SINGLE;

    if ((viddev->bktr_method == METEOR_CAP_CONTINOUS) &&
        (ioctl(dev_bktr, METEORSSIGNAL, &dummy) < 0)) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "METEORSSIGNAL");

        viddev->bktr_method = METEOR_CAP_SINGLE;

        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->bktr_method) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                ,_("METEORCAPTUR using single method Error capturing"));
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("METEORCAPTUR using single method Error capturing"));
        }
    } else {
        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->bktr_method) < 0) {
            viddev->bktr_method = METEOR_CAP_SINGLE;

            if (ioctl(dev_bktr, METEORCAPTUR, &viddev->bktr_method) < 0) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
                    ,_("METEORCAPTUR using single method Error capturing"));
            }
        }
    }

    if (viddev->bktr_method == METEOR_CAP_CONTINOUS)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "METEORCAPTUR METEOR_CAP_CONTINOUS");
    else
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "METEORCAPTUR METEOR_CAP_SINGLE");

    SLEEP(1, 0);

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "HUE [%d]",
               bktr_get_hue(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "SATURATION [%d]",
               bktr_get_saturation(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "BRIGHTNESS [%d]",
               bktr_get_brightness(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "CONTRAST [%d]",
               bktr_get_contrast(dev_bktr, &dummy));

    return map;
}

/**
 * bktr_capture fetches a video frame from a v4l device
 * Parameters:
 *     viddev     Pointer to struct containing video device handle
 *     map        Pointer to the buffer in which the function puts the new image
 *     width      Width of image in pixels
 *     height     Height of image in pixels
 *
 * Returns
 *     0          Success
 *    -1          Fatal error
 *     1          Non fatal error (not implemented)
 */
static int bktr_capture(struct video_dev *viddev, unsigned char *map, int width, int height) {
    int dev_bktr = viddev->fd_device;
    unsigned char *cap_map = NULL;
    unsigned char *common_buffer;
    int single = METEOR_CAP_SINGLE;
    sigset_t set, old;


    /* ONLY MMAP method is used to Capture */

    /*
     * Allocates a new mmap buffer
     * Block signals during IOCTL
     */
    sigemptyset (&set);
    sigaddset (&set, SIGCHLD);
    sigaddset (&set, SIGALRM);
    sigaddset (&set, SIGUSR1);
    sigaddset (&set, SIGTERM);
    sigaddset (&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, &old);
    cap_map = viddev->bktr_buffers[viddev->bktr_curbuffer];

    viddev->bktr_curbuffer++;
    if (viddev->bktr_curbuffer >= viddev->bktr_maxbuffer)
        viddev->bktr_curbuffer = 0;

    /* Capture */

    if (viddev->bktr_method == METEOR_CAP_CONTINOUS) {
        if (bktr_frame_waiting)
            bktr_frame_waiting = 0;

    } else if (ioctl(dev_bktr, METEORCAPTUR, &single) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO
            ,_("Error capturing using single method"));
        sigprocmask(SIG_UNBLOCK, &old, NULL);
        return -1;
    }

    /* Undo the signal blocking */
    pthread_sigmask(SIG_UNBLOCK, &old, NULL);
    /*
    METEOR_GEO_RGB16        16-bit RGB
    METEOR_GEO_RGB24        24-bit RGB in 32 bits
    METEOR_GEO_YUV_PACKED   16-bit 4:2:2 YUV
    METEOR_GEO_YUV_PLANAR   16-bit 4:2:2 YUV
    METEOR_GEO_YUV_422
    METEOR_GEO_YUV_12
    METEOR_GEO_YUV_9
    */
    /* FIXME:  These seem to be problematic on whether they work the same on bktr...*/
    switch (viddev->pixfmt_src) {
    case METEOR_GEO_RGB16:
        /*FALLTHROUGH*/
    case METEOR_GEO_RGB24:
        vid_rgb24toyuv420p(map, cap_map, width, height);
        break;
    case METEOR_GEO_YUV_PACKED:
        /*FALLTHROUGH*/
    case METEOR_GEO_YUV_422:
        vid_yuv422to420p(map, cap_map, width, height);
        break;
    case METEOR_GEO_YUV_PLANAR:
        vid_yuv422pto420p(map, cap_map, width, height);
        break;
    case METEOR_GEO_YUV_9:
        /*FALLTHROUGH*/
    case METEOR_GEO_YUV_12:
        common_buffer = mymalloc(width * height * 3);
        vid_y10torgb24(common_buffer, cap_map, width, height, 2);
        vid_rgb24toyuv420p(map, common_buffer, width, height);
        free(common_buffer);
        break;
    default:
        memcpy(map, cap_map, ((width*height*3)/2));
    }

    return 0;
}

static void bktr_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width,
                          int height, int input, int norm, int skip, unsigned long freq)
{
    if (input != viddev->input || norm != viddev->norm || freq != viddev->frequency) {
        int dummy;
        unsigned long frequnits = freq;


        if ((dummy = bktr_set_input_device(viddev, input)) == -1)
            return;

        viddev->input = dummy;

        if ((dummy = bktr_set_input_format(viddev, norm)) == -1)
            return;

        viddev->norm = dummy;

        if ((viddev->bktr_tuner != NULL) && (viddev->input == BKTR_IN_TV) &&
            (frequnits > 0)) {
            if (bktr_set_freq(viddev, freq) == -1)
                return;
        }

        bktr_picture_controls(cnt, viddev);

        viddev->frequency = freq;

        /* skip a few frames if needed */
        for (dummy = 0; dummy < skip; dummy++)
            bktr_capture(viddev, map, width, height);
    } else {
        /* No round robin - we only adjust picture controls */
        bktr_picture_controls(cnt, viddev);
    }
}

#endif /* HAVE_BKTR */


void bktr_mutex_init(void) {
#ifdef HAVE_BKTR
    pthread_mutex_init(&bktr_mutex, NULL);
#else
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("BKTR is not enabled."));
#endif
}

void bktr_mutex_destroy(void) {
#ifdef HAVE_BKTR
    pthread_mutex_destroy(&bktr_mutex);
#else
    MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("BKTR is not enabled."));
#endif
}

void bktr_cleanup(struct context *cnt){
#ifdef HAVE_BKTR

    struct video_dev *dev = viddevs;
    struct video_dev *prev = NULL;
    int indx;

    /* Cleanup the v4l part */
    pthread_mutex_lock(&bktr_mutex);

    while (dev) {
        if (dev->fd_device == cnt->video_dev)
            break;
        prev = dev;
        dev = dev->next;
    }

    pthread_mutex_unlock(&bktr_mutex);

    /* Set it as closed in thread context. */
    cnt->video_dev = -1;

    /* free the information we collected regarding the controls */
    if (cnt->vdev != NULL){
        if (cnt->vdev->usrctrl_count > 0){
            for (indx=0;indx<cnt->vdev->usrctrl_count;indx++){
                free(cnt->vdev->usrctrl_array[indx].ctrl_name);
                cnt->vdev->usrctrl_array[indx].ctrl_name=NULL;
            }
        }
        cnt->vdev->usrctrl_count = 0;
        if (cnt->vdev->usrctrl_array != NULL){
            free(cnt->vdev->usrctrl_array);
            cnt->vdev->usrctrl_array = NULL;
        }

        free(cnt->vdev);
    }

    if (dev == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO,_("Unable to find video device"));
        return;
    }

    if (--dev->usage_count == 0) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Closing video device %s"), dev->video_device);

        if (dev->bktr_fdtuner > 0)
            close(dev->bktr_fdtuner);

        if (dev->fd_device > 0) {
            if (dev->bktr_method == METEOR_CAP_CONTINOUS) {
                dev->bktr_fdtuner = METEOR_CAP_STOP_CONT;
                ioctl(dev->fd_device, METEORCAPTUR, &dev->bktr_fdtuner);
            }
            close(dev->fd_device);
            dev->bktr_fdtuner = -1;
        }


        munmap(viddevs->bktr_buffers[0], viddevs->bktr_bufsize);
        viddevs->bktr_buffers[0] = MAP_FAILED;

        dev->fd_device = -1;
        pthread_mutex_lock(&bktr_mutex);

        /* Remove from list */
        if (prev == NULL)
            viddevs = dev->next;
        else
            prev->next = dev->next;

        pthread_mutex_unlock(&bktr_mutex);

        pthread_mutexattr_destroy(&dev->attr);
        pthread_mutex_destroy(&dev->mutex);
        free(dev);
    } else {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
            ,_("Still %d users of video device %s, so we don't close it now")
            , dev->usage_count, dev->video_device);
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
    if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("BKTR is not enabled."));
#endif

}

int bktr_start(struct context *cnt) {
#ifdef HAVE_BKTR

    struct config *conf = &cnt->conf;
    struct video_dev *dev;
    int bktr_fdtuner = -1;
    int width, height, bktr_method;
    unsigned input, norm;
    unsigned long frequency;
    int fd_device = -1;

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "[%s]", conf->video_device);

    /*
     * We use width and height from conf in this function. They will be assigned
     * to width and height in imgs here, and capture_width_norm and capture_height_norm in
     * rotate_data won't be set until in rotate_init.
     * Motion requires that width and height are multiples of 8 so we check for this.
     */
    if (conf->width % 8) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO
            ,_("config image width (%d) is not modulo 8"), conf->width);
        return -2;
    }

    if (conf->height % 8) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO
            ,_("config image height (%d) is not modulo 8"), conf->height);
        return -2;
    }

    width = conf->width;
    height = conf->height;
    input = conf->input;
    norm = conf->norm;
    frequency = conf->frequency;
    bktr_method = METEOR_CAP_CONTINOUS;

    pthread_mutex_lock(&bktr_mutex);

    /*
     * Transfer width and height from conf to imgs. The imgs values are the ones
     * that is used internally in Motion. That way, setting width and height via
     * http remote control won't screw things up.
     */
    cnt->imgs.width = width;
    cnt->imgs.height = height;

    cnt->vdev = mymalloc(sizeof(struct vdev_context));
    memset(cnt->vdev, 0, sizeof(struct vdev_context));
    cnt->vdev->usrctrl_array = NULL;
    cnt->vdev->usrctrl_count = 0;
    cnt->vdev->update_parms = TRUE;     /*Set trigger that we have updated user parameters */

    /*
     * First we walk through the already discovered video devices to see
     * if we have already setup the same device before. If this is the case
     * the device is a Round Robin device and we set the basic settings
     * and return the file descriptor.
     */
    dev = viddevs;
    while (dev) {
        if (!strcmp(conf->video_device, dev->video_device)) {
            int dummy = METEOR_CAP_STOP_CONT;
            dev->usage_count++;

            if (ioctl(dev->fd_device, METEORCAPTUR, &dummy) < 0) {
                MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("Stopping capture"));
                return -1;
            }

            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("Reusing [%s] inputs [%d,%d] Change capture"
                " method METEOR_CAP_SINGLE")
                , dev->video_device, dev->input, conf->input);

            dev->bktr_method = METEOR_CAP_SINGLE;


            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
                ,_("VIDEO_PALETTE_YUV420P setting"
                " imgs.size_norm and imgs.motionsize"));
            cnt->imgs.motionsize = width * height;
            cnt->imgs.size_norm = (width * height * 3) / 2;

            pthread_mutex_unlock(&bktr_mutex);
            return dev->fd_device; // FIXME return bktr_fdtuner ?!
        }
        dev = dev->next;
    }


    dev = mymalloc(sizeof(struct video_dev));

    fd_device = open(conf->video_device, O_RDWR|O_CLOEXEC);

    if (fd_device < 0) {
        MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("open video device %s"),
                   conf->video_device);
        free(dev);
        pthread_mutex_unlock(&bktr_mutex);
        return -1;
    }

    /* Only open tuner if conf->tuner_device has set , freq and input is 1. */
    if ((conf->tuner_device != NULL) && (frequency > 0) && (input == BKTR_IN_TV)) {
        bktr_fdtuner = open(conf->tuner_device, O_RDWR|O_CLOEXEC);
        if (bktr_fdtuner < 0) {
            MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO,_("open tuner device %s"),
                       conf->tuner_device);
            free(dev);
            pthread_mutex_unlock(&bktr_mutex);
            return -1;
        }
    }

    pthread_mutexattr_init(&dev->attr);
    pthread_mutex_init(&dev->mutex, &dev->attr);

    dev->usage_count = 1;
    dev->video_device = conf->video_device;
    dev->bktr_tuner = conf->tuner_device;
    dev->fd_device = fd_device;
    dev->bktr_fdtuner = bktr_fdtuner;
    dev->input = input;
    dev->height = height;
    dev->width = width;
    dev->frequency = frequency;
    dev->owner = -1;
    dev->bktr_method = bktr_method;

    /*
     * We set brightness, contrast, saturation and hue = 0 so that they only get
     * set if the config is not zero.
     */

    dev->owner = -1;

    /* Default palette */
    dev->pixfmt_src = METEOR_GEO_YUV_422;
    dev->bktr_curbuffer = 0;
    dev->bktr_maxbuffer = 1;

    if (!bktr_device_init(dev, width, height, input, norm, frequency)) {
        close(dev->fd_device);
        pthread_mutexattr_destroy(&dev->attr);
        pthread_mutex_destroy(&dev->mutex);
        free(dev);

        pthread_mutex_unlock(&bktr_mutex);
        return -1;
    }

    cnt->imgs.size_norm = (width * height * 3) / 2;
    cnt->imgs.motionsize = width * height;

    /* Insert into linked list */
    dev->next = viddevs;
    viddevs = dev;

    pthread_mutex_unlock(&bktr_mutex);

    return fd_device;
#else
    if (!cnt) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("BKTR is not enabled."));
    return -1;
#endif

}

int bktr_next(struct context *cnt,  struct image_data *img_data) {
#ifdef HAVE_BKTR

    struct config *conf = &cnt->conf;
    struct video_dev *dev;
    int width, height;
    int dev_bktr = cnt->video_dev;
    int ret = -1;

    /* NOTE: Since this is a capture, we need to use capture dimensions. */
    width = cnt->rotate_data.capture_width_norm;
    height = cnt->rotate_data.capture_height_norm;

    pthread_mutex_lock(&bktr_mutex);
    dev = viddevs;

    while (dev) {
        if (dev->fd_device == dev_bktr)
            break;
        dev = dev->next;
    }

    pthread_mutex_unlock(&bktr_mutex);

    if (dev == NULL) return -1;

    if (dev->owner != cnt->threadnr) {
        pthread_mutex_lock(&dev->mutex);
        dev->owner = cnt->threadnr;
        dev->frames = conf->roundrobin_frames;
    }

    bktr_set_input(cnt, dev, img_data->image_norm, width, height, conf->input, conf->norm,
                  conf->roundrobin_skip, conf->frequency);

    ret = bktr_capture(dev, img_data->image_norm, width, height);

    if (--dev->frames <= 0) {
        dev->owner = -1;
        dev->frames = 0;
        pthread_mutex_unlock(&dev->mutex);
    }

    /* Rotate the image as specified */
    rotate_map(cnt, img_data);

    return ret;
#else
    if (!cnt || !img_data) MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO,_("BKTR is not enabled."));
    return -1;
#endif

}


