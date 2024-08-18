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

#ifndef _INCLUDE_WEBU_COMMON_HPP_
#define _INCLUDE_WEBU_COMMON_HPP_
    class cls_webu_common {
        public:
            cls_webu_common(cls_webu_ans *p_webua);
            ~cls_webu_common();
            bool check_finish();
            void delay();
            void set_fps();
            void all_getimg();
            void all_sizes();
            void all_buffer();
            void one_buffer();

            struct timespec time_last;      /* Keep track of processing time for stream thread*/

            size_t          resp_size;      /* The allocated size of the response */
            size_t          resp_used;      /* The amount of the response page used */
            unsigned char   *resp_image;    /* Response image to provide to user */
            unsigned char   *all_img_data;  /* Image for all cameras */
            int             stream_fps;     /* Stream rate per second */

        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;

            void img_sizes(cls_camera *cam, int &img_w, int &img_h);
            void img_resize(cls_camera *cam
                , uint8_t *src, uint8_t *dst, int dst_w, int dst_h);

    };

#endif