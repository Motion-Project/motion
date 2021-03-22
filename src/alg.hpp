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

#ifndef _INCLUDE_ALG_H
#define _INCLUDE_ALG_H

    struct ctx_coord;

    void alg_locate_center_size(struct ctx_images *imgs, int width, int height, struct ctx_coord *cent);
    void alg_diff(struct ctx_cam *cam);
    void alg_lightswitch(struct ctx_cam *cam);
    void alg_noise_tune(struct ctx_cam *cam, unsigned char *new_var);
    void alg_threshold_tune(struct ctx_cam *cam, int diffs, int motion);
    void alg_despeckle(struct ctx_cam *cam);
    void alg_tune_smartmask(struct ctx_cam *cam);
    void alg_update_reference_frame(struct ctx_cam *cam, int action);
    void alg_new_update_frame(ctx_cam *cam);
    void alg_new_diff(ctx_cam *cam);


#endif /* _INCLUDE_ALG_H */
