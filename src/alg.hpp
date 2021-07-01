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

#ifndef _INCLUDE_ALG_HPP_
#define _INCLUDE_ALG_HPP_

    void alg_diff(ctx_cam *cam);
    void alg_noise_tune(ctx_cam *cam);
    void alg_threshold_tune(ctx_cam *cam);
    void alg_tune_smartmask(ctx_cam *cam);
    void alg_update_reference_frame(ctx_cam *cam, int action);
    void alg_stddev(ctx_cam *cam);
    void alg_location(ctx_cam *cam);

#endif /* _INCLUDE_ALG_HPP_ */
