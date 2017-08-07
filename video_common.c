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

#include "motion.h"
#include "video_common.h"
#include "video_v4l2.h"
#include "video_bktr.h"
#include "jpegutils.h"

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

#define CLAMP(x)  ((x) < 0 ? 0 : ((x) > 255) ? 255 : (x))
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
static void vid_sonix_decompress_init(code_table_t * table)
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
int vid_sonix_decompress(unsigned char *outp, unsigned char *inp, int width, int height)
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
        vid_sonix_decompress_init(table);
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
void vid_bayer2rgb24(unsigned char *dst, unsigned char *src, long int width, long int height)
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

void vid_yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
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

void vid_uyvyto420p(unsigned char *map, unsigned char *cap_map, unsigned int width, unsigned int height)
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

void vid_rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
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
int vid_mjpegtoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height, unsigned int size)
{
    unsigned char *ptr_buffer;
    size_t soi_pos = 0;
    int ret = 0;

    ptr_buffer = memmem(cap_map, size, "\xff\xd8", 2);
    if (ptr_buffer == NULL) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "Corrupt image ... continue");
        return 1;
    }
    /**
     Some cameras are sending multiple SOIs in the buffer.
     Move the pointer to the last SOI in the buffer and proceed.
    */
    while (ptr_buffer != NULL && ((size - soi_pos - 1) > 2) ){
        soi_pos = ptr_buffer - cap_map;
        ptr_buffer = memmem(cap_map + soi_pos + 1, size - soi_pos - 1, "\xff\xd8", 2);
    }

    if (soi_pos != 0){
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "SOI position adjusted by %d bytes.", soi_pos);
    }

    memmove(cap_map, cap_map + soi_pos, size - soi_pos);
    size -= soi_pos;

    ret = jpgutl_decode_jpeg(cap_map,size, width, height, map);

    if (ret == -1) {
        MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO, "Corrupt image ... continue");
        ret = 1;
    }
    return ret;
}

void vid_y10torgb24(unsigned char *map, unsigned char *cap_map, int width, int height, int shift)
{
    /* Source code: raw2rgbpnm project */
    /* url: http://salottisipuli.retiisi.org.uk/cgi-bin/gitweb.cgi?p=~sailus/raw2rgbpnm.git;a=summary */

    /* bpp - bits per pixel */
    /* bpp: 'Pixels are stored in 16-bit words with unused high bits padded with 0' */
    /* url: https://linuxtv.org/downloads/v4l-dvb-apis/V4L2-PIX-FMT-Y12.html */
    /* url: https://linuxtv.org/downloads/v4l-dvb-apis/V4L2-PIX-FMT-Y10.html */

    int src_size[2] = {width,height};
    int bpp = 16;
    unsigned int src_stride = (src_size[0] * bpp) / 8;
    unsigned int rgb_stride = src_size[0] * 3;
    int a = 0;
    int src_x = 0, src_y = 0;
    int dst_x = 0, dst_y = 0;

    for (src_y = 0, dst_y = 0; dst_y < src_size[1]; src_y++, dst_y++) {
        for (src_x = 0, dst_x = 0; dst_x < src_size[0]; src_x++, dst_x++) {
            a = (cap_map[src_y*src_stride + src_x*2+0] |
                (cap_map[src_y*src_stride + src_x*2+1] << 8)) >> shift;
            map[dst_y*rgb_stride+3*dst_x+0] = a;
            map[dst_y*rgb_stride+3*dst_x+1] = a;
            map[dst_y*rgb_stride+3*dst_x+2] = a;
        }
    }
}

void vid_greytoyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
    /* This is a adaptation of the rgb to yuv.
     * For grey, we use just a single color
    */

    unsigned char *y, *u, *v;
    unsigned char *r;
    int i, loop;

    r = cap_map;

    y = map;
    u = y + width * height;
    v = u + (width * height) / 4;
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);

    for (loop = 0; loop < height; loop++) {
        for (i = 0; i < width; i += 2) {
            *y++ = (9796 **  r + 19235 ** r + 3736 **  r) >> 15;
            *u += ((-4784 ** r - 9437 **  r + 14221 ** r) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** r - 3277 **  r) >> 17) + 32;
            r++;

            *y++ = (9796 **  r + 19235 ** r + 3736 **  r) >> 15;
            *u += ((-4784 ** r - 9437 **  r + 14221 ** r) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** r - 3277 **  r) >> 17) + 32;
            r ++;

            u++;
            v++;
        }

        if ((loop & 1) == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }


}

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

void vid_mutex_init(void)
{
    v4l2_mutex_init();
    bktr_mutex_init();
}

void vid_mutex_destroy(void)
{
    v4l2_mutex_destroy();
    bktr_mutex_destroy();
}

void vid_close(struct context *cnt)
{

#ifdef HAVE_MMAL
    if (cnt->mmalcam) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "calling mmalcam_cleanup");
        mmalcam_cleanup(cnt->mmalcam);
        cnt->mmalcam = NULL;
        return;
    }
#endif

    if (cnt->netcam) {
        MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO, "calling netcam_cleanup");
        netcam_cleanup(cnt->netcam, 0);
        cnt->netcam = NULL;
        return;
    }

    if (cnt->camera_type == CAMERA_TYPE_V4L2) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Cleaning up V4L2 device");
        v4l2_cleanup(cnt);
        return;
    }

    if (cnt->camera_type == CAMERA_TYPE_BKTR) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Cleaning up BKTR device");
        bktr_cleanup(cnt);
        return;
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "No Camera device cleanup (MMAL, Netcam, V4L2, BKTR)");
    return;


}

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
 * - if the camera is V4L2 v4l2_start is called
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
    int dev = -1;

#ifdef HAVE_MMAL
    if (cnt->camera_type == CAMERA_TYPE_MMAL) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Opening MMAL cam");
        dev = mmalcam_start(cnt);
        if (dev < 0) {
            mmalcam_cleanup(cnt->mmalcam);
            cnt->mmalcam = NULL;
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "MMAL cam failed to open");
        }
        return dev;
    }
#endif

    if (cnt->camera_type == CAMERA_TYPE_NETCAM) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Opening Netcam");
        dev = netcam_start(cnt);
        if (dev < 0) {
            netcam_cleanup(cnt->netcam, 1);
            cnt->netcam = NULL;
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Netcam failed to open");
        }
        return dev;
    }

    if (cnt->camera_type == CAMERA_TYPE_V4L2) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Opening V4L2 device");
        dev = v4l2_start(cnt);
        if (dev < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "V4L2 device failed to open");
        }
        return dev;
    }

    if (cnt->camera_type == CAMERA_TYPE_BKTR) {
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "Opening BKTR device");
        dev = bktr_start(cnt);
        if (dev < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "BKTR device failed to open");
        }
        return dev;
    }

    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "No Camera device specified (MMAL, Netcam, V4L2, BKTR)");
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

#ifdef HAVE_MMAL
     if (cnt->camera_type == CAMERA_TYPE_MMAL) {
        if (cnt->mmalcam == NULL) {
            return NETCAM_GENERAL_ERROR;
        }
        return mmalcam_next(cnt, map);
    }
#endif

    if (cnt->camera_type == CAMERA_TYPE_NETCAM) {
        if (cnt->video_dev == -1)
            return NETCAM_GENERAL_ERROR;

        return netcam_next(cnt, map);
    }

    if (cnt->camera_type == CAMERA_TYPE_V4L2) {
        return v4l2_next(cnt, map);
   }

    if (cnt->camera_type == CAMERA_TYPE_BKTR) {
        return bktr_next(cnt, map);
    }

    return -2;
}
