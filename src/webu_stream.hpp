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
 *    Copyright 2020-2022 MotionMrDave@gmail.com
*/

#ifndef _INCLUDE_WEBU_STREAM_HPP_
#define _INCLUDE_WEBU_STREAM_HPP_

    void webu_stream_init(ctx_cam *cam);
    void webu_stream_deinit(ctx_cam *cam);
    void webu_stream_getimg(ctx_cam *cam, ctx_image_data *img_data);

    mhdrslt webu_stream_main(ctx_webui *webui);

#endif /* _INCLUDE_WEBU_STREAM_HPP_ */
