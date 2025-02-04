/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef _INCLUDE_ALG_HPP_
#define _INCLUDE_ALG_HPP_
    #define THRESHOLD_TUNE_LENGTH  256

    class cls_alg {
        public:
            cls_alg(cls_camera *p_cam);
            ~cls_alg();
            void diff();
            void noise_tune();
            void threshold_tune();
            void tune_smartmask();
            void ref_frame_update();
            void ref_frame_reset();
            void stddev();
            void location();
            u_char  *smartmask_final;
        private:
            cls_camera *cam;
            int     smartmask_count;
            u_char  *smartmask;
            int     *smartmask_buffer;
            int     diffs_last[THRESHOLD_TUNE_LENGTH];
            bool    calc_stddev;

            int iflood(int x, int y, int width, int height,
                u_char *out, int *labels, int newvalue, int oldvalue);
            int labeling();
            int dilate9(u_char *img, int width, int height, void *buffer);
            int dilate5(u_char *img, int width, int height, void *buffer);
            int erode9(u_char *img, int width, int height, void *buffer, u_char flag);
            int erode5(u_char *img, int width, int height, void *buffer, u_char flag);
            void despeckle();
            void diff_nomask();
            void diff_mask();
            void diff_smart();
            void diff_masksmart();
            bool diff_fast();
            void diff_standard();
            void lightswitch();
            void location_center();
            void location_dist_stddev();
            void location_dist_basic();
            void location_minmax();

    };

#endif /* _INCLUDE_ALG_HPP_ */
