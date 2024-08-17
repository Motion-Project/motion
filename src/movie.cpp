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

#include "motionplus.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "netcam.hpp"
#include "dbse.hpp"
#include "alg_sec.hpp"
#include "movie.hpp"

int movie_interrupt(void *ctx)
{
    cls_movie *movie = (cls_movie *)ctx;

    clock_gettime(CLOCK_MONOTONIC, &movie->cb_cr_ts);
    if ((movie->cb_cr_ts.tv_sec - movie->cb_st_ts.tv_sec ) > movie->cb_dur) {
        MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO,_("Movie timed out"));
        return 1;
    }
    return 0;
}

void cls_movie::free_pkt()
{
    av_packet_free(&pkt);
    pkt = nullptr;
}

void cls_movie::free_nal()
{
    if (nal_info) {
        free(nal_info);
        nal_info = nullptr;
        nal_info_len = 0;
    }
}

void cls_movie::encode_nal()
{
    // h264_v4l2m2m has NAL units separated from the first frame, which makes
    // some players very unhappy.
    if ((pkt->pts == 0) && (!(pkt->flags & AV_PKT_FLAG_KEY))) {
        free_nal();
        nal_info_len = pkt->size;
        nal_info =(char*)mymalloc((uint)nal_info_len);
        if (nal_info) {
            memcpy(nal_info, &pkt->data[0], (uint)nal_info_len);
        } else {
            nal_info_len = 0;
        }
    } else if (nal_info) {
        int old_size = pkt->size;
        av_grow_packet(pkt, nal_info_len);
        memmove(&pkt->data[nal_info_len], &pkt->data[0],(uint)old_size);
        memcpy(&pkt->data[0], nal_info, (uint)nal_info_len);
        free_nal();
    }
}

int cls_movie::timelapse_exists(const char *fname)
{
    struct stat statbuf;
    if (stat(fname, &statbuf) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int cls_movie::timelapse_append(AVPacket *p_pkt)
{
    FILE *file;

    file = myfopen(full_nm.c_str(), "abe");
    if (file == nullptr) {
        return -1;
    }

    fwrite(p_pkt->data, 1, (uint)p_pkt->size, file);

    myfclose(file);

    return 0;
}

void cls_movie::free_context()
{
    if (picture != nullptr) {
        av_frame_free(&picture);
        picture = nullptr;
    }

    if (ctx_codec != nullptr) {
        avcodec_free_context(&ctx_codec);
        ctx_codec = nullptr;
    }

    if (oc != nullptr) {
        avformat_free_context(oc);
        oc = nullptr;
    }
}

int cls_movie::get_oformat()
{
    if (tlapse == TIMELAPSE_APPEND) {
        oc->oformat = av_guess_format("mpeg2video", nullptr, nullptr);
        oc->video_codec_id = MY_CODEC_ID_MPEG2VIDEO;
        full_nm += ".mpg";
        movie_nm += ".mpg";
        if (oc->oformat == nullptr) {
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting timelapse append for container %s")
                , container.c_str());
            free_context();
            return -1;
        }
        return 0;
    }

    if (container == "mov") {
        oc->oformat = av_guess_format("mov", nullptr, nullptr);
        full_nm += ".mov";
        movie_nm += ".mov";
        oc->video_codec_id = MY_CODEC_ID_H264;
    }

    if (container == "flv") {
        oc->oformat = av_guess_format("flv", nullptr, nullptr);
        full_nm += ".flv";
        movie_nm += ".flv";
        oc->video_codec_id = MY_CODEC_ID_FLV1;
    }

    if (container == "ogg") {
        oc->oformat = av_guess_format("ogg", nullptr, nullptr);
        full_nm += ".ogg";
        movie_nm += ".ogg";
        oc->video_codec_id = MY_CODEC_ID_THEORA;
    }

    if (container == "webm") {
        oc->oformat = av_guess_format("webm", nullptr, nullptr);
        full_nm += ".webm";
        movie_nm += ".webm";
        oc->video_codec_id = MY_CODEC_ID_VP8;
    }

    if (container == "mp4") {
        oc->oformat = av_guess_format("mp4", nullptr, nullptr);
        full_nm += ".mp4";
        movie_nm += ".mp4";
        oc->video_codec_id = MY_CODEC_ID_H264;
    }

    if (container == "mkv") {
        oc->oformat = av_guess_format("matroska", nullptr, nullptr);
        full_nm += ".mkv";
        movie_nm += ".mkv";
        oc->video_codec_id = MY_CODEC_ID_H264;
    }

    if (container == "hevc") {
        oc->video_codec_id = MY_CODEC_ID_HEVC;
        oc->oformat = av_guess_format("mp4", nullptr, nullptr);
        full_nm += ".mp4";
        movie_nm += ".mp4";
        oc->video_codec_id = MY_CODEC_ID_HEVC;
    }

    if (oc->oformat == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("container option value %s is not supported")
            , container.c_str());
        free_context();
        return -1;
    }

    if (oc->oformat->video_codec == MY_CODEC_ID_NONE) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get the container"));
        free_context();
        return -1;
    }

    return 0;
}

int cls_movie::encode_video()
{
    int retcd = 0;
    char errstr[128];

    retcd = avcodec_send_frame(ctx_codec, picture);
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error sending frame for encoding:%s"),errstr);
        return -1;
    }
    retcd = avcodec_receive_packet(ctx_codec, pkt);
    if (retcd == AVERROR(EAGAIN)) {
        //Buffered packet.  Throw special return code
        free_pkt();
        return -2;
    }
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error receiving encoded packet video:%s"),errstr);
        //Packet is freed upon failure of encoding
        return -1;
    }

    if (preferred_codec == "h264_v4l2m2m") {
        encode_nal();
    }

    return 0;
}

int cls_movie::set_pts(const struct timespec *ts1)
{
    int64_t pts_interval;

    if (tlapse != TIMELAPSE_NONE) {
        last_pts++;
        picture->pts = last_pts;
    } else {
        pts_interval = ((1000000L * (ts1->tv_sec - start_time.tv_sec)) + (ts1->tv_nsec/1000) - (start_time.tv_nsec/1000));
        if (pts_interval < 0) {
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            reset_start_time(ts1);
            pts_interval = 0;
        }
        if (last_pts < 0) {
            // This is the very first frame, ensure PTS is zero
            picture->pts = 0;
        } else {
            picture->pts = base_pts +
                av_rescale_q(pts_interval
                    , av_make_q(1, 1000000L)
                    , strm_video->time_base);
        }
        if (test_mode == true) {
            MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO
                ,_("PTS %" PRId64 " Base PTS %" PRId64 " ms interval %" PRId64 " timebase %d-%d")
                ,picture->pts,base_pts,pts_interval
                ,strm_video->time_base.num,strm_video->time_base.den);
        }

        if (picture->pts <= last_pts) {
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (test_mode == true) {
                MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        last_pts = picture->pts;
    }
    return 0;
}

int cls_movie::set_quality()
{
    int quality;

    opts = 0;
    quality = cam->cfg->movie_quality;
    if (quality > 100) {
        quality = 100;
    }
    if (ctx_codec->codec_id == MY_CODEC_ID_H264 ||
        ctx_codec->codec_id == MY_CODEC_ID_HEVC) {
        if (quality <= 0) {
            quality = 45; // default to 45%
        }

        if (preferred_codec == "h264_v4l2m2m") {

            // bit_rate = width * height * fps * quality_factor
            quality = (int)(((int64_t)width * height * fps * quality) >> 7);
            // Clip bit rate to min
            if (quality < 4000) {
                // magic number
                quality = 4000;
            }
            ctx_codec->profile = FF_PROFILE_H264_HIGH;
            ctx_codec->bit_rate = quality;
            av_dict_set(&opts, "preset", "ultrafast", 0);
            av_dict_set(&opts, "tune", "zerolatency", 0);

        } else {
            /* Control other H264 encoders quality is via CRF.  To get the profiles
             * to work (main), (high), we are setting this via the opt instead of
             * dictionary.  The ultrafast is not used because at that level, the
             * profile reverts to (baseline) and a bit more efficiency is in
             * (main) or (high) so we choose next fastest option (superfast)
             */
            char crf[10];
            quality = (int)(( (100-quality) * 51)/100);
            snprintf(crf, 10, "%d", quality);
            if (ctx_codec->codec_id == MY_CODEC_ID_H264) {
                av_opt_set(ctx_codec->priv_data, "profile", "high", 0);
            }
            av_opt_set(ctx_codec->priv_data, "crf", crf, 0);
            av_opt_set(ctx_codec->priv_data, "tune", "zerolatency", 0);
            av_opt_set(ctx_codec->priv_data, "preset", "superfast",0);
        }
    } else {
        /* The selection of 8000 is a subjective number based upon viewing output files */
        if (quality > 0) {
            quality =(int)(((100-quality)*(100-quality)*(100-quality) * 8000) / 1000000) + 1;
            ctx_codec->flags |= MY_CODEC_FLAG_QSCALE;
            ctx_codec->global_quality=quality;
        }
    }
    MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("%s codec vbr/crf/bit_rate: %d"), codec->name, quality);

    return 0;
}

int cls_movie::set_codec_preferred()
{
    codec = nullptr;
    if (preferred_codec != "") {
        codec = avcodec_find_encoder_by_name(preferred_codec.c_str());
        if (codec == nullptr) {
            MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("Failed to find user requested codec %s")
                , preferred_codec.c_str());
            codec = avcodec_find_encoder(oc->video_codec_id);
        } else {
            MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("Using codec %s"), preferred_codec.c_str());
        }
    } else {
        codec = avcodec_find_encoder(oc->video_codec_id);
    }
    if (codec == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("codec for container %s not found"), container.c_str());
        free_context();
        return -1;
    }

    return 0;
}

int cls_movie::set_codec()
{
    int retcd;
    char errstr[128];
    int chkrate;

    if (set_codec_preferred() != 0) {
        return -1;
    }

    strm_video = avformat_new_stream(oc, codec);
    if (!strm_video) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
        free_context();
        return -1;
    }
    ctx_codec = avcodec_alloc_context3(codec);
    if (ctx_codec == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate codec context!"));
        free_context();
        return -1;
    }

    if (tlapse != TIMELAPSE_NONE) {
        ctx_codec->gop_size = 1;
    } else {
        if (fps <= 5) {
            ctx_codec->gop_size = 1;
        } else if (fps > 30) {
            ctx_codec->gop_size = 15;
        } else {
            ctx_codec->gop_size = (fps / 2);
        }
        gop_cnt = ctx_codec->gop_size - 1;
    }

    /*  For certain containers, setting the fps to very low numbers results in
    **  a very poor quality playback.  We can set the FPS to a higher number and
    **  then let the PTS display the frames correctly.
    */
    if ((tlapse == TIMELAPSE_NONE) && (fps <= 5)) {
        if ((container == "flv") ||
            (container == "mp4") ||
            (container == "hevc")) {
            MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                , "Low fps. Encoding %d frames into a %d frames container."
                , fps, 10);
            fps = 10;
        }
    }

    ctx_codec->codec_id      = codec->id;
    ctx_codec->codec_type    = AVMEDIA_TYPE_VIDEO;
    ctx_codec->bit_rate      = cam->cfg->movie_bps;
    ctx_codec->width         = width;
    ctx_codec->height        = height;
    ctx_codec->time_base.num = 1;
    ctx_codec->time_base.den = fps;
    ctx_codec->pix_fmt   = MY_PIX_FMT_YUV420P;
    ctx_codec->max_b_frames  = 0;
    ctx_codec->flags |= MY_CODEC_FLAG_GLOBAL_HEADER;

    if (set_quality() < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to set quality"));
        return -1;
    }

    retcd = avcodec_open2(ctx_codec, codec, &opts);
    if (retcd < 0) {
        if (codec->supported_framerates) {
            const AVRational *p_fps = codec->supported_framerates;
            while (p_fps->num) {
                MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO
                    ,_("Reported FPS Supported %d/%d"), p_fps->num, p_fps->den);
                p_fps++;
            }
        }
        chkrate = 1;
        while ((chkrate < 36) && (retcd != 0)) {
            ctx_codec->time_base.den = chkrate;
            retcd = avcodec_open2(ctx_codec, codec, &opts);
            chkrate++;
        }
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not open codec %s"),errstr);
            av_dict_free(&opts);
            free_context();
            return -1;
        }

    }
    av_dict_free(&opts);

    return 0;
}

int cls_movie::set_stream()
{
    int retcd;
    char errstr[128];

    retcd = avcodec_parameters_from_context(strm_video->codecpar,ctx_codec);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Failed to copy decoder parameters!: %s"), errstr);
        free_context();
        return -1;
    }

    strm_video->time_base =  av_make_q(1, fps);

    return 0;
}

/*Special allocation of video buffer for v4l2m2m codec*/
int cls_movie::alloc_video_buffer(AVFrame *frame, int align)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((enum AVPixelFormat)frame->format);
    int ret, i, padded_height;
    int plane_padding = FFMAX(16 + 16/*STRIDE_ALIGN*/, align);

    if (!desc) {
        return AVERROR(EINVAL);
    }

    if ((ret = av_image_check_size(
            (uint)frame->width, (uint)frame->height
            , 0, nullptr)) < 0) {
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
            , nullptr
            , frame->linesize);
    if (ret < 0) {
        return ret;
    }

    frame->buf[0] = av_buffer_alloc((uint)(ret + 4*plane_padding));
    if (!frame->buf[0]) {
        ret = AVERROR(ENOMEM);
        av_frame_unref(frame);
        return ret;
    }
    frame->buf[1] = av_buffer_alloc((uint)(ret + 4*plane_padding));
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

int cls_movie::set_picture()
{
    int retcd;
    char errstr[128];

    picture = av_frame_alloc();
    if (!picture) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc frame"));
        free_context();
        return -1;
    }

    if (cam->cfg->movie_quality) {
        picture->quality = (int)(FF_LAMBDA_MAX *
            (float)((100-cam->cfg->movie_quality)/100))+1;
    }

    picture->linesize[0] = ctx_codec->width;
    picture->linesize[1] = ctx_codec->width / 2;
    picture->linesize[2] = ctx_codec->width / 2;

    picture->format = ctx_codec->pix_fmt;
    picture->width  = ctx_codec->width;
    picture->height = ctx_codec->height;

    if (preferred_codec == "h264_v4l2m2m") {
        retcd = alloc_video_buffer(picture, 32);
        if (retcd) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc buffers %s"), errstr);
            free_context();
            return -1;
        }
    }

    return 0;
}

int cls_movie::set_outputfile()
{
    int retcd;
    char errstr[128];

    /* Open the output file, if needed. */
    if ((timelapse_exists(full_nm.c_str()) == 0) || (tlapse != TIMELAPSE_APPEND)) {
        clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);
        retcd = avio_open(&oc->pb, full_nm.c_str()
            , MY_FLAG_WRITE|AVIO_FLAG_NONBLOCK);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                ,_("avio_open: %s File %s")
                , errstr, full_nm.c_str());
            if (errno == ENOENT) {
                if (mycreate_path(full_nm.c_str()) == -1) {
                    remove(full_nm.c_str());
                    free_context();
                    return -1;
                }
                clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);
                retcd = avio_open(&oc->pb, full_nm.c_str(), MY_FLAG_WRITE| AVIO_FLAG_NONBLOCK);
                if (retcd < 0) {
                    av_strerror(retcd, errstr, sizeof(errstr));
                    MOTPLS_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("error %s opening file %s")
                        , errstr, full_nm.c_str());
                    remove(full_nm.c_str());
                    free_context();
                    return -1;
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                    ,_("Error opening file %s")
                    , full_nm.c_str());
                remove(full_nm.c_str());
                free_context();
                return -1;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);
        retcd = avformat_write_header(oc, nullptr);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Could not write movie header %s"),errstr);
            if ((container == "mp4") && (strm_audio != nullptr)) {
                MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    , _("Ensure audio codec is permitted with a MP4 container."));
            }
            remove(full_nm.c_str());
            free_context();
            return -1;
        }
        /* TIMELAPSE_APPEND uses standard file IO so we close it */
        if (tlapse == TIMELAPSE_APPEND) {
            av_write_trailer(oc);
            avio_close(oc->pb);
        }

    }

    return 0;
}

int cls_movie::flush_codec()
{
    int retcd;
    int recv_cd = 0;
    char errstr[128];

    if (passthrough) {
        return 0;
    }

    retcd = 0;
    recv_cd = 0;
    if (tlapse == TIMELAPSE_NONE) {
        retcd = avcodec_send_frame(ctx_codec, nullptr);
        if (retcd < 0 ) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error entering draining mode:%s"),errstr);
            return -1;
        }
        while (recv_cd != AVERROR_EOF){
            pkt = mypacket_alloc(pkt);
            recv_cd = avcodec_receive_packet(ctx_codec, pkt);
            if (recv_cd != AVERROR_EOF) {
                if (recv_cd < 0) {
                    av_strerror(recv_cd, errstr, sizeof(errstr));
                    MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                        ,_("Error draining codec:%s"),errstr);
                    free_pkt();
                    return -1;
                }
                // v4l2_m2m encoder uses pts 0 and size 0 to indicate AVERROR_EOF
                if ((pkt->pts == 0) || (pkt->size == 0)) {
                    recv_cd = AVERROR_EOF;
                    free_pkt();
                    continue;
                }
                retcd = av_write_frame(oc, pkt);
                if (retcd < 0) {
                    MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                        ,_("Error writing draining video frame"));
                    return -1;
                }
            }
            free_pkt();
        }
    }
    return 0;
}

int cls_movie::put_frame(const struct timespec *ts1)
{
    int retcd;

    pkt = mypacket_alloc(pkt);

    retcd = set_pts(ts1);
    if (retcd < 0) {
        //If there is an error, it has already been reported.
        free_pkt();
        return 0;
    }

    retcd = encode_video();
    if (retcd != 0) {
        if (retcd != -2) {
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while encoding picture"));
        }
        free_pkt();
        return retcd;
    }

    if (tlapse == TIMELAPSE_APPEND) {
        retcd = timelapse_append(pkt);
    } else {
        retcd = av_write_frame(oc, pkt);
    }
    free_pkt();

    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while writing video frame"));
        return -1;
    }
    return retcd;

}

/* Reset the written flag and movie start time at opening of each event */
void cls_movie::passthru_reset()
{
    int indx;

    pthread_mutex_lock(&netcam_data->mutex_pktarray);
        for(indx = 0; indx < netcam_data->pktarray_size; indx++) {
            netcam_data->pktarray[indx].iswritten = false;
        }
    pthread_mutex_unlock(&netcam_data->mutex_pktarray);

}

int cls_movie::passthru_pktpts()
{
    int64_t ts_interval, base_pdts;
    AVRational tmpbase;
    int indx;

    if (pkt->stream_index == netcam_data->audio_stream_index) {
        tmpbase = strm_audio->time_base;
        indx = netcam_data->audio_stream_index;
        base_pdts = pass_audio_base;
    } else {
        tmpbase = strm_video->time_base;
        indx = netcam_data->video_stream_index;
        base_pdts = pass_video_base;
    }

    if (pkt->pts != AV_NOPTS_VALUE) {
        if (pkt->pts < base_pdts) {
            ts_interval = 0;
        } else {
            ts_interval = pkt->pts - base_pdts;
        }
        pkt->pts = av_rescale_q(ts_interval
            , netcam_data->transfer_format->streams[indx]->time_base, tmpbase);
    }

    if (pkt->dts != AV_NOPTS_VALUE) {
        if (pkt->dts < base_pdts) {
            ts_interval = 0;
        } else {
            ts_interval = pkt->dts - base_pdts;
        }
        pkt->dts = av_rescale_q(ts_interval
            , netcam_data->transfer_format->streams[indx]->time_base, tmpbase);
    }

    ts_interval = pkt->duration;
    pkt->duration = av_rescale_q(ts_interval
        , netcam_data->transfer_format->streams[indx]->time_base, tmpbase);

    /*
    MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("base PTS %" PRId64 " new PTS %" PRId64 " srcbase %d-%d newbase %d-%d")
        ,ts_interval, pkt->duration
        ,netcam_data->transfer_format->streams[indx]->time_base.num
        ,netcam_data->transfer_format->streams[indx]->time_base.den
        ,tmpbase.num, tmpbase.den);
    */

    return 0;
}

void cls_movie::passthru_write(int indx)
{
    /* Write the packet in the buffer at indx to file */
    char errstr[128];
    int retcd;

    pkt = mypacket_alloc(pkt);
    netcam_data->pktarray[indx].iswritten = true;

    retcd = av_packet_ref(pkt, netcam_data->pktarray[indx].packet);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO, "av_copy_packet: %s",errstr);
        free_pkt();
        return;
    }

    retcd = passthru_pktpts();
    if (retcd < 0) {
        free_pkt();
        return;
    }

    retcd = av_interleaved_write_frame(oc, pkt);
    free_pkt();
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO
            ,_("Error while writing video frame: %s"),errstr);
        return;
    }

}

void cls_movie::passthru_minpts()
{
    int indx, indx_audio, indx_video;

    pass_audio_base = 0;
    pass_video_base = 0;

    pthread_mutex_lock(&netcam_data->mutex_pktarray);
        indx_audio =  netcam_data->audio_stream_index;
        indx_video =  netcam_data->video_stream_index;

        for (indx = 0; indx < netcam_data->pktarray_size; indx++) {
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_audio) &&
                (netcam_data->pktarray[indx].packet->pts != AV_NOPTS_VALUE)) {
                pass_audio_base = netcam_data->pktarray[indx].packet->pts;
            };
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_video) &&
                (netcam_data->pktarray[indx].packet->pts != AV_NOPTS_VALUE)) {
                pass_video_base = netcam_data->pktarray[indx].packet->pts;
            };
        }
        for (indx = 0; indx < netcam_data->pktarray_size; indx++) {
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_audio) &&
                (netcam_data->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (netcam_data->pktarray[indx].packet->pts < pass_audio_base)) {
                pass_audio_base = netcam_data->pktarray[indx].packet->pts;
            };
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_audio) &&
                (netcam_data->pktarray[indx].packet->dts != AV_NOPTS_VALUE) &&
                (netcam_data->pktarray[indx].packet->dts < pass_audio_base)) {
                pass_audio_base = netcam_data->pktarray[indx].packet->dts;
            };
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_video) &&
                (netcam_data->pktarray[indx].packet->pts != AV_NOPTS_VALUE) &&
                (netcam_data->pktarray[indx].packet->pts < pass_video_base)) {
                pass_video_base = netcam_data->pktarray[indx].packet->pts;
            };
            if ((netcam_data->pktarray[indx].packet->stream_index == indx_video) &&
                (netcam_data->pktarray[indx].packet->dts != AV_NOPTS_VALUE) &&
                (netcam_data->pktarray[indx].packet->dts < pass_video_base)) {
                pass_video_base = netcam_data->pktarray[indx].packet->dts;
            };
        }
    pthread_mutex_unlock(&netcam_data->mutex_pktarray);

    if (pass_audio_base < 0) {
         pass_audio_base = 0;
    }

    if (pass_video_base < 0) {
        pass_video_base = 0;
    }

}

int cls_movie::passthru_put(ctx_image_data *img_data)
{
    int64_t idnbr_image, idnbr_lastwritten, idnbr_stop, idnbr_firstkey;
    int indx, indx_lastwritten, indx_firstkey, indx_video;

    if (netcam_data == nullptr) {
        return -1;
    }

    if ((netcam_data->status == NETCAM_NOTCONNECTED  ) ||
        (netcam_data->status == NETCAM_RECONNECTING  ) ) {
        return 0;
    }

    if (high_resolution) {
        idnbr_image = img_data->idnbr_high;
    } else {
        idnbr_image = img_data->idnbr_norm;
    }

    pthread_mutex_lock(&netcam_data->mutex_pktarray);
        idnbr_lastwritten = 0;
        idnbr_firstkey = idnbr_image;
        idnbr_stop = 0;
        indx_lastwritten = -1;
        indx_firstkey = -1;
        indx_video = netcam_data->video_stream_index;

        for(indx = 0; indx < netcam_data->pktarray_size; indx++) {
            if ((netcam_data->pktarray[indx].iswritten) &&
                (netcam_data->pktarray[indx].idnbr > idnbr_lastwritten) &&
                (netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_lastwritten=netcam_data->pktarray[indx].idnbr;
                indx_lastwritten = indx;
            }
            if ((netcam_data->pktarray[indx].idnbr >  idnbr_stop) &&
                (netcam_data->pktarray[indx].idnbr <= idnbr_image)&&
                (netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                idnbr_stop=netcam_data->pktarray[indx].idnbr;
            }
            if ((netcam_data->pktarray[indx].iskey) &&
                (netcam_data->pktarray[indx].idnbr <= idnbr_firstkey)&&
                (netcam_data->pktarray[indx].packet->stream_index == indx_video)) {
                    idnbr_firstkey=netcam_data->pktarray[indx].idnbr;
                    indx_firstkey = indx;
            }
        }

        if (idnbr_stop == 0) {
            pthread_mutex_unlock(&netcam_data->mutex_pktarray);
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
            if ((!netcam_data->pktarray[indx].iswritten) &&
                (netcam_data->pktarray[indx].packet->size > 0) &&
                (netcam_data->pktarray[indx].idnbr >  idnbr_lastwritten) &&
                (netcam_data->pktarray[indx].idnbr <= idnbr_image)) {
                passthru_write(indx);
            }
            if (netcam_data->pktarray[indx].idnbr == idnbr_stop) {
                break;
            }
            indx++;
            if (indx == netcam_data->pktarray_size ) {
                indx = 0;
            }
        }
    pthread_mutex_unlock(&netcam_data->mutex_pktarray);
    return 0;
}

int cls_movie::passthru_streams_video(AVStream *stream_in)
{
    int retcd;

    strm_video = avformat_new_stream(oc, nullptr);
    if (strm_video == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc video stream"));
        return -1;
    }

    retcd = avcodec_parameters_copy(strm_video->codecpar, stream_in->codecpar);
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy video codec parameters"));
        return -1;
    }

    strm_video->codecpar->codec_tag  = 0;
    strm_video->time_base = stream_in->time_base;
    strm_video->avg_frame_rate = stream_in->avg_frame_rate;

    MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO
        , _("video timebase %d/%d fps %d/%d")
        , strm_video->time_base.num
        , strm_video->time_base.den
        , strm_video->avg_frame_rate.num
        , strm_video->avg_frame_rate.den);

    return 0;
}

int cls_movie::passthru_streams_audio( AVStream *stream_in)
{
    int retcd;

    strm_audio = avformat_new_stream(oc, nullptr);
    if (!strm_audio) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc audio stream"));
        return -1;
    }

    retcd = avcodec_parameters_copy(strm_audio->codecpar, stream_in->codecpar);
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy audio codec parameters"));
        return -1;
    }

    strm_audio->codecpar->codec_tag  = 0;
    strm_audio->time_base = stream_in->time_base;
    strm_audio->r_frame_rate = stream_in->time_base;
    strm_audio->avg_frame_rate= stream_in->time_base;
    strm_audio->codecpar->format = stream_in->codecpar->format;
    strm_audio->codecpar->sample_rate = stream_in->codecpar->sample_rate;
    strm_audio->avg_frame_rate = stream_in->avg_frame_rate;

    MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO
        , _("audio timebase %d/%d")
        , strm_audio->time_base.num
        , strm_audio->time_base.den);
    return 0;
}

int cls_movie::passthru_streams()
{
    int         retcd, indx;
    AVStream    *stream_in;

    if (netcam_data->finish == true) {
        return -1;
    }

    pthread_mutex_lock(&netcam_data->mutex_transfer);
        for (indx= 0; indx < (int)netcam_data->transfer_format->nb_streams; indx++) {
            stream_in = netcam_data->transfer_format->streams[indx];
            if (stream_in->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                retcd = passthru_streams_video(stream_in);
            } else if (stream_in->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                retcd = passthru_streams_audio(stream_in);
            }
            if (retcd < 0) {
                pthread_mutex_unlock(&netcam_data->mutex_transfer);
                return retcd;
            }
        }
    pthread_mutex_unlock(&netcam_data->mutex_transfer);

    return 0;
}

int cls_movie::passthru_check()
{
    if ((netcam_data->status == NETCAM_NOTCONNECTED  ) ||
        (netcam_data->status == NETCAM_RECONNECTING  )) {
        MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO
            ,_("rtsp camera not ready for pass-through."));
        return -1;
    }

    if (netcam_data == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("RTSP context not available."));
        return -1;
    }

    passthru_reset();

    return 0;
}

int cls_movie::passthru_open()
{
    int retcd;

    retcd = passthru_check();
    if (retcd < 0) {
        return retcd;
    }

    oc = avformat_alloc_context();
    if (!oc) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        free_context();
        return -1;
    }
    oc->interrupt_callback.callback = movie_interrupt;
    oc->interrupt_callback.opaque = this;
    cb_dur = 3;

    if ((container != "mp4") &&
        (container != "mov") &&
        (container != "mkv")) {
        MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO
            ,_("Changing to MP4 container for pass-through."));
        container = "mp4";
    }

    passthru_minpts();

    retcd = get_oformat();
    if (retcd < 0 ) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get output format!"));
        return -1;
    }

    retcd = passthru_streams();
    if (retcd < 0 ) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get streams!"));
        return -1;
    }

    retcd = set_outputfile();
    if (retcd < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not create output file"));
        return -1;
    }

    if (strm_audio != nullptr) {
        MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO
            , _("Timebase after open audio: %d/%d video: %d/%d")
            , strm_audio->time_base.num
            , strm_audio->time_base.den
            , strm_video->time_base.num
            , strm_video->time_base.den);
    }

    MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO, "Pass-through stream opened");

    return 0;
}

void cls_movie::put_pix_yuv420(ctx_image_data *img_data)
{
    unsigned char *image;

    if (high_resolution) {
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    // Usual setup for image pointers
    picture->data[0] = image;
    picture->data[1] = image + (ctx_codec->width * ctx_codec->height);
    picture->data[2] = picture->data[1] + ((ctx_codec->width * ctx_codec->height) / 4);
}

void cls_movie::on_movie_start()
{
    MOTPLS_LOG(DBG, TYPE_EVENTS, NO_ERRNO, _("Creating movie: %s"),full_nm.c_str());
    if (cam->cfg->on_movie_start != "") {
        util_exec_command(cam, cam->cfg->on_movie_start.c_str(), full_nm.c_str());
    }
}

void cls_movie::on_movie_end()
{
    MOTPLS_LOG(DBG, TYPE_EVENTS, NO_ERRNO, _("Finished movie: %s"),full_nm.c_str());
    if (cam->cfg->on_movie_end != "") {
        util_exec_command(cam, cam->cfg->on_movie_end.c_str(), full_nm.c_str());
    }
}

int cls_movie::movie_open()
{
    if (passthrough) {
        if (passthru_open() < 0 ) {
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not setup passthrough!"));
            free_context();
            return -1;
        }
        return 0;
    }

    oc = avformat_alloc_context();
    if (oc == nullptr) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        free_context();
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);
    cb_dur = 3;
    oc->interrupt_callback.callback = movie_interrupt;
    oc->interrupt_callback.opaque = this;

    if (get_oformat() < 0 ) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
        free_context();
        return -1;
    }

    if (set_codec() < 0 ) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate codec!"));
        return -1;
    }

    if (set_stream() < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    if (set_picture() < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the picture"));
        return -1;
    }

    if (set_outputfile() < 0) {
        MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not open output file"));
        return -1;
    }

    return 0;
}

void cls_movie::stop()
{
    timespec *ts;

    if (is_running == false) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);

    if (movie_type == "extpipe") {
        if (extpipe_stream != nullptr) {
            fflush(extpipe_stream);
            pclose(extpipe_stream);
            extpipe_stream = nullptr;
        }
    } else {
        if (flush_codec() < 0) {
            MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error flushing codec"));
        }
        if (oc != nullptr) {
            if (oc->pb != nullptr) {
                if (tlapse != TIMELAPSE_APPEND) {
                    av_write_trailer(oc);
                }
                if (!(oc->oformat->flags & AVFMT_NOFILE)) {
                    if (tlapse != TIMELAPSE_APPEND) {
                        avio_close(oc->pb);
                    }
                }
            }
        }
        free_context();
        free_nal();
    }

    if (movie_type == "motion") {
        ts = &cam->imgs.image_motion.imgts;
    } else {
        ts = &cam->current_image->imgts;
    }

    if ((movie_type == "norm") || (movie_type == "motion") || (movie_type == "extpipe")) {
        cam->filetype = FTYPE_MOVIE;
        on_movie_end();
        cam->motapp->dbse->exec(cam, full_nm, "movie_end");
        if ((cam->cfg->movie_retain == "secondary") &&
            (cam->algsec->detected == false) &&
            (cam->algsec->method != "none")) {
            if (remove(full_nm.c_str()) != 0) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    , _("Unable to remove file %s"), full_nm.c_str());
            } else {
                cam->motapp->dbse->movielist_add(cam, this, ts);
            }
        } else {
            cam->motapp->dbse->movielist_add(cam, this, ts);
        }
    } else if (movie_type == "timelapse") {
        cam->filetype = FTYPE_MOVIE_TIMELAPSE;
        on_movie_end();
        cam->motapp->dbse->exec(cam, full_nm, "movie_end");
    } else {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO,_("Invalid movie type"));
    }

    is_running = false;

}

int cls_movie::extpipe_put()
{
    int retcd;

    retcd = 0;
    if (fileno(extpipe_stream) > 0) {
        if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
            if (!fwrite(cam->current_image->image_high
                    , (uint)cam->imgs.size_high, 1, extpipe_stream)) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                    , _("Error writing in pipe , state error %d")
                    , ferror(extpipe_stream));
                retcd = -1;
            }
        } else {
            if (!fwrite(cam->current_image->image_norm
                    , (uint)cam->imgs.size_norm, 1, extpipe_stream)) {
                MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO
                  ,_("Error writing in pipe , state error %d")
                  , ferror(extpipe_stream));
                retcd = -1;
            }
        }
    }
    return retcd;
}

int cls_movie::put_image(ctx_image_data *img_data, const struct timespec *ts1)
{
    int retcd = 0;
    int cnt = 0;

    if (is_running == false) {
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &cb_st_ts);

    if (movie_type == "extpipe") {
        extpipe_put();
        return 0;
    }

    if (passthrough) {
        retcd = passthru_put(img_data);
        return retcd;
    }

    if (picture) {
        put_pix_yuv420(img_data);

        gop_cnt ++;
        if (gop_cnt == ctx_codec->gop_size ) {
            picture->pict_type = AV_PICTURE_TYPE_I;
            myframe_key(picture);
            gop_cnt = 0;
        } else {
            picture->pict_type = AV_PICTURE_TYPE_P;
             myframe_interlaced(picture);
        }

        /* A return code of -2 is thrown by the put_frame
         * when a image is buffered.  For timelapse, we absolutely
         * never want a frame buffered so we keep sending back the
         * the same pic until it flushes or fails in a different way
         */
        retcd = put_frame(ts1);
        while ((retcd == -2) && (tlapse != TIMELAPSE_NONE)) {
            retcd = put_frame(ts1);
            cnt++;
            if (cnt > 50) {
                MOTPLS_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    ,_("Excessive attempts to clear buffered packet"));
                retcd = -1;
            }
        }
        //non timelapse buffered is ok
        if (retcd == -2) {
            retcd = 0;
            MOTPLS_LOG(DBG, TYPE_ENCODER, NO_ERRNO, _("Buffered packet"));
        }
    }

    return retcd;
}

void cls_movie::reset_start_time(const struct timespec *ts1)
{
    int64_t one_frame_interval = av_rescale_q(1,av_make_q(1, fps), strm_video->time_base);
    if (one_frame_interval <= 0) {
        one_frame_interval = 1;
    }
    base_pts = last_pts + one_frame_interval;

    start_time.tv_sec = ts1->tv_sec;
    start_time.tv_nsec = ts1->tv_nsec;

}

void cls_movie::init_container()
{
    int codenbr;
    size_t col_pos;

    if (cam->cfg->movie_container == "test") {
        MOTPLS_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Running test of the various output formats.");
        codenbr = cam->event_curr_nbr % 10;
        if (codenbr == 1) {
            container = "flv";
        } else if (codenbr == 2) {
            container = "ogg";
        } else if (codenbr == 3) {
            container = "webm";
        } else if (codenbr == 4) {
            container = "mp4";
        } else if (codenbr == 5) {
            container = "mkv";
        } else if (codenbr == 6) {
            container = "hevc";
        } else if (codenbr == 7) {
            container = "flv";
        } else if (codenbr == 8) {
            container = "ogg";
        } else if (codenbr == 9) {
            container = "webm";
        } else                   {
            container = "mkv";
        }
    } else {
        container = cam->cfg->movie_container;
    }

    col_pos = container.find(":");
    if (col_pos == std::string::npos) {
        preferred_codec = "";
    } else {
        preferred_codec = container.substr(col_pos+1);
        container = container.substr(0,col_pos);
    }

}

void cls_movie::start_norm()
{
    char tmp[PATH_MAX];

    if (cam->cfg->movie_output == false) {
        is_running = false;
        return;
    }

    init_container();

    mystrftime(cam, tmp, sizeof(tmp), cam->cfg->movie_filename.c_str(), nullptr);

    movie_nm = tmp;
    movie_dir = cam->cfg->target_dir;
    if (container =="test") {
        full_nm = movie_dir + "/"  + container + "_" + movie_nm;
    } else {
        full_nm = movie_dir + "/"  + movie_nm;
    }

    if (cam->imgs.size_high > 0) {
        width  = cam->imgs.width_high;
        height = cam->imgs.height_high;
        high_resolution = true;
        netcam_data = cam->netcam_high;
    } else {
        width  = cam->imgs.width;
        height = cam->imgs.height;
        high_resolution = false;
        netcam_data = cam->netcam;
    }
    pkt = nullptr;
    tlapse = TIMELAPSE_NONE;
    fps = cam->lastrate;
    start_time.tv_sec = cam->current_image->imgts.tv_sec;
    start_time.tv_nsec = cam->current_image->imgts.tv_nsec;
    last_pts = -1;
    base_pts = 0;
    gop_cnt = 0;

    if (cam->cfg->movie_container == "test") {
        test_mode = true;
    } else {
        test_mode = false;
    }
    motion_images = false;
    passthrough = cam->movie_passthrough;

    if (movie_open() < 0) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
            ,_("Error initializing movie."));
        return;
    }

    cam->filetype = FTYPE_MOVIE;

    on_movie_start();

    cam->motapp->dbse->exec(cam, full_nm, "movie_start");

    is_running = true;

}

void cls_movie::start_motion()
{
    char tmp[PATH_MAX];
    ctx_image_data save_data;

    if (cam->cfg->movie_output_motion == false) {
        is_running = false;
        return;
    }

    init_container();

    memcpy(&save_data, cam->current_image, sizeof(ctx_image_data));
        memcpy(cam->current_image, &cam->imgs.image_motion, sizeof(ctx_image_data));
        mystrftime(cam, tmp, sizeof(tmp)
            , cam->cfg->movie_filename.c_str(), nullptr);
    memcpy(cam->current_image, &save_data, sizeof(ctx_image_data));

    movie_nm.assign(tmp).append("m");
    movie_dir = cam->cfg->target_dir;
    if (container =="test") {
        full_nm = movie_dir + "/"  + container + "_" + movie_nm;
    } else {
        full_nm = movie_dir + "/"  + movie_nm;
    }

    pkt = nullptr;
    width  = cam->imgs.width;
    height = cam->imgs.height;
    netcam_data = nullptr;
    tlapse = TIMELAPSE_NONE;
    fps = cam->lastrate;
    start_time.tv_sec = cam->imgs.image_motion.imgts.tv_sec;
    start_time.tv_nsec = cam->imgs.image_motion.imgts.tv_nsec;
    last_pts = -1;
    base_pts = 0;
    gop_cnt = 0;
    if (container == "test") {
        test_mode = true;
    } else {
        test_mode = false;
    }
    motion_images = true;
    passthrough = false;
    high_resolution = false;

    if (movie_open() < 0) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
            ,_("Error initializing movie."));
        return;
    }

    cam->filetype = FTYPE_MOVIE;
    on_movie_start();
    cam->motapp->dbse->exec(cam, full_nm, "movie_start");
    is_running = true;

}

void cls_movie::start_timelapse()
{
    char tmp[PATH_MAX];

    mystrftime(cam, tmp, sizeof(tmp), cam->cfg->timelapse_filename.c_str(), nullptr);

    movie_nm = tmp;
    movie_dir = cam->cfg->target_dir.c_str();
    full_nm = movie_dir + "/" + movie_nm;

    if ((cam->imgs.size_high > 0) && (cam->movie_passthrough == false)) {
        width  = cam->imgs.width_high;
        height = cam->imgs.height_high;
        high_resolution = true;
    } else {
        width  = cam->imgs.width;
        height = cam->imgs.height;
        high_resolution = false;
    }
    pkt = nullptr;
    fps = cam->cfg->timelapse_fps;
    start_time.tv_sec = cam->current_image->imgts.tv_sec;
    start_time.tv_nsec = cam->current_image->imgts.tv_nsec;
    last_pts = -1;
    base_pts = 0;
    test_mode = false;
    gop_cnt = 0;
    motion_images = false;
    passthrough = false;
    netcam_data = nullptr;

    if (cam->cfg->timelapse_container == "mpg") {
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpg container."));
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be appended to file"));
        tlapse = TIMELAPSE_APPEND;
        container = "mpg";
    } else {
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mkv container."));
        MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be trigger new files"));
        tlapse = TIMELAPSE_NEW;
        container = "mkv";
    }

    if (movie_open() < 0) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO
            ,_("Error initializing movie."));
        return;
    }

    cam->filetype = FTYPE_MOVIE_TIMELAPSE;
    on_movie_start();
    cam->motapp->dbse->exec(cam, full_nm, "movie_start");

    is_running = true;
}

void cls_movie::start_extpipe()
{
    char tmp[PATH_MAX];

    if (cam->cfg->movie_extpipe_use == false) {
        is_running = false;
        return;
    }

    mystrftime(cam, tmp, sizeof(tmp), cam->cfg->movie_filename.c_str(), nullptr);

    movie_nm = tmp;
    movie_dir = cam->cfg->target_dir;

    if (cam->cfg->movie_output) {
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Requested extpipe in addition to movie_output."));
        MOTPLS_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Adjusting file name of extpipe output."));
        full_nm = movie_dir + "/"  + movie_nm + "p";
    } else {
        full_nm = movie_dir + "/"  + movie_nm;
    }

    if (mycreate_path(full_nm.c_str()) == -1) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, _("create path failed"));
        return;
    }

    memset(&tmp,0,PATH_MAX);
    mystrftime(cam, tmp, sizeof(tmp)
        , cam->cfg->movie_extpipe.c_str(), full_nm.c_str());

    MOTPLS_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("extpipe cmd: %s"), tmp);

    extpipe_stream = popen(tmp, "we");
    if (extpipe_stream == nullptr) {
        MOTPLS_LOG(ERR, TYPE_EVENTS, SHOW_ERRNO, _("popen failed"));
        return;
    }

    setbuf(extpipe_stream, nullptr);

    cam->filetype = FTYPE_MOVIE;
    on_movie_start();
    cam->motapp->dbse->exec(cam, full_nm, "movie_start");
    is_running = true;

}

void cls_movie::start()
{
    if (movie_type == "norm") {
        start_norm();
    } else if (movie_type == "motion") {
        start_motion();
    } else if (movie_type == "timelapse") {
        start_timelapse();
    } else if (movie_type == "extpipe") {
        start_extpipe();
    } else {
        MOTPLS_LOG(ERR, TYPE_EVENTS, NO_ERRNO,_("Invalid movie type"));
    }
}

void cls_movie::init_vars()
{
    cb_st_ts.tv_nsec = 0;
    cb_st_ts.tv_sec = 0;
    cb_cr_ts = cb_st_ts;
    cb_dur =0;
    full_nm = "";
    movie_nm = "";
    movie_dir = "";

    oc = nullptr;
    strm_video = nullptr;
    strm_audio = nullptr;
    ctx_codec = nullptr;
    codec = nullptr;
    pkt = nullptr;
    picture = nullptr;
    opts = nullptr;
    netcam_data = nullptr;
    width = 640;
    height = 480;
    tlapse = TIMELAPSE_NONE;
    fps = 5;
    last_pts = 0;
    base_pts = 0;
    pass_audio_base = 0;
    pass_video_base = 0;
    test_mode = false;
    gop_cnt = 5;
    start_time.tv_nsec = 0;
    start_time.tv_sec = 0;
    high_resolution = false;
    motion_images = false;
    passthrough = false;

    nal_info = nullptr;
    nal_info_len = 0;
    extpipe_stream = nullptr;
    container = "";
    preferred_codec = "";

}

cls_movie::cls_movie(cls_camera *p_cam, std::string pmovie_type)
{
    cam = p_cam;

    is_running = false;

    movie_type = pmovie_type;

    init_vars();
}

cls_movie::~cls_movie()
{

}

