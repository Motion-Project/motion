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
#include "util.hpp"
#include "alg.hpp"
#include "draw.hpp"
#include "logger.hpp"

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))
#define NORM               100
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x)-(y)))
#define NDIFF(x, y)        (ABS(x) * NORM / (ABS(x) + 2 * DIFF(x, y)))
#define MAXS 10000               /* max depth of stack */
#define EXCLUDE_LEVEL_PERCENT 20
/* Increment for *smartmask_buffer in alg_diff_standard. */
#define SMARTMASK_SENSITIVITY_INCR 5

typedef struct {
    short y, xl, xr, dy;
} Segment;


void alg_noise_tune(ctx_cam *cam)
{
    struct ctx_images *imgs = &cam->imgs;
    int i;
    unsigned char *ref = imgs->ref;
    int diff, sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;
    unsigned char *new_img = cam->imgs.image_vprvcy;


    i = imgs->motionsize;

    for (; i > 0; i--) {
        diff = ABS(*ref - *new_img);

        if (mask) {
            diff = ((diff * *mask++) / 255);
        }

        if (*smartmask) {
            sum += diff + 1;
            count++;
        }

        ref++;
        new_img++;
        smartmask++;
    }

    if (count > 3)  {
        /* Avoid divide by zero. */
        sum /= count / 3;
    }

    /* 5: safe, 4: regular, 3: more sensitive */
    cam->noise = 4 + (cam->noise + sum) / 2;
}

void alg_threshold_tune(ctx_cam *cam)
{
    int i, top;
    int sum = 0;
    int diffs = cam->current_image->diffs;
    int motion = cam->detecting_motion;

    if (!diffs) {
        return;
    }

    top = diffs;

    if (motion) {
        diffs = cam->threshold / 4;
    }

    for (i = 0; i < THRESHOLD_TUNE_LENGTH - 1; i++) {
        sum += cam->diffs_last[i];

        if (cam->diffs_last[i + 1] && !motion) {
            cam->diffs_last[i] = cam->diffs_last[i + 1];
        } else {
            cam->diffs_last[i] = cam->threshold / 4;
        }

        if (cam->diffs_last[i] > top) {
            top = cam->diffs_last[i];
        }
    }

    sum += cam->diffs_last[i];
    cam->diffs_last[i] = diffs;

    sum /= THRESHOLD_TUNE_LENGTH / 4;

    if (sum < top * 2) {
        sum = top * 2;
    }

    if (sum < cam->conf->threshold) {
        cam->threshold = (cam->threshold + sum) / 2;
    }
}

/*
 * Labeling by Joerg Weber. Based on an idea from Hubert Mara.
 * Floodfill enhanced by Ian McConnel based on code from
 * http://www.acm.org/pubs/tog/GraphicsGems/
 * http://www.codeproject.com/gdi/QuickFill.asp

 * Filled horizontal segment of scanline y for xl <= x <= xr.
 * Parent segment was on line y - dy.  dy = 1 or -1
 */
#define PUSH(Y, XL, XR, DY)     /* push new segment on stack */  \
        if (sp<stack+MAXS && Y+(DY) >= 0 && Y+(DY) < height)     \
        {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)      /* pop segment off stack */      \
        {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}

static int alg_iflood(int x, int y, int width, int height,
        unsigned char *out, int *labels, int newvalue, int oldvalue)
{
    int l, x1, x2, dy;
    Segment stack[MAXS], *sp = stack; /* Stack of filled segments. */
    int count = 0;

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0;
    }

    PUSH(y, x, x, 1);      /* Needed in some cases. */
    PUSH(y + 1, x, x, -1); /* Seed segment (popped 1st). */

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

        if (x >= x1) {
            goto skip;
        }

        l = x + 1;

        if (l < x1) {
            PUSH(y, l, x1 - 1, -dy); /* Leak on left? */
        }

        x = x1 + 1;

        do {
            for (; x < width && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x++) {
                labels[y * width + x] = newvalue;
                count++;
            }

            PUSH(y, l, x - 1, dy);

            if (x > x2 + 1) {
                PUSH(y, x2 + 1, x - 1, -dy); /* Leak on right? */
            }

        skip:
            for (x++; x <= x2 && !(out[y * width + x] != 0 && labels[y * width + x] == oldvalue); x++);
            l = x;
        } while (x <= x2);
    }
    return count;
}

static int alg_labeling(ctx_cam *cam)
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

            /* Already visited by alg_iflood */
            if (labels[pixelpos] > 0) {
                continue;
            }

            labelsize = alg_iflood(ix, iy, width, height, out, labels, current_label, 0);

            if (labelsize > 0) {
                /* Label above threshold? Mark it again (add 32768 to labelnumber). */
                if (labelsize > cam->threshold) {
                    labelsize = alg_iflood(ix, iy, width, height, out, labels, current_label + 32768, current_label);
                    imgs->labelgroup_max += labelsize;
                    imgs->labels_above++;
                } else if(max_under < labelsize) {
                    max_under = labelsize;
                }

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

    /* Return group of significant labels or if that's none, the next largest
     * group (which is under the threshold, but especially for setup gives an
     * idea how close it was).
     */
    return imgs->labelgroup_max ? imgs->labelgroup_max : max_under;
}

/**  Dilates a 3x3 box. */
static int alg_dilate9(unsigned char *img, int width, int height, void *buffer)
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
    unsigned char *row1, *row2, *row3, *rowTemp, *yp;
    unsigned char window[3], blob, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = (unsigned char *)buffer;
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
        if (y == height - 1) {
            memset(row3, 0, width);
        } else {
            memcpy(row3, yp + width, width);
        }

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
            if (latest >= blob) {
                blob = latest;
            } else {
                blob = MAX3(window[0], window[1], window[2]);
            }

            /* Write the max value (blob) to the image. */
            if (blob != 0) {
                *(yp + i - 1) = blob;
                sum++;
            }

            /* Wrap around the window index if necessary. */
            if (++widx == 3) {
                widx = 0;
            }
        }

        /* Store zeros in the vertical sides. */
        *yp = *(yp + width - 1) = 0;
        yp += width;
    }

    return sum;
}

/**  Dilates a + shape. */
static int alg_dilate5(unsigned char *img, int width, int height, void *buffer)
{
    /*
     * - row1, row2 and row3 represent lines in the temporary buffer.
     * - mem holds the max value of the overlapping part of two + shapes.
     */
    int y, i, sum = 0;
    unsigned char *row1, *row2, *row3, *rowTemp, *yp;
    unsigned char blob, mem, latest;

    /* Set up row pointers in the temporary buffer. */
    row1 = (unsigned char *)buffer;
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
        if (y == height - 1) {
            memset(row3, 0, width);
        } else {
            memcpy(row3, yp + width, width);
        }

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

/**  Erodes a 3x3 box. */
static int alg_erode9(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{
    int y, i, sum = 0;
    char *Row1, *Row2, *Row3;

    Row1 = (char *)buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);

        if (y == height - 1) {
            memset(Row3, flag, width);
        } else {
            memcpy(Row3, img + (y + 1) * width, width);
        }

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i - 1] == 0 ||
                Row1[i]     == 0 ||
                Row1[i + 1] == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i - 1] == 0 ||
                Row3[i]     == 0 ||
                Row3[i + 1] == 0) {
                img[y * width + i] = 0;
            } else {
                sum++;
            }
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/* Erodes in a + shape. */
static int alg_erode5(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{
    int y, i, sum = 0;
    char *Row1, *Row2, *Row3;

    Row1 = (char *)buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2 * width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);

    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);

        if (y == height - 1) {
            memset(Row3, flag, width);
        } else {
            memcpy(Row3, img + (y + 1) * width, width);
        }

        for (i = width - 2; i >= 1; i--) {
            if (Row1[i]     == 0 ||
                Row2[i - 1] == 0 ||
                Row2[i]     == 0 ||
                Row2[i + 1] == 0 ||
                Row3[i]     == 0) {
                img[y * width + i] = 0;
            } else {
                sum++;
            }
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

static void alg_despeckle(ctx_cam *cam)
{
    int diffs, width, height, done, i, len;
    unsigned char *out, *common_buffer;

    if ((cam->conf->despeckle_filter == "") || cam->current_image->diffs <= 0) {
        if (cam->imgs.labelsize_max) {
            cam->imgs.labelsize_max = 0;
        }
        return;
    }

    diffs = 0;
    out = cam->imgs.image_motion.image_norm;
    width = cam->imgs.width;
    height = cam->imgs.height;
    done = 0;
    len = cam->conf->despeckle_filter.length();
    common_buffer = cam->imgs.common_buffer;
    cam->current_image->total_labels = 0;
    cam->imgs.largest_label = 0;
    cam->olddiffs = cam->current_image->diffs;

    for (i = 0; i < len; i++) {
        switch (cam->conf->despeckle_filter[i]) {
        case 'E':
            diffs = alg_erode9(out, width, height, common_buffer, 0);
            if (diffs == 0) {
                i = len;
            }
            done = 1;
            break;
        case 'e':
            diffs = alg_erode5(out, width, height, common_buffer, 0);
            if (diffs == 0) {
                i = len;
            }
            done = 1;
            break;
        case 'D':
            diffs = alg_dilate9(out, width, height, common_buffer);
            done = 1;
            break;
        case 'd':
            diffs = alg_dilate5(out, width, height, common_buffer);
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
        if (done != 2) {
            cam->imgs.labelsize_max = 0; // Disable Labeling
        }
        cam->current_image->diffs = diffs;
        return;
    } else {
        cam->imgs.labelsize_max = 0; // Disable Labeling
    }
    cam->current_image->diffs = cam->olddiffs;
    return;
}

void alg_tune_smartmask(ctx_cam *cam)
{
    int i, diff;
    int motionsize = cam->imgs.motionsize;
    unsigned char *smartmask = cam->imgs.smartmask;
    unsigned char *smartmask_final = cam->imgs.smartmask_final;
    int *smartmask_buffer = cam->imgs.smartmask_buffer;
    int sensitivity = cam->lastrate * (11 - cam->smartmask_speed);

    if (!cam->smartmask_speed ||
        (cam->event_nr == cam->prev_event) ||
        (--cam->smartmask_count)) {
        return;
    }

    for (i = 0; i < motionsize; i++) {
        /* Decrease smart_mask sensitivity every 5*speed seconds only. */
        if (smartmask[i] > 0) {
            smartmask[i]--;
        }
        /* Increase smart_mask sensitivity based on the buffered values. */
        diff = smartmask_buffer[i] / sensitivity;

        if (diff) {
            if (smartmask[i] <= diff + 80) {
                smartmask[i] += diff;
            } else {
                smartmask[i] = 80;
            }
            smartmask_buffer[i] %= sensitivity;
        }
        /* Transfer raw mask to the final stage when above trigger value. */
        if (smartmask[i] > 20) {
            smartmask_final[i] = 0;
        } else {
            smartmask_final[i] = 255;
        }
    }
    /* Further expansion (here:erode due to inverted logic!) of the mask. */
    diff = alg_erode9(smartmask_final, cam->imgs.width, cam->imgs.height,
                      cam->imgs.common_buffer, 255);
    diff = alg_erode5(smartmask_final, cam->imgs.width, cam->imgs.height,
                      cam->imgs.common_buffer, 255);
    cam->smartmask_count = cam->smartmask_ratio;
}

/* Increment for *smartmask_buffer in alg_diff_standard. */
#define SMARTMASK_SENSITIVITY_INCR 5

static void alg_diff_nomask(ctx_cam *cam)
{
    unsigned char *ref = cam->imgs.ref;
    unsigned char *out = cam->imgs.image_motion.image_norm;
    unsigned char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int lrgchg = cam->conf->threshold_ratio_change;

    memset(out + imgsz, 128, (imgsz / 2));
    memset(out, 0, imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }
        out++;
        ref++;
        new_img++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

static void alg_diff_mask(ctx_cam *cam)
{
    unsigned char *ref  = cam->imgs.ref;
    unsigned char *out  = cam->imgs.image_motion.image_norm;
    unsigned char *mask = cam->imgs.mask;
    unsigned char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int lrgchg = cam->conf->threshold_ratio_change;

    memset(out + imgsz, 128, (imgsz / 2));
    memset(out, 0, imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (mask) {
            curdiff = ((curdiff * *mask) / 255);
        }

        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }

        out++;
        ref++;
        new_img++;
        mask++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

static void alg_diff_smart(ctx_cam *cam)
{

    unsigned char *ref  = cam->imgs.ref;
    unsigned char *out  = cam->imgs.image_motion.image_norm;
    unsigned char *smartmask_final = cam->imgs.smartmask_final;
    unsigned char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int smartmask_speed = cam->smartmask_speed;
    int *smartmask_buffer = cam->imgs.smartmask_buffer;
    int lrgchg = cam->conf->threshold_ratio_change;

    imgsz = cam->imgs.motionsize;
    memset(out + imgsz, 128, (imgsz / 2));
    memset(out, 0, imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (smartmask_speed) {
            if (abs(curdiff) > noise) {
                if (cam->event_nr != cam->prev_event) {
                    (*smartmask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                }
                if (!*smartmask_final) {
                    curdiff = 0;
                }
            }
            smartmask_final++;
            smartmask_buffer++;
        }
        /* Pixel still in motion after all the masks? */
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }
        out++;
        ref++;
        new_img++;
    }
    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }
}

static void alg_diff_masksmart(ctx_cam *cam)
{
    unsigned char *ref = cam->imgs.ref;
    unsigned char *out = cam->imgs.image_motion.image_norm;
    unsigned char *mask = cam->imgs.mask;
    unsigned char *smartmask_final = cam->imgs.smartmask_final;
    unsigned char *new_img = cam->imgs.image_vprvcy;

    int i, curdiff;
    int imgsz = cam->imgs.motionsize;
    int diffs = 0, diffs_net = 0;
    int noise = cam->noise;
    int smartmask_speed = cam->smartmask_speed;
    int *smartmask_buffer = cam->imgs.smartmask_buffer;
    int lrgchg = cam->conf->threshold_ratio_change;

    imgsz= cam->imgs.motionsize;
    memset(out + imgsz, 128, (imgsz / 2));
    memset(out, 0, imgsz);

    for (i = 0; i < imgsz; i++) {
        curdiff = (*ref - *new_img);
        if (mask) {
            curdiff = ((curdiff * *mask) / 255);
        }

        if (smartmask_speed) {
            if (abs(curdiff) > noise) {
                if (cam->event_nr != cam->prev_event) {
                    (*smartmask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                }
                if (!*smartmask_final) {
                    curdiff = 0;
                }
            }
            smartmask_final++;
            smartmask_buffer++;
        }

        /* Pixel still in motion after all the masks? */
        if (abs(curdiff) > noise) {
            *out = *new_img;
            diffs++;
            if (curdiff > lrgchg) {
                diffs_net++;
            } else if (curdiff < -lrgchg) {
                diffs_net--;
            }
        }

        out++;
        ref++;
        new_img++;
        mask++;
    }

    cam->current_image->diffs_raw = diffs;
    cam->current_image->diffs = diffs;

    if (diffs > 0 ) {
        cam->current_image->diffs_ratio = (abs(diffs_net) * 100) / diffs;
    } else {
        cam->current_image->diffs_ratio = 100;
    }

}

static bool alg_diff_fast(ctx_cam *cam)
{
    struct ctx_images *imgs = &cam->imgs;
    int i, curdiff, diffs = 0;
    int step = cam->imgs.motionsize / 10000;
    int noise = cam->noise;
    int max_n_changes = cam->conf->threshold / 2;
    unsigned char *ref = imgs->ref;
    unsigned char *new_img = cam->imgs.image_vprvcy;

    if (!step % 2) {
        step++;
    }

    max_n_changes /= step;

    i = imgs->motionsize;

    for (; i > 0; i -= step) {
        curdiff = abs(*ref - *new_img); /* Using a temp variable is 12% faster. */
        if (curdiff >  noise) {
            diffs++;
            if (diffs > max_n_changes) {
               return true;
            }
        }
        ref += step;
        new_img += step;
    }

    return false;
}

static void alg_diff_standard(ctx_cam *cam)
{

    if (cam->smartmask_speed == 0) {
        if (cam->imgs.mask == NULL) {
            alg_diff_nomask(cam);
        } else {
            alg_diff_mask(cam);
        }
    } else {
        if (cam->imgs.mask == NULL) {
            alg_diff_smart(cam);
        } else {
            alg_diff_masksmart(cam);
        }
    }

}

static void alg_lightswitch(ctx_cam *cam)
{

    if (cam->conf->lightswitch_percent >= 1 && !cam->lost_connection) {
        if (cam->current_image->diffs > (cam->imgs.motionsize * cam->conf->lightswitch_percent / 100)) {
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, _("Lightswitch detected"));
            if (cam->frame_skip < (unsigned int)cam->conf->lightswitch_frames) {
                cam->frame_skip = (unsigned int)cam->conf->lightswitch_frames;
            }
            cam->current_image->diffs = 0;
            alg_update_reference_frame(cam, RESET_REF_FRAME);
        }
    }
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
void alg_update_reference_frame(ctx_cam *cam, int action)
{
    int accept_timer = cam->lastrate * cam->conf->static_object_time;
    int i, threshold_ref;
    int *ref_dyn = cam->imgs.ref_dyn;
    unsigned char *image_virgin = cam->imgs.image_vprvcy;
    unsigned char *ref = cam->imgs.ref;
    unsigned char *smartmask = cam->imgs.smartmask_final;
    unsigned char *out = cam->imgs.image_motion.image_norm;

    if (cam->lastrate > 5) {
        /* Match rate limit */
        accept_timer /= (cam->lastrate / 3);
    }

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

/*Copy in new reference frame*/
void alg_new_update_frame(ctx_cam *cam)
{

    /* There used to be a lot more to this function before.....*/
    memcpy(cam->imgs.ref, cam->imgs.image_vprvcy, cam->imgs.size_norm);

}

/*Calculate the center location of changes*/
static void alg_location_center(ctx_cam *cam)
{
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;
    unsigned char *out = cam->imgs.image_motion.image_norm;
    int x, y, centc = 0;

    cent->x = 0;
    cent->y = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                cent->x += x;
                cent->y += y;
                centc++;
            }
        }
    }

    if (centc) {
        cent->x = cent->x / centc;
        cent->y = cent->y / centc;
    }

    /* This allows for the redcross and boxes to be drawn*/
    if (cent->x < 10) {
        cent->x = 15;
    }
    if (cent->y < 10) {
        cent->y = 15;
    }
    if ((cent->x + 10) > width) {
        cent->x = width - 15;
    }
    if ((cent->y + 10) > height) {
        cent->y = height - 15;
    }

}

/*Calculate distribution and variances of changes*/
static void alg_location_dist(ctx_cam *cam)
{
    ctx_images *imgs = &cam->imgs;
    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;
    unsigned char *out = imgs->image_motion.image_norm;
    int x, y, centc = 0, xdist = 0, ydist = 0;
    uint64_t variance_x, variance_y, variance_xy, distance_mean;

    /* Note that the term variance refers to the statistical calulation.  It is
     * not really precise however since we are using integers rather than floats.
     * This is done to improve performance over the statistically correct
     * calculation for mean and variance
     */
    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;
    variance_x = 0;
    variance_y = 0;
    distance_mean = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                variance_x += ((x - cent->x) * (x - cent->x));
                variance_y += ((y - cent->y) * (y - cent->y));
                distance_mean += sqrt(
                        ((x - cent->x) * (x - cent->x)) +
                        ((y - cent->y) * (y - cent->y)));

                if (x > cent->x) {
                    xdist += x - cent->x;
                } else if (x < cent->x) {
                    xdist += cent->x - x;
                }

                if (y > cent->y) {
                    ydist += y - cent->y;
                } else if (y < cent->y) {
                    ydist += cent->y - y;
                }

                centc++;
            }
        }
    }

    if (centc) {
        cent->minx = cent->x - xdist / centc * 3;
        cent->maxx = cent->x + xdist / centc * 3;
        cent->miny = cent->y - ydist / centc * 3;
        cent->maxy = cent->y + ydist / centc * 3;
        cent->stddev_x = sqrt((variance_x / centc));
        cent->stddev_y = sqrt((variance_y / centc));
        distance_mean = (distance_mean / centc);
    } else {
        cent->stddev_y = 0;
        cent->stddev_x = 0;
        distance_mean = 0;
    }

    variance_xy = 0;
    out = imgs->image_motion.image_norm;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (*(out++)) {
                variance_xy += (
                    (sqrt(((x - cent->x) * (x - cent->x)) +
                          ((y - cent->y) * (y - cent->y))) - distance_mean) *
                    (sqrt(((x - cent->x) * (x - cent->x)) +
                          ((y - cent->y) * (y - cent->y))) - distance_mean));
            }
        }
    }
    /* Per statistics, divide by n-1 for calc of a standard deviation */
    if ((centc-1) > 0) {
        cent->stddev_xy = sqrt((variance_xy / (centc-1)));
    }
}

/* Ensure min/max are within limits*/
static void alg_location_minmax(ctx_cam *cam)
{

    int width = cam->imgs.width;
    int height = cam->imgs.height;
    ctx_coord *cent = &cam->current_image->location;

    if (cent->maxx > width - 1) {
        cent->maxx = width - 1;
    } else if (cent->maxx < 0) {
        cent->maxx = 0;
    }

    if (cent->maxy > height - 1) {
        cent->maxy = height - 1;
    } else if (cent->maxy < 0) {
        cent->maxy = 0;
    }

    if (cent->minx > width - 1) {
        cent->minx = width - 1;
    } else if (cent->minx < 0) {
        cent->minx = 0;
    }

    if (cent->miny > height - 1) {
        cent->miny = height - 1;
    } else if (cent->miny < 0) {
        cent->miny = 0;
    }

    /* Align for better locate box handling */
    cent->minx += cent->minx % 2;
    cent->miny += cent->miny % 2;
    cent->maxx -= cent->maxx % 2;
    cent->maxy -= cent->maxy % 2;

    cent->width = cent->maxx - cent->minx;
    cent->height = cent->maxy - cent->miny;
    cent->y = (cent->miny + cent->maxy) / 2;
}

/* Determine the location and standard deviations of changes*/
void alg_location(ctx_cam *cam)
{

    alg_location_center(cam);

    alg_location_dist(cam);

    alg_location_minmax(cam);
}

/* Apply user or default thresholds on standard deviations*/
void alg_stddev(ctx_cam *cam)
{

    MOTION_LOG(DBG, TYPE_ALL, NO_ERRNO, "dev_x %d dev_y %d dev_xy %d, diff %d ratio %d"
        , cam->current_image->location.stddev_x
        , cam->current_image->location.stddev_y
        , cam->current_image->location.stddev_xy
        , cam->current_image->diffs
        , cam->current_image->diffs_ratio);


    if (cam->conf->threshold_sdevx > 0) {
        if (cam->current_image->location.stddev_x > cam->conf->threshold_sdevx) {
            cam->current_image->diffs = 0;
            return;
        }
    } else if (cam->conf->threshold_sdevy > 0) {
        if (cam->current_image->location.stddev_y > cam->conf->threshold_sdevy) {
            cam->current_image->diffs = 0;
            return;
        }
    } else if (cam->conf->threshold_sdevxy > 0) {
        if (cam->current_image->location.stddev_xy > cam->conf->threshold_sdevxy) {
            cam->current_image->diffs = 0;
            return;
        }
    }

    return;

}

void alg_diff(ctx_cam *cam)
{

    if (cam->detecting_motion || cam->motapp->setup_mode) {
        alg_diff_standard(cam);
    } else {
        if (alg_diff_fast(cam)) {
            alg_diff_standard(cam);
        } else {
            cam->current_image->diffs = 0;
            cam->current_image->diffs_raw = 0;
            cam->current_image->diffs_ratio = 100;
        }
    }

    alg_lightswitch(cam);

    alg_despeckle(cam);

    return;

}
