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
#include "rotate.h"

#ifndef __uint32
/**
 * We don't have a 32-bit unsigned integer type, so define it, given
 * a 32-bit type was found by configure.
 */
#    ifdef TYPE_32BIT
typedef unsigned TYPE_32BIT __uint32;
#    else
#        error "Failed to find a 32-bit integer type."
#    endif
#endif

/*=============================================================================
                    Start of code from bits/byteswap.h
 =============================================================================*/

/**
 * The code below is copied (with modification) from bits/byteswap.h. It provides
 * a macro/function named rot__bswap_32 that swaps the bytes in a 32-bit integer,
 * preferably using the bswap assembler instruction if configure found support
 * for it.
 *
 * It would be neater to simply include byteswap.h and use the bswap_32 macro
 * defined there, but the problem is that the bswap asm instruction would then
 * only be used for certain processor architectures, excluding athlon (and
 * probably athlon64 as well). Moreover, byteswap.h doesn't seem to exist on
 * FreeBSD. So, we rely on the HAVE_BSWAP macro defined by configure instead.
 *
 * Note that the macro names have been prefixed with "rot" in order to avoid
 * collision since we have the include chain rotate.h -> motion.h -> netcam.h ->
 * netinet/in.h -> ... -> byteswap.h -> bits/byteswap.h.
 */

/* Swap bytes in 32 bit value. This is used as a fallback and for constants. */
#define rot__bswap_constant_32(x)                               \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |  \
     (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#ifdef __GNUC__
#    if (__GNUC__ >= 2) && (i386 || __i386 || __i386__)
/* We're on an Intel-compatible platform, so we can use inline Intel assembler
 * for the swapping.
 */
#        ifndef HAVE_BSWAP
/* Bswap is not available, we have to use three instructions instead. */
#            define rot__bswap_32(x)                                \
                (__extension__                                      \
                ({ register __uint32 __v, __x = (x);                \
                if (__builtin_constant_p (__x))                     \
                    __v = rot__bswap_constant_32 (__x);             \
                else                                                \
                    __asm__ ("rorw $8, %w0;"                        \
                            "rorl $16, %0;"                         \
                            "rorw $8, %w0"                          \
                            : "=r" (__v)                            \
                            : "0" (__x)                             \
                            : "cc");                                \
                __v; }))
#        else
#            define rot__bswap_32(x)                                \
                (__extension__                                      \
                ({ register __uint32 __v, __x = (x);                \
                if (__builtin_constant_p (__x))                     \
                    __v = rot__bswap_constant_32 (__x);             \
                else                                                \
                    __asm__ ("bswap %0" : "=r" (__v) : "0" (__x));  \
                __v; }))
#        endif
#    else
/* Non-Intel platform or too old version of gcc. */
#        define rot__bswap_32(x)                                    \
            (__extension__                                          \
            ({ register __uint32 __x = (x);                         \
            rot__bswap_constant_32 (__x); }))
#    endif
#else
/* Not a GNU compiler. */
static inline __uint32 rot__bswap_32(__uint32 __bsx)
{
    return __bswap_constant_32 (__bsx);
}
#endif

/*=============================================================================
                     End of code from bits/byteswap.h
 =============================================================================*/

/* Finally define a macro with a more appropriate name, to be used below. */
#define swap_bytes(x) rot__bswap_32(x)

/**
 * reverse_inplace_quad
 *
 *  Reverses a block of memory in-place, 4 bytes at a time. This function
 *  requires the __uint32 type, which is 32 bits wide.
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
    __uint32 *nsrc = (__uint32 *)src;              /* first quad */
    __uint32 *ndst = (__uint32 *)(src + size - 4); /* last quad */
    register __uint32 tmp;

    while (nsrc < ndst) {
        tmp = swap_bytes(*ndst);
        *ndst-- = swap_bytes(*nsrc);
        *nsrc++ = tmp;
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
void rotate_init(struct context *cnt)
{
    int size;

    /* Make sure temp_buf isn't freed if it hasn't been allocated. */
    cnt->rotate_data.temp_buf = NULL;

    /*
     * Assign the value in conf.rotate_deg to rotate_data.degrees. This way,
     * we have a value that is safe from changes caused by motion-control.
     */
    if ((cnt->conf.rotate_deg % 90) > 0) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO, "%s: Config option \"rotate\" not a multiple of 90: %d",
                   cnt->conf.rotate_deg);
        cnt->conf.rotate_deg = 0;     /* Disable rotation. */
        cnt->rotate_data.degrees = 0; /* Force return below. */
    } else {
        cnt->rotate_data.degrees = cnt->conf.rotate_deg % 360; /* Range: 0..359 */
    }

    /*
     * Upon entrance to this function, imgs.width and imgs.height contain the
     * capture dimensions (as set in the configuration file, or read from a
     * netcam source).
     *
     * If rotating 90 or 270 degrees, the capture dimensions and output dimensions
     * are not the same. Capture dimensions will be contained in cap_width and
     * cap_height in cnt->rotate_data, while output dimensions will be contained
     * in imgs.width and imgs.height.
     */

    /* 1. Transfer capture dimensions into cap_width and cap_height. */
    cnt->rotate_data.cap_width  = cnt->imgs.width;
    cnt->rotate_data.cap_height = cnt->imgs.height;

    if ((cnt->rotate_data.degrees == 90) || (cnt->rotate_data.degrees == 270)) {
        /* 2. "Swap" imgs.width and imgs.height. */
        cnt->imgs.width = cnt->rotate_data.cap_height;
        cnt->imgs.height = cnt->rotate_data.cap_width;
    }

    /*
     * If we're not rotating, let's exit once we have setup the capture dimensions
     * and output dimensions properly.
     */
    if (cnt->rotate_data.degrees == 0)
        return;

    switch (cnt->imgs.type) {
    case VIDEO_PALETTE_YUV420P:
        /*
         * For YUV 4:2:0 planar, the memory block used for 90/270 degrees
         * rotation needs to be width x height x 1.5 bytes large.
         */
        size = cnt->imgs.width * cnt->imgs.height * 3 / 2;
        break;
    case VIDEO_PALETTE_GREY:
        /*
         * For greyscale, the memory block used for 90/270 degrees rotation
         * needs to be width x height bytes large.
         */
        size = cnt->imgs.width * cnt->imgs.height;
        break;
    default:
        cnt->rotate_data.degrees = 0;
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO, "%s: Unsupported palette (%d), rotation is disabled",
                    cnt->imgs.type);
        return;
    }

    /*
     * Allocate memory if rotating 90 or 270 degrees, because those rotations
     * cannot be performed in-place (they can, but it would be too slow).
     */
    if ((cnt->rotate_data.degrees == 90) || (cnt->rotate_data.degrees == 270))
        cnt->rotate_data.temp_buf = mymalloc(size);
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
void rotate_deinit(struct context *cnt)
{
    if (cnt->rotate_data.temp_buf)
        free(cnt->rotate_data.temp_buf);
}

/**
 * rotate_map
 *
 *  Main entry point for rotation. This is the function that is called from
 *  video.c/video_freebsd.c to perform the rotation.
 *
 * Parameters:
 *
 *   map - pointer to the image/data to rotate
 *   cnt - the current thread's context structure
 *
 * Returns:
 *
 *   0  - success
 *   -1 - failure (shouldn't happen)
 */
int rotate_map(struct context *cnt, unsigned char *map)
{
    /*
     * The image format is either YUV 4:2:0 planar, in which case the pixel
     * data is divided in three parts:
     *    Y - width x height bytes
     *    U - width x height / 4 bytes
     *    V - as U
     * or, it is in greyscale, in which case the pixel data simply consists
     * of width x height bytes.
     */
    int wh, wh4 = 0, w2 = 0, h2 = 0;  /* width * height, width * height / 4 etc. */
    int size, deg;
    int width, height;

    deg = cnt->rotate_data.degrees;
    width = cnt->rotate_data.cap_width;
    height = cnt->rotate_data.cap_height;

    /*
     * Pre-calculate some stuff:
     *  wh   - size of the Y plane, or the entire greyscale image
     *  size - size of the entire memory block
     *  wh4  - size of the U plane, and the V plane
     *  w2   - width of the U plane, and the V plane
     *  h2   - as w2, but height instead
     */
    wh = width * height;
    if (cnt->imgs.type == VIDEO_PALETTE_YUV420P) {
        size = wh * 3 / 2;
        wh4 = wh / 4;
        w2 = width / 2;
        h2 = height / 2;
    } else { /* VIDEO_PALETTE_GREY */
        size = wh;
    }

    switch (deg) {
    case 90:
        /* First do the Y part */
        rot90cw(map, cnt->rotate_data.temp_buf, wh, width, height);
        if (cnt->imgs.type == VIDEO_PALETTE_YUV420P) {
            /* Then do U and V */
            rot90cw(map + wh, cnt->rotate_data.temp_buf + wh, wh4, w2, h2);
            rot90cw(map + wh + wh4, cnt->rotate_data.temp_buf + wh + wh4,
                    wh4, w2, h2);
        }

        /* Then copy back from the temp buffer to map. */
        memcpy(map, cnt->rotate_data.temp_buf, size);
        break;

    case 180:
        /*
         * 180 degrees is easy - just reverse the data within
         * Y, U and V.
         */
        reverse_inplace_quad(map, wh);
        if (cnt->imgs.type == VIDEO_PALETTE_YUV420P) {
            reverse_inplace_quad(map + wh, wh4);
            reverse_inplace_quad(map + wh + wh4, wh4);
        }
        break;

    case 270:

        /* First do the Y part */
        rot90ccw(map, cnt->rotate_data.temp_buf, wh, width, height);
        if (cnt->imgs.type == VIDEO_PALETTE_YUV420P) {
            /* Then do U and V */
            rot90ccw(map + wh, cnt->rotate_data.temp_buf + wh, wh4, w2, h2);
            rot90ccw(map + wh + wh4, cnt->rotate_data.temp_buf + wh + wh4,
                     wh4, w2, h2);
        }

        /* Then copy back from the temp buffer to map. */
        memcpy(map, cnt->rotate_data.temp_buf, size);
        break;

    default:
        /* Invalid */
        return -1;
    }

    return 0;
}

