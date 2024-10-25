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

#ifndef _INCLUDE_ALLCAM_HPP_
#define _INCLUDE_ALLCAM_HPP_

class cls_allcam {
    public:
        cls_allcam(cls_motapp *p_app);
        ~cls_allcam();

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();
        ctx_stream      stream;
        ctx_all_sizes   all_sizes;

        bool    restart;
        bool    finish;

    private:
        cls_motapp          *app;

        std::vector<cls_camera*>    active_cam;
        int active_cnt;

        int watchdog;
        struct timespec     curr_ts;

        void handler_startup();
        void handler_shutdown();
        void timing();
        void stream_free();
        void stream_alloc();
        void getsizes_img(cls_camera *p_cam, int &img_w, int &img_h);
        void getsizes_scale(int mx_row);
        void getsizes_alignv(int mx_row, int mx_col);
        void getsizes_alignh(int mx_col);
        void getsizes();
        void init_params();
        bool init_validate();
        void init_cams();
        void getimg(ctx_stream_data *strm_a, std::string imgtyp);

};

#endif /*_INCLUDE_ALLCAM_HPP_*/
