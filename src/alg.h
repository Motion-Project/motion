/*    alg.h
 *
 *    Detect changes in a video stream.
 *    Copyright 2001 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_ALG_H
#define _INCLUDE_ALG_H

#include "motion.h"

struct coord {
    int x;
    int y;
    int width;
    int height;
    int minx;
    int maxx;
    int miny;
    int maxy;
};

struct segment {
    struct coord coord;
    int width;
    int height;
    int open;
    int count;
};

void alg_locate_center_size(struct images *, int width, int height, struct coord *);
void alg_draw_location(struct coord *, struct images *, int width, unsigned char *, int, int, int);
void alg_draw_red_location(struct coord *, struct images *, int width, unsigned char *, int, int, int);
int alg_diff(struct context *, unsigned char *);
int alg_diff_standard(struct context *, unsigned char *);
int alg_lightswitch(struct context *, int diffs);
int alg_switchfilter(struct context *, int, unsigned char *);
void alg_noise_tune(struct context *, unsigned char *);
void alg_threshold_tune(struct context *, int, int);
int alg_despeckle(struct context *, int);
void alg_tune_smartmask(struct context *);
void alg_update_reference_frame(struct context *, int);

#endif /* _INCLUDE_ALG_H */
