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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 */
#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"

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
    uint32_t tmp;

    while (nsrc < ndst) {
        tmp = bswap_32(*ndst);
        *ndst-- = bswap_32(*nsrc);
        *nsrc++ = tmp;
    }
}

static void flip_inplace_horizontal(unsigned char *src, int width, int height)
{
    uint8_t *nsrc, *ndst;
    uint8_t tmp;
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
    uint8_t tmp;
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
static void rot90cw(unsigned char *src, unsigned char *dst, int size, int width, int height)
{
    unsigned char *endp;
    unsigned char *base;
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
static inline void rot90ccw(unsigned char *src, unsigned char *dst, int size, int width, int height)
{
    unsigned char *endp;
    unsigned char *base;
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
 *   cam - the current thread's context structure
 *
 * Returns: nothing
 */
void rotate_init(struct ctx_cam *cam)
{
    int size_norm, size_high;

    cam->rotate_data =(struct ctx_rotate*) mymalloc(sizeof(struct ctx_rotate));

    /* Make sure buffer_norm isn't freed if it hasn't been allocated. */
    cam->rotate_data->buffer_norm = NULL;
    cam->rotate_data->buffer_high = NULL;

    /*
     * Assign the value in conf.rotate to rotate_data->degrees. This way,
     * we have a value that is safe from changes caused by motion-control.
     */
    if ((cam->conf->rotate % 90) > 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Config option \"rotate\" not a multiple of 90: %d")
            ,cam->conf->rotate);
        cam->conf->rotate = 0;     /* Disable rotation. */
        cam->rotate_data->degrees = 0; /* Force return below. */
    } else {
        cam->rotate_data->degrees = cam->conf->rotate % 360; /* Range: 0..359 */
    }

    if (cam->conf->flip_axis == "horizontal") {
        cam->rotate_data->axis = FLIP_TYPE_HORIZONTAL;
    } else if (cam->conf->flip_axis == "vertical") {
        cam->rotate_data->axis = FLIP_TYPE_VERTICAL;
    } else {
        cam->rotate_data->axis = FLIP_TYPE_NONE;
    }

    /*
     * Upon entrance to this function, imgs.width and imgs.height contain the
     * capture dimensions (as set in the configuration file, or read from a
     * netcam source).
     *
     * If rotating 90 or 270 degrees, the capture dimensions and output dimensions
     * are not the same. Capture dimensions will be contained in capture_width_norm and
     * capture_height_norm in cam->rotate_data, while output dimensions will be contained
     * in imgs.width and imgs.height.
     */

    /* 1. Transfer capture dimensions into capture_width_norm and capture_height_norm. */
    cam->rotate_data->capture_width_norm  = cam->imgs.width;
    cam->rotate_data->capture_height_norm = cam->imgs.height;

    cam->rotate_data->capture_width_high  = cam->imgs.width_high;
    cam->rotate_data->capture_height_high = cam->imgs.height_high;

    size_norm = cam->imgs.width * cam->imgs.height * 3 / 2;
    size_high = cam->imgs.width_high * cam->imgs.height_high * 3 / 2;

    if ((cam->rotate_data->degrees == 90) || (cam->rotate_data->degrees == 270)) {
        /* 2. "Swap" imgs.width and imgs.height. */
        cam->imgs.width = cam->rotate_data->capture_height_norm;
        cam->imgs.height = cam->rotate_data->capture_width_norm;
        if (size_high > 0 ) {
            cam->imgs.width_high = cam->rotate_data->capture_height_high;
            cam->imgs.height_high = cam->rotate_data->capture_width_high;
        }
    }

    /*
     * If we're not rotating, let's exit once we have setup the capture dimensions
     * and output dimensions properly.
     */
    if (cam->rotate_data->degrees == 0) {
        return;
    }

    /*
     * Allocate memory if rotating 90 or 270 degrees, because those rotations
     * cannot be performed in-place (they can, but it would be too slow).
     */
    if ((cam->rotate_data->degrees == 90) || (cam->rotate_data->degrees == 270)) {
        cam->rotate_data->buffer_norm =(unsigned char*) mymalloc(size_norm);
        if (size_high > 0 ) {
            cam->rotate_data->buffer_high =(unsigned char*) mymalloc(size_high);
        }
    }

}

/**
 * rotate_deinit
 *
 *  Frees resources previously allocated by rotate_init.
 *
 * Parameters:
 *
 *   cam - the current thread's context structure
 *
 * Returns: nothing
 */
void rotate_deinit(struct ctx_cam *cam)
{

    if (cam->rotate_data == NULL) {
        return;
    }

    if (cam->rotate_data->buffer_norm) {
        free(cam->rotate_data->buffer_norm);
    }

    if (cam->rotate_data->buffer_high) {
        free(cam->rotate_data->buffer_high);
    }

    if (cam->rotate_data != NULL) {
        free(cam->rotate_data);
        cam->rotate_data = NULL;
    }
}

/**
 * rotate_map
 *
 *  Main entry point for rotation.
 *
 * Parameters:
 *
 *   img_data- pointer to the image data to rotate
 *   cam - the current thread's context structure
 *
 * Returns:
 *
 *   0  - success
 *   -1 - failure (shouldn't happen)
 */
int rotate_map(struct ctx_cam *cam, struct ctx_image_data *img_data)
{
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

    if (cam->rotate_data->degrees == 0 && cam->rotate_data->axis == FLIP_TYPE_NONE) {
        return 0;
    }

    indx = 0;
    if ((cam->rotate_data->capture_width_high != 0) && (cam->rotate_data->capture_height_high != 0)) {
        indx_max = 1;
    } else {
        indx_max = 0;
    }

    while (indx <= indx_max) {
        deg = cam->rotate_data->degrees;
        axis = cam->rotate_data->axis;
        wh4 = 0;
        w2 = 0;
        h2 = 0;
        if (indx == 0 ) {
            img = img_data->image_norm;
            width = cam->rotate_data->capture_width_norm;
            height = cam->rotate_data->capture_height_norm;
            temp_buff = cam->rotate_data->buffer_norm;
        } else {
            img = img_data->image_high;
            width = cam->rotate_data->capture_width_high;
            height = cam->rotate_data->capture_height_high;
            temp_buff = cam->rotate_data->buffer_high;
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

