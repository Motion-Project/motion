/*    alg.c
 *
 *    Detect changes in a video stream.
 *    Copyright 2001 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "motion.h"
#include "alg.h"

#ifdef __MMX__
#define HAVE_MMX
#include "mmx.h"
#endif

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))

/* locate the center and size of the movement. */
void alg_locate_center_size(struct images *imgs, int width, int height, struct coord *cent)
{
    unsigned char *out = imgs->out;
    int *labels = imgs->labels;
    int x, y, centc = 0, xdist = 0, ydist = 0;

    cent->x = 0;
    cent->y = 0;
    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;

    /* If Labeling enabled - locate center of largest labelgroup */
    if (imgs->labelsize_max) {
        /* Locate largest labelgroup */
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(labels++)&32768) {
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
    
    /* Now we find the size of the Motion */

    /* First reset pointers back to initial value */
    centc = 0;
    labels = imgs->labels;
    out = imgs->out;

    /* If Labeling then we find the area around largest labelgroup instead */
    if (imgs->labelsize_max) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (*(labels++)&32768) {
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
        /* Make the box a little bigger in y direction to make sure the
           heads fit in so we multiply by 3 instead of 2 which seems to
           to work well in practical */
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
    
    cent->width = cent->maxx - cent->minx;
    cent->height = cent->maxy - cent->miny;
    
    /* We want to center Y coordinate to be the center of the action.
       The head of a person is important so we correct the cent.y coordinate
       to match the correction to include a persons head that we just did above */
    cent->y = (cent->miny + cent->maxy) / 2;
    
}


/* draw a box around the movement */
void alg_draw_location(struct coord *cent, struct images *imgs, int width, unsigned char *new, int mode)
{
    unsigned char *out = imgs->out;
    int x, y;

    out = imgs->out;

    /* Draw a box around the movement */
    if (mode == LOCATE_BOTH){ /* both normal and motion image gets a box */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = x + width_miny;
            int width_maxy_x = x + width_maxy;
            new[width_miny_x]=~new[width_miny_x];
            new[width_maxy_x]=~new[width_maxy_x];
            out[width_miny_x]=~out[width_miny_x];
            out[width_maxy_x]=~out[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx + y * width; 
            int width_maxx_y = cent->maxx + y * width;
            new[width_minx_y]=~new[width_minx_y];
            new[width_maxx_y]=~new[width_maxx_y];
            out[width_minx_y]=~out[width_minx_y];
            out[width_maxx_y]=~out[width_maxx_y];
        }
    } else { /* normal image only (e.g. preview shot) */
        int width_miny = width * cent->miny;
        int width_maxy = width * cent->maxy;
        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = width_miny + x;
            int width_maxy_x = width_maxy + x;
            new[width_miny_x]=~new[width_miny_x];
            new[width_maxy_x]=~new[width_maxy_x];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int minx_y = cent->minx + y * width;
            int maxx_y = cent->maxx + y * width;
            new[minx_y]=~new[minx_y];
            new[maxx_y]=~new[maxx_y];
        }
    }
}



#define NORM               100
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x) - (y)))
#define NDIFF(x, y)        (ABS(x) * NORM/(ABS(x) + 2 * DIFF(x,y)))


void alg_noise_tune(struct context *cnt, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    int i;
    unsigned char *ref = imgs->ref;
    int diff, sum = 0, count = 0;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask = imgs->smartmask_final;

    i = imgs->motionsize;
            
    for (; i>0; i--) {
        diff = ABS(*ref - *new);
        if (mask)
            diff = ((diff * *mask++)/255);
        if (*smartmask){
            sum += diff + 1;
            count++;
        }
        ref++;
        new++;
        smartmask++;
    }

    if (count > 3) /* avoid divide by zero */
        sum /= count / 3;
    
    cnt->noise = 4 + (cnt->noise + sum) / 2;  /* 5: safe, 4: regular, 3: more sensitive */
}

void alg_threshold_tune(struct context *cnt, int diffs, int motion)
{
    int i;
    int sum = 0, top = diffs;

    if (!diffs)
        return;

    if (motion)
        diffs = cnt->threshold / 4;

    for (i = 0; i < THRESHOLD_TUNE_LENGTH - 1; i++) {
        sum += cnt->diffs_last[i];
        if (cnt->diffs_last[i + 1] && !motion)
            cnt->diffs_last[i] = cnt->diffs_last[i + 1];
        else
            cnt->diffs_last[i] = cnt->threshold / 4;

        if (cnt->diffs_last[i] > top)
            top = cnt->diffs_last[i];
    }
    sum += cnt->diffs_last[i];
    cnt->diffs_last[i] = diffs;

    sum /= THRESHOLD_TUNE_LENGTH / 4;
    if (sum < top * 2)
        sum = top * 2;

    if (sum < cnt->conf.max_changes)
        cnt->threshold = (cnt->threshold + sum) / 2;
}

/*
Labeling by Joerg Weber. Based on an idea from Hubert Mara.
Floodfill enhanced by Ian McConnel based on code from
http://www.acm.org/pubs/tog/GraphicsGems/
http://www.codeproject.com/gdi/QuickFill.asp
*/

/*
 * Filled horizontal segment of scanline y for xl<=x<=xr.
 * Parent segment was on line y-dy.  dy=1 or -1
 */

#define MAXS 10000               /* max depth of stack */

#define PUSH(Y, XL, XR, DY)     /* push new segment on stack */ \
        if (sp<stack+MAXS && Y+(DY) >= 0 && Y+(DY) < height) \
        {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)      /* pop segment off stack */ \
        {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}

typedef struct {short y, xl, xr, dy;} Segment;


static int iflood(int x, int y,       
                  int width, int height, unsigned char *out, int *labels, int newvalue, int oldvalue)
{
    int l, x1, x2, dy;
    Segment stack[MAXS], *sp = stack;    /* stack of filled segments */
    int count = 0;

    if (x < 0 || x >= width || y < 0 || y >= height)
        return 0;

    PUSH(y, x, x, 1);             /* needed in some cases */
    PUSH(y+1, x, x, -1);          /* seed segment (popped 1st) */

    while (sp > stack) {
        /* pop segment off stack and fill a neighboring scan line */
        POP(y, x1, x2, dy);
        /*
         * segment of scan line y-dy for x1<=x<=x2 was previously filled,
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
            PUSH(y, l, x1-1, -dy);  /* leak on left? */
        
        x = x1 + 1;
        
        do {
            for (; x < width && out[y * width + x] != 0 && labels[y * width + x] == oldvalue; x++) {
                labels[y * width + x] = newvalue;
                count++;
            }
            
            PUSH(y, l, x - 1, dy);
            
            if (x > x2 + 1)
                PUSH(y, x2 + 1, x - 1, -dy);  /* leak on right? */
            
            skip:
            
            for (x++; x <= x2 && !(out[y * width + x] != 0 && labels[y * width + x] == oldvalue); x++);
            
            l = x;
        } while (x <= x2);
    }
    return count;
}

static int alg_labeling(struct context *cnt)
{
    struct images *imgs = &cnt->imgs;
    unsigned char *out = imgs->out;
    int *labels = imgs->labels;
    int ix, iy, pixelpos;
    int width = imgs->width;
    int height = imgs->height;
    int labelsize = 0;
    int current_label = 2;
    cnt->current_image->total_labels = 0;
    imgs->labelsize_max = 0;
    /* ALL labels above threshold are counted as labelgroup */
    imgs->labelgroup_max = 0;
    imgs->labels_above = 0;

    /* init: 0 means no label set / not checked */
    memset(labels, 0, width * height * sizeof(labels));
    pixelpos = 0;
    for (iy = 0; iy < height - 1; iy++) {
        for (ix = 0; ix < width - 1; ix++, pixelpos++) {
            /* no motion - no label */
            if (out[pixelpos] == 0) {
                labels[pixelpos] = 1;
                continue;
            }
            
            /* already visited by iflood */
            if (labels[pixelpos] > 0)
                continue;
            labelsize=iflood(ix, iy, width, height, out, labels, current_label, 0);
            
            if (labelsize > 0) {
                /* Label above threshold? Mark it again (add 32768 to labelnumber) */
                if (labelsize > cnt->threshold) {
                    labelsize=iflood(ix, iy, width, height, out, labels, current_label + 32768, current_label);
                    imgs->labelgroup_max += labelsize;
                    imgs->labels_above++;
                }
                
                if (imgs->labelsize_max < labelsize) {
                    imgs->labelsize_max=labelsize;
                    imgs->largest_label=current_label;
                }
                
                cnt->current_image->total_labels++;
                current_label++;
            }
        }
        pixelpos++; /* compensate for ix<width-1 */
    }    
    /* return group of significant labels */
    return imgs->labelgroup_max;
}

/* Dilates a 3x3 box */
static int dilate9(unsigned char *img, int width, int height, void *buffer)
{
    /* - row1, row2 and row3 represent lines in the temporary buffer 
     * - window is a sliding window containing max values of the columns
     *   in the 3x3 matrix
     * - widx is an index into the sliding window (this is faster than 
     *   doing modulo 3 on i)
     * - blob keeps the current max value
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
            memcpy(row3, yp + width, width);
        
        /* Init slots 0 and 1 in the moving window. */
        window[0] = MAX3(row1[0], row2[0], row3[0]);
        window[1] = MAX3(row1[1], row2[1], row3[1]);

        /* Init blob to the current max, and set window index. */
        blob = MAX2(window[0], window[1]);
        widx = 2;

        /* Iterate over the current row; index i is off by one to eliminate
         * a lot of +1es in the loop.
         */
        for (i = 2; i <= width - 1; i++) {
            /* Get the max value of the next column in the 3x3 matrix. */
            latest = window[widx] = MAX3(row1[i], row2[i], row3[i]);

            /* If the value is larger than the current max, use it. Otherwise,
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

/* Dilates a + shape */
static int dilate5(unsigned char *img, int width, int height, void *buffer)
{
    /* - row1, row2 and row3 represent lines in the temporary buffer 
     * - mem holds the max value of the overlapping part of two + shapes
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
            memcpy(row3, yp+width, width);

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

/* Erodes a 3x3 box */
static int erode9(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{
    int y, i, sum = 0;
    char *Row1,*Row2,*Row3;
    Row1 = buffer;
    Row2 = Row1 + width;
    Row3 = Row1 + 2*width;
    memset(Row2, flag, width);
    memcpy(Row3, img, width);
    for (y = 0; y < height; y++) {
        memcpy(Row1, Row2, width);
        memcpy(Row2, Row3, width);
        if (y == height - 1)
            memset(Row3, flag, width);
        else
            memcpy(Row3, img+(y + 1) * width, width);

        for (i = width-2; i >= 1; i--) {
            if (Row1[i-1] == 0 ||
                Row1[i]   == 0 ||
                Row1[i+1] == 0 ||
                Row2[i-1] == 0 ||
                Row2[i]   == 0 ||
                Row2[i+1] == 0 ||
                Row3[i-1] == 0 ||
                Row3[i]   == 0 ||
                Row3[i+1] == 0)
                img[y * width + i] = 0;
            else
                sum++;
        }
        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/* Erodes in a + shape */
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

        for (i = width-2; i >= 1; i--) {
            if (Row1[i]   == 0 ||
                Row2[i-1] == 0 ||
                Row2[i]   == 0 ||
                Row2[i+1] == 0 ||
                Row3[i]   == 0)
                img[y * width + i] = 0;
            else
                sum++;
        }

        img[y * width] = img[y * width + width - 1] = flag;
    }
    return sum;
}

/* 
 * Despeckling routine to remove noisy detections.
 */
int alg_despeckle(struct context *cnt, int olddiffs)
{
    int diffs = 0;
    unsigned char *out = cnt->imgs.out;
    int width = cnt->imgs.width;
    int height= cnt->imgs.height;
    int done = 0, i, len = strlen(cnt->conf.despeckle);
    unsigned char *common_buffer = cnt->imgs.common_buffer;

    for (i = 0; i < len; i++) {
        switch (cnt->conf.despeckle[i]) {
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
        /* no further despeckle after labeling! */
        case 'l':
            diffs = alg_labeling(cnt);
            i = len;
            done = 2;
            break;
        }
    }

    /* If conf.despeckle contains any valid action EeDdl */
    if (done) {
        if (done != 2) 
            cnt->imgs.labelsize_max = 0; // Disable Labeling
        return diffs;
    } else {
        cnt->imgs.labelsize_max = 0; // Disable Labeling
    }

    return olddiffs;
}

/* Generate actual smartmask. Calculate sensitivity based on motion */
void alg_tune_smartmask(struct context *cnt)
{
    int i, diff;
    
    int motionsize = cnt->imgs.motionsize;
    unsigned char *smartmask = cnt->imgs.smartmask;
    unsigned char *smartmask_final = cnt->imgs.smartmask_final;
    int *smartmask_buffer = cnt->imgs.smartmask_buffer;
    int sensitivity=cnt->lastrate*(11-cnt->smartmask_speed);

    for (i = 0; i < motionsize; i++) {
    
        /* Decrease smart_mask sensitivity every 5*speed seconds only */
        if (smartmask[i] > 0)
            smartmask[i]--;
        /* Increase smart_mask sensitivity based on the buffered values */
        diff = smartmask_buffer[i]/sensitivity;
        if (diff) {
            if (smartmask[i] <= diff + 80)
                smartmask[i] += diff;
            else
                smartmask[i]=80;
            smartmask_buffer[i]%=sensitivity;
        }
        /* Transfer raw mask to the final stage when above trigger value */
        if (smartmask[i] > 20)
            smartmask_final[i] = 0;
        else
            smartmask_final[i] = 255;
    }
    /* Further expansion (here:erode due to inverted logic!) of the mask */
    diff = erode9(smartmask_final, cnt->imgs.width, cnt->imgs.height, cnt->imgs.common_buffer, 255);
    diff = erode5(smartmask_final, cnt->imgs.width, cnt->imgs.height, cnt->imgs.common_buffer, 255);
}

/* Increment for *smartmask_buffer in alg_diff_standard. */
#define SMARTMASK_SENSITIVITY_INCR 5

int alg_diff_standard (struct context *cnt, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    int i, diffs = 0;
    int noise = cnt->noise;
    int smartmask_speed = cnt->smartmask_speed;
    unsigned char *ref = imgs->ref;
    unsigned char *out = imgs->out;
    unsigned char *mask = imgs->mask;
    unsigned char *smartmask_final=imgs->smartmask_final;
    int *smartmask_buffer = imgs->smartmask_buffer;
#ifdef HAVE_MMX
    mmx_t mmtemp; /* used for transferring to/from memory */
    int unload;   /* counter for unloading diff counts */
#endif

    i = imgs->motionsize;
    memset(out + i, 128, i / 2); /* motion pictures are now b/w i.o. green */
    /* Keeping this memset in the MMX case when zeroes are necessarily 
     * written anyway seems to be beneficial in terms of speed. Perhaps a
     * cache thing?
     */
    memset(out, 0, i);

#ifdef HAVE_MMX
    /* NOTE: The Pentium has two instruction pipes: U and V. I have grouped MMX
     * instructions in pairs according to how I think they will be scheduled in 
     * the U and V pipes. Due to pairing constraints, the V pipe will sometimes
     * be empty (for example, memory access always goes into the U pipe).
     *
     * The following MMX registers are kept throughout the loop:
     * mm5 - 8 separate diff counters (unloaded periodically)
     * mm6 - mask: 00ff 00ff 00ff 00ff
     * mm7 - noise level as 8 packed bytes
     *
     * -- Per Jonsson
     */

    /* To avoid a div, we work with differences multiplied by 255 in the
     * default case and *mask otherwise. Thus, the limit to compare with is
     * 255*(noise+1)-1).
     */
    mmtemp.uw[0] = mmtemp.uw[1] = mmtemp.uw[2] = mmtemp.uw[3] = (unsigned short)(noise * 255 + 254);
    
    /* Reset mm5 to zero, set the mm6 mask, and store the multiplied noise
     * level as four words in mm7.
     */
    movq_m2r(mmtemp, mm7);             /* U */
    pcmpeqb_r2r(mm6, mm6);             /* V */
    
    pxor_r2r(mm5, mm5);                /* U */
    psrlw_i2r(8, mm6);                 /* V */

    /* We must unload mm5 every 255th round, because the diffs accumulate
     * in each packed byte, which can hold at most 255 diffs before it
     * gets saturated.
     */
    unload = 255;
    
    for (; i > 7; i -= 8) {
        /* Calculate abs(*ref-*new) for 8 pixels in parallel. */
        movq_m2r(*ref, mm0);           /* U: mm0 = r7 r6 r5 r4 r3 r2 r1 r0 */
        pxor_r2r(mm4, mm4);            /* V: mm4 = 0 */

        movq_m2r(*new, mm1);           /* U: mm1 = n7 n6 n5 n4 n3 n2 n1 n0 */
        movq_r2r(mm0, mm2);            /* V: mm2 = r7 r6 r5 r4 r3 r2 r1 r0 */

        /* These subtractions are saturated, i.e. won't go below 0. */
        psubusb_r2r(mm1, mm0);         /* U: mm0 = (r7-n7) ... (r0-n0) */
        psubusb_r2r(mm2, mm1);         /* V: mm1 = (n7-r7) ... (n0-r0) */
        
        /* Each byte dX in mm0 is abs(nX-rX). */
        por_r2r(mm1, mm0);             /* U: mm0 = d7 d6 d5 d4 d3 d2 d1 d0 */

        /* Expand the absolute differences to words in mm0 and mm1. */
        movq_r2r(mm0, mm1);            /* U: mm1 = d7 d6 d5 d4 d3 d2 d1 d0 */
        punpcklbw_r2r(mm4, mm0);       /* V: mm0 =    d3    d2    d1    d0 */
        
        punpckhbw_r2r(mm4, mm1);       /* U: mm1 =    d7    d6    d5    d4 */

        if (mask) {
            /* Load and expand 8 mask bytes to words in mm2 and mm3. Then
             * multiply by mm0 and mm1, respectively.
             */
            movq_m2r(*mask, mm2);      /* U: mm2 = m7 m6 m5 m4 m3 m2 m1 m0 */

            movq_r2r(mm2, mm3);        /* U: mm3 = m7 m6 m5 m4 m3 m2 m1 m0 */
            punpcklbw_r2r(mm4, mm2);   /* v: mm2 =    m3    m2    m1    m0 */
            
            punpckhbw_r2r(mm4, mm3);   /* U: mm3 =    m7    m6    m5    m4 */
            pmullw_r2r(mm2, mm0);      /* V: mm0 = (d3*m3) ... (d0*m0) */
            
            pmullw_r2r(mm3, mm1);      /* U: mm1 = (d7*m7) ... (d4*m4) */

            mask+=8;
        } else {
            /* Not using mask - multiply the absolute differences by 255. We
             * do this by left-shifting 8 places and then subtracting dX.
             */
            movq_r2r(mm0, mm2);        /* U: mm2 =    d3    d2    d1    d0 */
            psllw_i2r(8, mm0);         /* V: mm2 = (256*d3) ... (256*d0) */ 

            movq_r2r(mm1, mm3);        /* U: mm3 =    d7    d6    d5    d4 */
            psllw_i2r(8, mm1);         /* V: mm3 = (256*d7) ... (256*d4) */

            psubusw_r2r(mm2, mm0);     /* U */
            psubusw_r2r(mm3, mm1);     /* V */ 
        }

        /* Next, compare the multiplied absolute differences with the multiplied
         * noise level (repeated as 4 words in mm7), resulting in a "motion flag"
         * for each pixel.
         *
         * Since pcmpgtw performs signed comparisons, we have to subtract noise,
         * test for equality to 0 and then invert the result.
         *
         * Note that it is safe to generate the "motion flags" before the 
         * smartmask code, as all that can happen is that individual flags get
         * reset to 0 because of the smartmask.
         */
        psubusw_r2r(mm7, mm0);         /* U: subtract by (multiplied) noise */
        psubusw_r2r(mm7, mm1);         /* V */

        pcmpeqw_r2r(mm4, mm0);         /* U: test for equality with 0 */
        pcmpeqw_r2r(mm4, mm1);         /* V */

        pand_r2r(mm6, mm0);            /* U: convert 0xffff -> 0x00ff */
        pand_r2r(mm6, mm1);            /* V */

        pxor_r2r(mm6, mm0);            /* U: invert the result */
        pxor_r2r(mm6, mm1);            /* V */

        /* Each fX is the "motion flag" = 0 for no motion, 0xff for motion. */
        packuswb_r2r(mm1, mm0);        /* U: mm0 = f7 f6 f5 f4 f3 f2 f1 f0 */

        if (smartmask_speed) {
            /* Apply the smartmask. Basically, if *smartmask_final is 0, the
             * corresponding "motion flag" in mm0 will be reset.
             */
            movq_m2r(*smartmask_final, mm3); /* U: mm3 = s7 s6 s5 s4 s3 s2 s1 s0 */

            /* ...but move the "motion flags" to memory before, in order to
             * increment *smartmask_buffer properly below.
             */
            movq_r2m(mm0, mmtemp);           /* U */
            pcmpeqb_r2r(mm4, mm3);           /* V: mm3 = 0xff where sX==0 */

            /* ANDN negates the target before anding. */
            pandn_r2r(mm0, mm3);             /* U: mm3 = 0xff where dX>noise && sX>0 */

            movq_r2r(mm3, mm0);              /* U */

            /* Add to *smartmask_buffer. This is probably the fastest way to do it. */
            if (cnt->event_nr != cnt->prev_event) {
                if (mmtemp.ub[0]) smartmask_buffer[0]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[1]) smartmask_buffer[1]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[2]) smartmask_buffer[2]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[3]) smartmask_buffer[3]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[4]) smartmask_buffer[4]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[5]) smartmask_buffer[5]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[6]) smartmask_buffer[6]+=SMARTMASK_SENSITIVITY_INCR;
                if (mmtemp.ub[7]) smartmask_buffer[7]+=SMARTMASK_SENSITIVITY_INCR;
            }

            smartmask_buffer += 8;
            smartmask_final += 8;
        }

        movq_m2r(*new, mm2);           /* U: mm1 = n7 n6 n5 n4 n3 n2 n1 n0 */

        /* Cancel out pixels in *new according to the "motion flags" in mm0.
         * Each NX is either 0 or nX as from *new.
         */
        pand_r2r(mm0, mm2);            /* U: mm1 = N7 N6 N5 N4 N3 N2 N1 N0 */
        psubb_r2r(mm0, mm4);           /* V: mm4 = 0x01 where dX>noise */

        /* mm5 holds 8 separate counts - each one is increased according to
         * the contents of mm4 (where each byte is either 0x00 or 0x01). 
         */
        movq_r2m(mm2, *out);           /* U: this will stall */
        paddusb_r2r(mm4, mm5);         /* V: add counts to mm5 */
        
        /* Every 255th turn, we need to unload mm5 into the diffs variable,
         * because otherwise the packed bytes will get saturated.
         */
        if (--unload == 0) {
            /* Unload mm5 to memory and reset it. */
            movq_r2m(mm5, mmtemp);     /* U */
            pxor_r2r(mm5, mm5);        /* V: mm5 = 0 */

            diffs += mmtemp.ub[0] + mmtemp.ub[1] + mmtemp.ub[2] + mmtemp.ub[3] + 
                     mmtemp.ub[4] + mmtemp.ub[5] + mmtemp.ub[6] + mmtemp.ub[7];
            unload = 255;
        }

        out+=8;
        ref+=8;
        new+=8;
    }

    /* Check if there are diffs left in mm5 that need to be copied to the
     * diffs variable. 
     */
    if (unload < 255) {
        movq_r2m(mm5, mmtemp);
        diffs += mmtemp.ub[0] + mmtemp.ub[1] + mmtemp.ub[2] + mmtemp.ub[3] + 
                 mmtemp.ub[4] + mmtemp.ub[5] + mmtemp.ub[6] + mmtemp.ub[7];
    }

    emms();

#endif
    /* Note that the non-MMX code is present even if the MMX code is present.
     * This is necessary if the resolution is not a multiple of 8, in which
     * case the non-MMX code needs to take care of the remaining pixels.
     */

    for (; i>0; i--) {
        register unsigned char curdiff=(int)(abs(*ref - *new)); /* using a temp variable is 12% faster */
        /* apply fixed mask */
        if (mask)
            curdiff=((int)(curdiff * *mask++) / 255);
            
        if (smartmask_speed) {
            if (curdiff > noise) {
                /* increase smart_mask sensitivity every frame when motion
                   is detected. (with speed=5, mask is increased by 1 every
                   second. To be able to increase by 5 every second (with
                   speed=10) we add 5 here. NOT related to the 5 at ratio-
                   calculation. */
                if (cnt->event_nr != cnt->prev_event)
                    (*smartmask_buffer) += SMARTMASK_SENSITIVITY_INCR;
                /* apply smart_mask */
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

/*
    Very fast diff function, does not apply mask overlaying.
*/
static char alg_diff_fast(struct context *cnt, int max_n_changes, unsigned char *new)
{
    struct images *imgs = &cnt->imgs;
    int i, diffs = 0, step = imgs->motionsize / 10000;
    int noise=cnt->noise;
    unsigned char *ref = imgs->ref;

    if (!step % 2)
        step++;
    /* we're checking only 1 of several pixels */
    max_n_changes /= step;

    i = imgs->motionsize;

    for (; i > 0; i -= step) {
        register unsigned char curdiff = (int)(abs((char)(*ref-*new))); /* using a temp variable is 12% faster */
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

/* alg_diff uses diff_fast to quickly decide if there is anything worth
 * sending to diff_standard.
*/
int alg_diff(struct context *cnt, unsigned char *new)
{
    int diffs = 0;
    
    if (alg_diff_fast(cnt, cnt->conf.max_changes / 2, new))
        diffs = alg_diff_standard(cnt, new);

    return diffs;
}

/* Detect a sudden massive change in the picture.
   It is assumed to be the light being switched on or a camera displacement.
   In any way the user doesn't think it is worth capturing.
 */
int alg_lightswitch(struct context *cnt, int diffs)
{
    struct images *imgs = &cnt->imgs;
    
    if (cnt->conf.lightswitch < 0)
        cnt->conf.lightswitch = 0;
    if (cnt->conf.lightswitch > 100)
        cnt->conf.lightswitch = 100;
    
    /* is lightswitch percent of the image changed?  */
    if (diffs > (imgs->motionsize * cnt->conf.lightswitch / 100))
        return 1;
    
    return 0;
}

int alg_switchfilter(struct context *cnt, int diffs, unsigned char *newimg)
{
    int linediff = diffs / cnt->imgs.height;
    unsigned char *out = cnt->imgs.out;
    int y, x, line;
    int lines = 0, vertlines = 0;

    for (y = 0; y < cnt->imgs.height; y++) {
        line = 0;
        for (x = 0; x < cnt->imgs.width; x++) {
            if (*(out++)) 
                line++;
            
        }

        if (line > cnt->imgs.width / 18) 
            vertlines++;
        
        if (line > linediff * 2) 
            lines++;
        
    }

    if (vertlines > cnt->imgs.height / 10 && lines < vertlines / 3 &&
        (vertlines > cnt->imgs.height / 4 || lines - vertlines > lines / 2)) {
        if (cnt->conf.text_changes) {
            char tmp[80];
            sprintf(tmp, "%d %d", lines, vertlines);
            draw_text(newimg, cnt->imgs.width-10, 20, cnt->imgs.width, tmp, cnt->conf.text_double);
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
 *   cnt    - current thread's context struct
 *   action - UPDATE_REF_FRAME or RESET_REF_FRAME
 *
 */
/* Seconds */
#define ACCEPT_STATIC_OBJECT_TIME 10
#define EXCLUDE_LEVEL_PERCENT 20
void alg_update_reference_frame(struct context *cnt, int action) 
{
    int accept_timer = cnt->lastrate * ACCEPT_STATIC_OBJECT_TIME;
    int i, threshold_ref;
    int *ref_dyn = cnt->imgs.ref_dyn;
    unsigned char *image_virgin = cnt->imgs.image_virgin;
    unsigned char *ref = cnt->imgs.ref;
    unsigned char *smartmask = cnt->imgs.smartmask_final;
    unsigned char *out = cnt->imgs.out;

    if (cnt->lastrate > 5)  /* match rate limit */
        accept_timer /= (cnt->lastrate / 3);

    if (action == UPDATE_REF_FRAME) { /* black&white only for better performance */
        threshold_ref = cnt->noise * EXCLUDE_LEVEL_PERCENT / 100;
        for (i = cnt->imgs.motionsize; i > 0; i--) {
            /* exclude pixels from ref frame well below noise level */
            if (((int)(abs(*ref - *image_virgin)) > threshold_ref) && (*smartmask)) {
                if (*ref_dyn == 0) { /* Always give new pixels a chance */
                    *ref_dyn = 1;
                } else if (*ref_dyn > accept_timer) { /* Include static Object after some time */
                    *ref_dyn = 0;
                    *ref = *image_virgin;
                } else if (*out) {
                    (*ref_dyn)++; /* Motionpixel? Keep excluding from ref frame */
                } else {
                    *ref_dyn = 0; /* Nothing special - release pixel */
                    *ref = (*ref + *image_virgin) / 2;
                }

            } else {  /* No motion: copy to ref frame */
                *ref_dyn = 0; /* reset pixel */
                *ref = *image_virgin;
            }
            ref++;
            image_virgin++;
            smartmask++;
            ref_dyn++;
            out++;
        } /* end for i */
    } else {   /* action == RESET_REF_FRAME - also used to initialize the frame at startup */
        /* copy fresh image */
        memcpy(cnt->imgs.ref, cnt->imgs.image_virgin, cnt->imgs.size);
        /* reset static objects */
        memset(cnt->imgs.ref_dyn, 0, cnt->imgs.motionsize * sizeof(cnt->imgs.ref_dyn));
    }
}
