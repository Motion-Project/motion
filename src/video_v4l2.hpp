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

#ifndef _INCLUDE_VIDEO_V4L2_HPP_
#define _INCLUDE_VIDEO_V4L2_HPP_

typedef struct video_image_buff {
    unsigned char   *ptr;
    int             content_length;
    size_t          size;                    /* total allocated size */
    size_t          used;                    /* bytes already used */
    struct timeval  image_time;      /* time this image was received */
} video_buff;

typedef struct palette_item_struct{
    unsigned int    v4l2id;
    char            fourcc[5];
} palette_item;

struct ctx_v4l2cam_ctrl {
    char            *ctrl_name;       /* The name as provided by the device */
    char            *ctrl_iddesc;     /* A motion description of the ID number for the control*/
    int             ctrl_minimum;    /* The minimum value permitted as reported by device*/
    int             ctrl_maximum;    /* The maximum value permitted as reported by device*/
    int             ctrl_default;    /* The default value for the control*/
    int             ctrl_currval;    /* The current value the control was set to */
    int             ctrl_newval;     /* The new value to set for the control */
    unsigned int    ctrl_id;         /* The ID number for the control as provided by the device*/
    unsigned int    ctrl_type;       /* The type of control as reported by the device*/
    bool            ctrl_menuitem;   /* bool for whether item is a menu item description */
};

class cls_v4l2cam {
    public:
        cls_v4l2cam(ctx_dev *p_cam);
        ~cls_v4l2cam();

        int next(ctx_image_data *img_data);
        void restart_cam();

    private:
        ctx_dev *cam;
        int     fd_device;
        int     width;
        int     height;
        int     fps;
        int     pixfmt_src;
        int     buffer_count;
        ctx_v4l2cam_ctrl        *devctrl_array;        /*Array of all the controls in the device*/
        int     devctrl_count;         /*Count of the controls in the device*/
        int     device_type;           /*Camera, tuner, etc as provided by driver enum*/
        int     device_tuner;          /*Tuner number if applicable from driver*/
        ctx_params              *params;               /*User parameters for the camera */
        video_buff              *buffers;
        int                     pframe;
        volatile bool           finish;                /* End the thread */
        #ifdef HAVE_V4L2
            struct v4l2_capability cap;
            struct v4l2_format fmt;
            struct v4l2_requestbuffers req;
            struct v4l2_buffer v4l2buf;
        #endif

        void start_cam();
        void stop_cam();

        void palette_init(palette_item *palette_array);
        int xioctl(unsigned long request, void *arg);
        void device_close();
        void ctrls_count();
        void ctrls_log();
        void ctrls_list();
        void ctrls_set();
        int parms_set();
        void set_input();
        void set_norm();
        void set_frequency();
        int pixfmt_try(uint pixformat);
        int pixfmt_stride();
        int pixfmt_adjust();
        int pixfmt_set(uint pixformat);
        void params_check();
        int pixfmt_list(palette_item *palette_array);
        void palette_set();
        void set_mmap();
        void set_imgs();
        int capture();
        int convert(unsigned char *img_norm);
        void device_init();
        void device_select();
        void device_open();
        void log_types();
        void log_formats();
        void set_fps();

};


#endif /* _INCLUDE_VIDEO_V4L2_HPP_ */
