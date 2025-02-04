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
 *
*/

#ifndef _INCLUDE_MOVIE_HPP_
#define _INCLUDE_MOVIE_HPP_

enum TIMELAPSE_TYPE {
    TIMELAPSE_NONE,         /* No timelapse, regular processing */
    TIMELAPSE_APPEND,       /* Use append version of timelapse */
    TIMELAPSE_NEW           /* Use create new file version of timelapse */
};


class cls_movie {
    public:
        cls_movie(cls_camera *p_cam, std::string pmovie_type);
        ~cls_movie();
        void start();
        void stop();
        int put_image(ctx_image_data *img_data, const struct timespec *ts1);
        void reset_start_time(const struct timespec *ts1);

        struct timespec     cb_st_ts;    /* The time set before calling the av functions */
        struct timespec     cb_cr_ts;    /* Time during the interrupt to determine duration since start*/
        int                 cb_dur;      /* Seconds permitted before triggering a interrupt */
        std::string         full_nm;
        std::string         file_nm;
        std::string         file_dir;
        bool                is_running;

    private:
        cls_camera *cam;

        void free_pkt();
        void free_nal();
        void encode_nal();
        int timelapse_exists(const char *fname);
        int encode_video();
        int timelapse_append(AVPacket *pkt);
        void free_context();
        int get_oformat();
        int set_pts(const struct timespec *ts1);
        int set_quality();
        int set_codec_preferred();
        int set_codec();
        int set_stream();
        int alloc_video_buffer(AVFrame *frame, int align);
        int set_picture();
        int set_outputfile();
        int flush_codec();
        int put_frame(const struct timespec *ts1);
        void put_pix_yuv420(ctx_image_data *img_data);
        int movie_open();
        void init_container();
        void init_vars();

        void passthru_reset();
        int passthru_pktpts();
        void passthru_write(int indx);
        void passthru_minpts();
        int passthru_put(ctx_image_data *img_data);
        int passthru_streams_video(AVStream *stream_in);
        int passthru_streams_audio(AVStream *stream_in);
        int passthru_streams();
        int passthru_check();
        int passthru_open();

        void start_norm();
        void start_motion();
        void start_timelapse();
        void start_extpipe();
        int extpipe_put();
        void on_movie_start();
        void on_movie_end();

        AVFormatContext     *oc;
        AVStream            *strm_video;
        AVStream            *strm_audio;
        AVCodecContext      *ctx_codec;
        myAVCodec           *codec;
        AVPacket            *pkt;
        AVFrame             *picture;       /* contains default image pointers */
        AVDictionary        *opts;
        cls_netcam          *netcam_data;
        int                 width;
        int                 height;
        enum TIMELAPSE_TYPE tlapse;
        int                 fps;
        int64_t             last_pts;
        int64_t             base_pts;
        int64_t             pass_audio_base;
        int64_t             pass_video_base;
        bool                test_mode;
        int                 gop_cnt;
        struct timespec     start_time;
        bool                high_resolution;
        bool                motion_images;
        bool                passthrough;

        char                *nal_info;
        int                 nal_info_len;
        FILE                *extpipe_stream;
        std::string         container;
        std::string         preferred_codec;
        std::string         movie_type;

};

#endif /* #define _INCLUDE_MOVIE_HPP_ */
