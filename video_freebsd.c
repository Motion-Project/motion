/*    video_freebsd.c
 *
 *    BSD Video stream functions for motion.
 *    Copyright 2004 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

/* For rotation */
#include "rotate.h"     /* Already includes motion.h */
#include "video_freebsd.h"

#ifdef HAVE_BKTR

#include <sys/mman.h>

volatile sig_atomic_t bktr_frame_waiting;

static void catchsignal(int sig)
{
    bktr_frame_waiting++;
}

static void bktr_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    unsigned char *src, *dest, *src2, *dest2;
    int i, j;

    /* Create the Y plane */
    src = cap_map;
    dest = map;
    for (i = width * height; i; i--) {
        *dest++ = *src;
        src += 2;
    }
    /* Create U and V planes */
    src = cap_map + 1;
    src2 = cap_map + width * 2 + 1;
    dest = map + width* height;
    dest2 = dest + (width * height) / 4;
    for (i = height / 2; i; i--) {
        for (j = width / 2; j; j--) {
            *dest = ((int)*src + (int)*src2) / 2;
            src += 2;
            src2 += 2;
            dest++;
            *dest2 = ((int)*src + (int)*src2) / 2;
            src += 2;
            src2 += 2;
            dest2++;
        }
        src += width * 2;
        src2 += width * 2;
    }

}

static void bktr_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    unsigned char *y, *u, *v;
    unsigned char *r, *g, *b;
    int i, loop;

    b = cap_map;
    g = b + 1;
    r = g + 1;
    y = map;
    u = y + width * height;
    v = u + (width * height) / 4;
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);

    for (loop = 0; loop < height; loop++) {
        for (i = 0; i < width; i += 2) {
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941**g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            u++;
            v++;
        }

        if ((loop & 1) == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }

}

static int bktr_set_hue(int viddev, int new_hue)
{
    signed char ioctlval = new_hue;

    if (ioctl(viddev, METEORSHUE, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSHUE Error setting hue [%d]",
                   new_hue);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    return ioctlval;
}

static int bktr_get_hue(int viddev , int *hue)
{
    signed char ioctlval;

    if (ioctl(viddev, METEORGHUE, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORGHUE Error getting hue");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    *hue = ioctlval;
    return ioctlval;
}

static int bktr_set_saturation(int viddev, int new_saturation)
{
    unsigned char ioctlval= new_saturation;

    if (ioctl(viddev, METEORSCSAT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSCSAT Error setting saturation [%d]",
                   new_saturation);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    return ioctlval;
}

static int bktr_get_saturation(int viddev , int *saturation)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGCSAT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORGCSAT Error getting saturation");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    *saturation = ioctlval;
    return ioctlval;
}

static int bktr_set_contrast(int viddev, int new_contrast)
{
    unsigned char ioctlval = new_contrast;

    if (ioctl(viddev, METEORSCONT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSCONT Error setting contrast [%d]",
                   new_contrast);
        return 0;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    return ioctlval;
}

static int bktr_get_contrast(int viddev, int *contrast)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGCONT, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORGCONT Error getting contrast");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    *contrast = ioctlval;
    return ioctlval;
}

static int bktr_set_brightness(int viddev, int new_bright)
{
    unsigned char ioctlval = new_bright;

    if (ioctl(viddev, METEORSBRIG, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSBRIG  brightness [%d]",
                   new_bright);
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    return ioctlval;
}

static int bktr_get_brightness(int viddev, int *brightness)
{
    unsigned char ioctlval;

    if (ioctl(viddev, METEORGBRIG, &ioctlval) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORGBRIG  getting brightness");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", ioctlval);

    *brightness = ioctlval;
    return ioctlval;
}

static int bktr_set_freq(struct video_dev *viddev, unsigned long freq)
{
    int tuner_fd = viddev->fd_tuner;
    int old_audio;

    MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: Not implemented");
    return 0;

    /* HACK maybe not need it , but seems that is needed to mute before changing frequency */
    if (ioctl(tuner_fd, BT848_GAUDIO, &old_audio) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: BT848_GAUDIO");
        return -1;
    }

    if (ioctl(tuner_fd, TVTUNER_SETFREQ, &freq) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Tuning (TVTUNER_SETFREQ) failed, ",
                   "freq [%lu]", freq);
        return -1;
    }

    old_audio &= AUDIO_MUTE;
    if (old_audio) {
        old_audio = AUDIO_MUTE;
        if (ioctl(tuner_fd , BT848_SAUDIO, &old_audio) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: BT848_SAUDIO %i",
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
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: Device Input %d out of range (0-4)",
                   input);
        return -1;
    }

    actport = portdata[ input ];
    if (ioctl(viddev->fd_bktr, METEORSINPUT, &actport) < 0) {
        if (input != BKTR_IN_COMPOSITE) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSINPUT %d invalid -"
                       "Trying composite %d", input, BKTR_IN_COMPOSITE);
            input = BKTR_IN_COMPOSITE;
            actport = portdata[ input ];
            if (ioctl(viddev->fd_bktr, METEORSINPUT, &actport) < 0) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSINPUT %d init", input);
                return -1;
            }
        } else {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORSINPUT %d init",
                       input);
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d]", input);

    return input;
}

static int bktr_set_input_format(struct video_dev *viddev, unsigned newformat)
{
    int input_format[] = { BKTR_NORM_PAL, BKTR_NORM_NTSC, BKTR_NORM_SECAM, BKTR_NORM_DEFAULT};
    int format;

    if (newformat >= array_elem(input_format)) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: Input format %d out of range (0-2)",
                   newformat);
        return -1;
    }

    format = input_format[newformat];

    if (ioctl(viddev->fd_bktr, BT848SFMT, &format) < 0) {
        MOTION_LOG(WRN, TYPE_VIDEO, SHOW_ERRNO, "%s: BT848SFMT, Couldn't set the input format, "
                   "try again with default");
        format = BKTR_NORM_DEFAULT;
        newformat = 3;

        if (ioctl(viddev->fd_bktr, BT848SFMT, &format) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: BT848SFMT, Couldn't set the input format "
                       "either default");
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to %d", newformat);

    return newformat;
}

static int bktr_set_geometry(struct video_dev *viddev, int width, int height)
{
    struct meteor_geomet geom;
    int h_max;

    geom.columns = width;
    geom.rows = height;
    geom.oformat = METEOR_GEO_YUV_422 | METEOR_GEO_YUV_12;

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

    if (ioctl(viddev->fd_bktr, METEORSETGEO, &geom) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Couldn't set the geometry");
        return -1;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: to [%d/%d] Norm %d",
               width, height, viddev->norm);

    return 0;
}

static void bktr_picture_controls(struct context *cnt, struct video_dev *viddev)
{
    int dev = viddev->fd_bktr;

    if ((cnt->conf.contrast) && (cnt->conf.contrast != viddev->contrast)) {
        bktr_set_contrast(dev, cnt->conf.contrast);
        viddev->contrast = cnt->conf.contrast;
    }

    if ((cnt->conf.hue) && (cnt->conf.hue != viddev->hue)) {
        bktr_set_hue(dev, cnt->conf.hue);
        viddev->hue = cnt->conf.hue;
    }

    if ((cnt->conf.brightness) &&
        (cnt->conf.brightness != viddev->brightness)) {
        bktr_set_brightness(dev, cnt->conf.brightness);
        viddev->brightness = cnt->conf.brightness;
    }

    if ((cnt->conf.saturation) &&
        (cnt->conf.saturation != viddev->saturation)) {
        bktr_set_saturation(dev, cnt->conf.saturation);
        viddev->saturation = cnt->conf.saturation;
    }
}

static unsigned char *bktr_device_init(struct video_dev *viddev, int width, int height,
                                unsigned input, unsigned norm, unsigned long freq)
{
    int dev_bktr = viddev->fd_bktr;
    struct sigaction act, old;
    //int dev_tunner = viddev->fd_tuner;
    /* to ensure that all device will be support the capture mode
      _TODO_ : Autodected the best capture mode .
    */
    int dummy = 1;
    //    int pixelformat = BSD_VIDFMT_I420;

    void *map;

    /* If we have choose the tuner is needed to setup the frequency. */
    if ((viddev->tuner_device != NULL) && (input == BKTR_IN_TV)) {
        if (!freq) {
            MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: Not valid Frequency [%lu] for "
                       "Source input [%i]", freq, input);
            return NULL;
        } else if (bktr_set_freq(viddev, freq) == -1) {
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Frequency [%lu] Source input [%i]",
                       freq, input);
            return NULL;
        }
    }

    /* FIXME if we set as input tuner , we need to set option for tuner not for bktr */
    if ((dummy = bktr_set_input_device(viddev, input)) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: set input [%d]", input);
        return NULL;
    }

    viddev->input = dummy;

    if ((dummy = bktr_set_input_format(viddev, norm)) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: set input format [%d]",
                   norm);
        return NULL;
    }

    viddev->norm = dummy;

    if (bktr_set_geometry(viddev, width, height) == -1) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: set geometry [%d]x[%d]",
                   width, height);
        return NULL;
    }

    if (freq) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO, "%s: Frequency set (no implemented yet");

    }

    /*
     * Set capture mode and capture buffers
     * That is the buffer size for capture images ,
     * so is dependent of color space of input format / FIXME
     */
    viddev->v4l_bufsize = (((width * height * 3 / 2)) * sizeof(unsigned char));
    viddev->v4l_fmt = VIDEO_PALETTE_YUV420P;


    map = mmap((caddr_t)0, viddev->v4l_bufsize, PROT_READ|PROT_WRITE, MAP_SHARED,
               dev_bktr, (off_t)0);

    if (map == MAP_FAILED) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: mmap failed");
        return NULL;
    }

    /* FIXME double buffer */
    if (0) {
        viddev->v4l_maxbuffer = 2;
        viddev->v4l_buffers[0] = map;
        viddev->v4l_buffers[1] = (unsigned char *)map + 0; /* 0 is not valid just a test */
        //viddev->v4l_buffers[1] = map+vid_buf.offsets[1];
    } else {
        viddev->v4l_buffers[0] = map;
        viddev->v4l_maxbuffer = 1;
    }

    viddev->v4l_curbuffer = 0;

    /* Clear the buffer */
    if (ioctl(dev_bktr, BT848SCBUF, &dummy) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: BT848SCBUF");
        return NULL;
    }

    /* Signal handler to know when data is ready to be read() */
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = catchsignal;
    sigaction(SIGUSR2, &act, &old);

    dummy = SIGUSR2;

    //viddev->capture_method = METEOR_CAP_CONTINOUS;
    //viddev->capture_method = METEOR_CAP_SINGLE;

    if ((viddev->capture_method == METEOR_CAP_CONTINOUS) &&
        (ioctl(dev_bktr, METEORSSIGNAL, &dummy) < 0)) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: METEORSSIGNAL");

        viddev->capture_method = METEOR_CAP_SINGLE;

        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORCAPTUR using single method "
                       "Error capturing");
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: METEORCAPTUR using single method "
                       "Error capturing");
        }
    } else {
        if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
            viddev->capture_method = METEOR_CAP_SINGLE;

            if (ioctl(dev_bktr, METEORCAPTUR, &viddev->capture_method) < 0) {
                MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: METEORCAPTUR using single method "
                           "Error capturing");
            }
        }
    }

    if (viddev->capture_method == METEOR_CAP_CONTINOUS)
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: METEORCAPTUR METEOR_CAP_CONTINOUS");
    else
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: METEORCAPTUR METEOR_CAP_SINGLE");

    // settle , sleep(1) replaced
    SLEEP(1, 0);

    /* FIXME */
    switch (viddev->v4l_fmt) {
    case VIDEO_PALETTE_YUV420P:
        viddev->v4l_bufsize = (width * height * 3) / 2;
        break;
    case VIDEO_PALETTE_YUV422:
        viddev->v4l_bufsize = (width * height * 2);
        break;
    case VIDEO_PALETTE_RGB24:
        viddev->v4l_bufsize = (width * height * 3);
        break;
    case VIDEO_PALETTE_GREY:
        viddev->v4l_bufsize = width * height;
        break;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: HUE [%d]",
               bktr_get_hue(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: SATURATION [%d]",
               bktr_get_saturation(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: BRIGHTNESS [%d]",
               bktr_get_brightness(dev_bktr, &dummy));
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: CONTRAST [%d]",
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
static int bktr_capture(struct video_dev *viddev, unsigned char *map, int width, int height)
{
    int dev_bktr = viddev->fd_bktr;
    unsigned char *cap_map = NULL;
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
    cap_map = viddev->v4l_buffers[viddev->v4l_curbuffer];

    viddev->v4l_curbuffer++;
    if (viddev->v4l_curbuffer >= viddev->v4l_maxbuffer)
        viddev->v4l_curbuffer = 0;

    /* Capture */

    if (viddev->capture_method == METEOR_CAP_CONTINOUS) {
        if (bktr_frame_waiting)
            bktr_frame_waiting = 0;

    } else if (ioctl(dev_bktr, METEORCAPTUR, &single) < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Error capturing using single method");
        sigprocmask(SIG_UNBLOCK, &old, NULL);
        return -1;
    }

    /* Undo the signal blocking */
    pthread_sigmask(SIG_UNBLOCK, &old, NULL);

    switch (viddev->v4l_fmt) {
    case VIDEO_PALETTE_RGB24:
        bktr_rgb24toyuv420p(map, cap_map, width, height);
        break;
    case VIDEO_PALETTE_YUV422:
        bktr_yuv422to420p(map, cap_map, width, height);
        break;
    default:
        memcpy(map, cap_map, viddev->v4l_bufsize);
    }

    return 0;
}

static void bktr_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width,
                          int height, unsigned input, unsigned norm, int skip, unsigned long freq)
{
    if (input != viddev->input || norm != viddev->norm || freq != viddev->freq) {
        int dummy;
        unsigned long frequnits = freq;


        if ((dummy = bktr_set_input_device(viddev, input)) == -1)
            return;

        viddev->input = dummy;

        if ((dummy = bktr_set_input_format(viddev, norm)) == -1)
            return;

        viddev->norm = dummy;

        if ((viddev->tuner_device != NULL) && (viddev->input == BKTR_IN_TV) &&
            (frequnits > 0)) {
            if (bktr_set_freq(viddev, freq) == -1)
                return;
        }

        bktr_picture_controls(cnt, viddev);

        viddev->freq = freq;

        /* skip a few frames if needed */
        for (dummy = 0; dummy < skip; dummy++)
            bktr_capture(viddev, map, width, height);
    } else {
        /* No round robin - we only adjust picture controls */
        bktr_picture_controls(cnt, viddev);
    }
}

/*
 * Big lock for vid_start to ensure exclusive access to viddevs while adding
 * devices during initialization of each thread.
 */
static pthread_mutex_t bktr_mutex;

/*
 * Here we setup the viddevs structure which is used globally in the vid_*
 * functions.
 */
static struct video_dev *viddevs = NULL;

/**
 * vid_init
 *
 * Called from motion.c at the very beginning before setting up the threads.
 * Function prepares the bktr_mutex.
 */
void vid_init(void)
{
    //rename this function to bktr_mutex_init
    pthread_mutex_init(&bktr_mutex, NULL);
}

/**
 * vid_cleanup
 *
 * vid_cleanup is called from motion.c when Motion is stopped or restarted.
 */
void vid_cleanup(void)
{
//rename this function to bktr_mutex_destroy
    pthread_mutex_destroy(&bktr_mutex);
}


void bktr_cleanup(struct context *cnt)
{
    struct video_dev *dev = viddevs;
    struct video_dev *prev = NULL;

    /* Cleanup the v4l part */
    pthread_mutex_lock(&bktr_mutex);

    while (dev) {
        if (dev->fd_bktr == cnt->video_dev)
            break;
        prev = dev;
        dev = dev->next;
    }

    pthread_mutex_unlock(&bktr_mutex);

    /* Set it as closed in thread context. */
    cnt->video_dev = -1;

    if (dev == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: Unable to find video device");
        return;
    }

    if (--dev->usage_count == 0) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Closing video device %s",
                   dev->video_device);

        if (dev->fd_tuner > 0)
            close(dev->fd_tuner);

        if (dev->fd_bktr > 0) {
            if (dev->capture_method == METEOR_CAP_CONTINOUS) {
                dev->fd_tuner = METEOR_CAP_STOP_CONT;
                ioctl(dev->fd_bktr, METEORCAPTUR, &dev->fd_tuner);
            }
            close(dev->fd_bktr);
            dev->fd_tuner = -1;
        }


        munmap(viddevs->v4l_buffers[0], viddevs->v4l_bufsize);
        viddevs->v4l_buffers[0] = MAP_FAILED;

        dev->fd_bktr = -1;
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
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Still %d users of video device %s, "
                   "so we don't close it now", dev->usage_count,
                   dev->video_device);
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

}

int bktr_start(struct context *cnt)
{
    struct config *conf = &cnt->conf;
    int fd_bktr = -1;
        struct video_dev *dev;
        int fd_tuner = -1;
        int width, height, capture_method;
        unsigned input, norm;
        unsigned long frequency;


        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: [%s]",
                   conf->video_device);

        /*
         * We use width and height from conf in this function. They will be assigned
         * to width and height in imgs here, and cap_width and cap_height in
         * rotate_data won't be set until in rotate_init.
         * Motion requires that width and height are multiples of 8 so we check for this.
         */
        if (conf->width % 8) {
            MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO,
                       "%s: config image width (%d) is not modulo 8",
                        conf->width);
            return -1;
        }

        if (conf->height % 8) {
            MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO,
                       "%s: config image height (%d) is not modulo 8",
                        conf->height);
            return -1;
        }

        width = conf->width;
        height = conf->height;
        input = conf->input;
        norm = conf->norm;
        frequency = conf->frequency;
        capture_method = METEOR_CAP_CONTINOUS;

        pthread_mutex_lock(&bktr_mutex);

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
                int dummy = METEOR_CAP_STOP_CONT;
                dev->usage_count++;
                cnt->imgs.type = dev->v4l_fmt;

                if (ioctl(dev->fd_bktr, METEORCAPTUR, &dummy) < 0) {
                    MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO, "%s Stopping capture");
                    return -1;
                }

                MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s Reusing [%s] inputs [%d,%d] Change "
                           "capture method METEOR_CAP_SINGLE", dev->video_device,
                           dev->input, conf->input);

                dev->capture_method = METEOR_CAP_SINGLE;

                switch (cnt->imgs.type) {
                case VIDEO_PALETTE_GREY:
                    cnt->imgs.motionsize = width * height;
                    cnt->imgs.size = width * height;
                    break;
                case VIDEO_PALETTE_RGB24:
                case VIDEO_PALETTE_YUV422:
                    cnt->imgs.type = VIDEO_PALETTE_YUV420P;
                case VIDEO_PALETTE_YUV420P:
                    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s VIDEO_PALETTE_YUV420P setting"
                               " imgs.size and imgs.motionsize");
                    cnt->imgs.motionsize = width * height;
                    cnt->imgs.size = (width * height * 3) / 2;
                    break;
                }

                pthread_mutex_unlock(&bktr_mutex);
                return dev->fd_bktr; // FIXME return fd_tuner ?!
            }
            dev = dev->next;
        }


        dev = mymalloc(sizeof(struct video_dev));

        fd_bktr = open(conf->video_device, O_RDWR);

        if (fd_bktr < 0) {
            MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO, "%s: open video device %s",
                       conf->video_device);
            free(dev);
            pthread_mutex_unlock(&bktr_mutex);
            return -1;
        }


        /* Only open tuner if conf->tuner_device has set , freq and input is 1. */
        if ((conf->tuner_device != NULL) && (frequency > 0) && (input == BKTR_IN_TV)) {
            fd_tuner = open(conf->tuner_device, O_RDWR);
            if (fd_tuner < 0) {
                MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO, "%s: open tuner device %s",
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
        dev->tuner_device = conf->tuner_device;
        dev->fd_bktr = fd_bktr;
        dev->fd_tuner = fd_tuner;
        dev->input = input;
        dev->height = height;
        dev->width = width;
        dev->freq = frequency;
        dev->owner = -1;
        dev->capture_method = capture_method;

        /*
         * We set brightness, contrast, saturation and hue = 0 so that they only get
         * set if the config is not zero.
         */

        dev->brightness = 0;
        dev->contrast = 0;
        dev->saturation = 0;
        dev->hue = 0;
        dev->owner = -1;

        /* Default palette */
        dev->v4l_fmt = VIDEO_PALETTE_YUV420P;
        dev->v4l_curbuffer = 0;
        dev->v4l_maxbuffer = 1;

        if (!bktr_device_init(dev, width, height, input, norm, frequency)) {
            close(dev->fd_bktr);
            pthread_mutexattr_destroy(&dev->attr);
            pthread_mutex_destroy(&dev->mutex);
            free(dev);

            pthread_mutex_unlock(&bktr_mutex);
            return -1;
        }

        cnt->imgs.type = dev->v4l_fmt;

        switch (cnt->imgs.type) {
        case VIDEO_PALETTE_GREY:
            cnt->imgs.size = width * height;
            cnt->imgs.motionsize = width * height;
            break;
        case VIDEO_PALETTE_RGB24:
        case VIDEO_PALETTE_YUV422:
            cnt->imgs.type = VIDEO_PALETTE_YUV420P;
        case VIDEO_PALETTE_YUV420P:
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: VIDEO_PALETTE_YUV420P imgs.type");
            cnt->imgs.size = (width * height * 3) / 2;
            cnt->imgs.motionsize = width * height;
            break;
        }

        /* Insert into linked list */
        dev->next = viddevs;
        viddevs = dev;

        pthread_mutex_unlock(&bktr_mutex);

    return fd_bktr;

}

int bktr_next(struct context *cnt, unsigned char *map)
{
    struct config *conf = &cnt->conf;
    int ret = -1;
    struct video_dev *dev;
    int width, height;
    int dev_bktr = cnt->video_dev;

    /* NOTE: Since this is a capture, we need to use capture dimensions. */
    width = cnt->rotate_data.cap_width;
    height = cnt->rotate_data.cap_height;

    pthread_mutex_lock(&bktr_mutex);
    dev = viddevs;

    while (dev) {
        if (dev->fd_bktr == dev_bktr)
            break;
        dev = dev->next;
    }

    pthread_mutex_unlock(&bktr_mutex);

    if (dev == NULL)
        return V4L2_FATAL_ERROR;

    if (dev->owner != cnt->threadnr) {
        pthread_mutex_lock(&dev->mutex);
        dev->owner = cnt->threadnr;
        dev->frames = conf->roundrobin_frames;
    }

    bktr_set_input(cnt, dev, map, width, height, conf->input, conf->norm,
                  conf->roundrobin_skip, conf->frequency);

    ret = bktr_capture(dev, map, width, height);

    if (--dev->frames <= 0) {
        dev->owner = -1;
        dev->frames = 0;
        pthread_mutex_unlock(&dev->mutex);
    }

    /* Rotate the image as specified */
    if (cnt->rotate_data.degrees > 0)
        rotate_map(cnt, map);

    return ret;

}



#endif /* HAVE_BKTR */

/**
 * vid_close
 *
 * vid_close is called from motion.c when a Motion thread is stopped or restarted.
 */
void vid_close(struct context *cnt)
{
    /* Cleanup the netcam part */
    if (cnt->netcam) {
        netcam_cleanup(cnt->netcam, 0);
        cnt->netcam = NULL;
        return;
    }

#ifdef HAVE_BKTR

    bktr_cleanup(cnt);


#endif /* HAVE_BKTR */
}


int vid_start(struct context *cnt)
{
    struct config *conf = &cnt->conf;
    int fd_bktr = -1;

    if (conf->netcam_url) {
        fd_bktr = netcam_start(cnt);
        if (fd_bktr < 0) {
            netcam_cleanup(cnt->netcam, 1);
            cnt->netcam = NULL;
        }
    }
#ifndef HAVE_BKTR
    else
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: You must setup netcam_url");
#else
    else {
        fd_bktr = bktr_start(cnt);
    }
#endif /* HAVE_BKTR */

    return fd_bktr;
}


int vid_next(struct context *cnt, unsigned char *map)
{
    struct config *conf = &cnt->conf;
    int ret = -1;

    if (conf->netcam_url) {
        if (cnt->video_dev == -1)
            return NETCAM_GENERAL_ERROR;

        ret = netcam_next(cnt, map);
        return ret;
    }

#ifdef HAVE_BKTR

    ret = bktr_next(cnt, map);


#endif /* HAVE_BKTR */
    return ret;
}
