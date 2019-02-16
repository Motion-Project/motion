/*
 *    rotate.c
 *
 *    Module for handling image rotation.
 *
 *    Copyright 2004-2005, Per Jonsson (per@pjd.nu)
 *
 *    This software is distributed under the GNU Public license
 *    Version 2.  See also the file 'COPYING'.
 *
 *    Image rotation is a feature of Motion that can be used when the
 *    camera is mounted upside-down or on the side. The module only
 *    supports rotation in multiples of 90 degrees. Using rotation
 *    increases the Motion CPU usage slightly.
 *
 *    Version history:
 *      v6 (29-Aug-2005) - simplified the code as Motion now requires
 *                         that width and height are multiples of 16
 *      v5 (3-Aug-2005)  - cleanup in code comments
 *                       - better adherence to coding standard
 *                       - fix for __bswap_32 macro collision
 *                       - fixed bug where initialization would be
 *                         incomplete for invalid degrees of rotation
 *                       - now uses MOTION_LOG for error reporting
 *      v4 (26-Oct-2004) - new fix for width/height from imgs/conf due to
 *                         earlier misinterpretation
 *      v3 (11-Oct-2004) - cleanup of width/height from imgs/conf
 *      v2 (26-Sep-2004) - separation of capture/internal dimensions
 *                       - speed optimization, including bswap
 *      v1 (28-Aug-2004) - initial version
 */
#include "translate.h"
#include "rotate.h"
#include <stdint.h>
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_32(x) OSSwapInt32(x)
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#define bswap_32(x) bswap32(x)
#elif defined(__OpenBSD__)
#include <sys/types.h>
#define bswap_32(x) swap32(x)
#elif defined(__NetBSD__)
#include <sys/bswap.h>
#define bswap_32(x) bswap32(x)
#else
#include <byteswap.h>
#endif

/**
 * reverse_inplace_quad
 *
 *  Reverses a block of memory in-place, 4 bytes at a time. This function
 *  requires the uint32_t type, which is 32 bits wide.
 *
 * Parameters:
 *
 *   src  - the memory block to reverse
 *   size - the size (in bytes) of the memory block
 *
 * Returns: nothing
 */
static void reverse_inplace_quad(unsigned char *src, int size)
{
    uint32_t *nsrc = (uint32_t *)src;              /* first quad */
    uint32_t *ndst = (uint32_t *)(src + size - 4); /* last quad */
    register uint32_t tmp;

    while (nsrc < ndst) {
        tmp = bswap_32(*ndst);
        *ndst-- = bswap_32(*nsrc);
        *nsrc++ = tmp;
    }
}

static void flip_inplace_horizontal(unsigned char *src, int width, int height) {
    uint8_t *nsrc, *ndst;
    register uint8_t tmp;
    int l,w;

    for(l=0; l < height/2; l++) {
        nsrc = (uint8_t *)(src + l*width);
        ndst = (uint8_t *)(src + (width*(height-l-1)));
        for(w=0; w < width; w++) {
            tmp =*ndst;
            *ndst++ = *nsrc;
            *nsrc++ = tmp;
        }
    }

}

static void flip_inplace_vertical(unsigned char *src, int width, int height)
{
    uint8_t *nsrc, *ndst;
    register uint8_t tmp;
    int l;

    for(l=0; l < height; l++) {
        nsrc = (uint8_t *)src + l*width;
        ndst = nsrc + width - 1;
        while (nsrc < ndst) {
            tmp = *ndst;
            *ndst-- = *nsrc;
            *nsrc++ = tmp;
        }
    }
}

/**
 * rot90cw
 *
 *  Performs a 90 degrees clockwise rotation of the memory block pointed to
 *  by src. The rotation is NOT performed in-place; dst must point to a
 *  receiving memory block the same size as src.
 *
 * Parameters:
 *
 *   src    - pointer to the memory block (image) to rotate clockwise
 *   dst    - where to put the rotated memory block
 *   size   - the size (in bytes) of the memory blocks (both src and dst)
 *   width  - the width of the memory block when seen as an image
 *   height - the height of the memory block when seen as an image
 *
 * Returns: nothing
 */
static void rot90cw(unsigned char *src, register unsigned char *dst, int size,
                    int width, int height)
{
    unsigned char *endp;
    register unsigned char *base;
    int j;

    endp = src + size;
    for (base = endp - width; base < endp; base++) {
        src = base;
        for (j = 0; j < height; j++, src -= width)
            *dst++ = *src;

    }
}

/**
 * rot90ccw
 *
 *  Performs a 90 degrees counterclockwise rotation of the memory block pointed
 *  to by src. The rotation is not performed in-place; dst must point to a
 *  receiving memory block the same size as src.
 *
 * Parameters:
 *
 *   src    - pointer to the memory block (image) to rotate counterclockwise
 *   dst    - where to put the rotated memory block
 *   size   - the size (in bytes) of the memory blocks (both src and dst)
 *   width  - the width of the memory block when seen as an image
 *   height - the height of the memory block when seen as an image
 *
 * Returns: nothing
 */
static inline void rot90ccw(unsigned char *src, register unsigned char *dst,
                            int size, int width, int height)
{
    unsigned char *endp;
    register unsigned char *base;
    int j;

    endp = src + size;
    dst = dst + size - 1;
    for (base = endp - width; base < endp; base++) {
        src = base;
        for (j = 0; j < height; j++, src -= width)
            *dst-- = *src;

    }
}

/**
 * rotate_init
 *
 *  Initializes rotation data - allocates memory and determines which function
 *  to use for 180 degrees rotation.
 *
 * Parameters:
 *
 *   cnt - the current thread's context structure
 *
 * Returns: nothing
 */
void rotate_init(struct context *cnt){
    int size_norm, size_high;

    /* Make sure buffer_norm isn't freed if it hasn't been allocated. */
    cnt->rotate_data.buffer_norm = NULL;
    cnt->rotate_data.buffer_high = NULL;

    /*
     * Assign the value in conf.rotate to rotate_data.degrees. This way,
     * we have a value that is safe from changes caused by motion-control.
     */
    if ((cnt->conf.rotate % 90) > 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Config option \"rotate\" not a multiple of 90: %d")
            ,cnt->conf.rotate);
        cnt->conf.rotate = 0;     /* Disable rotation. */
        cnt->rotate_data.degrees = 0; /* Force return below. */
    } else {
        cnt->rotate_data.degrees = cnt->conf.rotate % 360; /* Range: 0..359 */
    }

    if (cnt->conf.flip_axis[0]=='h') {
        cnt->rotate_data.axis = FLIP_TYPE_HORIZONTAL;
    } else if (cnt->conf.flip_axis[0]=='v') {
        cnt->rotate_data.axis = FLIP_TYPE_VERTICAL;
    } else {
        cnt->rotate_data.axis = FLIP_TYPE_NONE;
    }

    /*
     * Upon entrance to this function, imgs.width and imgs.height contain the
     * capture dimensions (as set in the configuration file, or read from a
     * netcam source).
     *
     * If rotating 90 or 270 degrees, the capture dimensions and output dimensions
     * are not the same. Capture dimensions will be contained in capture_width_norm and
     * capture_height_norm in cnt->rotate_data, while output dimensions will be contained
     * in imgs.width and imgs.height.
     */

    /* 1. Transfer capture dimensions into capture_width_norm and capture_height_norm. */
    cnt->rotate_data.capture_width_norm  = cnt->imgs.width;
    cnt->rotate_data.capture_height_norm = cnt->imgs.height;

    cnt->rotate_data.capture_width_high  = cnt->imgs.width_high;
    cnt->rotate_data.capture_height_high = cnt->imgs.height_high;

    size_norm = cnt->imgs.width * cnt->imgs.height * 3 / 2;
    size_high = cnt->imgs.width_high * cnt->imgs.height_high * 3 / 2;

    if ((cnt->rotate_data.degrees == 90) || (cnt->rotate_data.degrees == 270)) {
        /* 2. "Swap" imgs.width and imgs.height. */
        cnt->imgs.width = cnt->rotate_data.capture_height_norm;
        cnt->imgs.height = cnt->rotate_data.capture_width_norm;
        if (size_high > 0 ) {
            cnt->imgs.width_high = cnt->rotate_data.capture_height_high;
            cnt->imgs.height_high = cnt->rotate_data.capture_width_high;
        }
    }

    /*
     * If we're not rotating, let's exit once we have setup the capture dimensions
     * and output dimensions properly.
     */
    if (cnt->rotate_data.degrees == 0) return;

    /*
     * Allocate memory if rotating 90 or 270 degrees, because those rotations
     * cannot be performed in-place (they can, but it would be too slow).
     */
    if ((cnt->rotate_data.degrees == 90) || (cnt->rotate_data.degrees == 270)){
        cnt->rotate_data.buffer_norm = mymalloc(size_norm);
        if (size_high > 0 ) cnt->rotate_data.buffer_high = mymalloc(size_high);
    }

}

/**
 * rotate_deinit
 *
 *  Frees resources previously allocated by rotate_init.
 *
 * Parameters:
 *
 *   cnt - the current thread's context structure
 *
 * Returns: nothing
 */
void rotate_deinit(struct context *cnt){

    if (cnt->rotate_data.buffer_norm)
        free(cnt->rotate_data.buffer_norm);

    if (cnt->rotate_data.buffer_high)
        free(cnt->rotate_data.buffer_high);
}

/**
 * rotate_map
 *
 *  Main entry point for rotation.
 *
 * Parameters:
 *
 *   img_data- pointer to the image data to rotate
 *   cnt - the current thread's context structure
 *
 * Returns:
 *
 *   0  - success
 *   -1 - failure (shouldn't happen)
 */
int rotate_map(struct context *cnt, struct image_data *img_data){
    /*
     * The image format is YUV 4:2:0 planar, which has the pixel
     * data is divided in three parts:
     *    Y - width x height bytes
     *    U - width x height / 4 bytes
     *    V - as U
     */

    int indx, indx_max;
    int wh, wh4 = 0, w2 = 0, h2 = 0;  /* width * height, width * height / 4 etc. */
    int size, deg;
    enum FLIP_TYPE axis;
    int width, height;
    unsigned char *img;
    unsigned char *temp_buff;

    if (cnt->rotate_data.degrees == 0 && cnt->rotate_data.axis == FLIP_TYPE_NONE) return 0;

    indx = 0;
    indx_max = 0;
    if ((cnt->rotate_data.capture_width_high != 0) && (cnt->rotate_data.capture_height_high != 0)) indx_max = 1;

    while (indx <= indx_max) {
        deg = cnt->rotate_data.degrees;
        axis = cnt->rotate_data.axis;
        wh4 = 0;
        w2 = 0;
        h2 = 0;
        if (indx == 0 ){
            img = img_data->image_norm;
            width = cnt->rotate_data.capture_width_norm;
            height = cnt->rotate_data.capture_height_norm;
            temp_buff = cnt->rotate_data.buffer_norm;
        } else {
            img = img_data->image_high;
            width = cnt->rotate_data.capture_width_high;
            height = cnt->rotate_data.capture_height_high;
            temp_buff = cnt->rotate_data.buffer_high;
        }
        /*
         * Pre-calculate some stuff:
         *  wh   - size of the Y plane
         *  size - size of the entire memory block
         *  wh4  - size of the U plane, and the V plane
         *  w2   - width of the U plane, and the V plane
         *  h2   - as w2, but height instead
         */
        wh = width * height;
        size = wh * 3 / 2;
        wh4 = wh / 4;
        w2 = width / 2;
        h2 = height / 2;

        switch (axis) {
        case FLIP_TYPE_HORIZONTAL:
            flip_inplace_horizontal(img,width, height);
            flip_inplace_horizontal(img + wh, w2, h2);
            flip_inplace_horizontal(img + wh + wh4, w2, h2);
            break;
        case FLIP_TYPE_VERTICAL:
            flip_inplace_vertical(img,width, height);
            flip_inplace_vertical(img + wh, w2, h2);
            flip_inplace_vertical(img + wh + wh4, w2, h2);
            break;
        default:
            break;
        }

        switch (deg) {
        case 90:
            rot90cw(img, temp_buff, wh, width, height);
            rot90cw(img + wh, temp_buff + wh, wh4, w2, h2);
            rot90cw(img + wh + wh4, temp_buff + wh + wh4, wh4, w2, h2);
            memcpy(img, temp_buff, size);
            break;
        case 180:
            reverse_inplace_quad(img, wh);
            reverse_inplace_quad(img + wh, wh4);
            reverse_inplace_quad(img + wh + wh4, wh4);
            break;
        case 270:
            rot90ccw(img, temp_buff, wh, width, height);
            rot90ccw(img + wh, temp_buff + wh, wh4, w2, h2);
            rot90ccw(img + wh + wh4, temp_buff + wh + wh4, wh4, w2, h2);
            memcpy(img, temp_buff, size);
            break;
        default:
            /* Invalid */
            return -1;
        }
            indx++;
    }

    return 0;
}

