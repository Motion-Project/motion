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

#ifndef _INCLUDE_WEBU_STREAM_HPP_
#define _INCLUDE_WEBU_STREAM_HPP_

    void webu_stream_init(ctx_dev *cam);
    void webu_stream_deinit(ctx_dev *cam);
    void webu_stream_getimg(ctx_dev *cam);

    mhdrslt webu_stream_main(ctx_webui *webui);
    void webu_stream_delay(ctx_webui *webui);
    void webu_stream_checkbuffers(ctx_webui *webui);
    void webu_stream_img_sizes(ctx_webui *webui, ctx_dev *cam, int &img_w, int &img_h);
    void webu_stream_all_sizes(ctx_webui *webui);
    void webu_stream_all_buffers(ctx_webui *webui);
    void webu_stream_all_getimg(ctx_webui *webui);
    bool webu_stream_check_finish(ctx_webui *webui);

#endif /* _INCLUDE_WEBU_STREAM_HPP_ */
