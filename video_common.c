/*      video_common.c
 *
 *      Video stream functions for motion.
 *      Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *                2006 by Krzysztof Blaszkowski (kb@sysmikro.com.pl)
 *                2007 by Angel Carpintero (motiondevelop@gmail.com)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

/* For rotation */
#include "rotate.h"    /* Already includes motion.h */
#include "video.h"
#include "jpegutils.h"

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

#define CLAMP(x)  ((x) < 0 ? 0 : ((x) > 255) ? 255 : (x))

typedef struct {
    int is_abs;
    int len;
    int val;
} code_table_t;

/**
 * sonix_decompress_init
 *   pre-calculates a locally stored table for efficient huffman-decoding.
 *
 *   Each entry at index x in the table represents the codeword
 *   present at the MSB of byte x.
 *
 */
static void sonix_decompress_init(code_table_t * table)
{
    int i;
    int is_abs, val, len;

    for (i = 0; i < 256; i++) {
        is_abs = 0;
        val = 0;
        len = 0;
        if ((i & 0x80) == 0) {
            /* code 0 */
            val = 0;
            len = 1;
        } else if ((i & 0xE0) == 0x80) {
            /* code 100 */
            val = +4;
            len = 3;
        } else if ((i & 0xE0) == 0xA0) {
            /* code 101 */
            val = -4;
            len = 3;
        } else if ((i & 0xF0) == 0xD0) {
            /* code 1101 */
            val = +11;
            len = 4;
        } else if ((i & 0xF0) == 0xF0) {
            /* code 1111 */
            val = -11;
            len = 4;
        } else if ((i & 0xF8) == 0xC8) {
            /* code 11001 */
            val = +20;
            len = 5;
        } else if ((i & 0xFC) == 0xC0) {
            /* code 110000 */
            val = -20;
            len = 6;
        } else if ((i & 0xFC) == 0xC4) {
            /* code 110001xx: unknown */
            val = 0;
            len = 8;
        } else if ((i & 0xF0) == 0xE0) {
            /* code 1110xxxx */
            is_abs = 1;
            val = (i & 0x0F) << 4;
            len = 8;
        }
        table[i].is_abs = is_abs;
        table[i].val = val;
        table[i].len = len;
    }
}

/**
 * sonix_decompress
 *      Decompresses an image encoded by a SN9C101 camera controller chip.
 *
 *   IN    width
 *         height
 *         inp     pointer to compressed frame (with header already stripped)
 *   OUT   outp    pointer to decompressed frame
 *
 *         Returns 0 if the operation was successful.
 *         Returns <0 if operation failed.
 *
 */
int sonix_decompress(unsigned char *outp, unsigned char *inp, int width, int height)
{
    int row, col;
    int val;
    int bitpos;
    unsigned char code;
    unsigned char *addr;

    /* Local storage */
    static code_table_t table[256];
    static int init_done = 0;

    if (!init_done) {
        init_done = 1;
        sonix_decompress_init(table);
        /* Do sonix_decompress_init first! */
        //return -1; // so it has been done and now fall through
    }

    bitpos = 0;
    for (row = 0; row < height; row++) {

        col = 0;

        /* First two pixels in first two rows are stored as raw 8-bit. */
        if (row < 2) {
            addr = inp + (bitpos >> 3);
            code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
            bitpos += 8;
            *outp++ = code;

            addr = inp + (bitpos >> 3);
            code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
            bitpos += 8;
            *outp++ = code;

            col += 2;
        }

        while (col < width) {
            /* Get bitcode from bitstream. */
            addr = inp + (bitpos >> 3);
            code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));

            /* Update bit position. */
            bitpos += table[code].len;

            /* Calculate pixel value. */
            val = table[code].val;
            if (!table[code].is_abs) {
                /* Value is relative to top and left pixel. */
                if (col < 2) {
                    /* Left column: relative to top pixel. */
                    val += outp[-2 * width];
                } else if (row < 2) {
                    /* Top row: relative to left pixel. */
                    val += outp[-2];
                } else {
                    /* Main area: average of left pixel and top pixel. */
                    val += (outp[-2] + outp[-2 * width]) / 2;
                }
            }

            /* Store pixel */
            *outp++ = CLAMP(val);
            col++;
        }
    }

    return 0;
}

/**
 * bayer2rgb24
 * BAYER2RGB24 ROUTINE TAKEN FROM:
 *
 * Sonix SN9C10x based webcam basic I/F routines
 * Takafumi Mizuno <taka-qce@ls-a.jp>
 *
 */
void bayer2rgb24(unsigned char *dst, unsigned char *src, long int width, long int height)
{
    long int i;
    unsigned char *rawpt, *scanpt;
    long int size;

    rawpt = src;
    scanpt = dst;
    size = width * height;

    for (i = 0; i < size; i++) {
        if (((i / width) & 1) == 0) {    // %2 changed to & 1
            if ((i & 1) == 0) {
                /* B */
                if ((i > width) && ((i % width) > 0)) {
                    *scanpt++ = *rawpt;     /* B */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) +
                                *(rawpt + width) + *(rawpt - width)) / 4;    /* G */
                    *scanpt++ = (*(rawpt - width - 1) + *(rawpt - width + 1) +
                                *(rawpt + width - 1) + *(rawpt + width + 1)) / 4;    /* R */
                } else {
                    /* First line or left column. */
                    *scanpt++ = *rawpt;     /* B */
                    *scanpt++ = (*(rawpt + 1) + *(rawpt + width)) / 2;    /* G */
                    *scanpt++ = *(rawpt + width + 1);       /* R */
                }
            } else {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1))) {
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;  /* B */
                    *scanpt++ = *rawpt;    /* G */
                    *scanpt++ = (*(rawpt + width) + *(rawpt - width)) / 2;  /* R */
                } else {
                    /* First line or right column. */
                    *scanpt++ = *(rawpt - 1);       /* B */
                    *scanpt++ = *rawpt;    /* G */
                    *scanpt++ = *(rawpt + width);   /* R */
                }
            }
        } else {
            if ((i & 1) == 0) {
                /* G(R) */
                if ((i < (width * (height - 1))) && ((i % width) > 0)) {
                    *scanpt++ = (*(rawpt + width) + *(rawpt - width)) / 2;  /* B */
                    *scanpt++ = *rawpt;    /* G */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1)) / 2;  /* R */
                } else {
                    /* Bottom line or left column. */
                    *scanpt++ = *(rawpt - width);   /* B */
                    *scanpt++ = *rawpt;    /* G */
                    *scanpt++ = *(rawpt + 1);       /* R */
                }
            } else {
                /* R */
                if (i < (width * (height - 1)) && ((i % width) < (width - 1))) {
                    *scanpt++ = (*(rawpt - width - 1) + *(rawpt - width + 1) +
                                *(rawpt + width - 1) + *(rawpt + width + 1)) / 4;    /* B */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt + 1) +
                                *(rawpt - width) + *(rawpt + width)) / 4;    /* G */
                    *scanpt++ = *rawpt;     /* R */
                } else {
                    /* Bottom line or right column. */
                    *scanpt++ = *(rawpt - width - 1);       /* B */
                    *scanpt++ = (*(rawpt - 1) + *(rawpt - width)) / 2;    /* G */
                    *scanpt++ = *rawpt;     /* R */
                }
            }
        }
        rawpt++;
    }

}

/**
 * conv_yuv422to420p
 *
 *
 */
void conv_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    unsigned char *src, *dest, *src2, *dest2;
    int i, j;

    /* Create the Y plane. */
    src = cap_map;
    dest = map;
    for (i = width * height; i > 0; i--) {
        *dest++ = *src;
        src += 2;
    }
    /* Create U and V planes. */
    src = cap_map + 1;
    src2 = cap_map + width * 2 + 1;
    dest = map + width * height;
    dest2 = dest + (width * height) / 4;
    for (i = height / 2; i > 0; i--) {
        for (j = width / 2; j > 0; j--) {
            *dest = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dest++;
            *dest2 = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dest2++;
        }
        src += width * 2;
        src2 += width * 2;
    }
}

/**
 * conv_uyvyto420p
 *
 *
 */
void conv_uyvyto420p(unsigned char *map, unsigned char *cap_map, unsigned int width, unsigned int height)
{
    uint8_t *pY = map;
    uint8_t *pU = pY + (width * height);
    uint8_t *pV = pU + (width * height) / 4;
    uint32_t uv_offset = width * 2 * sizeof(uint8_t);
    uint32_t ix, jx;

    for (ix = 0; ix < height; ix++) {
        for (jx = 0; jx < width; jx += 2) {
            uint16_t calc;

            if ((ix&1) == 0) {
                calc = *cap_map;
                calc += *(cap_map + uv_offset);
                calc /= 2;
                *pU++ = (uint8_t) calc;
            }

            cap_map++;
            *pY++ = *cap_map++;

            if ((ix&1) == 0) {
                calc = *cap_map;
                calc += *(cap_map + uv_offset);
                calc /= 2;
                *pV++ = (uint8_t) calc;
            }

            cap_map++;
            *pY++ = *cap_map++;
        }
    }
}

/**
 * conv_rgb24toyuv420p
 *
 *
 */
void conv_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
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
            *v += ((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32;
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

/**
 * mjpegtoyuv420p
 *
 * Return values
 *  -1 on fatal error
 *  0  on success
 *  2  if jpeg lib threw a "corrupt jpeg data" warning.
 *     in this case, "a damaged output image is likely."
 */
int mjpegtoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height, unsigned int size)
{
    int ret = decode_jpeg_raw(cap_map, size, 0, 420, width, height,
                map,
                map + (width * height),
                map + (width * height) + (width * height) / 4);

    if (ret == 1) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: Corrupt image ... continue");
        ret = 2;
    }
    return ret;
}

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MIN2(x, y) ((x) < (y) ? (x) : (y))

/* Constants used by auto brightness feature
 * Defined as constant to make it easier for people to tweak code for a
 * difficult camera.
 * The experience gained from people could help improving the feature without
 * adding too many new options.
 * AUTOBRIGHT_HYSTERESIS sets the minimum the light intensity must change before
 * we adjust brigtness.
 * AUTOBRIGHTS_DAMPER damps the speed with which we adjust the brightness
 * When the brightness changes a lot we step in large steps and as we approach the
 * target value we slow down to avoid overshoot and oscillations. If the camera
 * adjusts too slowly decrease the DAMPER value. If the camera oscillates try
 * increasing the DAMPER value. DAMPER must be minimum 1.
 * MAX and MIN are the max and min values of brightness setting we will send to
 * the camera device.
 */
#define AUTOBRIGHT_HYSTERESIS 10
#define AUTOBRIGHT_DAMPER 5
#define AUTOBRIGHT_MAX 255
#define AUTOBRIGHT_MIN 0

/**
 * vid_do_autobright
 */
int vid_do_autobright(struct context *cnt, struct video_dev *viddev)
{

    int brightness_window_high;
    int brightness_window_low;
    int brightness_target;
    int i, j = 0, avg = 0, step = 0;
    unsigned char *image = cnt->imgs.image_virgin; /* Or cnt->current_image ? */

    int make_change = 0;

    if (cnt->conf.brightness)
        brightness_target = cnt->conf.brightness;
    else
        brightness_target = 128;

    brightness_window_high = MIN2(brightness_target + AUTOBRIGHT_HYSTERESIS, 255);
    brightness_window_low = MAX2(brightness_target - AUTOBRIGHT_HYSTERESIS, 1);

    for (i = 0; i < cnt->imgs.motionsize; i += 101) {
        avg += image[i];
        j++;
    }
    avg = avg / j;

    /* Average is above window - turn down brightness - go for the target. */
    if (avg > brightness_window_high) {
        step = MIN2((avg - brightness_target) / AUTOBRIGHT_DAMPER + 1, viddev->brightness - AUTOBRIGHT_MIN);

        if (viddev->brightness > step + 1 - AUTOBRIGHT_MIN) {
            viddev->brightness -= step;
            make_change = 1;
        }
    } else if (avg < brightness_window_low) {
        /* Average is below window - turn up brightness - go for the target. */
        step = MIN2((brightness_target - avg) / AUTOBRIGHT_DAMPER + 1, AUTOBRIGHT_MAX - viddev->brightness);

        if (viddev->brightness < AUTOBRIGHT_MAX - step) {
            viddev->brightness += step;
            make_change = 1;
        }
    }

    return make_change;
}

/*****************************************************************************
    Wrappers calling the actual capture routines
 *****************************************************************************/

#ifndef WITHOUT_V4L
/*
 * Big lock for vid_start to ensure exclusive access to viddevs while adding
 * devices during initialization of each thread.
 */
static pthread_mutex_t vid_mutex;

/*
 * Here we setup the viddevs structure which is used globally in the vid_*
 * functions.
 */
static struct video_dev *viddevs = NULL;

/**
 * vid_init
 *
 * Called from motion.c at the very beginning before setting up the threads.
 * Function prepares the vid_mutex.
 */
void vid_init(void)
{
    pthread_mutex_init(&vid_mutex, NULL);
}

/**
 * vid_cleanup
 *
 * vid_cleanup is called from motion.c when Motion is stopped or restarted.
 */
void vid_cleanup(void)
{
    pthread_mutex_destroy(&vid_mutex);
}

#endif    /* WITHOUT_V4L */

/**
 * vid_close
 *
 * vid_close is called from motion.c when a Motion thread is stopped or restarted.
 */
void vid_close(struct context *cnt)
{
#ifndef WITHOUT_V4L
    struct video_dev *dev = viddevs;
    struct video_dev *prev = NULL;
#endif /* WITHOUT_V4L */

    /* Cleanup the netcam part */
    if (cnt->netcam) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "%s: calling netcam_cleanup");
        netcam_cleanup(cnt->netcam, 0);
        cnt->netcam = NULL;
        return;
    }

#ifndef WITHOUT_V4L

    /* Cleanup the v4l part */
    pthread_mutex_lock(&vid_mutex);
    while (dev) {
        if (dev->fd == cnt->video_dev)
            break;
        prev = dev;
        dev = dev->next;
    }
    pthread_mutex_unlock(&vid_mutex);

    /* Set it as closed in thread context. */
    cnt->video_dev = -1;

    if (dev == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: Unable to find video device");
        return;
    }

    if (--dev->usage_count == 0) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Closing video device %s",
                   dev->video_device);
#ifdef MOTION_V4L2
        if (dev->v4l2) {
            v4l2_close(dev);
            v4l2_cleanup(dev);
        } else {
#endif
            close(dev->fd);
            munmap(viddevs->v4l_buffers[0], dev->size_map);
#ifdef MOTION_V4L2
        }
#endif
        dev->fd = -1;
        pthread_mutex_lock(&vid_mutex);
        /* Remove from list */
        if (prev == NULL)
            viddevs = dev->next;
        else
            prev->next = dev->next;
        pthread_mutex_unlock(&vid_mutex);

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
#endif /* !WITHOUT_V4L */
}

#ifndef WITHOUT_V4L

/**
 * vid_v4lx_start
 *
 * Called from vid_start setup the V4L/V4L2 capture device
 * The function does the following:
 *
 * - Setup basic V4L/V4L2 properties incl palette incl setting
 * - Open the device
 * - Returns the device number.
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *
 * "Global" variable
 *     viddevs    The viddevs struct is "global" within the context of video.c
 *                and used in functions vid_*.
 *     vid_mutex  Mutex needed to handle exclusive access to the viddevs struct when
 *                each thread adds a new video device during startup calling vid_start
 *
 * Returns
 *     device number
 *     -1 if failed to open device.
 *     -3 image dimensions are not modulo 8
 */
static int vid_v4lx_start(struct context *cnt)
{
    struct config *conf = &cnt->conf;
    int fd = -1;
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
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: config image width (%d) is not modulo 8",
                   conf->width);
        return -3;
    }

    if (conf->height % 8) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "%s: config image height (%d) is not modulo 8",
                   conf->height);
        return -3;
    }

    width = conf->width;
    height = conf->height;
    input = conf->input;
    norm = conf->norm;
    frequency = conf->frequency;
    tuner_number = conf->tuner_number;

    pthread_mutex_lock(&vid_mutex);

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
            pthread_mutex_unlock(&vid_mutex);
            return dev->fd;
        }
        dev = dev->next;
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using videodevice %s and input %d",
               conf->video_device, conf->input);

    dev = mymalloc(sizeof(struct video_dev));

    dev->video_device = conf->video_device;

    fd = open(dev->video_device, O_RDWR);

    if (fd < 0) {
        MOTION_LOG(ALR, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed to open video device %s",
                   conf->video_device);
        free(dev);
        pthread_mutex_unlock(&vid_mutex);
        return -1;
    }

    pthread_mutexattr_init(&dev->attr);
    pthread_mutex_init(&dev->mutex, &dev->attr);

    dev->usage_count = 1;
    dev->fd = fd;
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
#ifdef MOTION_V4L2
    /* First lets try V4L2 and if it's not supported V4L1. */

    dev->v4l2 = 1;

    if (!v4l2_start(cnt, dev, width, height, input, norm, frequency, tuner_number)) {
        /*
         * Restore width & height before test with v4l
         * because could be changed in v4l2_start().
         */
        dev->width = width;
        dev->height = height;
#endif

#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))      
        if (!v4l_start(dev, width, height, input, norm, frequency, tuner_number)) {
            close(dev->fd);
            pthread_mutexattr_destroy(&dev->attr);
            pthread_mutex_destroy(&dev->mutex);
            free(dev);

            pthread_mutex_unlock(&vid_mutex);
            return -1;
        }
#endif

#ifdef MOTION_V4L2
        dev->v4l2 = 0;
    }
#endif
    if (dev->v4l2 == 0) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using V4L1");
    } else {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Using V4L2");
        /* Update width & height because could be changed in v4l2_start(). */
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

    pthread_mutex_unlock(&vid_mutex);

    return fd;
}
#endif /* !WITHOUT_V4L */

/**
 * vid_start
 *
 * vid_start setup the capture device. This will be either a V4L device or a netcam.
 * The function does the following:
 * - If the camera is a netcam - netcam_start is called and function returns
 * - Width and height are checked for valid value (multiple of 8)
 * - Copy the config height and width to the imgs struct. Note that height and width are
 *   only copied to the from the conf struct to the imgs struct during program startup
 *   The width and height can no later be changed via http remote control as this would
 *   require major re-memory allocations of all image buffers.
 *
 * - if the camera is V4L/V4L2 vid_v4lx_start is called
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *
 * Returns
 *     device number
 *     -1 if failed to open device.
 *     -3 image dimensions are not modulo 8 
 */
int vid_start(struct context *cnt)
{
    struct config *conf = &cnt->conf;
    int dev = -1;

    if (conf->netcam_url) {
        dev = netcam_start(cnt);
        if (dev < 0) {
            netcam_cleanup(cnt->netcam, 1);
            cnt->netcam = NULL;
        }
    }
#ifdef WITHOUT_V4L
    else
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "%s: You must setup netcam_url");
#else
    else
        dev = vid_v4lx_start(cnt);
#endif    /*WITHOUT_V4L */

    return dev;
}

/**
 * vid_next
 *
 * vid_next fetches a video frame from a either v4l device or netcam
 *
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *     map        Pointer to the buffer in which the function puts the new image
 *
 * Global variable
 *     viddevs    The viddevs struct is "global" within the context of video.c
 *                and used in functions vid_*.
 * Returns
 *     0                        Success
 *    -1                        Fatal V4L error
 *    -2                        Fatal Netcam error
 *    Positive numbers...
 *    with bit 0 set            Non fatal V4L error (copy grey image and discard this image)
 *    with bit 1 set            Non fatal Netcam error
 */
int vid_next(struct context *cnt, unsigned char *map)
{
    int ret = -2;
    struct config *conf = &cnt->conf;

    if (conf->netcam_url) {
        if (cnt->video_dev == -1)
            return NETCAM_GENERAL_ERROR;

        return netcam_next(cnt, map);
    }
#ifndef WITHOUT_V4L
    /*
     * We start a new block so we can make declarations without breaking
     * gcc 2.95 or older.
     */
    {
        struct video_dev *dev;
        int width, height;

        /* NOTE: Since this is a capture, we need to use capture dimensions. */
        width = cnt->rotate_data.cap_width;
        height = cnt->rotate_data.cap_height;

        pthread_mutex_lock(&vid_mutex);
        dev = viddevs;
        while (dev) {
            if (dev->fd == cnt->video_dev)
                break;
            dev = dev->next;
        }
        pthread_mutex_unlock(&vid_mutex);

        if (dev == NULL)
            return V4L_FATAL_ERROR;

        if (dev->owner != cnt->threadnr) {
            pthread_mutex_lock(&dev->mutex);
            dev->owner = cnt->threadnr;
            dev->frames = conf->roundrobin_frames;
        }
#ifdef MOTION_V4L2
        if (dev->v4l2) {
            v4l2_set_input(cnt, dev, map, width, height, conf);
            ret = v4l2_next(cnt, dev, map, width, height);
        } else {
#endif
#if defined(HAVE_LINUX_VIDEODEV_H) && (!defined(WITHOUT_V4L))           
            v4l_set_input(cnt, dev, map, width, height, conf);
            ret = v4l_next(dev, map, width, height);
#endif            
#ifdef MOTION_V4L2
        }
#endif
        if (--dev->frames <= 0) {
            dev->owner = -1;
            dev->frames = 0;
            pthread_mutex_unlock(&dev->mutex);
        }

        /* Rotate the image as specified. */
        if (cnt->rotate_data.degrees > 0)
            rotate_map(cnt, map);

    }
#endif  /*WITHOUT_V4L */
    return ret;
}
