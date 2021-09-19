/*   This file is part of Motion.
 *
 *   Motion is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   Motion is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 *  alg.h
 *    Detect changes in a video stream.
 *    Copyright 2001 by Jeroen Vreeken (pe1rxq@amsat.org)
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

void alg_locate_center_size(struct images *imgs, int width, int height, struct coord *cent);
void alg_draw_location(struct coord *cent, struct images *imgs, int width, unsigned char *new,
                       int style, int mode, int process_thisframe);
void alg_draw_red_location(struct coord *cent, struct images *imgs, int width, unsigned char *new,
                           int style, int mode, int process_thisframe);
void alg_noise_tune(struct context *cnt, unsigned char *new);
void alg_threshold_tune(struct context *cnt, int diffs, int motion);
int alg_despeckle(struct context *cnt, int olddiffs);
void alg_tune_smartmask(struct context *cnt);
int alg_diff_standard(struct context *cnt, unsigned char *new);
int alg_diff(struct context *cnt, unsigned char *new);
int alg_lightswitch(struct context *cnt, int diffs);
int alg_switchfilter(struct context *cnt, int diffs, unsigned char *newimg);
void alg_update_reference_frame(struct context *cnt, int action);

#endif /* _INCLUDE_ALG_H */
