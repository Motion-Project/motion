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

#ifndef _INCLUDE_WEBU_MPEGTS_HPP_
#define _INCLUDE_WEBU_MPEGTS_HPP_

    class cls_webu_mpegts {
        public:
            cls_webu_mpegts(cls_webu_ans *p_webua, cls_webu_stream *p_webus);
            ~cls_webu_mpegts();
            int avio_buf(myuint *buf, int buf_size);
            ssize_t response(char *buf, size_t max);
            mhdrslt main();

        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;
            cls_webu_stream *webus;

            AVFrame         *picture;
            AVFormatContext *fmtctx;
            AVCodecContext  *ctx_codec;
            size_t          stream_pos;     /* Stream position of sent image */
            struct timespec start_time;     /* Start time of the stream*/
            struct timespec st_mono_time;

            int pic_send(unsigned char *img);
            int pic_get();
            void resetpos();
            int getimg();
            int open_mpegts();
    };

#endif /* _INCLUDE_WEBU_MPEGTS_HPP_ */
