/*
 *    This file is part of Motionplus.
 *
 *    MotionPlus is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motionplus is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motionplus.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    Copyright 2020 MotionMrDave@gmail.com
*/

#ifndef _INCLUDE_WEBU_STREAM_H_
#define _INCLUDE_WEBU_STREAM_H_

    struct webui_ctx;

    void webu_stream_init(struct ctx_cam *cam);
    void webu_stream_deinit(struct ctx_cam *cam);
    void webu_stream_getimg(struct ctx_cam *cam, struct ctx_image_data *img_data);

    int webu_stream_mjpeg(struct webui_ctx *webui);
    int webu_stream_static(struct webui_ctx *webui);

#endif
