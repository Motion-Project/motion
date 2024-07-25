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

void cls_rotate::reverse_inplace_quad(u_char *src, int size)
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

void cls_rotate::flip_inplace_horizontal(u_char *src, int width, int height)
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

void cls_rotate::flip_inplace_vertical(u_char *src, int width, int height)
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

void cls_rotate::rot90cw(u_char *src, u_char *dst, int size, int width, int height)
{
    u_char *endp;
    u_char *base;
    int j;

    endp = src + size;
    for (base = endp - width; base < endp; base++) {
        src = base;
        for (j = 0; j < height; j++, src -= width)
            *dst++ = *src;

    }
}

void cls_rotate::rot90ccw(u_char *src, u_char *dst, int size, int width, int height)
{
    u_char *endp;
    u_char *base;
    int j;

    endp = src + size;
    dst = dst + size - 1;
    for (base = endp - width; base < endp; base++) {
        src = base;
        for (j = 0; j < height; j++, src -= width)
            *dst-- = *src;

    }
}

void cls_rotate::process(ctx_image_data *img_data)
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
    int size;
    int width, height;
    u_char *img;
    u_char *temp_buff;

    if ((degrees == 0) && (axis == FLIP_TYPE_NONE)) {
        return;
    }

    indx = 0;
    if ((capture_width_high != 0) && (capture_height_high != 0)) {
        indx_max = 1;
    } else {
        indx_max = 0;
    }

    while (indx <= indx_max) {
        if (indx == 0 ) {
            img = img_data->image_norm;
            width = capture_width_norm;
            height = capture_height_norm;
            temp_buff = buffer_norm;
        } else {
            img = img_data->image_high;
            width = capture_width_high;
            height = capture_height_high;
            temp_buff = buffer_high;
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

        switch (degrees) {
        case 90:
            rot90cw(img, temp_buff, wh, width, height);
            rot90cw(img + wh, temp_buff + wh, wh4, w2, h2);
            rot90cw(img + wh + wh4, temp_buff + wh + wh4, wh4, w2, h2);
            memcpy(img, temp_buff, (uint)size);
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
            memcpy(img, temp_buff, (uint)size);
            break;
        default:
            break;
        }
            indx++;
    }

    return;
}

cls_rotate::cls_rotate(ctx_dev *p_cam)
{
    cam = p_cam;
    int size_norm, size_high;

    buffer_norm = nullptr;
    buffer_high = nullptr;

    if ((cam->conf->rotate % 90) > 0) {
        MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO
            ,_("Config option \"rotate\" not a multiple of 90: %d")
            ,cam->conf->rotate);
        cam->conf->rotate = 0;     /* Disable rotation. */
        degrees = 0; /* Force return below. */
    } else {
        degrees = cam->conf->rotate % 360; /* Range: 0..359 */
    }

    if (cam->conf->flip_axis == "horizontal") {
        axis = FLIP_TYPE_HORIZONTAL;
    } else if (cam->conf->flip_axis == "vertical") {
        axis = FLIP_TYPE_VERTICAL;
    } else {
        axis = FLIP_TYPE_NONE;
    }

    /* At this point, imgs.width and imgs.height contain the capture dimensions.
     * If rotating 90 or 270 degrees, the output h/w will be swapped.
     */

    /* 1. Transfer capture dimensions into capture_width_norm and capture_height_norm. */
    capture_width_norm  = cam->imgs.width;
    capture_height_norm = cam->imgs.height;

    capture_width_high  = cam->imgs.width_high;
    capture_height_high = cam->imgs.height_high;

    size_norm = cam->imgs.width * cam->imgs.height * 3 / 2;
    size_high = cam->imgs.width_high * cam->imgs.height_high * 3 / 2;

    /* "Swap" imgs.width and imgs.height. */
    if ((degrees == 90) || (degrees == 270)) {
        cam->imgs.width = capture_height_norm;
        cam->imgs.height = capture_width_norm;
        if (size_high > 0 ) {
            cam->imgs.width_high = capture_height_high;
            cam->imgs.height_high = capture_width_high;
        }
    }

    if (degrees == 0) {
        return;
    }

    if ((degrees == 90) || (degrees == 270)) {
        buffer_norm =(u_char*) mymalloc((uint)size_norm);
        if (size_high > 0 ) {
            buffer_high =(u_char*) mymalloc((uint)size_high);
        }
    }

}

cls_rotate::~cls_rotate()
{
    myfree(&buffer_norm);
    myfree(&buffer_high);

}
