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
#include "logger.hpp"
#include "util.hpp"
#include "jpegutils.hpp"
#include "video_convert.hpp"

/**
 * sonix_decompress_init
 *   pre-calculates a locally stored table for efficient huffman-decoding.
 *
 *   Each entry at index x in the table represents the codeword
 *   present at the MSB of byte x.
 *
 */
void cls_convert::sonix_decompress_init(sonix_table *table)
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
int cls_convert::sonix_decompress(u_char *img_dst, u_char *img_src)
{
    int row, col;
    int val;
    int bitpos;
    u_char code;
    u_char *addr;

    /* Local storage */
    static sonix_table table[256];
    static int init_done = 0;

    if (!init_done) {
        init_done = 1;
        sonix_decompress_init(table);
    }

    bitpos = 0;
    for (row = 0; row < height; row++) {

        col = 0;

        /* First two pixels in first two rows are stored as raw 8-bit. */
        if (row < 2) {
            addr = img_src + (bitpos >> 3);
            code =(u_char)( (addr[0] << (bitpos & 7)) |
                    (addr[1] >> (8 - (bitpos & 7))));
            bitpos += 8;
            *img_dst++ = code;

            addr = img_src + (bitpos >> 3);
            code = (u_char)((addr[0] << (bitpos & 7)) |
                    (addr[1] >> (8 - (bitpos & 7))));
            bitpos += 8;
            *img_dst++ = code;

            col += 2;
        }

        while (col < width) {
            /* Get bitcode from bitstream. */
            addr = img_src + (bitpos >> 3);
            code =(u_char)((addr[0] << (bitpos & 7)) |
                (addr[1] >> (8 - (bitpos & 7))));

            /* Update bit position. */
            bitpos += table[code].len;

            /* Calculate pixel value. */
            val = table[code].val;
            if (!table[code].is_abs) {
                /* Value is relative to top and left pixel. */
                if (col < 2) {
                    /* Left column: relative to top pixel. */
                    val += img_dst[-2 * width];
                } else if (row < 2) {
                    /* Top row: relative to left pixel. */
                    val += img_dst[-2];
                } else {
                    /* Main area: average of left pixel and top pixel. */
                    val += (img_dst[-2] + img_dst[-2 * width]) / 2;
                }
            }

            /* Store pixel */
            if (val < 0) {
                *img_dst++ = 0;
            } else if (val > 255) {
                *img_dst++ = 255;
            } else {
                *img_dst++ =(u_char)val;
            }
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
void cls_convert::bayer2rgb24(u_char *img_dst, u_char *img_src)
{
    long int i;
    u_char *rawpt, *scanpt;
    long int size;

    rawpt = img_src;
    scanpt = img_dst;
    size = width * height;

    for (i = 0; i < size; i++) {
        if (((i / width) & 1) == 0) {
            if ((i & 1) == 0) {
                /* B */
                if ((i > width) && ((i % width) > 0)) {
                    *scanpt++ = (u_char)(*rawpt);        /* B */
                    *scanpt++ = (u_char)((*(rawpt - 1) +
                                *(rawpt + 1) +
                                *(rawpt + width) +
                                *(rawpt - width)) / 4);         /* G */
                    *scanpt++ = (u_char)((*(rawpt - width - 1) +
                                *(rawpt - width + 1) +
                                *(rawpt + width - 1) +
                                *(rawpt + width + 1)) / 4);     /* R */
                } else {
                    /* First line or left column. */
                    *scanpt++ = (u_char)(*rawpt);        /* B */
                    *scanpt++ = (u_char)((*(rawpt + 1) +
                                *(rawpt + width)) / 2);         /* G */
                    *scanpt++ = (u_char)(*(rawpt + width + 1));       /* R */
                }
            } else {
                /* (B)G */
                if ((i > width) && ((i % width) < (width - 1))) {
                    *scanpt++ = (u_char)((*(rawpt - 1) +
                                *(rawpt + 1)) / 2);             /* B */
                    *scanpt++ = (u_char)(*rawpt);        /* G */
                    *scanpt++ = (u_char)((*(rawpt + width) +
                                *(rawpt - width)) / 2);  /* R */
                } else {
                    /* First line or right column. */
                    *scanpt++ = (u_char)(*(rawpt - 1));      /* B */
                    *scanpt++ = (u_char)(*rawpt);            /* G */
                    *scanpt++ = (u_char)(*(rawpt + width));  /* R */
                }
            }
        } else {
            if ((i & 1) == 0) {
                /* G(R) */
                if ((i < (width * (height - 1))) && ((i % width) > 0)) {
                    *scanpt++ =(u_char)( (*(rawpt + width) +
                                *(rawpt - width)) / 2);                 /* B */
                    *scanpt++ =(u_char)( *rawpt);                /* G */
                    *scanpt++ =(u_char)( (*(rawpt - 1) +
                                *(rawpt + 1)) / 2);                     /* R */
                } else {
                    /* Bottom line or left column. */
                    *scanpt++ = (u_char)(*(rawpt - width));      /* B */
                    *scanpt++ = (u_char)(*rawpt);                /* G */
                    *scanpt++ = (u_char)(*(rawpt + 1));          /* R */
                }
            } else {
                /* R */
                if (i < (width * (height - 1)) && ((i % width) < (width - 1))) {
                    *scanpt++ = (u_char)( (*(rawpt - width - 1) +
                                *(rawpt - width + 1) +
                                *(rawpt + width - 1) +
                                *(rawpt + width + 1)) / 4);             /* B */
                    *scanpt++ = (u_char)((*(rawpt - 1) +
                                *(rawpt + 1) +
                                *(rawpt - width) +
                                *(rawpt + width)) / 4);                 /* G */
                    *scanpt++ = (u_char)(*rawpt);                /* R */
                } else {
                    /* Bottom line or right column. */
                    *scanpt++ = (u_char)(*(rawpt - width - 1));  /* B */
                    *scanpt++ = (u_char)((*(rawpt - 1) +
                                *(rawpt - width)) / 2);                 /* G */
                    *scanpt++ = (u_char)(*rawpt);                /* R */
                }
            }
        }
        rawpt++;
    }

}

void cls_convert::yuv422to420p(u_char *img_dst, u_char *img_src)
{
    u_char *src, *dest, *src2, *dest2;
    int i, j;

    /* Create the Y plane. */
    src = img_src;
    dest = img_dst;
    for (i = width * height; i > 0; i--) {
        *dest++ = *src;
        src += 2;
    }
    /* Create U and V planes. */
    src = img_src + 1;
    src2 = img_src + width * 2 + 1;
    dest = img_dst + width * height;
    dest2 = dest + (width * height) / 4;
    for (i = height / 2; i > 0; i--) {
        for (j = width / 2; j > 0; j--) {
            *dest = (u_char)(((int) *src + (int) *src2) / 2);
            src += 2;
            src2 += 2;
            dest++;
            *dest2 = (u_char)(((int) *src + (int) *src2) / 2);
            src += 2;
            src2 += 2;
            dest2++;
        }
        src += width * 2;
        src2 += width * 2;
    }
}

void cls_convert::yuv422pto420p(u_char *img_dst, u_char *img_src)
{
    u_char *src, *dest, *dest2;
    u_char *src_u, *src_u2, *src_v, *src_v2;

    int i, j;
    /*Planar version of 422 */
    /* Create the Y plane. */
    src = img_src;
    dest = img_dst;
    for (i = width * height; i > 0; i--) {
        *dest++ = *src++;
    }

    /* Create U and V planes. */
    dest = img_dst + width * height;
    dest2 = dest + (width * height) / 4;
    for (i = 0; i< (height / 2); i++) {
        src_u = img_src + (width * height) + ((i*2) * (width/2));
        src_u2 = src_u  + (width/2);
        src_v = src_u + (width/2 * height);
        src_v2 = src_v  + (width/2);

        for (j = 0; j < (width / 2); j++) {
            *dest = (u_char)(((int) *src_u + (int) *src_u2) / 2);
            src_u ++;
            src_u2++;
            dest++;

            *dest2 = (u_char)(((int) *src_v + (int) *src_v2) / 2);
            src_v ++;
            src_v2++;
            dest2++;
        }
    }
}

void cls_convert::uyvyto420p(u_char *img_dst, u_char *img_src)
{
    u_char *pY = img_dst;
    u_char *pU = pY + (width * height);
    u_char *pV = pU + (width * height) / 4;
    unsigned int uv_offset = width * 2 * sizeof(u_char);
    int ix, jx;

    for (ix = 0; ix < height; ix++) {
        for (jx = 0; jx < width; jx += 2) {
            unsigned short int calc;

            if ((ix&1) == 0) {
                calc = *img_src;
                calc += *(img_src + uv_offset);
                calc /= 2;
                *pU++ = (u_char) calc;
            }

            img_src++;
            *pY++ = *img_src++;

            if ((ix&1) == 0) {
                calc = *img_src;
                calc += *(img_src + uv_offset);
                calc /= 2;
                *pV++ = (u_char) calc;
            }

            img_src++;
            *pY++ = *img_src++;
        }
    }
}

void cls_convert::rgb_bgr(u_char *img_dst, u_char *img_src, int rgb)
{
    u_char *y, *u, *v;
    u_char *r, *g, *b;
    int i, loop;

    if (rgb == 1) {
        r = img_src;
        g = r + 1;
        b = g + 1;
    } else {
        b = img_src;
        g = b + 1;
        r = g + 1;
    }

    y = img_dst;
    u = y + width * height;
    v = u + (width * height) / 4;
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);

    for (loop = 0; loop < height; loop++) {
        for (i = 0; i < width; i += 2) {
            *y++ = (u_char)((9796 ** r + 19235 ** g + 3736 ** b) >> 15);
            *u += (u_char)(((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32);
            *v += (u_char)(((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32);
            r += 3;
            g += 3;
            b += 3;
            *y++ = (u_char)((9796 ** r + 19235 ** g + 3736 ** b) >> 15);
            *u += (u_char)(((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32);
            *v += (u_char)(((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32);
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

void cls_convert::rgb24toyuv420p(u_char *img_dst, u_char *img_src
    )
{
    rgb_bgr(img_dst, img_src, 1);
}

void cls_convert::bgr24toyuv420p(u_char *img_dst, u_char *img_src
    )
{
    rgb_bgr(img_dst, img_src, 0);
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
int cls_convert::mjpegtoyuv420p(u_char *img_dst, u_char *img_src, int size)
{
    u_char *ptr_buffer;
    size_t soi_pos = 0;
    int ret = 0;

    ptr_buffer =(u_char*) memmem(img_src, size, "\xff\xd8", 2);
    if (ptr_buffer == NULL) {
        MOTPLS_LOG(CRT, TYPE_VIDEO, NO_ERRNO,_("Corrupt image ... continue"));
        return 1;
    }
    /**
     Some cameras are sending multiple SOIs in the buffer.
     Move the pointer to the last SOI in the buffer and proceed.
    */
    while (ptr_buffer != NULL && ((size - soi_pos - 1) > 2) ){
        soi_pos = ptr_buffer - img_src;
        ptr_buffer =(u_char*) memmem(img_src + soi_pos + 1, size - soi_pos - 1, "\xff\xd8", 2);
    }

    if (soi_pos != 0) {
        MOTPLS_LOG(INF, TYPE_VIDEO, NO_ERRNO,_("SOI position adjusted by %d bytes."), soi_pos);
    }

    memmove(img_src, img_src + soi_pos, size - soi_pos);
    size -= (unsigned int)soi_pos;

    ret = jpgutl_decode_jpeg(img_src,size, width, height, img_dst);

    if (ret == -1) {
        MOTPLS_LOG(CRT, TYPE_VIDEO, NO_ERRNO,_("Corrupt image ... continue"));
        ret = 1;
    }
    return ret;
}

void cls_convert::y10torgb24(u_char *img_dst, u_char *img_src, int shift)
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
            a = (img_src[src_y*src_stride + src_x*2+0] |
                (img_src[src_y*src_stride + src_x*2+1] << 8)) >> shift;
            img_dst[dst_y*rgb_stride+3*dst_x+0] = (u_char)a;
            img_dst[dst_y*rgb_stride+3*dst_x+1] = (u_char)a;
            img_dst[dst_y*rgb_stride+3*dst_x+2] = (u_char)a;
        }
    }
}

void cls_convert::greytoyuv420p(u_char *img_dst, u_char *img_src)
{
    memcpy(img_dst, img_src, (width*height));
    memset(img_dst+(width*height), 128, (width * height) / 2);
}

/* Convert captured image to the standard pixel format*/
int cls_convert::process(u_char *img_dst, u_char *img_src, int clen)
{
    #ifdef HAVE_V4L2
        /*The FALLTHROUGH is a special comment required by compiler. */
        switch (pixfmt_src) {
        case V4L2_PIX_FMT_RGB24:
            rgb24toyuv420p(img_dst, img_src);
            return 0;

        case V4L2_PIX_FMT_UYVY:
            uyvyto420p(img_dst, img_src);
            return 0;

        case V4L2_PIX_FMT_YUYV:
            yuv422to420p(img_dst, img_src);
            return 0;

        case V4L2_PIX_FMT_YUV422P:
            yuv422pto420p(img_dst, img_src);
            return 0;

        case V4L2_PIX_FMT_YUV420:
            memcpy(img_dst, img_src, clen);
            return 0;

        case V4L2_PIX_FMT_PJPG:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_JPEG:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_MJPEG:
            return mjpegtoyuv420p(img_dst, img_src, clen);

        case V4L2_PIX_FMT_SBGGR16:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_SGBRG8:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_SGRBG8:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_SBGGR8:    /* bayer */
            bayer2rgb24(common_buffer, img_src);
            rgb24toyuv420p(img_dst, common_buffer);
            return 0;

        case V4L2_PIX_FMT_SRGGB8: /*New Pi Camera format*/
            bayer2rgb24(common_buffer, img_src);
            rgb24toyuv420p(img_dst, common_buffer);
            return 0;

        case V4L2_PIX_FMT_SPCA561:
            /*FALLTHROUGH*/
        case V4L2_PIX_FMT_SN9C10X:
            sonix_decompress(img_dst, img_src);
            bayer2rgb24(common_buffer, img_dst);
            rgb24toyuv420p(img_dst, common_buffer);
            return 0;

        case V4L2_PIX_FMT_Y12:
            y10torgb24(common_buffer, img_src, 2);
            rgb24toyuv420p(img_dst, common_buffer);
            return 0;
        case V4L2_PIX_FMT_Y10:
            y10torgb24(common_buffer, img_src, 4);
            rgb24toyuv420p(img_dst, common_buffer);
            return 0;
        case V4L2_PIX_FMT_GREY:
            greytoyuv420p(img_dst, img_src);
            return 0;
        }
    #else
        (void)img_dst;
        (void)img_src;
        (void)clen;
    #endif
    return -1;

}


cls_convert::cls_convert(ctx_dev *p_cam, int p_pix, int p_w, int p_h)
{
    cam = p_cam;
    width = p_w;
    height = p_h;
    pixfmt_src = p_pix;

    common_buffer =(u_char*) mymalloc(3 * width * height);

}

cls_convert::~cls_convert()
{
    if (common_buffer != nullptr) {
        free(common_buffer);
    }
}

