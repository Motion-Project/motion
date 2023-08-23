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
 *    Copyright 2020-2023 MotionMrDave@gmail.com
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "picture.hpp"
#include "webu.hpp"
#include "webu_stream.hpp"
#include "webu_mpegts.hpp"
#include "alg_sec.hpp"

void webu_mpegts_free_context(ctx_webui *webui)
{
    if (webui->picture != NULL) {
        myframe_free(webui->picture);
        webui->picture = NULL;
    }
    if (webui->ctx_codec != NULL) {
        myavcodec_close(webui->ctx_codec);
        webui->ctx_codec = NULL;
    }
    if (webui->fmtctx != NULL) {
        if (webui->fmtctx->pb != NULL) {
            if (webui->fmtctx->pb->buffer != NULL) {
                av_free(webui->fmtctx->pb->buffer);
                webui->fmtctx->pb->buffer = NULL;
            }
            avio_context_free(&webui->fmtctx->pb);
            webui->fmtctx->pb = NULL;
        }
        avformat_free_context(webui->fmtctx);
        webui->fmtctx = NULL;
    }
    MOTPLS_LOG(DBG, TYPE_STREAM, NO_ERRNO, _("closed"));

}

static int webu_mpegts_pic_send(ctx_webui *webui, unsigned char *img)
{
    int retcd;
    char errstr[128];
    struct timespec curr_ts;
    int64_t pts_interval;

    if (webui->picture == NULL) {
        webui->picture = myframe_alloc();
        webui->picture->linesize[0] = webui->ctx_codec->width;
        webui->picture->linesize[1] = webui->ctx_codec->width / 2;
        webui->picture->linesize[2] = webui->ctx_codec->width / 2;

        webui->picture->format = webui->ctx_codec->pix_fmt;
        webui->picture->width  = webui->ctx_codec->width;
        webui->picture->height = webui->ctx_codec->height;

        webui->picture->pict_type = AV_PICTURE_TYPE_I;
        webui->picture->key_frame = 1;
        webui->picture->pts = 1;
    }

    webui->picture->data[0] = img;
    webui->picture->data[1] = webui->picture->data[0] +
        (webui->ctx_codec->width * webui->ctx_codec->height);
    webui->picture->data[2] = webui->picture->data[1] +
        ((webui->ctx_codec->width * webui->ctx_codec->height) / 4);

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    pts_interval = ((1000000L * (curr_ts.tv_sec - webui->start_time.tv_sec)) +
        (curr_ts.tv_nsec/1000) - (webui->start_time.tv_nsec/1000));
    webui->picture->pts = av_rescale_q(pts_interval
        ,(AVRational){1, 1000000L}, webui->ctx_codec->time_base);

    retcd = avcodec_send_frame(webui->ctx_codec, webui->picture);
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            , _("Error sending frame for encoding:%s"), errstr);
        myframe_free(webui->picture);
        webui->picture = NULL;
        return -1;
    }

    return 0;
}

static int webu_mpegts_pic_get(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    AVPacket *pkt;

    pkt = NULL;
    pkt = mypacket_alloc(pkt);

    retcd = avcodec_receive_packet(webui->ctx_codec, pkt);
    if (retcd == AVERROR(EAGAIN)) {
        mypacket_free(pkt);
        pkt = NULL;
        return -1;
    }
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("Error receiving encoded packet video:%s"), errstr);
        //Packet is freed upon failure of encoding
        return -1;
    }

    pkt->pts = webui->picture->pts;

    retcd =  av_interleaved_write_frame(webui->fmtctx, pkt);
    if (retcd < 0 ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("Error while writing video frame. %s"), errstr);
        return -1;
    }

    mypacket_free(pkt);
    pkt = NULL;

    return 0;
}

static void webu_mpegts_resetpos(ctx_webui *webui)
{
    webui->stream_pos = 0;
    webui->resp_used = 0;
}

static int webu_mpegts_getimg(ctx_webui *webui)
{
    ctx_stream_data *local_stream;
    struct timespec curr_ts;

    if ((webui->motapp->webcontrol_finish) || (webui->cam->finish_dev)) {
        webu_mpegts_resetpos(webui);
        return 0;
    }

    clock_gettime(CLOCK_REALTIME, &curr_ts);

    memset(webui->resp_image, '\0', webui->resp_size);
    webui->resp_used = 0;

    /* Assign to a local pointer the stream we want */
    if (webui->cnct_type == WEBUI_CNCT_TS_FULL) {
        local_stream = &webui->cam->stream.norm;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SUB) {
        local_stream = &webui->cam->stream.sub;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_MOTION) {
        local_stream = &webui->cam->stream.motion;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SOURCE) {
        local_stream = &webui->cam->stream.source;
    } else if (webui->cnct_type == WEBUI_CNCT_TS_SECONDARY) {
        local_stream = &webui->cam->stream.secondary;
    } else {
        return 0;
    }
    pthread_mutex_lock(&webui->cam->stream.mutex);
        if ((webui->cam->detecting_motion == false) &&
            (webui->motapp->cam_list[webui->camindx]->conf->stream_motion)) {
            webui->stream_fps = 1;
        } else {
            webui->stream_fps = webui->motapp->cam_list[webui->camindx]->conf->stream_maxrate;
        }
        if (local_stream->image == NULL) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return -1;
        }
        if (webu_mpegts_pic_send(webui, local_stream->image) < 0) {
            pthread_mutex_unlock(&webui->cam->stream.mutex);
            return -1;
        }
        local_stream->consumed = true;
    pthread_mutex_unlock(&webui->cam->stream.mutex);

    if (webu_mpegts_pic_get(webui) < 0) {
        return -1;
    }

    return 0;
}

static int webu_mpegts_avio_buf(void *opaque, uint8_t *buf, int buf_size)
{
    ctx_webui *webui =(ctx_webui *)opaque;

    if (webui->resp_size < (size_t)(buf_size + webui->resp_used)) {
        webui->resp_size = (size_t)(buf_size + webui->resp_used);
        webui->resp_image = (unsigned char*)realloc(
            webui->resp_image, webui->resp_size);
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("resp_image reallocated %d %d %d")
            ,webui->resp_size
            ,webui->resp_used
            ,buf_size);
    }

    memcpy(webui->resp_image + webui->resp_used, buf, buf_size);
    webui->resp_used += buf_size;

    return buf_size;
}

static ssize_t webu_mpegts_response(void *cls, uint64_t pos, char *buf, size_t max)
{
    ctx_webui *webui =(ctx_webui *)cls;
    size_t sent_bytes;
    (void)pos;

    if ((webui->motapp->webcontrol_finish) || (webui->cam->finish_dev)) {
        return -1;
    }

    if (webui->stream_pos == 0) {
        webu_stream_delay(webui);
        webu_mpegts_resetpos(webui);
        if (webu_mpegts_getimg(webui) < 0) {
            return 0;
        }
    }

    /* If we don't have anything in the avio buffer at this point bail out */
    if (webui->resp_used == 0) {
        webu_mpegts_resetpos(webui);
        return 0;
    }

    if ((webui->resp_used - webui->stream_pos) > max) {
        sent_bytes = max;
    } else {
        sent_bytes = webui->resp_used - webui->stream_pos;
    }

    memcpy(buf, webui->resp_image + webui->stream_pos, sent_bytes);

    webui->stream_pos = webui->stream_pos + sent_bytes;
    if (webui->stream_pos >= webui->resp_used) {
        webui->stream_pos = 0;
    }

    return sent_bytes;

}

int webu_mpegts_open(ctx_webui *webui)
{
    int retcd;
    char errstr[128];
    unsigned char   *buf_image;
    AVStream        *strm;
    const AVCodec   *codec;
    AVDictionary    *opts;

    webui->picture = NULL;
    webui->ctx_codec = NULL;
    webui->fmtctx = NULL;
    webui->stream_fps = 10000;   /* For quick start up*/
    clock_gettime(CLOCK_REALTIME, &webui->start_time);

    webui->fmtctx = avformat_alloc_context();
    webui->fmtctx->oformat = av_guess_format("mpegts", NULL, NULL);
    webui->fmtctx->video_codec_id = MY_CODEC_ID_H264;

    codec = avcodec_find_encoder(MY_CODEC_ID_H264);
    strm = avformat_new_stream(webui->fmtctx, codec);

    webui->ctx_codec = avcodec_alloc_context3(codec);
    webui->ctx_codec->gop_size      = 15;
    webui->ctx_codec->codec_id      = MY_CODEC_ID_H264;
    webui->ctx_codec->codec_type    = AVMEDIA_TYPE_VIDEO;
    webui->ctx_codec->bit_rate      = 400000;
    webui->ctx_codec->width         = webui->cam->imgs.width;
    webui->ctx_codec->height        = webui->cam->imgs.height;
    webui->ctx_codec->time_base.num = 1;
    webui->ctx_codec->time_base.den = 90000;
    webui->ctx_codec->pix_fmt       = MY_PIX_FMT_YUV420P;
    webui->ctx_codec->max_b_frames  = 1;
    webui->ctx_codec->flags         |= MY_CODEC_FLAG_GLOBAL_HEADER;
    webui->ctx_codec->framerate.num  = 1;
    webui->ctx_codec->framerate.den  = 1;
    av_opt_set(webui->ctx_codec->priv_data, "profile", "main", 0);
    av_opt_set(webui->ctx_codec->priv_data, "crf", "22", 0);
    av_opt_set(webui->ctx_codec->priv_data, "tune", "zerolatency", 0);
    av_opt_set(webui->ctx_codec->priv_data, "preset", "superfast",0);
    av_dict_set(&opts, "movflags", "empty_moov", 0);

    retcd = avcodec_open2(webui->ctx_codec, codec, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("Failed to copy decoder parameters!: %s"), errstr);
        webu_mpegts_free_context(webui);
        av_dict_free(&opts);
        return -1;
    }

    retcd = avcodec_parameters_from_context(strm->codecpar, webui->ctx_codec);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("Failed to copy decoder parameters!: %s"), errstr);
        webu_mpegts_free_context(webui);
        av_dict_free(&opts);
        return -1;
    }

    webu_stream_checkbuffers(webui);

    webui->aviobuf_sz = 4096;
    buf_image = (unsigned char*)av_malloc(webui->aviobuf_sz);
    webui->fmtctx->pb = avio_alloc_context(
        buf_image, (int)webui->aviobuf_sz, 1, webui
        , NULL, &webu_mpegts_avio_buf, NULL);
    webui->fmtctx->flags = AVFMT_FLAG_CUSTOM_IO;

    retcd = avformat_write_header(webui->fmtctx, &opts);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO
            ,_("Failed to write header!: %s"), errstr);
        webu_mpegts_free_context(webui);
        av_dict_free(&opts);
        return -1;
    }

    webui->stream_pos = 0;
    webui->resp_used = 0;

    av_dict_free(&opts);

    return 0;

}

mhdrslt webu_mpegts_main(ctx_webui *webui)
{
    mhdrslt retcd;
    struct MHD_Response *response;
    int indx;

    if (webu_mpegts_open(webui) < 0 ) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Unable top open mpegts"));
        return MHD_NO;
    }

    clock_gettime(CLOCK_MONOTONIC, &webui->time_last);

    response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN, 4096
        ,&webu_mpegts_response, webui, NULL);
    if (!response) {
        MOTPLS_LOG(ERR, TYPE_STREAM, NO_ERRNO, _("Invalid response"));
        return MHD_NO;
    }

    if (webui->motapp->webcontrol_headers->params_count > 0) {
        for (indx = 0; indx < webui->motapp->webcontrol_headers->params_count; indx++) {
            MHD_add_response_header (response
                , webui->motapp->webcontrol_headers->params_array[indx].param_name
                , webui->motapp->webcontrol_headers->params_array[indx].param_value
            );
        }
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TRANSFER_ENCODING, "BINARY");

    retcd = MHD_queue_response (webui->connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);

    return retcd;

}

