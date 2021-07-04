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
 *    Copyright 2020-2021 MotionMrDave@gmail.com
 */

/*
 * The contents of this file has been derived from the output_example.c
 * and apiexample.c from the FFmpeg distribution.
 *
 * This file has been modified so that only major versions greater than
 * 53 are supported.
 * Note that while the conditions are based upon LIBAVFORMAT, not all of the changes are
 * specific to libavformat.h.  Some changes could be related to other components of ffmpeg.
 * This is for simplicity.  The avformat version has historically changed at the same time
 * as the other components so it is easier to have a single version number to track rather
 * than the particular version numbers which are associated with each component.
 * The libav variant also has different apis with the same major/minor version numbers.
 * As such, it is occasionally necessary to look at the microversion number.  Numbers
 * greater than 100 for micro version indicate ffmpeg whereas numbers less than 100
 * indicate libav
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "netcam.hpp"
#include "movie.hpp"


static void movie_free_pkt(struct ctx_movie *movie)
{
    mypacket_free(movie->pkt);
    movie->pkt = NULL;
}

static void movie_free_nal(struct ctx_movie *movie)
{
    if (movie->nal_info) {
        free(movie->nal_info);
        movie->nal_info = NULL;
        movie->nal_info_len = 0;
    }
}

static void movie_encode_nal(struct ctx_movie *movie)
{
    // h264_v4l2m2m has NAL units separated from the first frame, which makes
    // some players very unhappy.
    if ((movie->pkt->pts == 0) && (!(movie->pkt->flags & AV_PKT_FLAG_KEY))) {
        movie_free_nal(movie);
        movie->nal_info_len = movie->pkt->size;
        movie->nal_info =(char*) malloc(movie->nal_info_len);
        if (movie->nal_info) {
            memcpy(movie->nal_info, &movie->pkt->data[0], movie->nal_info_len);
        } else {
            movie->nal_info_len = 0;
        }
    } else if (movie->nal_info) {
        int old_size = movie->pkt->size;
        av_grow_packet(movie->pkt, movie->nal_info_len);
        memmove(&movie->pkt->data[movie->nal_info_len], &movie->pkt->data[0], old_size);
        memcpy(&movie->pkt->data[0], movie->nal_info, movie->nal_info_len);
        movie_free_nal(movie);
    }
}

static int movie_timelapse_exists(const char *fname)
{
    FILE *file;
    file = fopen(fname, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

static int movie_timelapse_append(struct ctx_movie *movie, AVPacket *pkt)
{
    FILE *file;

    file = fopen(movie->filename, "a");
    if (!file) {
        return -1;
    }

    fwrite(pkt->data, 1, pkt->size, file);

    fclose(file);

    return 0;
}

static void movie_free_context(struct ctx_movie *movie)
{

        if (movie->picture != NULL) {
            myframe_free(movie->picture);
            movie->picture = NULL;
        }

        if (movie->ctx_codec != NULL) {
            myavcodec_close(movie->ctx_codec);
            movie->ctx_codec = NULL;
        }

        if (movie->oc != NULL) {
            avformat_free_context(movie->oc);
            movie->oc = NULL;
        }

}

static int movie_get_oformat(struct ctx_movie *movie)
{

    size_t codec_name_len = strcspn(movie->codec_name, ":");
    char *codec_name =(char*) malloc(codec_name_len + 1);
    char basename[PATH_MAX];
    int retcd;

    /* TODO:  Rework the extenstion asssignment along with the code in event.c
     * If extension is ever something other than three bytes,
     * preceded by . then lots of things will fail
     */
    if (codec_name == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Failed to allocate memory for codec name"));
        movie_free_context(movie);
        return -1;
    }
    memcpy(codec_name, movie->codec_name, codec_name_len);
    codec_name[codec_name_len] = 0;

    retcd = snprintf(basename,PATH_MAX,"%s",movie->filename);
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting base file name"));
        movie_free_context(movie);
        if (codec_name != NULL) {
            free(codec_name);
        }
        codec_name = NULL;
        return -1;
    }

    if (movie->tlapse == TIMELAPSE_APPEND) {

        movie->oc->oformat = av_guess_format ("mpeg2video", NULL, NULL);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_MPEG2VIDEO;
        }
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mpg",basename);
        if ((!movie->oc->oformat) ||
            (retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting timelapse append for codec %s"), codec_name);
            movie_free_context(movie);
            if (codec_name != NULL) {
                free(codec_name);
            }
            codec_name = NULL;
            return -1;
        }
        if (codec_name != NULL) {
            free(codec_name);
        }
        codec_name = NULL;
        return 0;
    }

    if (mystreq(codec_name, "flv")) {
        movie->oc->oformat = av_guess_format("flv", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.flv",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_FLV1;
        }
    }

    if (mystreq(codec_name, "ogg")) {
        movie->oc->oformat = av_guess_format("ogg", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.ogg",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_THEORA;
        }
    }

    if (mystreq(codec_name, "vp8")) {
        movie->oc->oformat = av_guess_format("webm", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.webm",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_VP8;
        }
    }

    if (mystreq(codec_name, "mp4")) {
        movie->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mp4",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_H264;
        }
    }

    if (mystreq(codec_name, "mkv")) {
        movie->oc->oformat = av_guess_format("matroska", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mkv",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_H264;
        }
    }

    if (mystreq(codec_name, "hevc")) {
        movie->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mp4",basename);
        if (movie->oc->oformat) {
            movie->oc->video_codec_id = MY_CODEC_ID_HEVC;
        }
    }

    //Check for valid results
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting file name"));
        movie_free_context(movie);
        if (codec_name != NULL) {
            free(codec_name);
        }
        codec_name = NULL;
        return -1;
    }

    if (!movie->oc->oformat) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("codec option value %s is not supported"), codec_name);
        movie_free_context(movie);
        if (codec_name != NULL) {
            free(codec_name);
        }
        codec_name = NULL;
        return -1;
    }

    if (movie->oc->oformat->video_codec == MY_CODEC_ID_NONE) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get the codec"));
        movie_free_context(movie);
        if (codec_name != NULL) {
            free(codec_name);
        }
        codec_name = NULL;
        return -1;
    }

    if (codec_name != NULL) {
        free(codec_name);
    }
    codec_name = NULL;

    return 0;
}

static int movie_encode_video(struct ctx_movie *movie)
{

    #if (MYFFVER >= 57041)
        //ffmpeg version 3.1 and after
        int retcd = 0;
        char errstr[128];

        retcd = avcodec_send_frame(movie->ctx_codec, movie->picture);
        if (retcd < 0 ) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error sending frame for encoding:%s"),errstr);
abort();
            return -1;
        }
        retcd = avcodec_receive_packet(movie->ctx_codec, movie->pkt);
        if (retcd == AVERROR(EAGAIN)) {
            //Buffered packet.  Throw special return code
            movie_free_pkt(movie);
            return -2;
        }
        if (retcd < 0 ) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error receiving encoded packet video:%s"),errstr);
            //Packet is freed upon failure of encoding
            return -1;
        }

        if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
            movie_encode_nal(movie);
        }

        return 0;

    #elif (MYFFVER > 54006)

        int retcd = 0;
        char errstr[128];
        int got_packet_ptr;

        retcd = avcodec_encode_video2(movie->ctx_codec, movie->pkt, movie->picture, &got_packet_ptr);
        if (retcd < 0 ) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video:%s"),errstr);
            //Packet is freed upon failure of encoding
            return -1;
        }
        if (got_packet_ptr == 0) {
            //Buffered packet.  Throw special return code
            movie_free_pkt(movie);
            return -2;
        }

        /* This kills compiler warnings.  Nal setting is only for recent movie versions*/
        if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
            movie_encode_nal(movie);
        }

        return 0;

    #else

        int retcd = 0;
        uint8_t *video_outbuf;
        int video_outbuf_size;

        video_outbuf_size = (movie->ctx_codec->width +16) * (movie->ctx_codec->height +16) * 1;
        video_outbuf =(uint8_t *) mymalloc(video_outbuf_size);

        retcd = avcodec_encode_video(movie->strm_video->codec, video_outbuf, video_outbuf_size, movie->picture);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video"));
            movie_free_pkt(movie);
            return -1;
        }
        if (retcd == 0 ) {
            // No bytes encoded => buffered=>special handling
            movie_free_pkt(movie);
            return -2;
        }

        // Encoder did not provide metadata, set it up manually
        movie->pkt->size = retcd;
        movie->pkt->data = video_outbuf;

        if (movie->picture->key_frame == 1) {
            movie->pkt->flags |= AV_PKT_FLAG_KEY;
        }

        movie->pkt->pts = movie->picture->pts;
        movie->pkt->dts = movie->pkt->pts;

        if (video_outbuf != NULL) {
            free(video_outbuf);
        }
        video_outbuf = NULL;

        /* This kills compiler warnings.  Nal setting is only for recent movie versions*/
        if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
            movie_encode_nal(movie);
        }

        return 0;

    #endif

}

static int movie_set_pts(struct ctx_movie *movie, const struct timespec *ts1)
{

    int64_t pts_interval;

    if (movie->tlapse != TIMELAPSE_NONE) {
        movie->last_pts++;
        movie->picture->pts = movie->last_pts;
    } else {
        pts_interval = ((1000000L * (ts1->tv_sec - movie->start_time.tv_sec)) + (ts1->tv_nsec/1000) - (movie->start_time.tv_nsec/1000));
        if (pts_interval < 0) {
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            movie_reset_start_time(movie, ts1);
            pts_interval = 0;
        }
        if (movie->last_pts < 0) {
            // This is the very first frame, ensure PTS is zero
            movie->picture->pts = 0;
        } else
            movie->picture->pts = av_rescale_q(pts_interval,(AVRational){1, 1000000L},movie->strm_video->time_base) + movie->base_pts;

        if (movie->test_mode == true) {
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                ,_("PTS %" PRId64 " Base PTS %" PRId64 " ms interval %" PRId64 " timebase %d-%d")
                ,movie->picture->pts,movie->base_pts,pts_interval
                ,movie->strm_video->time_base.num,movie->strm_video->time_base.den);
        }

        if (movie->picture->pts <= movie->last_pts) {
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (movie->test_mode == true) {
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        movie->last_pts = movie->picture->pts;
    }
    return 0;
}

static int movie_set_quality(struct ctx_movie *movie)
{

    movie->opts = 0;
    if (movie->quality > 100) {
        movie->quality = 100;
    }
    if (movie->ctx_codec->codec_id == MY_CODEC_ID_H264 ||
        movie->ctx_codec->codec_id == MY_CODEC_ID_HEVC) {
        if (movie->quality <= 0) {
            movie->quality = 45; // default to 45% quality
        }

        /* This next if statement needs validation.  Are mpeg4omx
         * and v4l2m2m even MY_CODEC_ID_H264 or MY_CODEC_ID_HEVC
         * such that it even would be possible to be part of this
         * if block to start with? */
        if ((movie->preferred_codec == USER_CODEC_H264OMX) ||
            (movie->preferred_codec == USER_CODEC_MPEG4OMX) ||
            (movie->preferred_codec == USER_CODEC_V4L2M2M)) {

            // bit_rate = movie->width * movie->height * movie->fps * quality_factor
            movie->quality = (int)(((int64_t)movie->width * movie->height * movie->fps * movie->quality) >> 7);
            // Clip bit rate to min
            if (movie->quality < 4000) {
                // magic number
                movie->quality = 4000;
            }
            movie->ctx_codec->profile = FF_PROFILE_H264_HIGH;
            movie->ctx_codec->bit_rate = movie->quality;
            av_dict_set(&movie->opts, "preset", "ultrafast", 0);
            av_dict_set(&movie->opts, "tune", "zerolatency", 0);

        } else {
            /* Control other H264 encoders quality is via CRF.  To get the profiles
             * to work (main), (high), we are setting this via the opt instead of
             * dictionary.  The ultrafast is not used because at that level, the
             * profile reverts to (baseline) and a bit more efficiency is in
             * (main) or (high) so we choose next fastest option (superfast)
             */
            char crf[10];
            movie->quality = (int)(( (100-movie->quality) * 51)/100);
            snprintf(crf, 10, "%d", movie->quality);
            av_opt_set(movie->ctx_codec->priv_data, "profile", "high", 0);
            av_opt_set(movie->ctx_codec->priv_data, "crf", crf, 0);
            av_opt_set(movie->ctx_codec->priv_data, "tune", "zerolatency", 0);
            av_opt_set(movie->ctx_codec->priv_data, "preset", "superfast",0);
        }
    } else {
        /* The selection of 8000 is a subjective number based upon viewing output files */
        if (movie->quality > 0) {
            movie->quality =(int)(((100-movie->quality)*(100-movie->quality)*(100-movie->quality) * 8000) / 1000000) + 1;
            movie->ctx_codec->flags |= MY_CODEC_FLAG_QSCALE;
            movie->ctx_codec->global_quality=movie->quality;
        }
    }
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("%s codec vbr/crf/bit_rate: %d"), movie->codec->name, movie->quality);

    return 0;
}

struct blacklist_t
{
    const char *codec_name;
    const char *reason;
};

static const char *movie_codec_is_blacklisted(const char *codec_name)
{

    static struct blacklist_t blacklisted_codec[] =
    {
    #if (MYFFVER <= 58029)
            /* h264_omx & ffmpeg combination locks up on Raspberry Pi.
            * Newer versions of ffmpeg allow zerocopy to be disabled to workaround
            * this issue.
            * To use h264_omx encoder on older versions of ffmpeg:
            * - disable input_zerocopy in ffmpeg omx.c:omx_encode_init function.
            * - remove the "h264_omx" from this blacklist.
            * More information: https://github.com/Motion-Project/motion/issues/433
            */
            {"h264_omx", "Codec causes lock up on your FFMpeg version"},
    #endif
    #if (MYFFVER < 57041)
            {"h264_v4l2m2m", "FFMpeg version is too old"},
    #endif
    };
    size_t i, i_mx;

    i_mx = (size_t)(sizeof(blacklisted_codec)/sizeof(blacklisted_codec[0]));
    for (i = 0; i < i_mx; i++) {
        if (mystreq(codec_name, blacklisted_codec[i].codec_name)) {
            return blacklisted_codec[i].reason;
        }
    }
    return NULL;
}

static int movie_set_codec_preferred(struct ctx_movie *movie)
{
    size_t codec_name_len = strcspn(movie->codec_name, ":");

    movie->codec = NULL;
    if (movie->codec_name[codec_name_len]) {
        const char *blacklist_reason = movie_codec_is_blacklisted(&movie->codec_name[codec_name_len+1]);
        if (blacklist_reason) {
            MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                ,_("Preferred codec %s has been blacklisted: %s")
                ,&movie->codec_name[codec_name_len+1], blacklist_reason);
        } else {
            movie->codec = avcodec_find_encoder_by_name(&movie->codec_name[codec_name_len+1]);
            if ((movie->oc->oformat) && (movie->codec != NULL)) {
                    movie->oc->video_codec_id = movie->codec->id;
            } else if (movie->codec == NULL) {
                MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                    ,_("Preferred codec %s not found")
                    ,&movie->codec_name[codec_name_len+1]);
            }
        }
    }
    if (!movie->codec) {
        movie->codec = avcodec_find_encoder(movie->oc->oformat->video_codec);
    }
    if (!movie->codec) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Codec %s not found"), movie->codec_name);
        movie_free_context(movie);
        return -1;
    }

    if (mystreq(movie->codec->name, "h264_v4l2m2m")) {
        movie->preferred_codec = USER_CODEC_V4L2M2M;
    } else if (mystreq(movie->codec->name, "h264_omx")) {
        movie->preferred_codec = USER_CODEC_H264OMX;
    } else if (mystreq(movie->codec->name, "mpeg4_omx")) {
        movie->preferred_codec = USER_CODEC_MPEG4OMX;
    } else {
        movie->preferred_codec = USER_CODEC_DEFAULT;
    }

    if (movie->codec_name[codec_name_len]) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO,_("Using codec %s"), movie->codec->name);
    }

    return 0;

}

static int movie_set_codec(struct ctx_movie *movie)
{

    int retcd;
    char errstr[128];
    int chkrate;

    retcd = movie_set_codec_preferred(movie);
    if (retcd != 0) {
        return retcd;
    }

    #if (MYFFVER >= 57041)
        //If we provide the codec to this, it results in a memory leak.  ffmpeg ticket: 5714
        movie->strm_video = avformat_new_stream(movie->oc, NULL);
        if (!movie->strm_video) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            movie_free_context(movie);
            return -1;
        }
        movie->ctx_codec = avcodec_alloc_context3(movie->codec);
        if (movie->ctx_codec == NULL) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate decoder!"));
            movie_free_context(movie);
            return -1;
        }
    #else
        movie->strm_video = avformat_new_stream(movie->oc, movie->codec);
        if (!movie->strm_video) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            movie_free_context(movie);
            return -1;
        }
        movie->ctx_codec = movie->strm_video->codec;
    #endif


    if (movie->tlapse != TIMELAPSE_NONE) {
        movie->ctx_codec->gop_size = 1;
    } else {
        if (movie->fps <= 5) {
            movie->ctx_codec->gop_size = 1;
        } else if (movie->fps > 30) {
            movie->ctx_codec->gop_size = 15;
        } else {
            movie->ctx_codec->gop_size = (movie->fps / 2);
        }
        movie->gop_cnt = movie->ctx_codec->gop_size - 1;
    }

    /*  For certain containers, setting the fps to very low numbers results in
    **  a very poor quality playback.  We can set the FPS to a higher number and
    **  then let the PTS display the frames correctly.
    */
    if ((movie->tlapse == TIMELAPSE_NONE) && (movie->fps <= 5)) {
        if (mystreq(movie->codec_name, "flv") ||
            mystreq(movie->codec_name, "mp4") ||
            mystreq(movie->codec_name, "hevc")) {
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Low fps. Encoding %d frames into a %d frames container.", movie->fps, 10);
            movie->fps = 10;
        }
    }

    movie->ctx_codec->codec_id      = movie->oc->oformat->video_codec;
    movie->ctx_codec->codec_type    = AVMEDIA_TYPE_VIDEO;
    movie->ctx_codec->bit_rate      = movie->bps;
    movie->ctx_codec->width         = movie->width;
    movie->ctx_codec->height        = movie->height;
    movie->ctx_codec->time_base.num = 1;
    movie->ctx_codec->time_base.den = movie->fps;
    if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
        movie->ctx_codec->pix_fmt   = AV_PIX_FMT_NV21;
    } else {
        movie->ctx_codec->pix_fmt   = MY_PIX_FMT_YUV420P;
    }
    movie->ctx_codec->max_b_frames  = 0;
    if (mystreq(movie->codec_name, "ffv1")) {
      movie->ctx_codec->strict_std_compliance = -2;
      movie->ctx_codec->level = 3;
    }
    movie->ctx_codec->flags |= MY_CODEC_FLAG_GLOBAL_HEADER;

    if ((mystreq(movie->codec->name, "h264_omx")) ||
        (mystreq(movie->codec->name, "mpeg4_omx"))) {
        /* h264_omx & ffmpeg combination locks up on Raspberry Pi.
         * To use h264_omx encoder, we need to disable zerocopy.
         * More information: https://github.com/Motion-Project/motion/issues/433
         */
        av_dict_set(&movie->opts, "zerocopy", "0", 0);
    }

    retcd = movie_set_quality(movie);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to set quality"));
        return -1;
    }

    retcd = avcodec_open2(movie->ctx_codec, movie->codec, &movie->opts);
    if (retcd < 0) {
        if (movie->codec->supported_framerates) {
            const AVRational *fps = movie->codec->supported_framerates;
            while (fps->num) {
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                    ,_("Reported FPS Supported %d/%d"), fps->num, fps->den);
                fps++;
            }
        }
        chkrate = 1;
        while ((chkrate < 36) && (retcd != 0)) {
            movie->ctx_codec->time_base.den = chkrate;
            retcd = avcodec_open2(movie->ctx_codec, movie->codec, &movie->opts);
            chkrate++;
        }
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not open codec %s"),errstr);
            av_dict_free(&movie->opts);
            movie_free_context(movie);
            return -1;
        }

    }
    av_dict_free(&movie->opts);

    return 0;
}

static int movie_set_stream(struct ctx_movie *movie)
{

    #if (MYFFVER >= 57041)
        int retcd;
        char errstr[128];

        retcd = avcodec_parameters_from_context(movie->strm_video->codecpar,movie->ctx_codec);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Failed to copy decoder parameters!: %s"), errstr);
            movie_free_context(movie);
            return -1;
        }
    #endif

    movie->strm_video->time_base = (AVRational){1, movie->fps};

    return 0;

}

/*Special allocation of video buffer for v4l2m2m codec*/
static int movie_alloc_video_buffer(AVFrame *frame, int align)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((enum AVPixelFormat)frame->format);
    int ret, i, padded_height;
    int plane_padding = FFMAX(16 + 16/*STRIDE_ALIGN*/, align);

    if (!desc) {
        return AVERROR(EINVAL);
    }

    if ((ret = av_image_check_size(frame->width, frame->height, 0, NULL)) < 0) {
        return ret;
    }

    if (!frame->linesize[0]) {
        if (align <= 0) {
            align = 32; /* STRIDE_ALIGN. Should be av_cpu_max_align() */
        }

        for(i=1; i<=align; i+=i) {
            ret = av_image_fill_linesizes(frame->linesize,(enum AVPixelFormat) frame->format,
                                          FFALIGN(frame->width, i));
            if (ret < 0) {
                return ret;
            }
            if (!(frame->linesize[0] & (align-1))) {
                break;
            }
        }

        for (i = 0; i < 4 && frame->linesize[i]; i++)
            frame->linesize[i] = FFALIGN(frame->linesize[i], align);
    }

    padded_height = FFALIGN(frame->height, 32);
    ret = av_image_fill_pointers(frame->data
            ,(enum AVPixelFormat) frame->format
            , padded_height
            , NULL
            , frame->linesize);
    if (ret < 0) {
        return ret;
    }

    frame->buf[0] = av_buffer_alloc(ret + 4*plane_padding);
    if (!frame->buf[0]) {
        ret = AVERROR(ENOMEM);
        av_frame_unref(frame);
        return ret;
    }
    frame->buf[1] = av_buffer_alloc(ret + 4*plane_padding);
    if (!frame->buf[1]) {
        ret = AVERROR(ENOMEM);
        av_frame_unref(frame);
        return ret;
    }

    frame->data[0] = frame->buf[0]->data;
    frame->data[1] = frame->buf[1]->data;
    frame->data[2] = frame->data[1] + ((frame->width * padded_height) / 4);

    frame->extended_data = frame->data;

    return 0;
}


static int movie_set_picture(struct ctx_movie *movie)
{

    int retcd;
    char errstr[128];

    movie->picture = myframe_alloc();
    if (!movie->picture) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc frame"));
        movie_free_context(movie);
        return -1;
    }

    /* Take care of variable bitrate setting. */
    if (movie->quality) {
        movie->picture->quality = movie->quality;
    }

    movie->picture->linesize[0] = movie->ctx_codec->width;
    movie->picture->linesize[1] = movie->ctx_codec->width / 2;
    movie->picture->linesize[2] = movie->ctx_codec->width / 2;

    movie->picture->format = movie->ctx_codec->pix_fmt;
    movie->picture->width  = movie->ctx_codec->width;
    movie->picture->height = movie->ctx_codec->height;

    if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
        retcd = movie_alloc_video_buffer(movie->picture, 32);
        if (retcd) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc buffers %s"), errstr);
            movie_free_context(movie);
            return -1;
        }
    }

    return 0;

}

static int movie_set_outputfile(struct ctx_movie *movie)
{

    int retcd;
    char errstr[128];

    #if (MYFFVER < 58000)
        retcd = snprintf(movie->oc->filename, sizeof(movie->oc->filename), "%s", movie->filename);
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting file name"));
            return -1;
        }
    #endif

    /* Open the output file, if needed. */
    if ((movie_timelapse_exists(movie->filename) == 0) || (movie->tlapse != TIMELAPSE_APPEND)) {
        if (!(movie->oc->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&movie->oc->pb, movie->filename, MY_FLAG_WRITE) < 0) {
                if (errno == ENOENT) {
                    if (mycreate_path(movie->filename) == -1) {
                        movie_free_context(movie);
                        return -1;
                    }
                    if (avio_open(&movie->oc->pb, movie->filename, MY_FLAG_WRITE) < 0) {
                        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                            ,_("error opening file %s"), movie->filename);
                        movie_free_context(movie);
                        return -1;
                    }
                    /* Permission denied */
                } else if (errno ==  EACCES) {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Permission denied. %s"),movie->filename);
                    movie_free_context(movie);
                    return -1;
                } else {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Error opening file %s"), movie->filename);
                    movie_free_context(movie);
                    return -1;
                }
            }
        }

        /* Write the stream header,  For the TIMELAPSE_APPEND
         * we write the data via standard file I/O so we close the
         * items here
         */
        retcd = avformat_write_header(movie->oc, NULL);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Could not write movie header %s"),errstr);
            movie_free_context(movie);
            return -1;
        }
        if (movie->tlapse == TIMELAPSE_APPEND) {
            av_write_trailer(movie->oc);
            avio_close(movie->oc->pb);
        }

    }

    return 0;

}

static int movie_flush_codec(struct ctx_movie *movie)
{

    #if (MYFFVER >= 57041)
        //ffmpeg version 3.1 and after

        int retcd;
        int recv_cd = 0;
        char errstr[128];

        if (movie->passthrough) {
            return 0;
        }

        retcd = 0;
        recv_cd = 0;
        if (movie->tlapse == TIMELAPSE_NONE) {
            retcd = avcodec_send_frame(movie->ctx_codec, NULL);
            if (retcd < 0 ) {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    ,_("Error entering draining mode:%s"),errstr);
                return -1;
            }
            while (recv_cd != AVERROR_EOF){
                movie->pkt = mypacket_alloc(movie->pkt);
                recv_cd = avcodec_receive_packet(movie->ctx_codec, movie->pkt);
                if (recv_cd != AVERROR_EOF) {
                    if (recv_cd < 0) {
                        av_strerror(recv_cd, errstr, sizeof(errstr));
                        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                            ,_("Error draining codec:%s"),errstr);
                        movie_free_pkt(movie);
                        return -1;
                    }
                    // v4l2_m2m encoder uses pts 0 and size 0 to indicate AVERROR_EOF
                    if ((movie->pkt->pts == 0) || (movie->pkt->size == 0)) {
                        recv_cd = AVERROR_EOF;
                        movie_free_pkt(movie);
                        continue;
                    }
                    retcd = av_write_frame(movie->oc, movie->pkt);
                    if (retcd < 0) {
                        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                            ,_("Error writing draining video frame"));
                        return -1;
                    }
                }
                movie_free_pkt(movie);
            }
        }
        return 0;
    #else
        (void)movie;
        return 0;
    #endif

}

static int movie_put_frame(struct ctx_movie *movie, const struct timespec *ts1)
{
    int retcd;

    movie->pkt = mypacket_alloc(movie->pkt);

    retcd = movie_set_pts(movie, ts1);
    if (retcd < 0) {
        //If there is an error, it has already been reported.
        movie_free_pkt(movie);
        return 0;
    }

    retcd = movie_encode_video(movie);
    if (retcd != 0) {
        if (retcd != -2) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while encoding picture"));
        }
        movie_free_pkt(movie);
        return retcd;
    }

    if (movie->tlapse == TIMELAPSE_APPEND) {
        retcd = movie_timelapse_append(movie, movie->pkt);
    } else {
        retcd = av_write_frame(movie->oc, movie->pkt);
    }
    movie_free_pkt(movie);

    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while writing video frame"));
        return -1;
    }
    return retcd;

}

/* Reset the written flag and movie start time at opening of each event */
static void movie_passthru_reset(struct ctx_movie *movie)
{
    int indx;

    pthread_mutex_lock(&movie->netcam_data->mutex_pktarray);
        for(indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            movie->netcam_data->pktarray[indx].iswritten = false;
        }
    pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);

}


static int movie_passthru_pktpts(struct ctx_movie *movie)
{
    int64_t ts_interval, base_pdts;
    AVRational tmpbase;
    int indx;

    if (movie->pkt->stream_index == movie->netcam_data->audio_stream_index) {
        tmpbase = movie->strm_audio->time_base;
        indx = movie->netcam_data->audio_stream_index;
        base_pdts = movie->pass_audio_base;
    } else {
        tmpbase = movie->strm_video->time_base;
        indx = movie->netcam_data->video_stream_index;
        base_pdts = movie->pass_video_base;
    }

    if (movie->pkt->pts < base_pdts) {
        ts_interval = 0;
    } else {
        ts_interval = movie->pkt->pts - base_pdts;
    }
    movie->pkt->pts = av_rescale_q(ts_interval
        , movie->netcam_data->transfer_format->streams[indx]->time_base, tmpbase);

    if (movie->pkt->dts < base_pdts) {
        ts_interval = 0;
    } else {
        ts_interval = movie->pkt->dts - base_pdts;
    }
    movie->pkt->dts = av_rescale_q(ts_interval
        , movie->netcam_data->transfer_format->streams[indx]->time_base, tmpbase);

    ts_interval = movie->pkt->duration;
    movie->pkt->duration = av_rescale_q(ts_interval
        , movie->netcam_data->transfer_format->streams[indx]->time_base, tmpbase);

    /*
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("base PTS %" PRId64 " new PTS %" PRId64 " srcbase %d-%d newbase %d-%d")
        ,ts_interval, movie->pkt->duration
        ,movie->netcam_data->transfer_format->streams[indx]->time_base.num
        ,movie->netcam_data->transfer_format->streams[indx]->time_base.den
        ,tmpbase.num, tmpbase.den);
    */

    return 0;
}

static void movie_passthru_write(struct ctx_movie *movie, int indx)
{
    /* Write the packet in the buffer at indx to file */
    char errstr[128];
    int retcd;

    movie->pkt = mypacket_alloc(movie->pkt);
    movie->netcam_data->pktarray[indx].iswritten = true;

    retcd = mycopy_packet(movie->pkt, movie->netcam_data->pktarray[indx].packet);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "av_copy_packet: %s",errstr);
        movie_free_pkt(movie);
        return;
    }

    retcd = movie_passthru_pktpts(movie);
    if (retcd < 0) {
        movie_free_pkt(movie);
        return;
    }

    retcd = av_write_frame(movie->oc, movie->pkt);
    movie_free_pkt(movie);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error while writing video frame: %s"),errstr);
        return;
    }

}

static void movie_passthru_minpts(struct ctx_movie *movie)
{

    int indx, indx_audio, indx_video;

    movie->pass_audio_base = 0;
    movie->pass_video_base = 0;

    pthread_mutex_lock(&movie->netcam_data->mutex_pktarray);
        indx_audio =  movie->netcam_data->audio_stream_index;
        indx_video =  movie->netcam_data->video_stream_index;

        for (indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            if (movie->netcam_data->pktarray[indx].packet->stream_index == indx_audio) {
                movie->pass_audio_base = movie->netcam_data->pktarray[indx].packet->pts;
            };
            if (movie->netcam_data->pktarray[indx].packet->stream_index == indx_video) {
                movie->pass_video_base = movie->netcam_data->pktarray[indx].packet->pts;
            };
        }
        for (indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            if ((movie->netcam_data->pktarray[indx].packet->stream_index == indx_audio) &&
                (movie->netcam_data->pktarray[indx].packet->pts < movie->pass_audio_base)) {
                movie->pass_audio_base = movie->netcam_data->pktarray[indx].packet->pts;
            };
            if ((movie->netcam_data->pktarray[indx].packet->stream_index == indx_audio) &&
                (movie->netcam_data->pktarray[indx].packet->dts < movie->pass_audio_base)) {
                movie->pass_audio_base = movie->netcam_data->pktarray[indx].packet->dts;
            };
            if ((movie->netcam_data->pktarray[indx].packet->stream_index == indx_video) &&
                (movie->netcam_data->pktarray[indx].packet->pts < movie->pass_video_base)) {
                movie->pass_video_base = movie->netcam_data->pktarray[indx].packet->pts;
            };
            if ((movie->netcam_data->pktarray[indx].packet->stream_index == indx_video) &&
                (movie->netcam_data->pktarray[indx].packet->dts < movie->pass_video_base)) {
                movie->pass_video_base = movie->netcam_data->pktarray[indx].packet->dts;
            };
        }
    pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);

    if (movie->pass_audio_base < 0) {
         movie->pass_audio_base = 0;
    }

    if (movie->pass_video_base < 0) {
        movie->pass_video_base = 0;
    }

}

static int movie_passthru_put(struct ctx_movie *movie, struct ctx_image_data *img_data)
{
    int idnbr_image, idnbr_lastwritten, idnbr_stop, idnbr_firstkey;
    int indx, indx_lastwritten, indx_firstkey, indx_video;

    if (movie->netcam_data == NULL) {
        return -1;
    }

    if ((movie->netcam_data->status == NETCAM_NOTCONNECTED  ) ||
        (movie->netcam_data->status == NETCAM_RECONNECTING  ) ) {
        return 0;
    }

    if (movie->high_resolution) {
        idnbr_image = img_data->idnbr_high;
    } else {
        idnbr_image = img_data->idnbr_norm;
    }

    pthread_mutex_lock(&movie->netcam_data->mutex_pktarray);
        idnbr_lastwritten = 0;
        idnbr_firstkey = idnbr_image;
        idnbr_stop = 0;
        indx_lastwritten = -1;
        indx_firstkey = -1;
        indx_video = movie->netcam_data->video_stream_index;

        for(indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            if ((movie->netcam_data->pktarray[indx].iswritten) &&
                (movie->netcam_data->pktarray[indx].idnbr > idnbr_lastwritten) &&
                (movie->netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_lastwritten=movie->netcam_data->pktarray[indx].idnbr;
                indx_lastwritten = indx;
            }
            if ((movie->netcam_data->pktarray[indx].idnbr >  idnbr_stop) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_image)&&
                (movie->netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_stop=movie->netcam_data->pktarray[indx].idnbr;
            }
            if ((movie->netcam_data->pktarray[indx].iskey) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_firstkey)&&
                (movie->netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                    idnbr_firstkey=movie->netcam_data->pktarray[indx].idnbr;
                    indx_firstkey = indx;
            }
        }

        if (idnbr_stop == 0) {
            pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);
            return 0;
        }

        if (indx_lastwritten != -1) {
            indx = indx_lastwritten;
        } else if (indx_firstkey != -1) {
            indx = indx_firstkey;
        } else {
            indx = 0;
        }

        while (true){
            if ((!movie->netcam_data->pktarray[indx].iswritten) &&
                (movie->netcam_data->pktarray[indx].packet->size > 0) &&
                (movie->netcam_data->pktarray[indx].idnbr >  idnbr_lastwritten) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_image)) {
                movie_passthru_write(movie, indx);
            }
            if (movie->netcam_data->pktarray[indx].idnbr == idnbr_stop) {
                break;
            }
            indx++;
            if (indx == movie->netcam_data->pktarray_size ) {
                indx = 0;
            }
        }
    pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);
    return 0;
}

static int movie_passthru_streams_video(struct ctx_movie *movie, AVStream *stream_in)
{
    int retcd;

    movie->strm_video = avformat_new_stream(movie->oc, NULL);
    if (!movie->strm_video) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc video stream"));
        return -1;
    }

    retcd = avcodec_parameters_copy(movie->strm_video->codecpar, stream_in->codecpar);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy video codec parameters"));
        return -1;
    }

    movie->strm_video->codecpar->codec_tag  = 0;
    movie->strm_video->time_base = stream_in->time_base;
    movie->strm_video->avg_frame_rate = stream_in->avg_frame_rate;

    MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO
        , _("video timebase %d/%d fps %d/%d")
        , movie->strm_video->time_base.num
        , movie->strm_video->time_base.den
        , movie->strm_video->avg_frame_rate.num
        , movie->strm_video->avg_frame_rate.den);

    return 0;
}

static int movie_passthru_streams_audio(struct ctx_movie *movie, AVStream *stream_in)
{
    int         retcd;

    movie->strm_audio = avformat_new_stream(movie->oc, NULL);
    if (!movie->strm_audio) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc audio stream"));
        return -1;
    }

    retcd = avcodec_parameters_copy(movie->strm_audio->codecpar, stream_in->codecpar);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy audio codec parameters"));
        return -1;
    }

    movie->strm_audio->codecpar->codec_tag  = 0;
    movie->strm_audio->time_base = stream_in->time_base;
    movie->strm_audio->r_frame_rate = stream_in->time_base;
    movie->strm_audio->avg_frame_rate= stream_in->time_base;
    movie->strm_audio->codecpar->format = stream_in->codecpar->format;
    movie->strm_audio->codecpar->sample_rate = stream_in->codecpar->sample_rate;
    movie->strm_audio->avg_frame_rate = stream_in->avg_frame_rate;

    MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO
        , _("audio timebase %d/%d")
        , movie->strm_audio->time_base.num
        , movie->strm_audio->time_base.den);
    return 0;
}

static int movie_passthru_streams(struct ctx_movie *movie)
{
    #if (MYFFVER >= 57041)
        int         retcd, indx;
        AVStream    *stream_in;

        pthread_mutex_lock(&movie->netcam_data->mutex_transfer);
            for (indx= 0; indx < (int)movie->netcam_data->transfer_format->nb_streams; indx++) {
                stream_in = movie->netcam_data->transfer_format->streams[indx];
                movie->oc->video_codec_id = stream_in->codecpar->codec_id;
                if (stream_in->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    retcd = movie_passthru_streams_video(movie, stream_in);
                } else if (stream_in->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                    retcd = movie_passthru_streams_audio(movie, stream_in);
                }
                if (retcd < 0) {
                    pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
                    return retcd;
                }
            }
        pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);

        return 0;
    #else
        /* This is disabled in the check_passthrough but we need it here for compiling */
        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("Pass-through disabled.  ffmpeg too old"));
        return -1;
    #endif
}

static int movie_passthru_check(struct ctx_movie *movie)
{
    if ((movie->netcam_data->status == NETCAM_NOTCONNECTED  ) ||
        (movie->netcam_data->status == NETCAM_RECONNECTING  )) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
            ,_("rtsp camera not ready for pass-through."));
        return -1;
    }

    if (movie->netcam_data == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("RTSP context not available."));
        return -1;
    }

    movie_passthru_reset(movie);

    return 0;
}

static int movie_passthru_open(struct ctx_movie *movie)
{
    int retcd;

    retcd = movie_passthru_check(movie);
    if (retcd < 0) {
        return retcd;
    }

    movie->oc = avformat_alloc_context();
    if (!movie->oc) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        movie_free_context(movie);
        return -1;
    }

    if (mystrne(movie->codec_name, "mp4") && mystrne(movie->codec_name, "mkv")) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
            ,_("Changing to MP4 container for pass-through."));
        movie->codec_name = "mp4";
    }

    movie_passthru_minpts(movie);

    retcd = movie_get_oformat(movie);
    if (retcd < 0 ) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get output format!"));
        return -1;
    }

    retcd = movie_passthru_streams(movie);
    if (retcd < 0 ) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get streams!"));
        return -1;
    }

    retcd = movie_set_outputfile(movie);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the output file"));
        return -1;
    }

    if (movie->strm_audio != NULL) {
        MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO
            , _("Timebase after open audio: %d/%d video: %d/%d")
            , movie->strm_audio->time_base.num
            , movie->strm_audio->time_base.den
            , movie->strm_video->time_base.num
            , movie->strm_video->time_base.den);
    }

    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "Pass-through stream opened");

    return 0;
}

void movie_avcodec_log(void *ignoreme, int errno_flag, const char *fmt, va_list vl)
{

    char buf[1024];
    char *end;
    int retcd;

    (void)ignoreme;
    (void)errno_flag;

    /* Valgrind occasionally reports use of uninitialized values in here when we interrupt
     * some rtsp functions.  The offending value is either fmt or vl and seems to be from a
     * debug level of av functions.  To address it we flatten the message after we know
     * the log level.  Now we put the avcodec messages to INF level since their error
     * are not necessarily our errors.
     */

    if (errno_flag <= AV_LOG_WARNING) {
        retcd = vsnprintf(buf, sizeof(buf), fmt, vl);
        if (retcd >=1024) {
            MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO, "av message truncated %d bytes",(retcd - 1024));
        }
        end = buf + strlen(buf);
        if (end > buf && end[-1] == '\n') {
            *--end = 0;
        }

        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s", buf);
    }

}

static void movie_put_pix_nv21(struct ctx_movie *movie, struct ctx_image_data *img_data)
{
    unsigned char *image,*imagecr, *imagecb;
    int cr_len, x, y;

    if (movie->high_resolution) {
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    cr_len = movie->ctx_codec->width * movie->ctx_codec->height / 4;
    imagecr = image + (movie->ctx_codec->width * movie->ctx_codec->height);
    imagecb = image + (movie->ctx_codec->width * movie->ctx_codec->height) + cr_len;

    memcpy(movie->picture->data[0], image, movie->ctx_codec->width * movie->ctx_codec->height);
    for (y = 0; y < movie->ctx_codec->height; y++) {
        for (x = 0; x < movie->ctx_codec->width/4; x++) {
            movie->picture->data[1][y*movie->ctx_codec->width/2 + x*2] = *imagecb;
            movie->picture->data[1][y*movie->ctx_codec->width/2 + x*2 + 1] = *imagecr;
            imagecb++;
            imagecr++;
        }
    }

}

static void movie_put_pix_yuv420(struct ctx_movie *movie, struct ctx_image_data *img_data)
{
    unsigned char *image;

    if (movie->high_resolution) {
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    // Usual setup for image pointers
    movie->picture->data[0] = image;
    movie->picture->data[1] = image + (movie->ctx_codec->width * movie->ctx_codec->height);
    movie->picture->data[2] = movie->picture->data[1] + ((movie->ctx_codec->width * movie->ctx_codec->height) / 4);

}

void movie_global_init(void)
{

    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("libavcodec  version %d.%d.%d")
        , LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("libavformat version %d.%d.%d")
        , LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);

    #if (MYFFVER < 58000)
        av_register_all();
        avcodec_register_all();
    #endif

    avformat_network_init();
    avdevice_register_all();
    av_log_set_callback(movie_avcodec_log);

}

void movie_global_deinit(void)
{

    avformat_network_deinit();

    #if (MYFFVER < 58000)
        /* TODO Determine if this is even needed for old versions */
        if (av_lockmgr_register(NULL) < 0) {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("av_lockmgr_register reset failed on cleanup"));
        }
    #endif


}

int movie_open(struct ctx_movie *movie)
{
    int retcd;

    if (movie->passthrough) {
        retcd = movie_passthru_open(movie);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not setup passthrough!"));
            movie_free_context(movie);
            return -1;
        }
        return 0;
    }

    movie->oc = avformat_alloc_context();
    if (!movie->oc) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        movie_free_context(movie);
        return -1;
    }

    retcd = movie_get_oformat(movie);
    if (retcd < 0 ) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
        movie_free_context(movie);
        return -1;
    }

    retcd = movie_set_codec(movie);
    if (retcd < 0 ) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate codec!"));
        return -1;
    }

    retcd = movie_set_stream(movie);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    retcd = movie_set_picture(movie);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    retcd = movie_set_outputfile(movie);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    return 0;

}

void movie_close(struct ctx_movie *movie)
{

    if (movie != NULL) {

        if (movie_flush_codec(movie) < 0) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error flushing codec"));
        }
        if (movie->oc != NULL) {
            if (movie->oc->pb != NULL) {
                if (movie->tlapse != TIMELAPSE_APPEND) {
                    av_write_trailer(movie->oc);
                }
                if (!(movie->oc->oformat->flags & AVFMT_NOFILE)) {
                    if (movie->tlapse != TIMELAPSE_APPEND) {
                        avio_close(movie->oc->pb);
                    }
                }
            }
        }
        movie_free_context(movie);
        movie_free_nal(movie);
    }

}

int movie_put_image(struct ctx_movie *movie, struct ctx_image_data *img_data, const struct timespec *ts1)
{

    int retcd = 0;
    int cnt = 0;

    if (movie->passthrough) {
        retcd = movie_passthru_put(movie, img_data);
        return retcd;
    }

    if (movie->picture) {

        if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
            movie_put_pix_nv21(movie, img_data);
        } else {
            movie_put_pix_yuv420(movie, img_data);
        }

        movie->gop_cnt ++;
        if (movie->gop_cnt == movie->ctx_codec->gop_size ) {
            movie->picture->pict_type = AV_PICTURE_TYPE_I;
            movie->picture->key_frame = 1;
            movie->gop_cnt = 0;
        } else {
            movie->picture->pict_type = AV_PICTURE_TYPE_P;
            movie->picture->key_frame = 0;
        }

        /* A return code of -2 is thrown by the put_frame
         * when a image is buffered.  For timelapse, we absolutely
         * never want a frame buffered so we keep sending back the
         * the same pic until it flushes or fails in a different way
         */
        retcd = movie_put_frame(movie, ts1);
        while ((retcd == -2) && (movie->tlapse != TIMELAPSE_NONE)) {
            retcd = movie_put_frame(movie, ts1);
            cnt++;
            if (cnt > 50) {
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    ,_("Excessive attempts to clear buffered packet"));
                retcd = -1;
            }
        }
        //non timelapse buffered is ok
        if (retcd == -2) {
            retcd = 0;
            MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO, _("Buffered packet"));
        }
    }

    return retcd;

}

void movie_reset_start_time(struct ctx_movie *movie, const struct timespec *ts1)
{
    int64_t one_frame_interval = av_rescale_q(1,(AVRational){1, movie->fps}, movie->strm_video->time_base);
    if (one_frame_interval <= 0) {
        one_frame_interval = 1;
    }
    movie->base_pts = movie->last_pts + one_frame_interval;

    movie->start_time.tv_sec = ts1->tv_sec;
    movie->start_time.tv_nsec = ts1->tv_nsec;

}

static const char* movie_init_codec(struct ctx_cam *cam)
{

    /* The following section allows for testing of all the various containers
    * that Motion permits. The container type is pre-pended to the name of the
    * file so that we can determine which container type created what movie.
    * The intent for this is be used for developer testing when the ffmpeg libs
    * change or the code inside our movie module changes.  For each event, the
    * container type will change.  This way, you can turn on emulate motion, then
    * specify a maximum movie time and let Motion run for days creating all the
    * different types of movies checking for crashes, warnings, etc.
    */
    int codenbr;

    if (cam->conf->movie_codec == "test") {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Running test of the various output formats.");
        codenbr = cam->event_nr % 10;
        if (codenbr == 1) {
            return "flv";
        } else if (codenbr == 2) {
            return "ogg";
        } else if (codenbr == 3) {
            return "vp8";
        } else if (codenbr == 4) {
            return "mp4";
        } else if (codenbr == 5) {
            return "mkv";
        } else if (codenbr == 6) {
            return "hevc";
        } else if (codenbr == 7) {
            return "flv";
        } else if (codenbr == 8) {
            return "ogg";
        } else if (codenbr == 9) {
            return "vp8";
        } else                   {
            return "mkv";
        }
    }

    return cam->conf->movie_codec.c_str();

}

int movie_init_norm(struct ctx_cam *cam, struct timespec *ts1)
{
    char stamp[PATH_MAX];
    const char *codec;
    int retcd;

    mystrftime(cam, stamp, sizeof(stamp), cam->conf->movie_filename.c_str(), ts1, NULL, 0);

    codec = movie_init_codec(cam);

    cam->movie_norm =(struct ctx_movie*) mymalloc(sizeof(struct ctx_movie));
    if (mystreq(codec, "test")) {
        retcd = snprintf(cam->movie_norm->filename, PATH_MAX, "%s/%s_%s"
            , cam->conf->target_dir.c_str(), codec, stamp);
    } else {
        retcd = snprintf(cam->movie_norm->filename, PATH_MAX, "%s/%s"
            , cam->conf->target_dir.c_str(), stamp);
    }
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting file name"));
        return -1;
    }
    if (cam->imgs.size_high > 0) {
        cam->movie_norm->width  = cam->imgs.width_high;
        cam->movie_norm->height = cam->imgs.height_high;
        cam->movie_norm->high_resolution = true;
        cam->movie_norm->netcam_data = cam->netcam_high;
    } else {
        cam->movie_norm->width  = cam->imgs.width;
        cam->movie_norm->height = cam->imgs.height;
        cam->movie_norm->high_resolution = false;
        cam->movie_norm->netcam_data = cam->netcam;
    }
    cam->movie_norm->pkt = NULL;
    cam->movie_norm->tlapse = TIMELAPSE_NONE;
    cam->movie_norm->fps = cam->lastrate;
    cam->movie_norm->bps = cam->conf->movie_bps;
    cam->movie_norm->quality = cam->conf->movie_quality;
    cam->movie_norm->start_time.tv_sec = ts1->tv_sec;
    cam->movie_norm->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_norm->last_pts = -1;
    cam->movie_norm->base_pts = 0;
    cam->movie_norm->gop_cnt = 0;
    cam->movie_norm->codec_name = codec;
    if (cam->conf->movie_codec == "test") {
        cam->movie_norm->test_mode = true;
    } else {
        cam->movie_norm->test_mode = false;
    }
    cam->movie_norm->motion_images = 0;
    cam->movie_norm->passthrough = cam->movie_passthrough;

    retcd = movie_open(cam->movie_norm);

    if (retcd == 0) {
        /* We set this after the movie open so we get the extension */
        retcd = snprintf(cam->newfilename, PATH_MAX, "%s",cam->movie_norm->filename);
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting file name"));
            return -1;
        }
    }

    return retcd;

}

int movie_init_motion(struct ctx_cam *cam, struct timespec *ts1)
{
    char stamp[PATH_MAX];
    const char *codec;
    int retcd;

    mystrftime(cam, stamp, sizeof(stamp), cam->conf->movie_filename.c_str(), ts1, NULL, 0);
    codec = movie_init_codec(cam);

    cam->movie_motion =(struct ctx_movie*)mymalloc(sizeof(struct ctx_movie));
    if (mystreq(codec, "test")) {
        retcd = snprintf(cam->movie_motion->filename, PATH_MAX, "%s/%s_%sm"
            , cam->conf->target_dir.c_str(), codec, stamp);
    } else {
        retcd = snprintf(cam->movie_motion->filename, PATH_MAX, "%s/%sm"
            , cam->conf->target_dir.c_str(), stamp);
    }
    if ((retcd < 0) || (retcd >= PATH_MAX)) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting file name"));
        return -1;
    }
    cam->movie_motion->pkt = NULL;
    cam->movie_motion->width  = cam->imgs.width;
    cam->movie_motion->height = cam->imgs.height;
    cam->movie_motion->netcam_data = NULL;
    cam->movie_motion->tlapse = TIMELAPSE_NONE;
    cam->movie_motion->fps = cam->lastrate;
    cam->movie_motion->bps = cam->conf->movie_bps;
    cam->movie_motion->quality = cam->conf->movie_quality;
    cam->movie_motion->start_time.tv_sec = ts1->tv_sec;
    cam->movie_motion->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_motion->last_pts = -1;
    cam->movie_motion->base_pts = 0;
    cam->movie_motion->gop_cnt = 0;
    cam->movie_motion->codec_name = codec;
    if (cam->conf->movie_codec == "test") {
        cam->movie_motion->test_mode = true;
    } else {
        cam->movie_motion->test_mode = false;
    }
    cam->movie_motion->motion_images = true;
    cam->movie_motion->passthrough = false;
    cam->movie_motion->high_resolution = false;
    cam->movie_motion->netcam_data = NULL;

    retcd = movie_open(cam->movie_motion);

    if (retcd == 0) {
        retcd = snprintf(cam->motionfilename, PATH_MAX, "%s",cam->movie_motion->filename);
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting file name"));
            return -1;
        }
    }

    return retcd;

}

int movie_init_timelapse(struct ctx_cam *cam, struct timespec *ts1)
{
    char tmp[PATH_MAX];
    const char *codec_mpg = "mpg";
    const char *codec_mkv = "mkv";
    int retcd;

    cam->movie_timelapse =(struct ctx_movie*)mymalloc(sizeof(struct ctx_movie));
    mystrftime(cam, tmp, sizeof(tmp), cam->conf->timelapse_filename.c_str(), ts1, NULL, 0);

    retcd = snprintf(cam->movie_timelapse->filename, PATH_MAX, "%s/%s"
        , cam->conf->target_dir.c_str(), tmp);
    if (retcd > PATH_MAX) {
        MOTION_LOG(ERR, TYPE_ALL, NO_ERRNO
            , _("Error setting timelapse file name %s"), tmp);
    }

    if ((cam->imgs.size_high > 0) && (!cam->movie_passthrough)) {
        cam->movie_timelapse->width  = cam->imgs.width_high;
        cam->movie_timelapse->height = cam->imgs.height_high;
        cam->movie_timelapse->high_resolution = true;
    } else {
        cam->movie_timelapse->width  = cam->imgs.width;
        cam->movie_timelapse->height = cam->imgs.height;
        cam->movie_timelapse->high_resolution = false;
    }
    cam->movie_timelapse->pkt = NULL;
    cam->movie_timelapse->fps = cam->conf->timelapse_fps;
    cam->movie_timelapse->bps = cam->conf->movie_bps;
    cam->movie_timelapse->quality = cam->conf->movie_quality;
    cam->movie_timelapse->start_time.tv_sec = ts1->tv_sec;
    cam->movie_timelapse->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_timelapse->last_pts = -1;
    cam->movie_timelapse->base_pts = 0;
    cam->movie_timelapse->test_mode = false;
    cam->movie_timelapse->gop_cnt = 0;
    cam->movie_timelapse->motion_images = false;
    cam->movie_timelapse->passthrough = false;
    cam->movie_timelapse->netcam_data = NULL;

    if (cam->conf->timelapse_container == "mpg") {
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpg container."));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be appended to file"));

        cam->movie_timelapse->tlapse = TIMELAPSE_APPEND;
        cam->movie_timelapse->codec_name = codec_mpg;
        retcd = movie_open(cam->movie_timelapse);
    } else {
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mkv container."));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be trigger new files"));

        cam->movie_timelapse->tlapse = TIMELAPSE_NEW;
        cam->movie_timelapse->codec_name = codec_mkv;
        retcd = movie_open(cam->movie_timelapse);
    }

    if (retcd == 0) {
        retcd = snprintf(cam->timelapsefilename, PATH_MAX, "%s",cam->movie_timelapse->filename);
        if ((retcd < 0) || (retcd >= PATH_MAX)) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting file name"));
            return -1;
        }
    }

    return retcd;

}
