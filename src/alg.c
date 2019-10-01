/*    alg.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2001 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.h"
#include "util.h"
#include "alg.h"
#include "draw.h"

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))

/*
struct segment {
    struct ctx_coord coord;
    int width;
    int height;
    int open;
    int count;
};
*/

/**
 * alg_locate_center_size
 *      Locates the center and size of the movement.
 */
void alg_locate_center_size(struct ctx_images *imgs, int width, int height, struct ctx_coord *cent)
{
    unsigned char *out = imgs->image_motion.image_norm;
    int *labels = imgs->labels;
    int x, y, centc = 0, xdist = 0, ydist = 0;

    cent->x = 0;
    cent->y = 0;
    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;

    /* If Labeling enabled - locate center of largest labelgroup. */
    if (imgs->labelsize_max) {
        /* Locate largest labelgroup */
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(labels++) & 32768) {
                    cent->x += x;
                    cent->y += y;
                    centc++;
                }
            }
        }

    } else {
        /* Locate movement */
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(out++)) {
                    cent->x += x;
                    cent->y += y;
                    centc++;
                }
            }
        }

    }

    if (centc) {
        cent->x = cent->x / centc;
        cent->y = cent->y / centc;
    }

    /* Now we find the size of the Motion. */

    /* First reset pointers back to initial value. */
    centc = 0;
    labels = imgs->labels;
    out = imgs->image_motion.image_norm;

    /* If Labeling then we find the area around largest labelgroup instead. */
    if (imgs->labelsize_max) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(labels++) & 32768) {
                    if (x > cent->x)
                        xdist += x - cent->x;
                    else if (x < cent->x)
                        xdist += cent->x - x;

                    if (y > cent->y)
                        ydist += y - cent->y;
                    else if (y < cent->y)
                        ydist += cent->y - y;

                    centc++;
                }
            }
        }

    } else {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(out++)) {
                    if (x > cent->x)
                        xdist += x - cent->x;
                    else if (x < cent->x)
                        xdist += cent->x - x;

                    if (y > cent->y)
                        ydist += y - cent->y;
                    else if (y < cent->y)
                        ydist += cent->y - y;

                    centc++;
                }
            }
        }

    }

    if (centc) {
        cent->minx = cent->x - xdist / centc * 2;
        cent->maxx = cent->x + xdist / centc * 2;
        /*
         * Make the box a little bigger in y direction to make sure the
         * heads fit in so we multiply by 3 instead of 2 which seems to
         * to work well in practical.
         */
        cent->miny = cent->y - ydist / centc * 3;
        cent->maxy = cent->y + ydist / centc * 2;
    }

    if (cent->maxx > width - 1)
        cent->maxx = width - 1;
    else if (cent->maxx < 0)
        cent->maxx = 0;

    if (cent->maxy > height - 1)
        cent->maxy = height - 1;
    else if (cent->maxy < 0)
        cent->maxy = 0;

    if (cent->minx > width - 1)
        cent->minx = width - 1;
    else if (cent->minx < 0)
        cent->minx = 0;

    if (cent->miny > height - 1)
        cent->miny = height - 1;
    else if (cent->miny < 0)
        cent->miny = 0;

    /* Align for better locate box handling */
    cent->minx += cent->minx % 2;
    cent->miny += cent->miny % 2;
    cent->maxx -= cent->maxx % 2;
    cent->maxy -= cent->maxy % 2;

    cent->width = cent->maxx - cent->minx;
    cent->height = cent->maxy - cent->miny;

    /*
     * We want to center Y coordinate to be the center of the action.
     * The head of a person is important so we correct the cent.y coordinate
     * to match the correction to include a persons head that we just did above.
     */
    cent->y = (cent->miny + cent->maxy) / 2;

}


/**
 * alg_draw_location
 *      Draws a box around the movement.
 */
void alg_draw_location(struct ctx_coord *cent, struct ctx_images *imgs, int width, unsigned char *new,
                       int style, int mode, int process_thisframe)
{
    unsigned char *out = imgs->image_motion.image_norm;
    int x, y;

    out = imgs->image_motion.image_norm;

    /* Debug image always gets a 'normal' box. */
    if ((mode == LOCATE_BOTH) && process_thisframe) {
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            out[width_miny_x] =~out[width_miny_x];
            out[width_maxy_x] =~out[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            out[width_minx_y] =~out[width_minx_y];
            out[width_maxx_y] =~out[width_maxx_y];
        }
    }
    if (style == LOCATE_BOX) { /* Draw a box on normal images. */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            new[width_miny_x] =~new[width_miny_x];
            new[width_maxy_x] =~new[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            new[width_minx_y] =~new[width_minx_y];
            new[width_maxx_y] =~new[width_maxx_y];
        }
    } else if (style == LOCATE_CROSS) { /* Draw a cross on normal images. */
        int centy = cent->y * width;

        for (x = cent->x - 10;  x <= cent->x + 10; x++) {
            new[centy + x] =~new[centy + x];
            out[centy + x] =~out[centy + x];
        }

        for (y = cent->y - 10; y <= cent->y + 10; y++) {
            new[cent->x + y * width] =~new[cent->x + y * width];
            out[cent->x + y * width] =~out[cent->x + y * width];
        }
    }
}


/**
 * alg_draw_red_location
 *          Draws a RED box around the movement.
 */
void alg_draw_red_location(struct ctx_coord *cent, struct ctx_images *imgs, int width, unsigned char *new,
                           int style, int mode, int process_thisframe)
{
    unsigned char *out = imgs->image_motion.image_norm;
    unsigned char *new_u, *new_v;
    int x, y, v, cwidth, cblock;

    cwidth = width / 2;
    cblock = imgs->motionsize / 4;
    x = imgs->motionsize;
    v = x + cblock;
    out = imgs->image_motion.image_norm;
    new_u = new + x;
    new_v = new + v;

    /* Debug image always gets a 'normal' box. */
    if ((mode == LOCATE_BOTH) && process_thisframe) {
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;

            out[width_miny_x] =~out[width_miny_x];
            out[width_maxy_x] =~out[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;

            out[width_minx_y] =~out[width_minx_y];
            out[width_maxx_y] =~out[width_maxx_y];
        }
    }

    if (style == LOCATE_REDBOX) { /* Draw a red box on normal images. */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;
        int cwidth_miny = cwidth * (cent->miny / 2);
        int cwidth_maxy = cwidth * (cent->maxy / 2);

        for (x = cent->minx + 2; x <= cent->maxx - 2; x += 2) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;
            int cwidth_miny_x = x / 2 + cwidth_miny;
            int cwidth_maxy_x = x / 2 + cwidth_maxy;

            new_u[cwidth_miny_x] = 128;
            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_miny_x] = 255;
            new_v[cwidth_maxy_x] = 255;

            new[width_miny_x] = 128;
            new[width_maxy_x] = 128;

            new[width_miny_x + 1] = 128;
            new[width_maxy_x + 1] = 128;

            new[width_miny_x + width] = 128;
            new[width_maxy_x + width] = 128;

            new[width_miny_x + 1 + width] = 128;
            new[width_maxy_x + 1 + width] = 128;
        }

        for (y = cent->miny; y <= cent->maxy; y += 2) {
            int width_minx_y = cent->minx + y * width;
            int width_maxx_y = cent->maxx + y * width;
            int cwidth_minx_y = (cent->minx / 2) + (y / 2) * cwidth;
            int cwidth_maxx_y = (cent->maxx / 2) + (y / 2) * cwidth;

            new_u[cwidth_minx_y] = 128;
            new_u[cwidth_maxx_y] = 128;
            new_v[cwidth_minx_y] = 255;
            new_v[cwidth_maxx_y] = 255;

            new[width_minx_y] = 128;
            new[width_maxx_y] = 128;

            new[width_minx_y + width] = 128;
            new[width_maxx_y + width] = 128;

            new[width_minx_y + 1] = 128;
            new[width_maxx_y + 1] = 128;

            new[width_minx_y + width + 1] = 128;
            new[width_maxx_y + width + 1] = 128;
        }
    } else if (style == LOCATE_REDCROSS) { /* Draw a red cross on normal images. */
        int cwidth_maxy = cwidth * (cent->y / 2);

        for (x = cent->x - 10; x <= cent->x + 10; x += 2) {
            int cwidth_maxy_x = x / 2 + cwidth_maxy;

            new_u[cwidth_maxy_x] = 128;
            new_v[cwidth_maxy_x] = 255;
        }

        for (y = cent->y - 10; y <= cent->y + 10; y += 2) {
            int cwidth_minx_y = (cent->x / 2) + (y / 2) * cwidth;

            new_u[cwidth_minx_y] = 128;
            new_v[cwidth_minx_y] = 255;
        }
    }
}


#define NORM               100
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x)-(y)))
#define NDIFF(x, y)        (ABS(x) * NORM / (ABS(x) + 2 * DIFF(x, y)))

/**
 * alg_noise_tune
 *
 */
void alg_noise_tune(struct ctx_cam *cam, unsigned char *new)
{
    struct ctx_images *imgs = &cam->imgs;
    int i;
    unsigned char *ref = imgs->ref;
    int diff, sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;

    i = imgs->motionsize;

    for (; i > 0; i--) {
        diff = ABS(*ref - *new);

        if (mask)
            diff = ((diff * *mask++) / 255);

        if (*smartmask) {
            sum += diff + 1;
            count++;
        }

        ref++;
        new++;
        smartmask++;
    }

    if (count > 3)  /* Avoid divide by zero. */
        sum /= count / 3;

    /* 5: safe, 4: regular, 3: more sensitive */
    cam->noise = 4 + (cam->noise + sum) / 2;
}

/**
 * alg_threshold_tune
 *
 */
void alg_threshold_tune(struct ctx_cam *cam, int diffs, int motion)
{
    int i;
    int sum = 0, top = diffs;

    if (!diffs)
        return;

    if (motion)
        diffs = cam->threshold / 4;

    for (i = 0; i < THRESHOLD_TUNE_LENGTH - 1; i++) {
        sum += cam->diffs_last[i];

        if (cam->diffs_last[i + 1] && !motion)
            cam->diffs_last[i] = cam->diffs_last[i + 1];
        else
            cam->diffs_last[i] = cam->threshold / 4;

        if (cam->diffs_last[i] > top)
            top = cam->diffs_last[i];
    }

    sum += cam->diffs_last[i];
    cam->diffs_last[i] = diffs;

    sum /= THRESHOLD_TUNE_LENGTH / 4;

    if (sum < top * 2)
        sum = top * 2;

    if (sum < cam->conf.threshold)
        cam->threshold = (cam->threshold + sum) / 2;
}

/*
 * Labeling by Joerg Weber. Based on an idea from Hubert Mara.
 * Floodfill enhanced by Ian McConnel based on code from
 * http://www.acm.org/pubs/tog/GraphicsGems/
 * http://www.codeproject.com/gdi/QuickFill.asp

 * Filled horizontal segment of scanline y for xl <= x <= xr.
 * Parent segment was on line y - dy.  dy = 1 or -1
 */

#define MAXS 10000               /* max depth of stack */

#define PUSH(Y, XL, XR, DY)     /* push new segment on stack */  \
        if (sp<stack+MAXS && Y+(DY) >= 0 && Y+(DY) < height)     \
        {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)      /* pop segment off stack */      \
        {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}

typedef struct {
    short y, xl, xr, dy;
} Segment;

/**
 * iflood
 *
 */
static int iflood(int x, int y, int width, int height,
                  unsigned char *out, int *labels, int newvalue, int oldvalue)
{
    int l, x1, x2, dy;
    Segment stack[MAXS], *sp = stack;    /* Stack of filled segments. */
    int count = 0;

    if (x < 0 || x >= width || y < 0 || y >= height)
        return 0;

    PUSH(y, x, x, 1);             /* Needed in some cases. */
    PUSH(y+1, x, x, -1);          /* Seed segment (popped 1st). */

    while (sp > stack) {
        /* Pop segment off stack and fill a neighboring scan line. */
        POP(y, x1, x2, dy);
        /*
         * Segment of scan line y-dy for x1<=x<=x2 was previously filled,
         * now explore adjacent pixels in scan line y
         */
        for (x = x1; x >= 0 && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x--) {
            labels[y * width + x] = newvalue;
            count++;
        }

        if (x >= x1)
            goto skip;

        l = x + 1;

        if (l < x1)
            PUSH(y, l, x1 - 1, -dy);  /* Leak on left? */

        x = x1 + 1;

        do {
            for (; x < width && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x++) {
                labels[y * width + x] = newvalue;
                count++;
            }

            PUSH(y, l, x - 1, dy);

            if (x > x2 + 1)
                PUSH(y, x2 + 1, x - 1, -dy);  /* Leak on right? */

            skip:

            for (x++; x <= x2 && !(out[y * width + x] != 0 && labels[y * width + x] == oldvalue); x++);

            l = x;
        } while (x <= x2);
    }
    return count;
}

/**
 * alg_labeling
 *
 */
static int alg_labeling(struct ctx_cam *cam)
{
    struct ctx_images *imgs = &cam->imgs;
    unsigned char *out = imgs->image_motion.image_norm;
    int *labels = imgs->labels;
    int ix, iy, pixelpos;
    int width = imgs->width;
    int height = imgs->height;
    int labelsize = 0;
    int current_label = 2;
    /* Keep track of the area just under the threshold.  */
    int max_under = 0;

    cam->current_image->total_labels = 0;
    imgs->labelsize_max = 0;
    /* ALL labels above threshold are counted as labelgroup. */
    imgs->labelgroup_max = 0;
    imgs->labels_above = 0;

    /* Init: 0 means no label set / not checked. */
    memset(labels, 0, width * height * sizeof(*labels));
    pixelpos = 0;

    for (iy = 0; iy < height - 1; iy++) {
        for (ix = 0; ix < width - 1; ix++, pixelpos++) {
            /* No motion - no label */
            if (out[pixelpos] == 0) {
                labels[pixelpos] = 1;
                continue;
            }

            /* Already visited by iflood */
            if (labels[pixelpos] > 0)
                continue;

            labelsize = iflood(ix, iy, width, height, out, labels, current_label, 0);

            if (labelsize > 0) {
                //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "Label: %i (%i) Size: %i (%i,%i)",
                //            current_label, cam->current_image->total_labels,
                //           labelsize, ix, iy);

                /* Label above threshold? Mark it again (add 32768 to labelnumber). */
                if (labelsize > cam->threshold) {
                    labelsize = iflood(ix, iy, width, height, out, labels, current_label + 32768, current_label);
                    imgs->labelgroup_max += labelsize;
                    imgs->labels_above++;
                } else if(max_under < labelsize)
                    max_under = labelsize;

                if (imgs->labelsize_max < labelsize) {
                    imgs->labelsize_max = labelsize;
                    imgs->largest_label = current_label;
                }

                cam->current_image->total_labels++;
                current_label++;
            }
        }
        pixelpos++; /* Compensate for ix < width - 1 */
    }

    //MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "%i Labels found. Largest connected Area: %i Pixel(s). "
    //           "Largest Label: %i", imgs->largest_label, imgs->labelsize_max,
    //           cam->current_image->total_labels);

    /* Return group of significant labels or if that's none, the next largest
     * group (which is under the threshold, but especially for setup gives an
     * idea how close it was).
     */
    return imgs->labelgroup_max ? imgs->labelgroup_max : max_under;
}

/**
 * dilate9
 *      Dilates a 3x3 box.
 */
static int dilate9(unsigned char *img, int width, int height, void *buffer)
{
    /*
     * - row1, row2 and row3 represent lines in the temporary buffer.
     * - Window is a sliding window containing max values of the columns
     *   in the 3x3 matrix.
     * - width is an index into the sliding window (this is faster than
     *   doing modulo 3 on i).
     * - blob keeps the current max value.
     */
    int y, i, sum = 0, widx;
    unsigned char *row1, *row2, *row3, *rowTemp,*yp;
    unsigned char window[3], blob, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = buffer;
    row2 = row1 + width;
    row3 = row2 + width;

    /* Init rows 2 and 3. */
    memset(row2, 0, width);
    memcpy(row3, img, width);

    /* Pointer to the current row in img. */
    yp = img;

    for (y = 0; y < height; y++) {
        /* Move down one step; row 1 becomes the previous row 2 and so on. */
        rowTemp = row1;
        row1 = row2;
        row2 = row3;
        row3 = rowTemp;

        /* If we're at the last row, fill with zeros, otherwise copy from img. */
        if (y == height - 1)
            memset(row3, 0, width);
        else
            memcpy(row3, yp+width, width);

        /* Init slots 0 and 1 in the moving window. */
        window[0] = MAX3(row1[0], row2[0], row3[0]);
        window[1] = MAX3(row1[1], row2[1], row3[1]);

        /* Init blob to the current max, and set window index. */
        blob = MAX2(window[0], window[1]);
        widx = 2;

        /*
         * Iterate over the current row; index i is off by one to eliminate
         * a lot of +1es in the loop.
         */
        for (i = 2; i <= width - 1; i++) {
            /* Get the max value of the next column in the 3x3 matrix. */
            latest = window[widx] = MAX3(row1[i], row2[i], row3[i]);

            /*
             * If the value is larger than the current max, use it. Otherwise,
             * calculate a new max (because the new value may not be the max.
             */
            if (latest >= blob)
                blob = latest;
            else
                blob = MAX3(window[0], window[1], window[2]);

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i - 1) = blob;
                sum++;
            }

            /* Wrap around the window index if necessary. */
            if (++widx == 3)
                widx = 0;
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }

    return sum;
}

/**
 * dilate5
 *      Dilates a + shape.
 */
static int dilate5(unsigned char *img, int width, int height, void *buffer)
{
    /*
     * - row1, row2 and row3 represent lines in the temporary buffer.
     * - mem holds the max value of the overlapping part of two + shapes.
     */
    int y, i, sum = 0;
    unsigned char *row1, *row2, *row3, *rowTemp, *yp;
    unsigned char blob, mem, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = buffer;
    row2 = row1 + width;
    row3 = row2 + width;

    /* Init rows 2 and 3. */
    memset(row2, 0, width);
    memcpy(row3, img, width);

    /* Pointer to the current row in img. */
    yp = img;

    for (y = 0; y < height; y++) {
        /* Move down one step; row 1 becomes the previous row 2 and so on. */
        rowTemp = row1;
        row1 = row2;
        row2 = row3;
        row3 = rowTemp;

        /* If we're at the last row, fill with zeros, otherwise copy from img. */
        if (y == height - 1)
            memset(row3, 0, width);
        else
            memcpy(row3, yp + width, width);

        /* Init mem and set blob to force an evaluation of the entire + shape. */
        mem = MAX2(row2[0], row2[1]);
        blob = 1; /* dummy value, must be > 0 */

        for (i = 1; i < width - 1; i++) {
            /* Get the max value of the "right edge" of the + shape. */
            latest = MAX3(row1[i], row2[i + 1], row3[i]);

            if (blob == 0) {
                /* In case the last blob is zero, only latest matters. */
                blob = latest;
                mem = row2[i + 1];
            } else {
                /* Otherwise, we have to check both latest and mem. */
                blob = MAX2(mem, latest);
                mem = MAX2(row2[i], row2[i + 1]);
            }

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i) = blob;
                sum++;
            }
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }
    return sum;
}

/**
 * erode9
 *      Erodes a 3x3 box.
 */
static int erode9(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{
    int y, i, sum = 0;
    char *Row1,*Row2,*Row3;

    Row1 = buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);

        if (y == height-1)
            memset(Row3, flag, width);
        else
            memcpy(Row3, img + (y+1) * width, width);

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i - 1] == 0 ||
                Row1[i]     == 0 ||
                Row1[i + 1] == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i - 1] == 0 ||
                Row3[i]     == 0 ||
                Row3[i + 1] == 0)
                img[y * width + i] = 0;
            else
                sum++;
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/**
 * erode5
 *      Erodes in a + shape.
 */
static int erode5(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{
    int y, i, sum = 0;
    char *Row1,*Row2,*Row3;

    Row1 = buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);

        if (y == height-1)
            memset(Row3, flag, width);
        else
            memcpy(Row3, img + (y + 1) * width, width);

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i]     == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i]     == 0)
                img[y * width + i] = 0;
            else
                sum++;
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/**
 * alg_despeckle
 *      Despeckling routine to remove noisy detections.
 */
int alg_despeckle(struct ctx_cam *cam, int olddiffs)
{
    int diffs = 0;
    unsigned char *out = cam->imgs.image_motion.image_norm;
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    int done = 0, i, len = strlen(cam->conf.despeckle_filter);
    unsigned char *common_buffer = cam->imgs.common_buffer;

    for (i = 0; i < len; i++) {
        switch (cam->conf.despeckle_filter[i]) {
        case 'E':
            if ((diffs = erode9(out, width, height, common_buffer, 0)) == 0)
                i = len;
            done = 1;
            break;
        case 'e':
            if ((diffs = erode5(out, width, height, common_buffer, 0)) == 0)
                i = len;
            done = 1;
            break;
        case 'D':
            diffs = dilate9(out, width, height, common_buffer);
            done = 1;
            break;
        case 'd':
            diffs = dilate5(out, width, height, common_buffer);
            done = 1;
            break;
        /* No further despeckle after labeling! */
        case 'l':
            diffs = alg_labeling(cam);
            i = len;
            done = 2;
            break;
        }
    }

    /* If conf.despeckle_filter contains any valid action EeDdl */
    if (done) {
        if (done != 2)
            cam->imgs.labelsize_max = 0; // Disable Labeling
        return diffs;
    } else {
        cam->imgs.labelsize_max = 0; // Disable Labeling
    }

    return olddiffs;
}

/**
 * alg_tune_smartmask
 *      Generates actual smartmask. Calculate sensitivity based on motion.
 */
void alg_tune_smartmask(struct ctx_cam *cam)
{
    int i, diff;
    int motionsize = cam->imgs.motionsize;
    unsigned char *smartmask = cam->imgs.smartmask;
    unsigned char *smartmask_final = cam->imgs.smartmask_final;
    int *smartmask_buffer = cam->imgs.smartmask_buffer;
    int sensitivity = cam->lastrate * (11 - cam->smartmask_speed);

    for (i = 0; i < motionsize; i++) {
        /* Decrease smart_mask sensitivity every 5*speed seconds only. */
        if (smartmask[i] > 0)
            smartmask[i]--;
        /* Increase smart_mask sensitivity based on the buffered values. */
        diff = smartmask_buffer[i]/sensitivity;

        if (diff) {
            if (smartmask[i] <= diff + 80)
                smartmask[i] += diff;
            else
                smartmask[i] = 80;
            smartmask_buffer[i] %= sensitivity;
        }
        /* Transfer raw mask to the final stage when above trigger value. */
        if (smartmask[i] > 20)
            smartmask_final[i] = 0;
        else
            smartmask_final[i] = 255;
    }
    /* Further expansion (here:erode due to inverted logic!) of the mask. */
    diff = erode9(smartmask_final, cam->imgs.width, cam->imgs.height,
                  cam->imgs.common_buffer, 255);
    diff = erode5(smartmask_final, cam->imgs.width, cam->imgs.height,
                  cam->imgs.common_buffer, 255);
}

/* Increment for *smartmask_buffer in alg_diff_standard. */
#define SMARTMASK_SENSITIVITY_INCR 5

/**
 * alg_diff_standard
 *
 */
int alg_diff_standard(struct ctx_cam *cam, unsigned char *new)
{
    struct ctx_images *imgs = &cam->imgs;
    int i, diffs = 0;
    int noise = cam->noise;
    int smartmask_speed = cam->smartmask_speed;
    unsigned char *ref = imgs->ref;
    unsigned char *out = imgs->image_motion.image_norm;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask_final = imgs->smartmask_final;
    int *smartmask_buffer = imgs->smartmask_buffer;

    i = imgs->motionsize;
    memset(out + i, 128, i / 2); /* Motion pictures are now b/w i.o. green */
    memset(out, 0, i);

    for (; i > 0; i--) {
        register unsigned char curdiff = (int)(abs(*ref - *new)); /* Using a temp variable is 12% faster. */
        /* Apply fixed mask */
        if (mask)
            curdiff = ((int)(curdiff * *mask++) / 255);

        if (smartmask_speed) {
            if (curdiff > noise) {
                /*
                 * Increase smart_mask sensitivity every frame when motion
                 * is detected. (with speed=5, mask is increased by 1 every
                 * second. To be able to increase by 5 every second (with
                 * speed=10) we add 5 here. NOT related to the 5 at ratio-
                 * calculation.
                 */
                if (cam->event_nr != cam->prev_event)
                    (*smartmask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                /* Apply smart_mask */
                if (!*smartmask_final)
                    curdiff = 0;
            }
            smartmask_final++;
            smartmask_buffer++;
        }
        /* Pixel still in motion after all the masks? */
        if (curdiff > noise) {
            *out = *new;
            diffs++;
        }
        out++;
        ref++;
        new++;
    }
    return diffs;
}

/**
 * alg_diff_fast
 *      Very fast diff function, does not apply mask overlaying.
 */
static char alg_diff_fast(struct ctx_cam *cam, int max_n_changes, unsigned char *new)
{
    struct ctx_images *imgs = &cam->imgs;
    int i, diffs = 0, step = imgs->motionsize/10000;
    int noise = cam->noise;
    unsigned char *ref = imgs->ref;

    if (!step % 2)
        step++;
    /* We're checking only 1 of several pixels. */
    max_n_changes /= step;

    i = imgs->motionsize;

    for (; i > 0; i -= step) {
        register unsigned char curdiff = (int)(abs((char)(*ref - *new))); /* Using a temp variable is 12% faster. */
        if (curdiff >  noise) {
            diffs++;
            if (diffs > max_n_changes)
                return 1;
        }
        ref += step;
        new += step;
    }

    return 0;
}

/**
 * alg_diff
 *      Uses diff_fast to quickly decide if there is anything worth
 *      sending to diff_standard.
 */
int alg_diff(struct ctx_cam *cam, unsigned char *new)
{
    int diffs = 0;

    if (alg_diff_fast(cam, cam->conf.threshold / 2, new))
        diffs = alg_diff_standard(cam, new);

    return diffs;
}

/**
 * alg_lightswitch
 *      Detects a sudden massive change in the picture.
 *      It is assumed to be the light being switched on or a camera displacement.
 *      In any way the user doesn't think it is worth capturing.
 */
int alg_lightswitch(struct ctx_cam *cam, int diffs)
{
    struct ctx_images *imgs = &cam->imgs;

    if (cam->conf.lightswitch_percent < 0)
        cam->conf.lightswitch_percent = 0;
    if (cam->conf.lightswitch_percent > 100)
        cam->conf.lightswitch_percent = 100;

    /* Is lightswitch percent of the image changed? */
    if (diffs > (imgs->motionsize * cam->conf.lightswitch_percent / 100))
        return 1;

    return 0;
}

/**
 * alg_switchfilter
 *
 */
int alg_switchfilter(struct ctx_cam *cam, int diffs, unsigned char *newimg)
{
    int linediff = diffs / cam->imgs.height;
    unsigned char *out = cam->imgs.image_motion.image_norm;
    int y, x, line;
    int lines = 0, vertlines = 0;

    for (y = 0; y < cam->imgs.height; y++) {
        line = 0;
        for (x = 0; x < cam->imgs.width; x++) {
            if (*(out++))
                line++;
        }

        if (line > cam->imgs.width / 18)
            vertlines++;

        if (line > linediff * 2)
            lines++;
    }

    if (vertlines > cam->imgs.height / 10 && lines < vertlines / 3 &&
        (vertlines > cam->imgs.height / 4 || lines - vertlines > lines / 2)) {
        if (cam->conf.text_changes) {
            char tmp[80];
            sprintf(tmp, "%d %d", lines, vertlines);
            draw_text(newimg, cam->imgs.width, cam->imgs.height, cam->imgs.width - 10, 20, tmp, cam->conf.text_scale);
        }
        return diffs;
    }
    return 0;
}

/**
 * alg_update_reference_frame
 *
 *   Called from 'motion_loop' to calculate the reference frame
 *   Moving objects are excluded from the reference frame for a certain
 *   amount of time to improve detection.
 *
 * Parameters:
 *
 *   cam    - current thread's context struct
 *   action - UPDATE_REF_FRAME or RESET_REF_FRAME
 *
 */
#define ACCEPT_STATIC_OBJECT_TIME 10  /* Seconds */
#define EXCLUDE_LEVEL_PERCENT 20
void alg_update_reference_frame(struct ctx_cam *cam, int action)
{
    int accept_timer = cam->lastrate * ACCEPT_STATIC_OBJECT_TIME;
    int i, threshold_ref;
    int *ref_dyn = cam->imgs.ref_dyn;
    unsigned char *image_virgin = cam->imgs.image_vprvcy;
    unsigned char *ref = cam->imgs.ref;
    unsigned char *smartmask = cam->imgs.smartmask_final;
    unsigned char *out = cam->imgs.image_motion.image_norm;

    if (cam->lastrate > 5)  /* Match rate limit */
        accept_timer /= (cam->lastrate / 3);

    if (action == UPDATE_REF_FRAME) { /* Black&white only for better performance. */
        threshold_ref = cam->noise * EXCLUDE_LEVEL_PERCENT / 100;

        for (i = cam->imgs.motionsize; i > 0; i--) {
            /* Exclude pixels from ref frame well below noise level. */
            if (((int)(abs(*ref - *image_virgin)) > threshold_ref) && (*smartmask)) {
                if (*ref_dyn == 0) { /* Always give new pixels a chance. */
                    *ref_dyn = 1;
                } else if (*ref_dyn > accept_timer) { /* Include static Object after some time. */
                    *ref_dyn = 0;
                    *ref = *image_virgin;
                } else if (*out) {
                    (*ref_dyn)++; /* Motionpixel? Keep excluding from ref frame. */
                } else {
                    *ref_dyn = 0; /* Nothing special - release pixel. */
                    *ref = (*ref + *image_virgin) / 2;
                }

            } else {  /* No motion: copy to ref frame. */
                *ref_dyn = 0; /* Reset pixel */
                *ref = *image_virgin;
            }

            ref++;
            image_virgin++;
            smartmask++;
            ref_dyn++;
            out++;
        } /* end for i */

    } else {   /* action == RESET_REF_FRAME - also used to initialize the frame at startup. */
        /* Copy fresh image */
        memcpy(cam->imgs.ref, cam->imgs.image_vprvcy, cam->imgs.size_norm);
        /* Reset static objects */
        memset(cam->imgs.ref_dyn, 0, cam->imgs.motionsize * sizeof(*cam->imgs.ref_dyn));
    }
}
