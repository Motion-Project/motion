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
    class cls_webu_stream {
        public:
            cls_webu_stream(cls_webu_ans *webua);
            ~cls_webu_stream();

            mhdrslt main();
            ssize_t mjpeg_response (char *buf, size_t max);

        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;
            cls_webu_common *webuc;
            cls_webu_mpegts *webu_mpegts;

            size_t          stream_pos;

            void mjpeg_all_img();
            void mjpeg_one_img();
            void static_all_img();
            void static_one_img();
            mhdrslt stream_static();
            mhdrslt stream_mjpeg();

            int checks();
            bool all_ready();
            void all_cnct();
            void jpg_cnct();
            void ts_cnct();
            void set_cnct_type();
    };

#endif /* _INCLUDE_WEBU_STREAM_HPP_ */
