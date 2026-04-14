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

#ifndef _INCLUDE_WEBU_ALLCAM_HPP_
#define _INCLUDE_WEBU_ALLCAM_HPP_

struct ctx_allcam_info {
    cls_camera *cam;

    int     row;
    int     col;
    int     offset_row;
    int     offset_col;
    int     offset_user_row;
    int     offset_user_col;
    int     scale;
    int     xpct_st;    /*Starting x location of image on percentage basis*/
    int     xpct_en;    /*Ending x location of image on percentage basis*/
    int     ypct_st;    /*Starting y location of image on percentage basis*/
    int     ypct_en;    /*Ending y location of image on percentage basis*/

    int     src_w;
    int     src_h;
    int     src_sz;
    int     dst_w;
    int     dst_h;
    int     dst_sz;
};

class cls_allcam {
    public:
        cls_allcam(cls_webu *p_webu, int p_indx);
        ~cls_allcam();

        bool            handler_stop;
        bool            handler_running;
        pthread_t       handler_thread;
        void            handler();
        ctx_stream      stream;
        ctx_allcam_info info;
        std::vector<ctx_allcam_info>    active_cam;

        int     active_cnt;
        bool    restart;
        bool    finish;
        bool    reset;

    private:
        cls_motapp  *app;
        cls_webu    *webu;
        int watchdog;
        int max_col;
        int max_row;
        int webuindx;

        struct timespec     curr_ts;

        void handler_startup();
        void handler_shutdown();
        void timing();
        void stream_free();
        void stream_alloc();
        void getsizes_img(int indx);
        void getsizes_scale();
        void getsizes_alignv();
        void getsizes_alignh();
        void getsizes_offset_user();
        void getsizes_pct();
        void getsizes();
        void init_active();
        void init_params();
        void init_validate();
        void init_cams();
        void getimg_src(cls_camera *p_cam, std::string imgtyp, u_char *dst_img, u_char *src_img, int indx_act);
        void getimg(ctx_stream_data *strm_a, std::string imgtyp);

};

#endif /*_INCLUDE_WEBU_ALLCAM_HPP_*/
