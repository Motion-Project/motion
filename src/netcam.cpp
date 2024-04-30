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
 *
*/

#include "motionplus.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "netcam.hpp"
#include "movie.hpp"

static void netcam_check_buffsize(netcam_buff_ptr buff, size_t numbytes)
{
    int min_size_to_alloc;
    int real_alloc;
    int new_size;

    if ((buff->size - buff->used) >= numbytes) {
        return;
    }

    min_size_to_alloc = (int)(numbytes - (buff->size - buff->used));
    real_alloc = ((min_size_to_alloc / NETCAM_BUFFSIZE) * NETCAM_BUFFSIZE);

    if ((min_size_to_alloc - real_alloc) > 0) {
        real_alloc += NETCAM_BUFFSIZE;
    }

    new_size = (int)(buff->size + real_alloc);

    buff->ptr =(char*) myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

static char *netcam_url_match(regmatch_t m, const char *input)
{
    char *match = NULL;
    int len;

    if (m.rm_so != -1) {
        len = m.rm_eo - m.rm_so;

        if ((match =(char*) mymalloc(len + 1)) != NULL) {
            strncpy(match, input + m.rm_so, len);
            match[len] = '\0';
        }
    }

    return match;
}

static void netcam_url_invalid(ctx_url *parse_url)
{
    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Invalid URL.  Can not parse values."));

    parse_url->port = 0;
    parse_url->host = "????";
    parse_url->service = "????";
    parse_url->path = "INVALID";
    parse_url->userpass = "INVALID";

}
static void netcam_url_parse(ctx_url *parse_url, std::string text_url)
{
    char *s;
    int i, retcd;
    std::string regstr;

    regex_t pattbuf;
    regmatch_t matches[10];

    if (text_url.substr(0,4) == "file") {
        regstr = "(file)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";
    } else if (text_url.substr(0,4) == "v4l2") {
        regstr = "(v4l2)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";
    } else {
        regstr = "(.*)://(((.*):(.*))@)?"
            "([^/:]|[-_.a-z0-9]+)(:([0-9]+))?($|(/[^*]*))";
    }

    parse_url->host = "";
    parse_url->path = "";
    parse_url->port = 0;
    parse_url->service = "";
    parse_url->userpass = "";

    retcd = regcomp(&pattbuf, regstr.c_str(), REG_EXTENDED | REG_ICASE);
    if (retcd != 0) {
        netcam_url_invalid(parse_url);
        return;
    }

    retcd = regexec(&pattbuf, text_url.c_str(), 10, matches, 0);
    if (retcd == REG_NOMATCH) {
        regfree(&pattbuf);
        netcam_url_invalid(parse_url);
        return;
    }

    for (i = 0; i < 10; i++) {
        if ((s = netcam_url_match(matches[i], text_url.c_str())) != NULL) {
            //MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Parse case %d data %s", i, s);
            switch (i) {
            case 1:
                parse_url->service = s;
                break;
            case 3:
                parse_url->userpass = s;
                break;
            case 6:
                parse_url->host = s;
                break;
            case 8:
                parse_url->port = atoi(s);
                break;
            case 9:
                parse_url->path = s;
                break;
                /* Other components ignored */
            default:
                break;
            }
            free(s);
        }
    }

    if (((parse_url->port == 0) && (parse_url->service != "")) ||
        ((parse_url->port > 65535) && (parse_url->service!= ""))) {
        if (parse_url->service == "http") {
            parse_url->port = 80;
        } else if (parse_url->service == "https") {
            parse_url->port = 443;
        } else if (parse_url->service == "ftp") {
            parse_url->port = 21;
        } else if (parse_url->service == "rtmp") {
            parse_url->port = 1935;
        } else if (parse_url->service == "rtsp") {
            parse_url->port = 554;
        }
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("Using port number %d"), parse_url->port);
    }

    regfree(&pattbuf);
}

static void netcam_free_pkt(ctx_netcam *netcam)
{
    mypacket_free(netcam->packet_recv);
    netcam->packet_recv = NULL;
}

static int netcam_check_pixfmt(ctx_netcam *netcam)
{
    int retcd = -1;
    if (((enum AVPixelFormat)netcam->frame->format == MY_PIX_FMT_YUV420P) ||
        ((enum AVPixelFormat)netcam->frame->format == MY_PIX_FMT_YUVJ420P)) {
        retcd = 0;
    }
    return retcd;
}

static void netcam_pktarray_free(ctx_netcam *netcam)
{
    int indx;
    pthread_mutex_lock(&netcam->mutex_pktarray);
        if (netcam->pktarray_size > 0) {
            for(indx = 0; indx < netcam->pktarray_size; indx++) {
                mypacket_free(netcam->pktarray[indx].packet);
                netcam->pktarray[indx].packet = NULL;
            }
        }
        myfree(&netcam->pktarray);
        netcam->pktarray_size = 0;
        netcam->pktarray_index = -1;
    pthread_mutex_unlock(&netcam->mutex_pktarray);
}

static void netcam_null_context(ctx_netcam *netcam)
{
    netcam->swsctx          = NULL;
    netcam->swsframe_in     = NULL;
    netcam->swsframe_out    = NULL;
    netcam->frame           = NULL;
    netcam->codec_context   = NULL;
    netcam->format_context  = NULL;
    netcam->transfer_format = NULL;
    netcam->hw_device_ctx   = NULL;
}

static void netcam_close_context(ctx_netcam *netcam)
{
    if (netcam->swsctx          != NULL) sws_freeContext(netcam->swsctx);
    if (netcam->swsframe_in     != NULL) myframe_free(netcam->swsframe_in);
    if (netcam->swsframe_out    != NULL) myframe_free(netcam->swsframe_out);
    if (netcam->frame           != NULL) myframe_free(netcam->frame);
    if (netcam->pktarray        != NULL) netcam_pktarray_free(netcam);
    if (netcam->codec_context   != NULL) myavcodec_close(netcam->codec_context);
    if (netcam->format_context  != NULL) avformat_close_input(&netcam->format_context);
    if (netcam->transfer_format != NULL) avformat_close_input(&netcam->transfer_format);
    if (netcam->hw_device_ctx   != NULL) av_buffer_unref(&netcam->hw_device_ctx);
    netcam_null_context(netcam);
}

static void netcam_pktarray_resize(ctx_dev *cam, bool is_highres)
{
    /* This is called from netcam_next and is on the motion loop thread
     * The netcam->mutex is locked around the call to this function.
    */

    /* Remember that this is a ring and we have two threads chasing around it
     * the ffmpeg is writing out of this ring while we are filling it up.  "Bad"
     * things will occur if the "add" thread catches up with the "write" thread.
     * We need this ring to be big enough so they don't collide.
     * The alternative is that we'd need to make a copy of the entire packet
     * array in the ffmpeg module and do our writing from that copy.  The
     * downside is that is a lot to be copying around for each image we want
     * to write out.  And putting a mutex on the array during adding function would
     * slow down the capture thread to the speed of the writing thread.  And that
     * writing thread operates at the user specified FPS which could be really slow
     * ...So....make this array big enough so we never catch our tail.  :)
     */

    int64_t         idnbr_last, idnbr_first;
    int             indx;
    ctx_netcam      *netcam;
    ctx_packet_item *tmp;
    int             newsize;

    if (is_highres) {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_high;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_high;
        netcam = cam->netcam_high;
    } else {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_norm;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_norm;
        netcam = cam->netcam;
    }

    if (!netcam->passthrough) {
        return;
    }

    /* The 30 is arbitrary */
    /* Double the size plus double last diff so we don't catch our tail */
    newsize =(int)(((idnbr_first - idnbr_last) * 1 ) +
        ((netcam->idnbr - idnbr_last ) * 2));
    if (newsize < 30) {
        newsize = 30;
    }

    pthread_mutex_lock(&netcam->mutex_pktarray);
        if ((netcam->pktarray_size < newsize) ||  (netcam->pktarray_size < 30)) {
            tmp =(ctx_packet_item*) mymalloc(newsize * sizeof(ctx_packet_item));
            if (netcam->pktarray_size > 0 ) {
                memcpy(tmp, netcam->pktarray, sizeof(ctx_packet_item) * netcam->pktarray_size);
            }
            for(indx = netcam->pktarray_size; indx < newsize; indx++) {
                tmp[indx].packet = NULL;
                tmp[indx].packet = mypacket_alloc(tmp[indx].packet);
                tmp[indx].idnbr = 0;
                tmp[indx].iskey = false;
                tmp[indx].iswritten = false;
            }

            myfree(&netcam->pktarray);
            netcam->pktarray = tmp;
            netcam->pktarray_size = newsize;

            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                , _("%s:Resized packet array to %d")
                , netcam->cameratype.c_str(), newsize);
        }
    pthread_mutex_unlock(&netcam->mutex_pktarray);
}

static void netcam_pktarray_add(ctx_netcam *netcam)
{
    int indx_next;
    int retcd;
    char errstr[128];

    pthread_mutex_lock(&netcam->mutex_pktarray);

        if (netcam->pktarray_size == 0) {
            pthread_mutex_unlock(&netcam->mutex_pktarray);
            return;
        }

        /* Recall pktarray_size is one based but pktarray is zero based */
        if (netcam->pktarray_index == (netcam->pktarray_size-1)) {
            indx_next = 0;
        } else {
            indx_next = netcam->pktarray_index + 1;
        }

        netcam->pktarray[indx_next].idnbr = netcam->idnbr;

        mypacket_free(netcam->pktarray[indx_next].packet);
        netcam->pktarray[indx_next].packet = NULL;
        netcam->pktarray[indx_next].packet = mypacket_alloc(netcam->pktarray[indx_next].packet);

        retcd = mycopy_packet(netcam->pktarray[indx_next].packet, netcam->packet_recv);
        if ((netcam->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:av_copy_packet:%s ,Interrupt:%s")
                ,netcam->cameratype.c_str()
                ,errstr, netcam->interrupted ? _("true"):_("false"));
            mypacket_free(netcam->pktarray[indx_next].packet);
            netcam->pktarray[indx_next].packet = NULL;
        }

        if (netcam->pktarray[indx_next].packet->flags & AV_PKT_FLAG_KEY) {
            netcam->pktarray[indx_next].iskey = true;
        } else {
            netcam->pktarray[indx_next].iskey = false;
        }
        netcam->pktarray[indx_next].iswritten = false;

        netcam->pktarray_index = indx_next;
    pthread_mutex_unlock(&netcam->mutex_pktarray);
}

static int netcam_decode_sw(ctx_netcam *netcam)
{
    #if (MYFFVER >= 57041)
        int retcd;
        char errstr[128];

        retcd = avcodec_receive_frame(netcam->codec_context, netcam->frame);
        if ((netcam->interrupted) || (netcam->finish) || (retcd < 0)) {
            if (retcd == AVERROR(EAGAIN)) {
                retcd = 0;
            } else if (retcd == AVERROR_INVALIDDATA) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Ignoring packet with invalid data")
                    ,netcam->cameratype.c_str());
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s:Rec frame error:%s")
                        ,netcam->cameratype.c_str(), errstr);
                retcd = -1;
            } else {
                retcd = -1;
            }
            return retcd;
        }

        return 1;
    #else
        (void)netcam;
        return -1;
    #endif
}

static int netcam_decode_vaapi(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;
        char errstr[128];
        AVFrame *hw_frame = NULL;

        hw_frame = myframe_alloc();

        retcd = av_hwframe_get_buffer(netcam->codec_context->hw_frames_ctx, hw_frame, 0);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                , _("%s:Error getting hw frame buffer")
                , netcam->cameratype.c_str());
            myframe_free(hw_frame);
            return -1;
        }

        retcd = avcodec_receive_frame(netcam->codec_context, hw_frame);
        if ((netcam->interrupted) || (netcam->finish) || (retcd < 0)) {
            if (retcd == AVERROR(EAGAIN)) {
                retcd = 0;
            } else if (retcd == AVERROR_INVALIDDATA) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Ignoring packet with invalid data")
                    ,netcam->cameratype.c_str());
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s:Rec frame error:%s")
                        ,netcam->cameratype.c_str(), errstr);
                retcd = -1;
            } else {
                retcd = -1;
            }
            myframe_free(hw_frame);
            return retcd;
        }

        retcd = av_hwframe_transfer_data(netcam->frame, hw_frame, 0);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error transferring HW decoded to system memory")
                ,netcam->cameratype.c_str());
            myframe_free(hw_frame);
            return -1;
        }
        myframe_free(hw_frame);

        return 1;
    #else
        (void)netcam;
        return -1;
    #endif
}

static int netcam_decode_cuda(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;
        char errstr[128];
        AVFrame *hw_frame = NULL;

        hw_frame = myframe_alloc();

        retcd = avcodec_receive_frame(netcam->codec_context, hw_frame);
        if ((netcam->interrupted) || (netcam->finish) || (retcd < 0) ){
            if (retcd == AVERROR(EAGAIN)){
                retcd = 0;
            } else if (retcd == AVERROR_INVALIDDATA) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Ignoring packet with invalid data")
                    ,netcam->cameratype.c_str());
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s:Rec frame error:%s")
                        ,netcam->cameratype.c_str(), errstr);
                retcd = -1;
            } else {
                retcd = -1;
            }
            myframe_free(hw_frame);
            return retcd;
        }
        netcam->frame->format=AV_PIX_FMT_NV12;

        retcd = av_hwframe_transfer_data(netcam->frame, hw_frame, 0);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error transferring HW decoded to system memory")
                ,netcam->cameratype.c_str());
            myframe_free(hw_frame);
            return -1;
        }

        myframe_free(hw_frame);

        return 1;
    #else
        (void)netcam;
        return 1;
    #endif
}

static int netcam_decode_drm(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;
        char errstr[128];
        AVFrame *hw_frame = NULL;

        hw_frame = myframe_alloc();

        retcd = avcodec_receive_frame(netcam->codec_context, hw_frame);
        if ((netcam->interrupted) || (netcam->finish) || (retcd < 0) ){
            if (retcd == AVERROR(EAGAIN)){
                retcd = 0;
            } else if (retcd == AVERROR_INVALIDDATA) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Ignoring packet with invalid data")
                    ,netcam->cameratype.c_str());
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s:Rec frame error:%s")
                        ,netcam->cameratype.c_str(), errstr);
                retcd = -1;
            } else {
                retcd = -1;
            }
            myframe_free(hw_frame);
            return retcd;
        }
        netcam->frame->format=AV_PIX_FMT_NV12;

        retcd = av_hwframe_transfer_data(netcam->frame, hw_frame, 0);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error transferring HW decoded to system memory")
                ,netcam->cameratype.c_str());
            myframe_free(hw_frame);
            return -1;
        }

        myframe_free(hw_frame);

        return 1;
    #else
        (void)netcam;
        return 1;
    #endif
}

static int netcam_decode_video(ctx_netcam *netcam)
{
    #if (MYFFVER >= 57041)
        int retcd;
        char errstr[128];

        /* The Invalid data problem comes frequently.  Usually at startup of rtsp cameras.
        * We now ignore those packets so this function would need to fail on a different error.
        * We should consider adding a maximum count of these errors and reset every time
        * we get a good image.
        */
        if (netcam->finish) {
            return 0;
        }

        retcd = avcodec_send_packet(netcam->codec_context, netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Interrupted or finish on send")
                ,netcam->cameratype.c_str());
            return -1;
        }
        if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Send ignoring packet with invalid data")
                ,netcam->cameratype.c_str());
            return 0;
        }
        if (retcd < 0 && retcd != AVERROR_EOF) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error sending packet to codec:%s")
                ,netcam->cameratype.c_str(), errstr);
            if (netcam->service == "file") {
                return 0;
            } else {
                return -1;
            }
        }

        if (netcam->hw_type == AV_HWDEVICE_TYPE_VAAPI) {
            retcd = netcam_decode_vaapi(netcam);
        } else if (netcam->hw_type == AV_HWDEVICE_TYPE_CUDA) {
            retcd = netcam_decode_cuda(netcam);
        } else if (netcam->hw_type == AV_HWDEVICE_TYPE_DRM) {
            retcd = netcam_decode_drm(netcam);
        } else {
            retcd = netcam_decode_sw(netcam);
        }

        return retcd;

    #else
        int retcd;
        int check = 0;
        char errstr[128];

        (void)netcam_decode_sw;
        (void)netcam_decode_vaapi;
        (void)netcam_decode_cuda;
        (void)netcam_decode_drm;

        if (netcam->finish) {
            return 0;
        }
        retcd = avcodec_decode_video2(netcam->codec_context, netcam->frame, &check, &netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)) {
            return -1;
        }
        if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Ignoring packet with invalid data"));
            return 0;
        }

        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Error decoding packet: %s"),errstr);
            return -1;
        }

        if (check == 0 || retcd == 0) {
            return 0;
        }
        return 1;
    #endif
}

static int netcam_decode_packet(ctx_netcam *netcam)
{
    int frame_size;
    int retcd;

    if (netcam->finish) {
        return -1;
    }

    if (netcam->packet_recv->stream_index == netcam->audio_stream_index) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error decoding video packet...it is audio")
            ,netcam->cameratype.c_str());
    }

    retcd = netcam_decode_video(netcam);
    if (retcd <= 0) {
        return retcd;
    }
    frame_size = myimage_get_buffer_size((enum AVPixelFormat) netcam->frame->format
                                        ,netcam->frame->width
                                        ,netcam->frame->height);

    netcam_check_buffsize(netcam->img_recv, frame_size);
    netcam_check_buffsize(netcam->img_latest, frame_size);

    retcd = myimage_copy_to_buffer(netcam->frame
                                    ,(uint8_t *)netcam->img_recv->ptr
                                    ,(enum AVPixelFormat) netcam->frame->format
                                    ,netcam->frame->width
                                    ,netcam->frame->height
                                    ,frame_size);
    if ((retcd < 0) || (netcam->interrupted)) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error decoding video packet:Copying to buffer")
            ,netcam->cameratype.c_str());
        return -1;
    }

    netcam->img_recv->used = frame_size;

    return frame_size;
}

static void netcam_hwdecoders(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        /* High Res pass through does not decode images into frames*/
        if (netcam->high_resolution && netcam->passthrough) {
            return;
        }
        if ((netcam->hw_type == AV_HWDEVICE_TYPE_NONE) && (netcam->first_image)) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:HW Devices:")
                , netcam->cameratype.c_str());
            while((netcam->hw_type = av_hwdevice_iterate_types(netcam->hw_type)) != AV_HWDEVICE_TYPE_NONE){
                if ((netcam->hw_type == AV_HWDEVICE_TYPE_VAAPI) ||
                    (netcam->hw_type == AV_HWDEVICE_TYPE_CUDA)  ||
                    (netcam->hw_type == AV_HWDEVICE_TYPE_DRM)) {
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: %s(available)")
                        , netcam->cameratype.c_str()
                        , av_hwdevice_get_type_name(netcam->hw_type));
                } else {
                    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: %s(not implemented)")
                        , netcam->cameratype.c_str()
                        , av_hwdevice_get_type_name(netcam->hw_type));
                }
            }
        }
        return;
    #else
        (void)netcam;
        return;
    #endif
}

static enum AVPixelFormat netcam_getfmt_vaapi(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    #if ( MYFFVER >= 57083)
        const enum AVPixelFormat *p;
        (void)avctx;

        for (p = pix_fmts; *p != -1; p++) {
            if (*p == AV_PIX_FMT_VAAPI) {
                return *p;
            }
        }

        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get vaapi pix format"));
        return AV_PIX_FMT_NONE;
    #else
        (void)avctx;
        (void)pix_fmts;
        return AV_PIX_FMT_NONE;
    #endif
}

static enum AVPixelFormat netcam_getfmt_cuda(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    #if ( MYFFVER >= 57083)
        const enum AVPixelFormat *p;
        (void)avctx;

        for (p = pix_fmts; *p != -1; p++) {
            if (*p == AV_PIX_FMT_CUDA) return *p;
        }

        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get cuda pix format"));
        return AV_PIX_FMT_NONE;
    #else
        (void)avctx;
        (void)pix_fmts;
        return AV_PIX_FMT_NONE;
    #endif
}

static enum AVPixelFormat netcam_getfmt_drm(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    #if ( MYFFVER >= 57083)
        const enum AVPixelFormat *p;
        (void)avctx;

        for (p = pix_fmts; *p != -1; p++) {
            if (*p == AV_PIX_FMT_DRM_PRIME) return *p;
        }

        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get drm pix format"));
        return AV_PIX_FMT_NONE;
    #else
        (void)avctx;
        (void)pix_fmts;
        return AV_PIX_FMT_NONE;
    #endif
}

static void netcam_decoder_error(ctx_netcam *netcam, int retcd, const char* fnc_nm)
{
    char errstr[128];
    p_lst *lst = &netcam->params->params_array;
    p_it it;

    if (netcam->interrupted) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Interrupted"),netcam->cameratype.c_str());
    } else {
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:%s:%s"),netcam->cameratype.c_str()
                ,fnc_nm, errstr);
        } else {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:%s:Failed"), netcam->cameratype.c_str()
                ,fnc_nm);
        }
    }

    if (netcam->decoder_nm != "NULL") {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Decoder %s did not work.")
            ,netcam->cameratype.c_str(), netcam->decoder_nm.c_str());
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Ignoring and removing the user requested decoder %s")
            ,netcam->cameratype.c_str(), netcam->decoder_nm.c_str());

        for (it = lst->begin(); it != lst->end(); it++) {
            if (it->param_name == "decoder") {
                it->param_value = "NULL";
                break;
            }
        }

        if (netcam->high_resolution) {
            util_parms_update(netcam->params, netcam->conf->netcam_high_params);
        } else {
            util_parms_update(netcam->params, netcam->conf->netcam_params);
        }

        netcam->decoder_nm = "NULL";
    }
}

static int netcam_init_vaapi(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd, indx;
        AVPixelFormat *pixelformats = NULL;

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Initializing vaapi decoder")
            ,netcam->cameratype.c_str());

        netcam->hw_type = av_hwdevice_find_type_by_name("vaapi");
        if (netcam->hw_type == AV_HWDEVICE_TYPE_NONE) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find vaapi hw device")
                , netcam->cameratype.c_str());
            netcam_decoder_error(netcam, 0, "av_hwdevice");
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(netcam->decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, 0, "avcodec_alloc_context3");
            return -1;
        }

        retcd = avcodec_parameters_to_context(netcam->codec_context,netcam->strm->codecpar);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_parameters_to_context");
            return -1;
        }

        netcam->hw_pix_fmt = AV_PIX_FMT_VAAPI;
        netcam->codec_context->get_format  = netcam_getfmt_vaapi;
        av_opt_set_int(netcam->codec_context, "refcounted_frames", 1, 0);
        netcam->codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
        netcam->codec_context->pix_fmt= AV_PIX_FMT_YUV420P;
        netcam->codec_context->hwaccel_flags=
            AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
            AV_HWACCEL_FLAG_IGNORE_LEVEL;

        retcd = av_hwdevice_ctx_create(&netcam->hw_device_ctx, netcam->hw_type, NULL, NULL, 0);
        if (retcd < 0) {
            netcam_decoder_error(netcam, retcd, "hwctx");
            return -1;
        }

        netcam->codec_context->hw_device_ctx = av_buffer_ref(netcam->hw_device_ctx);

        AVBufferRef *hw_frames_ref = NULL;
        AVHWFramesContext *frames_ctx = NULL;

        hw_frames_ref = av_hwframe_ctx_alloc(
            netcam->codec_context->hw_device_ctx);
        if (hw_frames_ref == NULL) {
            netcam_decoder_error(netcam, retcd, "initvaapi 2");
            return -1;
        }

        frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
        frames_ctx->format    = AV_PIX_FMT_VAAPI;
        frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
        frames_ctx->width     = netcam->codec_context->width;
        frames_ctx->height    = netcam->codec_context->height;
        frames_ctx->initial_pool_size = 20;
        retcd = av_hwframe_ctx_init(hw_frames_ref);
        if (retcd < 0) {
            netcam_decoder_error(netcam, retcd, "initvaapi 2");
            av_buffer_unref(&hw_frames_ref);
            return -1;
        }
        netcam->codec_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
        if (netcam->codec_context->hw_frames_ctx == NULL) {
            netcam_decoder_error(netcam, retcd, "initvaapi 2");
            av_buffer_unref(&hw_frames_ref);
            return -1;
        }
        av_buffer_unref(&hw_frames_ref);

        retcd = av_hwframe_transfer_get_formats(
            netcam->codec_context->hw_frames_ctx
            , AV_HWFRAME_TRANSFER_DIRECTION_FROM
            , &pixelformats
            , 0);
        if (retcd < 0) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error enumerating HW pixel types")
                ,netcam->cameratype.c_str());
            netcam_decoder_error(netcam, retcd, "initvaapi 3");
            return -1;
        }
        /* Note that while these are listed as available, there
           is currently no way to get them from HW decoder without
           triggering a segfault in vaapi driver.
        */
        const AVPixFmtDescriptor *descr;
        for (indx=0; pixelformats[indx] != AV_PIX_FMT_NONE; indx++) {
            descr = av_pix_fmt_desc_get(pixelformats[indx]);
            MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                , _("%s:Available HW pixel type:%s")
                , netcam->cameratype.c_str()
                ,descr->name);
        }
        av_freep(&pixelformats);

        return 0;
    #else
        (void)netcam;
        (void)netcam_getfmt_vaapi;
        return -1;
    #endif
}

static int netcam_init_cuda(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Initializing cuda decoder"),netcam->cameratype.c_str());

        netcam->hw_type = av_hwdevice_find_type_by_name("cuda");
        if (netcam->hw_type == AV_HWDEVICE_TYPE_NONE){
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find cuda hw device")
                , netcam->cameratype.c_str());
            netcam_decoder_error(netcam, 0, "av_hwdevice");
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(netcam->decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)){
            netcam_decoder_error(netcam, 0, "avcodec_alloc_context3");
            return -1;
        }

        retcd = avcodec_parameters_to_context(netcam->codec_context,netcam->strm->codecpar);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_parameters_to_context");
            return -1;
        }

        netcam->hw_pix_fmt = AV_PIX_FMT_CUDA;
        netcam->codec_context->get_format  = netcam_getfmt_cuda;
        av_opt_set_int(netcam->codec_context, "refcounted_frames", 1, 0);
        netcam->codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
        netcam->codec_context->hwaccel_flags=
            AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
            AV_HWACCEL_FLAG_IGNORE_LEVEL;

        retcd = av_hwdevice_ctx_create(&netcam->hw_device_ctx, netcam->hw_type, NULL, NULL, 0);
        if (retcd < 0){
            netcam_decoder_error(netcam, retcd, "hwctx");
            return -1;
        }
        netcam->codec_context->hw_device_ctx = av_buffer_ref(netcam->hw_device_ctx);

        return 0;
    #else
        (void)netcam;
        (void)netcam_getfmt_cuda;
        return 0;
    #endif
}

static int netcam_init_drm(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Initializing drm decoder")
            ,netcam->cameratype.c_str());

        netcam->hw_type = av_hwdevice_find_type_by_name("drm");
        if (netcam->hw_type == AV_HWDEVICE_TYPE_NONE){
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find drm hw device")
                , netcam->cameratype.c_str());
            netcam_decoder_error(netcam, 0, "av_hwdevice");
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(netcam->decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)){
            netcam_decoder_error(netcam, 0, "avcodec_alloc_context3");
            return -1;
        }

        retcd = avcodec_parameters_to_context(netcam->codec_context,netcam->strm->codecpar);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_parameters_to_context");
            return -1;
        }

        netcam->hw_pix_fmt = AV_PIX_FMT_DRM_PRIME;
        netcam->codec_context->get_format  = netcam_getfmt_drm;
        av_opt_set_int(netcam->codec_context, "refcounted_frames", 1, 0);
        netcam->codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
        netcam->codec_context->hwaccel_flags=
            AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
            AV_HWACCEL_FLAG_IGNORE_LEVEL;

        retcd = av_hwdevice_ctx_create(&netcam->hw_device_ctx, netcam->hw_type, NULL, NULL, 0);
        if (retcd < 0){
            netcam_decoder_error(netcam, retcd, "hwctx");
            return -1;
        }
        netcam->codec_context->hw_device_ctx = av_buffer_ref(netcam->hw_device_ctx);

        return 0;
    #else
        (void)netcam;
        (void)netcam_getfmt_drm;
        return 0;
    #endif
}

static int netcam_init_swdecoder(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57041)
        int retcd;

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Initializing decoder"),netcam->cameratype.c_str());

        if (netcam->decoder_nm != "NULL") {
            netcam->decoder = avcodec_find_decoder_by_name(
                netcam->decoder_nm.c_str());
            if (netcam->decoder == NULL) {
                netcam_decoder_error(netcam, 0
                    , "avcodec_find_decoder_by_name");
            } else {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Using decoder %s")
                    , netcam->cameratype.c_str()
                    , netcam->decoder_nm.c_str());
            }
        }
        if (netcam->decoder == NULL) {
            netcam->decoder = avcodec_find_decoder(netcam->strm->codecpar->codec_id);
        }
        if ((netcam->decoder == NULL) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, 0, "avcodec_find_decoder");
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(netcam->decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, 0, "avcodec_alloc_context3");
            return -1;
        }

        retcd = avcodec_parameters_to_context(netcam->codec_context, netcam->strm->codecpar);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_parameters_to_context");
            return -1;
        }

        netcam->codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
        netcam->codec_context->err_recognition = AV_EF_EXPLODE;

        return 0;
    #else
        int retcd;
        netcam->codec_context = netcam->strm->codec;
        netcam->decoder = avcodec_find_decoder(netcam->codec_context->codec_id);
        if ((netcam->decoder == NULL) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, 0, "avcodec_find_decoder");
            return -1;
        }
        retcd = avcodec_open2(netcam->codec_context, netcam->decoder, NULL);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_open2");
            return -1;
        }
        return 0;
    #endif
}

static int netcam_open_codec(ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57041)

        int retcd;

        if (netcam->finish) {
            return -1;
        }

        netcam_hwdecoders(netcam);

        retcd = av_find_best_stream(netcam->format_context
                    , AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam->audio_stream_index = -1;
        } else {
            netcam->audio_stream_index = retcd;
        }

        netcam->decoder = NULL;
        retcd = av_find_best_stream(netcam->format_context
                    , AVMEDIA_TYPE_VIDEO, -1, -1, &netcam->decoder, 0);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "av_find_best_stream");
            return -1;
        }
        netcam->video_stream_index = retcd;
        netcam->strm = netcam->format_context->streams[netcam->video_stream_index];

        if (netcam->decoder_nm == "vaapi") {
            if (netcam_init_vaapi(netcam) < 0) {
                netcam_decoder_error(netcam, retcd, "hwvaapi_init");
                return -1;
            }
        } else if (netcam->decoder_nm == "cuda"){
            if (netcam_init_cuda(netcam) <0 ) {
                netcam_decoder_error(netcam, retcd, "hwcuda_init");
                return -1;
            }
	    } else if (netcam->decoder_nm == "drm"){
            if (netcam_init_drm(netcam) < 0) {;
                netcam_decoder_error(netcam, retcd, "hwdrm_init");
                return -1;
            }
        } else {
            netcam_init_swdecoder(netcam);
        }

        retcd = avcodec_open2(netcam->codec_context, netcam->decoder, NULL);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "avcodec_open2");
            return -1;
        }

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Decoder opened"),netcam->cameratype.c_str());

        return 0;
    #else
        int retcd;

        (void)netcam_init_vaapi;
        (void)netcam_init_cuda;
        (void)netcam_init_drm;

        if (netcam->finish) {

            return -1;
        }

        netcam_hwdecoders(netcam);
        netcam->decoder = NULL;

        retcd = av_find_best_stream(netcam->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if ((retcd < 0) || (netcam->interrupted)) {
            netcam_decoder_error(netcam, retcd, "av_find_best_stream");
            return -1;
        }
        netcam->video_stream_index = retcd;
        netcam->strm = netcam->format_context->streams[netcam->video_stream_index];

        retcd = netcam_init_swdecoder(netcam);

        return retcd;
    #endif
}

static int netcam_interrupt(void *ctx)
{
    ctx_netcam *netcam = (ctx_netcam *)ctx;

    if (netcam->finish) {
        netcam->interrupted = true;
        return true;
    }

    if (netcam->status == NETCAM_CONNECTED) {
        return false;
    } else if (netcam->status == NETCAM_READINGIMAGE) {
        clock_gettime(CLOCK_MONOTONIC, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Camera reading (%s) timed out")
                , netcam->cameratype.c_str(), netcam->camera_name.c_str());
            netcam->interrupted = true;
            return true;
        } else{
            return false;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * netcam_connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        clock_gettime(CLOCK_MONOTONIC, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Camera (%s) timed out")
                , netcam->cameratype.c_str(), netcam->camera_name.c_str());
            netcam->interrupted = true;
            return true;
        } else{
            return false;
        }
    }

    /* should not be possible to get here */
    return false;
}

static int netcam_open_sws(ctx_netcam *netcam)
{
    if (netcam->finish) {
        return -1;
    }

    netcam->swsframe_in = myframe_alloc();
    if (netcam->swsframe_in == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate swsframe_in.")
                , netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_out = myframe_alloc();
    if (netcam->swsframe_out == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate swsframe_out.")
                , netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    if (netcam_check_pixfmt(netcam) != 0) {
        const AVPixFmtDescriptor *descr;
        descr = av_pix_fmt_desc_get((AVPixelFormat)netcam->frame->format);
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("%s:Pixel format %s will be converted.")
            , netcam->cameratype.c_str(), descr->name);
    }

    netcam->swsctx = sws_getContext(
         netcam->frame->width
        ,netcam->frame->height
        ,(enum AVPixelFormat)netcam->frame->format
        ,netcam->imgsize.width
        ,netcam->imgsize.height
        ,MY_PIX_FMT_YUV420P
        ,SWS_BICUBIC,NULL,NULL,NULL);
    if (netcam->swsctx == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate scaling context.")
                , netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_size = myimage_get_buffer_size(
            MY_PIX_FMT_YUV420P
            ,netcam->imgsize.width
            ,netcam->imgsize.height);
    if (netcam->swsframe_size <= 0) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Error determining size of frame out")
                , netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    netcam_check_buffsize(netcam->img_recv, netcam->swsframe_size);
    netcam_check_buffsize(netcam->img_latest, netcam->swsframe_size);

    return 0;
}

static int netcam_resize(ctx_netcam *netcam)
{
    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    if (netcam->finish) {
        return -1;
    }

    if (netcam->swsctx == NULL) {
        if (netcam_open_sws(netcam) < 0) {
            return -1;
        }
    }

    retcd=myimage_fill_arrays(
        netcam->swsframe_in
        ,(uint8_t*)netcam->img_recv->ptr
        ,(enum AVPixelFormat)netcam->frame->format
        ,netcam->frame->width
        ,netcam->frame->height);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error allocating picture in:%s")
                , netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    buffer_out=(uint8_t *)av_malloc(netcam->swsframe_size*sizeof(uint8_t));

    retcd=myimage_fill_arrays(
        netcam->swsframe_out
        ,buffer_out
        ,MY_PIX_FMT_YUV420P
        ,netcam->imgsize.width
        ,netcam->imgsize.height);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error allocating picture out:%s")
                , netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    retcd = sws_scale(
        netcam->swsctx
        ,(const uint8_t* const *)netcam->swsframe_in->data
        ,netcam->swsframe_in->linesize
        ,0
        ,netcam->frame->height
        ,netcam->swsframe_out->data
        ,netcam->swsframe_out->linesize);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error resizing/reformatting:%s")
                , netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    retcd=myimage_copy_to_buffer(
         netcam->swsframe_out
        ,(uint8_t *)netcam->img_recv->ptr
        ,MY_PIX_FMT_YUV420P
        ,netcam->imgsize.width
        ,netcam->imgsize.height
        ,netcam->swsframe_size);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error putting frame into output buffer:%s")
                , netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }
    netcam->img_recv->used = netcam->swsframe_size;

    av_free(buffer_out);

    return 0;
}

static int netcam_read_image(ctx_netcam *netcam)
{
    int  size_decoded, retcd, errcnt, nodata;
    bool haveimage;
    char errstr[128];
    netcam_buff *xchg;

    if (netcam->finish) {
        return -1;
    }

    netcam->packet_recv = mypacket_alloc(netcam->packet_recv);

    netcam->interrupted=false;
    clock_gettime(CLOCK_MONOTONIC, &netcam->interruptstarttime);
    netcam->interruptduration = 10;

    netcam->status = NETCAM_READINGIMAGE;
    netcam->img_recv->used = 0;
    size_decoded = 0;
    errcnt = 0;
    haveimage = false;
    nodata = 0;

    while ((!haveimage) && (!netcam->interrupted)) {
        retcd = av_read_frame(netcam->format_context, netcam->packet_recv);
        if (retcd < 0 ) {
            errcnt++;
        }
        if ((netcam->interrupted) || (errcnt > 1)) {
            if (netcam->interrupted) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Interrupted")
                    ,netcam->cameratype.c_str());
            } else {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:av_read_frame:%s" )
                    ,netcam->cameratype.c_str(),errstr);
            }
            netcam_free_pkt(netcam);
            netcam_close_context(netcam);
            return -1;
        } else {
            errcnt = 0;
            if ((netcam->packet_recv->stream_index == netcam->video_stream_index) ||
                (netcam->packet_recv->stream_index == netcam->audio_stream_index)) {
                /* For a high resolution pass-through we don't decode the image */
                if ((netcam->high_resolution && netcam->passthrough) ||
                    (netcam->packet_recv->stream_index != netcam->video_stream_index)) {
                    if (netcam->packet_recv->data != NULL) {
                        size_decoded = 1;
                    }
                } else {
                    size_decoded = netcam_decode_packet(netcam);
                }
            }
            if (size_decoded > 0) {
                haveimage = true;
            } else if (size_decoded == 0) {
                /* Did not fail, just didn't get anything.  Try again */
                netcam_free_pkt(netcam);
                netcam->packet_recv = mypacket_alloc(netcam->packet_recv);

                /* The 1000 is arbitrary */
                nodata++;
                if (nodata > 1000) {
                    netcam_close_context(netcam);
                    return -1;
                }

            } else {
                netcam_free_pkt(netcam);
                netcam_close_context(netcam);
                return -1;
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &netcam->img_recv->image_time);
    netcam->last_stream_index = netcam->packet_recv->stream_index;
    netcam->last_pts = netcam->packet_recv->pts;

    if (!netcam->first_image) {
        netcam->status = NETCAM_CONNECTED;
    }

    /* Skip resize/pix format for high pass-through */
    if (!(netcam->high_resolution && netcam->passthrough) &&
        (netcam->packet_recv->stream_index == netcam->video_stream_index)) {

        if ((netcam->imgsize.width  != netcam->frame->width) ||
            (netcam->imgsize.height != netcam->frame->height) ||
            (netcam_check_pixfmt(netcam) != 0)) {
            if (netcam_resize(netcam) < 0) {
                netcam_free_pkt(netcam);
                netcam_close_context(netcam);
                return -1;
            }
        }
    }

    pthread_mutex_lock(&netcam->mutex);
        netcam->idnbr++;
        if (netcam->passthrough) {
            netcam_pktarray_add(netcam);
        }
        if (!(netcam->high_resolution && netcam->passthrough) &&
            (netcam->packet_recv->stream_index == netcam->video_stream_index)) {
            xchg = netcam->img_latest;
            netcam->img_latest = netcam->img_recv;
            netcam->img_recv = xchg;
        }
    pthread_mutex_unlock(&netcam->mutex);

    netcam_free_pkt(netcam);

    if (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den > 0) {
        netcam->src_fps = (int)(
            (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.num /
            netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den) +
            0.5);
        if (netcam->capture_rate < 1) {
            netcam->capture_rate = netcam->src_fps + 1;
            if (netcam->pts_adj == false) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:capture_rate not specified in netcam_params. Using %d")
                    ,netcam->cameratype.c_str(),netcam->capture_rate);
            }
        }
    } else {
        if (netcam->capture_rate < 1) {
            netcam->capture_rate = netcam->conf->framerate;
            if (netcam->pts_adj == false) {
               MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:capture_rate not specified.")
                    ,netcam->cameratype.c_str());
               MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Using framerate %d")
                    ,netcam->cameratype.c_str(), netcam->capture_rate);
            }
        }
    }

    return 0;
}

static int netcam_ntc(ctx_netcam *netcam)
{
    if ((netcam->finish) || (!netcam->first_image)) {
        return 0;
    }

    /* High Res pass through does not decode images into frames*/
    if (netcam->high_resolution && netcam->passthrough) {
        return 0;
    }

    if ((netcam->imgsize.width == 0) || (netcam->imgsize.height == 0) ||
        (netcam->frame->width == 0) || (netcam->frame->height == 0)){
            return 0;
    }

    if ((netcam->imgsize.width  != netcam->frame->width) ||
        (netcam->imgsize.height != netcam->frame->height)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures at %dx%d")
            , netcam->frame->width,netcam->frame->height);
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("resolution but config is %dx%d. If possible change")
            , netcam->imgsize.width,netcam->imgsize.height);
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("the netcam or config so that the image height and"));
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("width are the same to lower the CPU usage."));
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
    }

    return 0;
}

static void netcam_set_options(ctx_netcam *netcam)
{
    std::string tmp;
    p_lst   *lst = &netcam->params->params_array;
    p_it    it;

    if ((netcam->service == "rtsp") ||
        (netcam->service == "rtsps") ||
        (netcam->service == "rtmp")) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s:Setting rtsp/rtmp")
            ,netcam->cameratype.c_str());
        util_parms_add_default(netcam->params,"rtsp_transport","tcp");
        //util_parms_add_default(netcam->params,"allowed_media_types", "video");

    } else if ((netcam->service == "http") ||
               (netcam->service == "https")) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Setting input_format mjpeg")
            ,netcam->cameratype.c_str());
        netcam->format_context->iformat = av_find_input_format("mjpeg");
        util_parms_add_default(netcam->params,"reconnect_on_network_error","1");
        util_parms_add_default(netcam->params,"reconnect_at_eof","1");
        util_parms_add_default(netcam->params,"reconnect","1");
        util_parms_add_default(netcam->params,"multiple_requests","1");
        util_parms_add_default(netcam->params,"reconnect_streamed","1");

    } else if (netcam->service == "v4l2") {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Setting input_format video4linux2")
            ,netcam->cameratype.c_str());
        netcam->format_context->iformat = av_find_input_format("video4linux2");

        tmp = std::to_string(netcam->conf->framerate);
        util_parms_add_default(netcam->params,"framerate", tmp);

        tmp = std::to_string(netcam->conf->width) + "x" +
            std::to_string(netcam->conf->height);
        util_parms_add_default(netcam->params,"video_size", tmp);

        netcam->interruptduration = 55;

    } else if (netcam->service == "file") {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Setting up movie file")
            ,netcam->cameratype.c_str());

    } else {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s:Setting up %s")
            , netcam->cameratype.c_str()
            , netcam->service.c_str());
    }

    for (it = lst->begin(); it != lst->end(); it++) {
        if ((it->param_name != "decoder") &&
            (it->param_name != "capture_rate")) {
            av_dict_set(&netcam->opts
                , it->param_name.c_str(), it->param_value.c_str(), 0);
            if (netcam->status == NETCAM_NOTCONNECTED) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s:%s = %s")
                    ,netcam->cameratype.c_str()
                    ,it->param_name.c_str(),it->param_value.c_str());
            }
        }
    }
}

static void netcam_set_path (ctx_dev *cam, ctx_netcam *netcam )
{
    ctx_url url;

    netcam->path = "";

    if (netcam->high_resolution) {
        netcam_url_parse(&url, cam->conf->netcam_high_url);
    } else {
        netcam_url_parse(&url, cam->conf->netcam_url);
    }

    if (cam->conf->netcam_userpass != "") {
        url.userpass = cam->conf->netcam_userpass;
    }

    if  (url.service == "v4l2") {
        netcam->path = url.path;
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up v4l2"));
    } else if (url.service == "file") {
        netcam->path = url.path;
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up file"));
    } else {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up %s "),url.service.c_str());
        if (url.userpass.length() > 0) {
            netcam->path = url.service + "://" +
                url.userpass + "@" + url.host +":"+
                std::to_string(url.port) + url.path;
        } else {
            netcam->path = url.service + "://" +
                url.host + ":" + std::to_string(url.port) + url.path;
        }
    }

    netcam->service = url.service;
}

static void netcam_set_parms (ctx_dev *cam, ctx_netcam *netcam )
{
    p_it    it;

    netcam->finish = false;
    netcam->motapp = cam->motapp;
    netcam->conf = cam->conf;
    netcam->params = new ctx_params;

    pthread_mutex_init(&netcam->mutex, NULL);
    pthread_mutex_init(&netcam->mutex_pktarray, NULL);
    pthread_mutex_init(&netcam->mutex_transfer, NULL);

    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->threadnbr = ++netcam->motapp->threads_running;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    netcam_null_context(netcam);

    if (netcam->high_resolution) {
        netcam->imgsize.width = 0;
        netcam->imgsize.height = 0;
        netcam->cameratype = _("High");
        netcam->params->update_params = true;
        util_parms_parse(netcam->params
            ,"netcam_high_params", cam->conf->netcam_high_params);
    } else {
        netcam->imgsize.width = cam->conf->width;
        netcam->imgsize.height = cam->conf->height;
        netcam->cameratype = _("Norm");
        netcam->params->update_params = true;
        util_parms_parse(netcam->params
            ,"netcam_params", cam->conf->netcam_params);
    }
    netcam->camera_name = cam->conf->device_name;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        , _("%s:Setting up camera(%s).")
        , netcam->cameratype.c_str(), netcam->camera_name.c_str());

    netcam->status = NETCAM_NOTCONNECTED;
    mycheck_passthrough(cam);
    util_parms_add_default(netcam->params,"decoder","NULL");
    netcam->img_recv =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_recv->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->img_latest =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_latest->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->pktarray_size = 0;
    netcam->pktarray_index = -1;
    netcam->pktarray = NULL;
    netcam->packet_recv = NULL;
    netcam->handler_finished = true;
    netcam->first_image = true;
    netcam->reconnect_count = 0;
    netcam->src_fps =  -1; /* Default to neg so we know it has not been set */
    netcam->pts_adj = false;
    netcam->capture_rate = -1;
    netcam->video_stream_index = -1;
    netcam->audio_stream_index = -1;
    netcam->last_stream_index = -1;
    netcam->strm = NULL;
    netcam->opts = NULL;
    netcam->decoder = NULL;
    netcam->idnbr = 0;
    netcam->swsframe_size = 0;
    netcam->hw_type = AV_HWDEVICE_TYPE_NONE;
    netcam->hw_pix_fmt = AV_PIX_FMT_NONE;
    netcam->connection_pts = 0;
    netcam->last_pts = 0;

    for (it = netcam->params->params_array.begin();
        it != netcam->params->params_array.end(); it++) {
        if (it->param_name == "decoder") {
            netcam->decoder_nm = it->param_value;
        }
        if (it->param_name == "capture_rate") {
            if (it->param_value == "pts") {
                netcam->pts_adj = true;
            } else {
                netcam->capture_rate = mtoi(it->param_value);
            }
        }
    }

    /* If this is the norm and we have a highres, then disable passthru on the norm */
    if ((!netcam->high_resolution) &&
        (cam->conf->netcam_high_url != "")) {
        netcam->passthrough = false;
    } else {
        netcam->passthrough = mycheck_passthrough(cam);
    }

    netcam->threadname = _("Unknown");

    clock_gettime(CLOCK_MONOTONIC, &netcam->frame_curr_tm);
    clock_gettime(CLOCK_MONOTONIC, &netcam->frame_prev_tm);
    clock_gettime(CLOCK_MONOTONIC, &netcam->connection_tm);
    clock_gettime(CLOCK_MONOTONIC, &netcam->interruptstarttime);
    clock_gettime(CLOCK_MONOTONIC, &netcam->interruptcurrenttime);

    netcam->interruptduration = 5;
    netcam->interrupted = false;

    netcam_set_path(cam, netcam);
}

static void netcam_set_dimensions (ctx_dev *cam)
{
    cam->imgs.width = 0;
    cam->imgs.height = 0;
    cam->imgs.size_norm = 0;
    cam->imgs.motionsize = 0;

    cam->imgs.width_high  = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high   = 0;

    if (cam->conf->width % 8) {
        MOTPLS_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8.")
            , cam->conf->width);
        cam->conf->width = cam->conf->width - (cam->conf->width % 8) + 8;
        MOTPLS_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Adjusting width to next higher multiple of 8 (%d).")
            , cam->conf->width);
    }
    if (cam->conf->height % 8) {
        MOTPLS_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cam->conf->height);
        cam->conf->height = cam->conf->height - (cam->conf->height % 8) + 8;
        MOTPLS_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cam->conf->height);
    }

    /* Fill in camera details into context structure. */
    cam->imgs.width = cam->conf->width;
    cam->imgs.height = cam->conf->height;
    cam->imgs.size_norm = (cam->conf->width * cam->conf->height * 3) / 2;
    cam->imgs.motionsize = cam->conf->width * cam->conf->height;
}

/* Make a static copy of the stream information for use in passthrough processing */
static int netcam_copy_stream(ctx_netcam *netcam)
{
    /* Make a static copy of the stream information for use in passthrough processing */
    #if (MYFFVER >= 57041)
        AVStream  *transfer_stream, *stream_in;
        int        retcd, indx;

        pthread_mutex_lock(&netcam->mutex_transfer);
            if (netcam->transfer_format != NULL) {
                avformat_close_input(&netcam->transfer_format);
            }
            netcam->transfer_format = avformat_alloc_context();
            for (indx = 0; indx < (int)netcam->format_context->nb_streams; indx++) {
                if ((netcam->audio_stream_index == indx) ||
                    (netcam->video_stream_index == indx)) {
                    transfer_stream = avformat_new_stream(netcam->transfer_format, NULL);
                    stream_in = netcam->format_context->streams[indx];
                    retcd = avcodec_parameters_copy(transfer_stream->codecpar, stream_in->codecpar);
                    if (retcd < 0) {
                        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                            ,_("%s:Unable to copy codec parameters")
                            , netcam->cameratype.c_str());
                        pthread_mutex_unlock(&netcam->mutex_transfer);
                        return -1;
                    }
                    transfer_stream->time_base = stream_in->time_base;
                    transfer_stream->avg_frame_rate = stream_in->avg_frame_rate;
                }
            }
        pthread_mutex_unlock(&netcam->mutex_transfer);

        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
        return 0;
    #else
        /* This is disabled in the mycheck_passthrough but we need it here for compiling */
        if (netcam != NULL) {
            MOTPLS_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("ffmpeg too old"));
        }
        return -1;
    #endif
}

static int netcam_open_context(ctx_netcam *netcam)
{
    int  retcd;
    char errstr[128];

    if (netcam->finish) {
        return -1;
    }

    if (netcam->path == "") {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("No path passed to connect"));
        }
        return -1;
    }

    netcam->opts = NULL;
    netcam->format_context = avformat_alloc_context();
    netcam->format_context->interrupt_callback.callback = netcam_interrupt;
    netcam->format_context->interrupt_callback.opaque = netcam;
    netcam->interrupted = false;

    clock_gettime(CLOCK_MONOTONIC, &netcam->interruptstarttime);

    netcam->interruptduration = 20;

    netcam_set_options(netcam);

    retcd = avformat_open_input(&netcam->format_context
        , netcam->path.c_str(), NULL, &netcam->opts);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to open camera(%s):%s")
                , netcam->cameratype.c_str()
                , netcam->camera_name.c_str(), errstr);
        }
        av_dict_free(&netcam->opts);
        netcam_close_context(netcam);
        return -1;
    }
    av_dict_free(&netcam->opts);
    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Opened camera(%s)"), netcam->cameratype.c_str()
        , netcam->camera_name.c_str());


    /* fill out stream information */
    retcd = avformat_find_stream_info(netcam->format_context, NULL);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to find stream info:%s")
                ,netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */

    mythreadname_get(netcam->threadname);
    mythreadname_set("av", netcam->threadnbr, netcam->camera_name.c_str());
        retcd = netcam_open_codec(netcam);
    mythreadname_set(NULL, 0, netcam->threadname.c_str());
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to open codec context:%s")
                ,netcam->cameratype.c_str(), errstr);
        } else {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Connected and unable to open codec context:%s")
                ,netcam->cameratype.c_str(), errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->codec_context->width <= 0 ||
        netcam->codec_context->height <= 0) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Camera image size is invalid")
            ,netcam->cameratype.c_str());
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->high_resolution) {
        netcam->imgsize.width = netcam->codec_context->width;
        netcam->imgsize.height = netcam->codec_context->height;
    }

    netcam->frame = myframe_alloc();
    if (netcam->frame == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to allocate frame.")
                ,netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->passthrough) {
        retcd = netcam_copy_stream(netcam);
        if ((retcd < 0) || (netcam->interrupted)) {
            if (netcam->status == NETCAM_NOTCONNECTED) {
                MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Failed to copy stream for pass-through.")
                    ,netcam->cameratype.c_str());
            }
            netcam->passthrough = false;
        }
    }

    /* Validate that the previous steps opened the camera */
    retcd = netcam_read_image(netcam);
    if ((retcd < 0) || (netcam->interrupted)) {
        if (netcam->status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Failed to read first image")
                ,netcam->cameratype.c_str());
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->connection_pts = AV_NOPTS_VALUE;

    return 0;
}

static int netcam_connect(ctx_netcam *netcam)
{
    if (netcam_open_context(netcam) < 0) {
        return -1;
    }

    if (netcam_ntc(netcam) < 0 ) {
        return -1;
    }

    if (netcam_read_image(netcam) < 0) {
        return -1;
    }

    /* We use the status for determining whether to grab a image from
     * the Motion loop(see "next" function).  When we are initially starting,
     * we open and close the context and during this process we do not want the
     * Motion loop to start quite yet on this first image so we do
     * not set the status to connected
     */
    if (!netcam->first_image) {
        netcam->status = NETCAM_CONNECTED;

        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Camera (%s) connected")
            , netcam->cameratype.c_str(),netcam->camera_name.c_str());

        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("%s:Netcam capture FPS is %d.")
            , netcam->cameratype.c_str(), netcam->capture_rate);

        if (netcam->src_fps > 0) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Camera source is %d FPS")
                , netcam->cameratype.c_str(), netcam->src_fps);
        } else {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to determine the camera source FPS.")
                , netcam->cameratype.c_str());
        }

        if (netcam->capture_rate < netcam->src_fps) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Capture FPS less than camera FPS. Decoding errors will occur.")
                , netcam->cameratype.c_str());
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Capture FPS should be greater than camera FPS.")
                , netcam->cameratype.c_str());
        }

        if (netcam->audio_stream_index != -1) {
            /* The following is not technically precise but we want to convey in simple terms
            * that the capture rate must go faster to account for the additional packets read
            * for the audio stream.  The technically correct process is that our wait timer in
            * the handler is only triggered when the last packet is a video stream
            */
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:An audio stream was detected.  Capture_rate increased to compensate.")
                ,netcam->cameratype.c_str());
        }
    }

    return 0;
}

static void netcam_shutdown(ctx_netcam *netcam)
{
    if (netcam) {
        netcam_close_context(netcam);

        netcam->status = NETCAM_NOTCONNECTED;

        if (netcam->img_latest != NULL) {
            free(netcam->img_latest->ptr);
            free(netcam->img_latest);
            netcam->img_latest = NULL;
        }

        if (netcam->img_recv != NULL) {
            free(netcam->img_recv->ptr);
            free(netcam->img_recv);
            netcam->img_recv   = NULL;
        }
    }
}

static void netcam_handler_wait(ctx_netcam *netcam)
{
    int64_t usec_ltncy;
    AVRational tbase;
    struct timespec tmp_tm;

    clock_gettime(CLOCK_MONOTONIC, &tmp_tm);

    if (netcam->capture_rate < 1) {
        netcam->capture_rate = 1;
    }
    tbase = netcam->format_context->streams[netcam->video_stream_index]->time_base;
    if (tbase.num == 0) {
        tbase.num = 1;
    }

    /* FPS calculation from last frame capture */
    netcam->frame_curr_tm = tmp_tm;
    usec_ltncy = (1000000 / netcam->capture_rate) -
        ((netcam->frame_curr_tm.tv_sec - netcam->frame_prev_tm.tv_sec) * 1000000) -
        ((netcam->frame_curr_tm.tv_nsec - netcam->frame_prev_tm.tv_nsec)/1000);

    /* Adjust to clock and pts timer */
    if (netcam->pts_adj == true) {
        if (netcam->connection_pts == AV_NOPTS_VALUE) {
            netcam->connection_pts = netcam->last_pts;
            clock_gettime(CLOCK_MONOTONIC, &netcam->connection_tm);
            return;
        }
        if (netcam->last_pts != AV_NOPTS_VALUE) {
            usec_ltncy +=
                + (av_rescale(netcam->last_pts, 1000000, tbase.den/tbase.num)
                  - av_rescale(netcam->connection_pts, 1000000, tbase.den/tbase.num))
                -(((tmp_tm.tv_sec - netcam->connection_tm.tv_sec) * 1000000) +
                  ((tmp_tm.tv_nsec - netcam->connection_tm.tv_nsec) / 1000));
        }
    }

    if ((usec_ltncy > 0) && (usec_ltncy < 1000000L)) {
        SLEEP(0, usec_ltncy * 1000);
    }
}

static void netcam_handler_reconnect(ctx_netcam *netcam)
{
    int retcd, indx;

    if ((netcam->status == NETCAM_CONNECTED) ||
        (netcam->status == NETCAM_READINGIMAGE)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Reconnecting with camera....")
            ,netcam->cameratype.c_str());
    }
    netcam->status = NETCAM_RECONNECTING;

    /* When doing passthrough movies, any movies in progress
    * must end and start again because the cache'd packets and timing
    * on them gets all messed up.  So we loop through the list of cameras
    * and find the pointer that matches our passed in netcam.
    */
    if (netcam->passthrough == true) {
        for (indx=0; indx<netcam->motapp->cam_cnt; indx++) {
            if ((netcam->motapp->cam_list[indx]->netcam == netcam) ||
                (netcam->motapp->cam_list[indx]->netcam_high == netcam)) {
                netcam->motapp->cam_list[indx]->event_stop = true;
            }
        }
    }

    /*
    * The retry count of 100 is arbritrary.
    * We want to try many times quickly to not lose too much information
    * before we go into the long wait phase
    */
    retcd = netcam_connect(netcam);
    if (retcd < 0) {
        if (netcam->reconnect_count < 100) {
            netcam->reconnect_count++;
        } else if ((netcam->reconnect_count >= 100) && (netcam->reconnect_count <= 199)) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Camera did not reconnect.")
                , netcam->cameratype.c_str());
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Checking for camera every 10 seconds.")
                ,netcam->cameratype.c_str());
            netcam->reconnect_count++;
            SLEEP(10,0);
        } else if (netcam->reconnect_count >= 200) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Camera did not reconnect.")
                , netcam->cameratype.c_str());
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Checking for camera every 10 minutes.")
                ,netcam->cameratype.c_str());
            SLEEP(600,0);
        } else {
            netcam->reconnect_count++;
            SLEEP(600,0);
        }
    } else {
        netcam->reconnect_count = 0;
    }
}

static void *netcam_handler(void *arg)
{
    ctx_netcam *netcam =(ctx_netcam *) arg;

    netcam->handler_finished = false;

    mythreadname_set("nc",netcam->threadnbr, netcam->camera_name.c_str());

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)netcam->threadnbr));

    MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Camera handler thread [%d] started")
        ,netcam->cameratype.c_str(), netcam->threadnbr);

    while (!netcam->finish) {
        if (!netcam->format_context) {      /* We must have disconnected.  Try to reconnect */
            clock_gettime(CLOCK_MONOTONIC, &netcam->frame_prev_tm);
            netcam_handler_reconnect(netcam);
            continue;
        } else {            /* We think we are connected...*/
            clock_gettime(CLOCK_MONOTONIC, &netcam->frame_prev_tm);
            if (netcam_read_image(netcam) < 0) {
                if (!netcam->finish) {   /* Nope.  We are not or got bad image.  Reconnect*/
                    netcam_handler_reconnect(netcam);
                }
                continue;
            }
            if (netcam->last_stream_index == netcam->video_stream_index) {
                netcam_handler_wait(netcam);
            }
        }
    }

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Loop finished."),netcam->cameratype.c_str());
    netcam_shutdown(netcam);

    /* Our thread is finished - decrement motion's thread count. */
    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->motapp->threads_running--;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Exiting"),netcam->cameratype.c_str());
    netcam->handler_finished = true;

    pthread_exit(NULL);
}

static int netcam_start_handler(ctx_netcam *netcam)
{
    int retcd, wait_counter;
    pthread_attr_t handler_attribute;

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    retcd = pthread_create(&netcam->thread_id, &handler_attribute, &netcam_handler, netcam);
    if (retcd < 0) {
        MOTPLS_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("%s:Error starting handler thread")
            ,netcam->cameratype.c_str());
        pthread_attr_destroy(&handler_attribute);
        return -1;
    }
    pthread_attr_destroy(&handler_attribute);


    /* Now give a few tries to check that an image has been captured.
     * This ensures that by the time the setup routine exits, the
     * handler is completely set up and has images available
     */
    wait_counter = 60;
    while (wait_counter > 0) {
        pthread_mutex_lock(&netcam->mutex);
            if (netcam->img_latest->ptr != NULL ) {
                wait_counter = -1;
            }
        pthread_mutex_unlock(&netcam->mutex);

        if (wait_counter > 0 ) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Waiting for first image from the handler.")
                ,netcam->cameratype.c_str());
            SLEEP(0,5000000);
            wait_counter--;
        }
    }

    return 0;
}

void netcam_cleanup(ctx_dev *cam)
{
    int wait_counter;
    int indx_cam, indx_max;
    ctx_netcam *netcam;

    indx_cam = 1;
    if (cam->netcam_high) {
        indx_max = 2;
    } else {
        indx_max = 1;
    }

    while (indx_cam <= indx_max) {
        if (indx_cam == 1) {
            netcam = cam->netcam;
        } else {
            netcam = cam->netcam_high;
        }

        if (netcam) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Shutting down network camera.")
                ,netcam->cameratype.c_str());
            netcam->finish = true;
            netcam->interruptduration = 0;
            wait_counter = 0;
            while ((!netcam->handler_finished) && (wait_counter < 10)) {
                SLEEP(1,0);
                wait_counter++;
            }
            if (!netcam->handler_finished) {
                MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:No response from handler thread.")
                    ,netcam->cameratype.c_str());
                /* Last resort.  Kill the thread.*/
                pthread_cancel(netcam->thread_id);
                pthread_kill(netcam->thread_id, SIGVTALRM);
                pthread_mutex_lock(&netcam->motapp->global_lock);
                    netcam->motapp->threads_running--;
                pthread_mutex_unlock(&netcam->motapp->global_lock);
            }
            netcam_shutdown(netcam);

            pthread_mutex_destroy(&netcam->mutex);
            pthread_mutex_destroy(&netcam->mutex_pktarray);
            pthread_mutex_destroy(&netcam->mutex_transfer);

            delete netcam->params;
            delete netcam;

            if (indx_cam == 1) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("Norm: Shut down complete."));
            } else {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("High: Shut down complete."));
            }
        }
        indx_cam++;
    }
    cam->netcam = NULL;
    cam->netcam_high = NULL;
    cam->device_status = STATUS_CLOSED;
}

void netcam_start(ctx_dev *cam)
{
    int indx_cam, indx_max;
    ctx_netcam *netcam;

    MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Opening Netcam"));

    cam->netcam = NULL;
    cam->netcam_high = NULL;

    netcam_set_dimensions(cam);

    indx_cam = 1;
    if (cam->conf->netcam_high_url != "") {
        indx_max = 2;
    } else {
        indx_max = 1;
    }

    while (indx_cam <= indx_max) {
        if (indx_cam == 1) {
            cam->netcam = new ctx_netcam;
            netcam = cam->netcam;
            netcam->high_resolution = false;
        } else {
            cam->netcam_high = new ctx_netcam;
            netcam = cam->netcam_high;
            netcam->high_resolution = true;
        }
        netcam_set_parms(cam, netcam);
        if (netcam_connect(netcam) != 0) {
            netcam_cleanup(cam);
            return;
        }
        if (netcam_read_image(netcam) != 0) {
            MOTPLS_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Failed trying to read first image"));
            netcam->status = NETCAM_NOTCONNECTED;
            netcam_cleanup(cam);
            return;
        }
        /* When running dual, there seems to be contamination across norm/high with codec functions. */
        netcam_close_context(netcam);       /* Close in this thread to open it again within handler thread */
        netcam->status = NETCAM_RECONNECTING;      /* Set as reconnecting to avoid excess messages when starting */
        netcam->first_image = false;             /* Set flag that we are not processing our first image */

        /* For normal resolution, we resize the image to the config parms so we do not need
         * to set the dimension parameters here (it is done in the set_parms).  For high res
         * we must get the dimensions from the first image captured
         */
        if (netcam->high_resolution) {
            cam->imgs.width_high = netcam->imgsize.width;
            cam->imgs.height_high = netcam->imgsize.height;
        }

        if (netcam_start_handler(netcam) != 0 ) {
            netcam_cleanup(cam);
            return;
        }

        indx_cam++;
    }

    cam->device_status = STATUS_OPENED;

    return;
}

/* netcam_next (Called from the motion loop thread) */
int netcam_next(ctx_dev *cam, ctx_image_data *img_data)
{
    if ((cam == NULL) || (cam->netcam == NULL)) {
        return CAPTURE_FAILURE;
    }

    if ((cam->netcam->status == NETCAM_RECONNECTING) ||
        (cam->netcam->status == NETCAM_NOTCONNECTED)) {
        return CAPTURE_ATTEMPTED;
    }
    pthread_mutex_lock(&cam->netcam->mutex);
        netcam_pktarray_resize(cam, false);
        memcpy(img_data->image_norm
            , cam->netcam->img_latest->ptr
            , cam->netcam->img_latest->used);
        img_data->idnbr_norm = cam->netcam->idnbr;
    pthread_mutex_unlock(&cam->netcam->mutex);

    if (cam->netcam_high != NULL) {
        if ((cam->netcam_high->status == NETCAM_RECONNECTING) ||
            (cam->netcam_high->status == NETCAM_NOTCONNECTED)) {
            return CAPTURE_ATTEMPTED;
        }

        pthread_mutex_lock(&cam->netcam_high->mutex);
            netcam_pktarray_resize(cam, true);
            if (!cam->netcam_high->passthrough) {
                memcpy(img_data->image_high
                       ,cam->netcam_high->img_latest->ptr
                       ,cam->netcam_high->img_latest->used);
            }
            img_data->idnbr_high = cam->netcam_high->idnbr;
        pthread_mutex_unlock(&cam->netcam_high->mutex);
    }

    rotate_map(cam, img_data);

    return CAPTURE_SUCCESS;
}

