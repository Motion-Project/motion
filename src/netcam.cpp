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
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "rotate.hpp"
#include "netcam.hpp"
#include "movie.hpp"

static void *netcam_handler(void *arg)
{
    ((cls_netcam *)arg)->handler();
    return nullptr;
}

enum AVPixelFormat netcam_getfmt_vaapi(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    (void)avctx;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_VAAPI) {
            return *p;
        }
    }

    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get vaapi pix format"));
    return AV_PIX_FMT_NONE;
}

enum AVPixelFormat netcam_getfmt_cuda(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    (void)avctx;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_CUDA) return *p;
    }

    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get cuda pix format"));
    return AV_PIX_FMT_NONE;
}

enum AVPixelFormat netcam_getfmt_drm(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    (void)avctx;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_DRM_PRIME) return *p;
    }

    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get drm pix format"));
    return AV_PIX_FMT_NONE;
}

int netcam_interrupt(void *ctx)
{
    cls_netcam *netcam = (cls_netcam *)ctx;

    if (netcam->handler_stop) {
        netcam->interrupted = true;
        return true;
    }

    if (netcam->status == NETCAM_CONNECTED) {
        return false;
    } else if (netcam->status == NETCAM_READINGIMAGE) {
        clock_gettime(CLOCK_MONOTONIC, &netcam->icur_tm);
        if ((netcam->icur_tm.tv_sec -
             netcam->ist_tm.tv_sec ) > netcam->idur){
            if (netcam->cam->finish == false) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Camera reading (%s) timed out")
                    , netcam->cameratype.c_str(), netcam->camera_name.c_str());
            }
            netcam->interrupted = true;
            return true;
        } else{
            return false;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        clock_gettime(CLOCK_MONOTONIC, &netcam->icur_tm);
        if ((netcam->icur_tm.tv_sec - netcam->ist_tm.tv_sec ) > netcam->idur){
            if (netcam->cam->finish == false) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Camera (%s) timed out")
                    , netcam->cameratype.c_str(), netcam->camera_name.c_str());
            }
            netcam->interrupted = true;
            return true;
        } else{
            return false;
        }
    }

    /* should not be possible to get here */
    return false;
}

bool netcam_filelist_cmp(const ctx_filelist_item &a, const ctx_filelist_item &b)
{
    return a.filenm < b.filenm;
}

void cls_netcam::filelist_load()
{
    DIR             *d;
    struct dirent   *dir_ent;
    struct stat     sbuf;
    ctx_filelist_item fileitm;
    int             retcd;
    size_t          chkloc;

    filenbr++;
    if ((filenbr == (int)filelist.size()) ||
        (path == "")) {
        filelist.clear();
        retcd = stat(filedir.c_str(), &sbuf);
        if ((sbuf.st_mode & S_IFREG ) && (retcd == 0)) {
            MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                , _("File specified: %s"),filedir.c_str());
            fileitm.fullnm = filedir;
            chkloc = fileitm.fullnm.find_last_of("/");
            if (chkloc == std::string::npos) {
                fileitm.filenm = fileitm.fullnm;
            } else {
                fileitm.filenm = fileitm.fullnm.substr(chkloc+1);
            }
            chkloc = fileitm.filenm.find_last_of(".");
            if (chkloc == std::string::npos) {
                fileitm.displaynm = fileitm.filenm;
            } else {
                fileitm.displaynm = fileitm.filenm.substr(0,chkloc);
            }
            filelist.push_back(fileitm);
        } else if ((sbuf.st_mode & S_IFDIR ) && (retcd == 0)) {
            MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO
                , _("Directory specified: %s"),filedir.c_str());
            d = opendir(filedir.c_str());
            if (d != NULL) {
                while ((dir_ent=readdir(d)) != NULL) {
                    fileitm.fullnm = filedir;
                    fileitm.fullnm += dir_ent->d_name;
                    fileitm.filenm = dir_ent->d_name;
                    chkloc = fileitm.filenm.find_last_of(".");
                    if (chkloc == std::string::npos) {
                        fileitm.displaynm = fileitm.filenm;
                    } else {
                        fileitm.displaynm = fileitm.filenm.substr(0,chkloc);
                    }
                    retcd = stat(fileitm.fullnm.c_str(), &sbuf);
                    if ((sbuf.st_mode & S_IFREG) && (retcd == 0)) {
                        filelist.push_back(fileitm);
                    }
                }
            } else {
                MOTPLS_LOG(DBG, TYPE_NETCAM, SHOW_ERRNO
                    , _("Directory did not open: %s"),filedir.c_str());
            }
            closedir(d);
            std::sort(filelist.begin()
                , filelist.end()
                , netcam_filelist_cmp);
        }
        filenbr = 0;
    }
    if (filelist.size() == 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("Directory/file not found: %s"), filedir.c_str());
    } else {
        path = filelist[(uint)filenbr].fullnm;
    }
    MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO
            , _("Netcam Path: %s"),path.c_str());

}

void cls_netcam::check_buffsize(netcam_buff_ptr buff, size_t numbytes)
{
    int min_size_to_alloc;
    int real_alloc;
    uint new_size;

    if ((buff->size - buff->used) >= numbytes) {
        return;
    }

    min_size_to_alloc = (int)(numbytes - (buff->size - buff->used));
    real_alloc = ((min_size_to_alloc / NETCAM_BUFFSIZE) * NETCAM_BUFFSIZE);

    if ((min_size_to_alloc - real_alloc) > 0) {
        real_alloc += NETCAM_BUFFSIZE;
    }

    new_size = (uint)buff->size + (uint)real_alloc;

    buff->ptr =(char*) myrealloc(buff->ptr, new_size,"check_buf_size");
    buff->size = new_size;
}

char *cls_netcam::url_match(regmatch_t m, const char *input)
{
    char *match = NULL;
    int len;

    if (m.rm_so != -1) {
        len = m.rm_eo - m.rm_so;
        if (len > 0) {
            if ((match =(char*) mymalloc(uint(len + 1))) != NULL) {
                strncpy(match, input + m.rm_so, (uint)len);
                match[len] = '\0';
            }
        }
    }

    return match;
}

void cls_netcam::url_invalid(ctx_url *parse_url)
{
    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Invalid URL.  Can not parse values."));

    parse_url->port = 0;
    parse_url->host = "????";
    parse_url->service = "????";
    parse_url->path = "INVALID";
    parse_url->userpass = "INVALID";

}

void cls_netcam::url_parse(ctx_url *parse_url, std::string text_url)
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
        url_invalid(parse_url);
        return;
    }

    retcd = regexec(&pattbuf, text_url.c_str(), 10, matches, 0);
    if (retcd == REG_NOMATCH) {
        regfree(&pattbuf);
        url_invalid(parse_url);
        return;
    }

    for (i = 0; i < 10; i++) {
        if ((s = url_match(matches[i], text_url.c_str())) != NULL) {
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

void cls_netcam::free_pkt()
{
    av_packet_free(&packet_recv);
    packet_recv = nullptr;
}

int cls_netcam::check_pixfmt()
{
    int retcd = -1;
    if (((enum AVPixelFormat)frame->format == AV_PIX_FMT_YUV420P) ||
        ((enum AVPixelFormat)frame->format == AV_PIX_FMT_YUVJ420P)) {
        retcd = 0;
    }
    return retcd;
}

void cls_netcam::pktarray_free()
{
    int indx;
    pthread_mutex_lock(&mutex_pktarray);
        if (pktarray_size > 0) {
            for(indx = 0; indx < pktarray_size; indx++) {
                av_packet_free(&pktarray[indx].packet);
                pktarray[indx].packet = NULL;
            }
        }
        myfree(pktarray);
        pktarray_size = 0;
        pktarray_index = -1;
    pthread_mutex_unlock(&mutex_pktarray);
}

void cls_netcam::context_null()
{
    swsctx          = nullptr;
    swsframe_in     = nullptr;
    swsframe_out    = nullptr;
    frame           = nullptr;
    codec_context   = nullptr;
    format_context  = nullptr;
    transfer_format = nullptr;
    hw_device_ctx   = nullptr;
}

void cls_netcam::context_close()
{
    if (swsctx          != nullptr) sws_freeContext(swsctx);
    if (swsframe_in     != nullptr) av_frame_free(&swsframe_in);
    if (swsframe_out    != nullptr) av_frame_free(&swsframe_out);
    if (frame           != nullptr) av_frame_free(&frame);
    if (pktarray        != nullptr) pktarray_free();
    if (codec_context   != nullptr) avcodec_free_context(&codec_context);
    if (format_context  != nullptr) avformat_close_input(&format_context);
    if (transfer_format != nullptr) avformat_close_input(&transfer_format);
    if (hw_device_ctx   != nullptr) av_buffer_unref(&hw_device_ctx);
    context_null();
}

void cls_netcam::pktarray_resize()
{
    /* This is called from next and is on the motion loop thread
     * The mutex is locked around the call to this function.
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
    ctx_packet_item *tmp;
    int             newsize;

    if (high_resolution) {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_high;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_high;
    } else {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_norm;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_norm;
    }

    if (!passthrough) {
        return;
    }

    /* The 30 is arbitrary */
    /* Double the size plus double last diff so we don't catch our tail */
    newsize =(int)(((idnbr_first - idnbr_last) * 1 ) +
        ((idnbr - idnbr_last ) * 2));
    if (newsize < 30) {
        newsize = 30;
    }

    pthread_mutex_lock(&mutex_pktarray);
        if ((pktarray_size < newsize) ||  (pktarray_size < 30)) {
            tmp =(ctx_packet_item*) mymalloc((uint)newsize * sizeof(ctx_packet_item));
            if (pktarray_size > 0 ) {
                memcpy(tmp, pktarray, sizeof(ctx_packet_item) * (uint)pktarray_size);
            }
            for(indx = pktarray_size; indx < newsize; indx++) {
                tmp[indx].packet = nullptr;
                tmp[indx].packet = mypacket_alloc(tmp[indx].packet);
                tmp[indx].idnbr = 0;
                tmp[indx].iskey = false;
                tmp[indx].iswritten = false;
            }

            myfree(pktarray);
            pktarray = tmp;
            pktarray_size = newsize;

            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                , _("%s:Resized packet array to %d")
                , cameratype.c_str(), newsize);
        }
    pthread_mutex_unlock(&mutex_pktarray);
}

void cls_netcam::pktarray_add()
{
    int indx_next;
    int retcd;
    char errstr[128];

    pthread_mutex_lock(&mutex_pktarray);

        if (pktarray_size == 0) {
            pthread_mutex_unlock(&mutex_pktarray);
            return;
        }

        /* Recall pktarray_size is one based but pktarray is zero based */
        if (pktarray_index == (pktarray_size-1)) {
            indx_next = 0;
        } else {
            indx_next = pktarray_index + 1;
        }

        pktarray[indx_next].idnbr = idnbr;

        av_packet_free(&pktarray[indx_next].packet);
        pktarray[indx_next].packet = nullptr;
        pktarray[indx_next].packet = mypacket_alloc(pktarray[indx_next].packet);

        retcd = av_packet_ref(pktarray[indx_next].packet, packet_recv);
        if ((interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:av_copy_packet:%s ,Interrupt:%s")
                ,cameratype.c_str()
                ,errstr, interrupted ? _("true"):_("false"));
            av_packet_free(&pktarray[indx_next].packet);
            pktarray[indx_next].packet = nullptr;
        }

        if (pktarray[indx_next].packet->flags & AV_PKT_FLAG_KEY) {
            pktarray[indx_next].iskey = true;
        } else {
            pktarray[indx_next].iskey = false;
        }
        pktarray[indx_next].iswritten = false;

        pktarray_index = indx_next;
    pthread_mutex_unlock(&mutex_pktarray);
}

int cls_netcam::decode_sw()
{
    int retcd;
    char errstr[128];

    retcd = avcodec_receive_frame(codec_context, frame);
    if ((interrupted) || (handler_stop) || (retcd < 0)) {
        if (retcd == AVERROR(EAGAIN)) {
            retcd = 0;
        } else if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Ignoring packet with invalid data")
                ,cameratype.c_str());
            retcd = 0;
        } else if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Rec frame error:%s")
                    ,cameratype.c_str(), errstr);
            retcd = -1;
        } else {
            retcd = -1;
        }
        return retcd;
    }

    return 1;
}

int cls_netcam::decode_vaapi()
{
    int retcd;
    char errstr[128];
    AVFrame *hw_frame = nullptr;

    hw_frame = av_frame_alloc();

    retcd = av_hwframe_get_buffer(codec_context->hw_frames_ctx, hw_frame, 0);
    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("%s:Error getting hw frame buffer")
            , cameratype.c_str());
        av_frame_free(&hw_frame);
        return -1;
    }

    retcd = avcodec_receive_frame(codec_context, hw_frame);
    if ((interrupted) || (handler_stop) || (retcd < 0)) {
        if (retcd == AVERROR(EAGAIN)) {
            retcd = 0;
        } else if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Ignoring packet with invalid data")
                ,cameratype.c_str());
            retcd = 0;
        } else if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Rec frame error:%s")
                    ,cameratype.c_str(), errstr);
            retcd = -1;
        } else {
            retcd = -1;
        }
        av_frame_free(&hw_frame);
        return retcd;
    }

    retcd = av_hwframe_transfer_data(frame, hw_frame, 0);
    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error transferring HW decoded to system memory")
            ,cameratype.c_str());
        av_frame_free(&hw_frame);
        return -1;
    }
    av_frame_free(&hw_frame);

    return 1;
}

int cls_netcam::decode_cuda()
{
    int retcd;
    char errstr[128];
    AVFrame *hw_frame = nullptr;

    hw_frame = av_frame_alloc();

    retcd = avcodec_receive_frame(codec_context, hw_frame);
    if ((interrupted) || (handler_stop) || (retcd < 0) ){
        if (retcd == AVERROR(EAGAIN)){
            retcd = 0;
        } else if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Ignoring packet with invalid data")
                ,cameratype.c_str());
            retcd = 0;
        } else if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Rec frame error:%s")
                    ,cameratype.c_str(), errstr);
            retcd = -1;
        } else {
            retcd = -1;
        }
        av_frame_free(&hw_frame);
        return retcd;
    }
    frame->format=AV_PIX_FMT_NV12;

    retcd = av_hwframe_transfer_data(frame, hw_frame, 0);
    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error transferring HW decoded to system memory")
            ,cameratype.c_str());
        av_frame_free(&hw_frame);
        return -1;
    }

    av_frame_free(&hw_frame);

    return 1;
}

int cls_netcam::decode_drm()
{
    int retcd;
    char errstr[128];
    AVFrame *hw_frame = nullptr;

    hw_frame = av_frame_alloc();

    retcd = avcodec_receive_frame(codec_context, hw_frame);
    if ((interrupted) || (handler_stop) || (retcd < 0) ){
        if (retcd == AVERROR(EAGAIN)){
            retcd = 0;
        } else if (retcd == AVERROR_INVALIDDATA) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Ignoring packet with invalid data")
                ,cameratype.c_str());
            retcd = 0;
        } else if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Rec frame error:%s")
                    ,cameratype.c_str(), errstr);
            retcd = -1;
        } else {
            retcd = -1;
        }
        av_frame_free(&hw_frame);
        return retcd;
    }
    frame->format=AV_PIX_FMT_NV12;

    retcd = av_hwframe_transfer_data(frame, hw_frame, 0);
    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error transferring HW decoded to system memory")
            ,cameratype.c_str());
        av_frame_free(&hw_frame);
        return -1;
    }

    av_frame_free(&hw_frame);

    return 1;
}

int cls_netcam::decode_video()
{
    int retcd;
    char errstr[128];

    /* The Invalid data problem comes frequently.  Usually at startup of rtsp cameras.
    * We now ignore those packets so this function would need to fail on a different error.
    * We should consider adding a maximum count of these errors and reset every time
    * we get a good image.
    */
    if (handler_stop) {
        return 0;
    }

    retcd = avcodec_send_packet(codec_context, packet_recv);
    if ((interrupted) || (handler_stop)) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Interrupted or handler_stop on send")
            ,cameratype.c_str());
        return -1;
    }
    if (retcd == AVERROR_INVALIDDATA) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Send ignoring packet with invalid data")
            ,cameratype.c_str());
        return 0;
    }
    if (retcd < 0 && retcd != AVERROR_EOF) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error sending packet to codec:%s")
            ,cameratype.c_str(), errstr);
        if (service == "file") {
            return 0;
        } else {
            return -1;
        }
    }

    if (hw_type == AV_HWDEVICE_TYPE_VAAPI) {
        retcd = decode_vaapi();
    } else if (hw_type == AV_HWDEVICE_TYPE_CUDA) {
        retcd = decode_cuda();
    } else if (hw_type == AV_HWDEVICE_TYPE_DRM) {
        retcd = decode_drm();
    } else {
        retcd = decode_sw();
    }

    return retcd;
}

int cls_netcam::decode_packet()
{
    int frame_size;
    int retcd;

    if (handler_stop) {
        return -1;
    }

    if (packet_recv->stream_index == audio_stream_index) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error decoding video packet...it is audio")
            ,cameratype.c_str());
    }

    retcd = decode_video();
    if (retcd <= 0) {
        return retcd;
    }
    frame_size = av_image_get_buffer_size(
        (enum AVPixelFormat) frame->format
        , frame->width, frame->height, 1);

    check_buffsize(img_recv, (uint)frame_size);
    check_buffsize(img_latest, (uint)frame_size);

    retcd = av_image_copy_to_buffer(
        (uint8_t *)img_recv->ptr
        , frame_size
        , (const uint8_t * const*)frame
        , frame->linesize
        , (enum AVPixelFormat)frame->format
        , frame->width, frame->height, 1);
    if ((retcd < 0) || (interrupted)) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error decoding video packet:Copying to buffer")
            ,cameratype.c_str());
        return -1;
    }

    img_recv->used = (uint)frame_size;

    return frame_size;
}

void cls_netcam::hwdecoders()
{
    /* High Res pass through does not decode images into frames*/
    if (high_resolution && passthrough) {
        return;
    }
    if ((hw_type == AV_HWDEVICE_TYPE_NONE) && (first_image)) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:HW Devices:")
            , cameratype.c_str());
        while((hw_type = av_hwdevice_iterate_types(hw_type)) != AV_HWDEVICE_TYPE_NONE){
            if ((hw_type == AV_HWDEVICE_TYPE_VAAPI) ||
                (hw_type == AV_HWDEVICE_TYPE_CUDA)  ||
                (hw_type == AV_HWDEVICE_TYPE_DRM)) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: %s(available)")
                    , cameratype.c_str()
                    , av_hwdevice_get_type_name(hw_type));
            } else {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: %s(not implemented)")
                    , cameratype.c_str()
                    , av_hwdevice_get_type_name(hw_type));
            }
        }
    }
    return;
}

void cls_netcam::decoder_error(int retcd, const char* fnc_nm)
{
    char errstr[128];
    int indx;

    if (interrupted) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Interrupted"),cameratype.c_str());
    } else {
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:%s:%s"),cameratype.c_str()
                ,fnc_nm, errstr);
        } else {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:%s:Failed"), cameratype.c_str()
                ,fnc_nm);
        }
    }

    if (decoder_nm != "NULL") {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Decoder %s did not work.")
            ,cameratype.c_str(), decoder_nm.c_str());
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Ignoring and removing the user requested decoder %s")
            ,cameratype.c_str(), decoder_nm.c_str());

        for (indx=0;indx<params->params_cnt;indx++) {
            if (params->params_array[indx].param_name == "decoder") {
                params->params_array[indx].param_value = "NULL";
                break;
            }
        }

        util_parms_update(params, cfg_params);

        decoder_nm = "NULL";
    }
}

int cls_netcam::init_vaapi()
{
    int retcd, indx;
    AVPixelFormat *pixelformats = nullptr;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Initializing vaapi decoder")
        ,cameratype.c_str());

    hw_type = av_hwdevice_find_type_by_name("vaapi");
    if (hw_type == AV_HWDEVICE_TYPE_NONE) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find vaapi hw device")
            , cameratype.c_str());
        decoder_error(0, "av_hwdevice");
        return -1;
    }

    codec_context = avcodec_alloc_context3(decoder);
    if ((codec_context == nullptr) || (interrupted)) {
        decoder_error(0, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_context,strm->codecpar);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "avcodec_parameters_to_context");
        return -1;
    }

    hw_pix_fmt = AV_PIX_FMT_VAAPI;
    codec_context->get_format  = netcam_getfmt_vaapi;
    av_opt_set_int(codec_context, "refcounted_frames", 1, 0);
    codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->pix_fmt= AV_PIX_FMT_YUV420P;
    codec_context->hwaccel_flags=
        AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
        AV_HWACCEL_FLAG_IGNORE_LEVEL;

    retcd = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, NULL, NULL, 0);
    if (retcd < 0) {
        decoder_error(retcd, "hwctx");
        return -1;
    }

    codec_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    AVBufferRef *hw_frames_ref = nullptr;
    AVHWFramesContext *frames_ctx = nullptr;

    hw_frames_ref = av_hwframe_ctx_alloc(
        codec_context->hw_device_ctx);
    if (hw_frames_ref == nullptr) {
        decoder_error(retcd, "initvaapi 2");
        return -1;
    }

    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
    frames_ctx->width     = codec_context->width;
    frames_ctx->height    = codec_context->height;
    frames_ctx->initial_pool_size = 20;
    retcd = av_hwframe_ctx_init(hw_frames_ref);
    if (retcd < 0) {
        decoder_error(retcd, "initvaapi 2");
        av_buffer_unref(&hw_frames_ref);
        return -1;
    }
    codec_context->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (codec_context->hw_frames_ctx == nullptr) {
        decoder_error(retcd, "initvaapi 2");
        av_buffer_unref(&hw_frames_ref);
        return -1;
    }
    av_buffer_unref(&hw_frames_ref);

    retcd = av_hwframe_transfer_get_formats(
        codec_context->hw_frames_ctx
        , AV_HWFRAME_TRANSFER_DIRECTION_FROM
        , &pixelformats
        , 0);
    if (retcd < 0) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Error enumerating HW pixel types")
            ,cameratype.c_str());
        decoder_error(retcd, "initvaapi 3");
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
            , cameratype.c_str()
            ,descr->name);
    }
    av_freep(&pixelformats);

    return 0;
}

int cls_netcam::init_cuda()
{
    int retcd;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Initializing cuda decoder"),cameratype.c_str());

    hw_type = av_hwdevice_find_type_by_name("cuda");
    if (hw_type == AV_HWDEVICE_TYPE_NONE){
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find cuda hw device")
            , cameratype.c_str());
        decoder_error(0, "av_hwdevice");
        return -1;
    }

    codec_context = avcodec_alloc_context3(decoder);
    if ((codec_context == nullptr) || (interrupted)){
        decoder_error(0, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_context,strm->codecpar);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "avcodec_parameters_to_context");
        return -1;
    }

    hw_pix_fmt = AV_PIX_FMT_CUDA;
    codec_context->get_format  = netcam_getfmt_cuda;
    av_opt_set_int(codec_context, "refcounted_frames", 1, 0);
    codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->hwaccel_flags=
        AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
        AV_HWACCEL_FLAG_IGNORE_LEVEL;

    retcd = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, NULL, NULL, 0);
    if (retcd < 0){
        decoder_error(retcd, "hwctx");
        return -1;
    }
    codec_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return 0;
}

int cls_netcam::init_drm()
{
    int retcd;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Initializing drm decoder")
        ,cameratype.c_str());

    hw_type = av_hwdevice_find_type_by_name("drm");
    if (hw_type == AV_HWDEVICE_TYPE_NONE){
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s:Unable to find drm hw device")
            , cameratype.c_str());
        decoder_error(0, "av_hwdevice");
        return -1;
    }

    codec_context = avcodec_alloc_context3(decoder);
    if ((codec_context == nullptr) || (interrupted)){
        decoder_error(0, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_context,strm->codecpar);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "avcodec_parameters_to_context");
        return -1;
    }

    hw_pix_fmt = AV_PIX_FMT_DRM_PRIME;
    codec_context->get_format  = netcam_getfmt_drm;
    av_opt_set_int(codec_context, "refcounted_frames", 1, 0);
    codec_context->sw_pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->hwaccel_flags=
        AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH |
        AV_HWACCEL_FLAG_IGNORE_LEVEL;

    retcd = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, NULL, NULL, 0);
    if (retcd < 0){
        decoder_error(retcd, "hwctx");
        return -1;
    }
    codec_context->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return 0;
}

int cls_netcam::init_swdecoder()
{
    int retcd;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Initializing decoder"),cameratype.c_str());

    if (decoder_nm != "NULL") {
        decoder = avcodec_find_decoder_by_name(
            decoder_nm.c_str());
        if (decoder == nullptr) {
            decoder_error(0
                , "avcodec_find_decoder_by_name");
        } else {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Using decoder %s")
                , cameratype.c_str()
                , decoder_nm.c_str());
        }
    }
    if (decoder == nullptr) {
        decoder = avcodec_find_decoder(strm->codecpar->codec_id);
    }
    if ((decoder == nullptr) || (interrupted)) {
        decoder_error(0, "avcodec_find_decoder");
        return -1;
    }

    codec_context = avcodec_alloc_context3(decoder);
    if ((codec_context == nullptr) || (interrupted)) {
        decoder_error(0, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_parameters_to_context(codec_context, strm->codecpar);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "avcodec_parameters_to_context");
        return -1;
    }

    codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    codec_context->err_recognition = AV_EF_IGNORE_ERR;

    return 0;
}

int cls_netcam::open_codec()
{
    int retcd;

    if (handler_stop) {
        return -1;
    }

    hwdecoders();

    retcd = av_find_best_stream(format_context
                , AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if ((retcd < 0) || (interrupted)) {
        audio_stream_index = -1;
    } else {
        audio_stream_index = retcd;
    }

    decoder = nullptr;
    retcd = av_find_best_stream(format_context
                , AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "av_find_best_stream");
        return -1;
    }
    video_stream_index = retcd;
    strm = format_context->streams[video_stream_index];

    if (decoder_nm == "vaapi") {
        if (init_vaapi() < 0) {
            decoder_error(retcd, "hwvaapi_init");
            return -1;
        }
    } else if (decoder_nm == "cuda"){
        if (init_cuda() <0 ) {
            decoder_error(retcd, "hwcuda_init");
            return -1;
        }
    } else if (decoder_nm == "drm"){
        if (init_drm() < 0) {;
            decoder_error(retcd, "hwdrm_init");
            return -1;
        }
    } else {
        init_swdecoder();
    }

    retcd = avcodec_open2(codec_context, decoder, nullptr);
    if ((retcd < 0) || (interrupted)) {
        decoder_error(retcd, "avcodec_open2");
        return -1;
    }

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Decoder opened"),cameratype.c_str());

    return 0;
}

int cls_netcam::open_sws()
{
    if (handler_stop) {
        return -1;
    }

    swsframe_in = av_frame_alloc();
    if (swsframe_in == nullptr) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate swsframe_in.")
                , cameratype.c_str());
        }
        context_close();
        return -1;
    }

    swsframe_out = av_frame_alloc();
    if (swsframe_out == nullptr) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate swsframe_out.")
                , cameratype.c_str());
        }
        context_close();
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    if (check_pixfmt() != 0) {
        const AVPixFmtDescriptor *descr;
        descr = av_pix_fmt_desc_get((AVPixelFormat)frame->format);
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("%s:Pixel format %s will be converted.")
            , cameratype.c_str(), descr->name);
    }

    swsctx = sws_getContext(
         frame->width
        ,frame->height
        ,(enum AVPixelFormat)frame->format
        ,imgsize.width
        ,imgsize.height
        ,AV_PIX_FMT_YUV420P
        ,SWS_BICUBIC,nullptr,nullptr,nullptr);
    if (swsctx == nullptr) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to allocate scaling context.")
                , cameratype.c_str());
        }
        context_close();
        return -1;
    }

    swsframe_size = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P, imgsize.width, imgsize.height, 1);
    if (swsframe_size <= 0) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s:Error determining size of frame out")
                , cameratype.c_str());
        }
        context_close();
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    check_buffsize(img_recv, (uint)swsframe_size);
    check_buffsize(img_latest, (uint)swsframe_size);

    return 0;
}

int cls_netcam::resize()
{
    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    if (handler_stop) {
        return -1;
    }

    if (swsctx == nullptr) {
        if (open_sws() < 0) {
            return -1;
        }
    }
    retcd = av_image_fill_arrays(
        swsframe_in->data
        , swsframe_in->linesize
        , (uint8_t*)img_recv->ptr
        , (enum AVPixelFormat)frame->format
        , frame->width, frame->height, 1);
    if (retcd < 0) {
        if (status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error allocating picture in:%s")
                , cameratype.c_str(), errstr);
        }
        context_close();
        return -1;
    }

    buffer_out=(uint8_t *)av_malloc((uint)swsframe_size*sizeof(uint8_t));

    retcd = av_image_fill_arrays(
        swsframe_out->data
        , swsframe_out->linesize
        , buffer_out, AV_PIX_FMT_YUV420P
        , imgsize.width, imgsize.height, 1);
    if (retcd < 0) {
        if (status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error allocating picture out:%s")
                , cameratype.c_str(), errstr);
        }
        context_close();
        return -1;
    }

    retcd = sws_scale(
        swsctx
        ,(const uint8_t* const *)swsframe_in->data
        ,swsframe_in->linesize
        ,0
        ,frame->height
        ,swsframe_out->data
        ,swsframe_out->linesize);
    if (retcd < 0) {
        if (status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error resizing/reformatting:%s")
                , cameratype.c_str(), errstr);
        }
        context_close();
        return -1;
    }

    retcd = av_image_copy_to_buffer(
        (uint8_t *)img_recv->ptr
        , swsframe_size
        , (const uint8_t * const*)swsframe_out
        , swsframe_out->linesize
        , AV_PIX_FMT_YUV420P
        , imgsize.width, imgsize.height, 1);
    if (retcd < 0) {
        if (status == NETCAM_NOTCONNECTED) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Error putting frame into output buffer:%s")
                , cameratype.c_str(), errstr);
        }
        context_close();
        return -1;
    }
    img_recv->used = (uint)swsframe_size;

    av_free(buffer_out);

    return 0;
}

int cls_netcam::read_image()
{
    int  size_decoded, retcd, errcnt, nodata;
    bool haveimage;
    char errstr[128];
    netcam_buff *xchg;

    if (handler_stop) {
        return -1;
    }

    packet_recv = mypacket_alloc(packet_recv);

    interrupted=false;
    idur = cfg_idur;

    status = NETCAM_READINGIMAGE;
    img_recv->used = 0;
    size_decoded = 0;
    errcnt = 0;
    haveimage = false;
    nodata = 0;

    while ((!haveimage) && (!interrupted)) {
        clock_gettime(CLOCK_MONOTONIC, &ist_tm);
        retcd = av_read_frame(format_context, packet_recv);
        if (retcd < 0 ) {
            errcnt++;
        }
        if ((interrupted) || (errcnt > 1)) {
            if (interrupted) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Interrupted")
                    ,cameratype.c_str());
            } else {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:av_read_frame:%s" )
                    ,cameratype.c_str(),errstr);
            }
            free_pkt();
            context_close();
            return -1;
        } else {
            errcnt = 0;
            if ((packet_recv->stream_index == video_stream_index) ||
                (packet_recv->stream_index == audio_stream_index)) {
                /* For a high resolution pass-through we don't decode the image */
                if ((high_resolution && passthrough) ||
                    (packet_recv->stream_index != video_stream_index)) {
                    if (packet_recv->data != nullptr) {
                        size_decoded = 1;
                    }
                } else {
                    size_decoded = decode_packet();
                }
            }
            if (size_decoded > 0) {
                haveimage = true;
            } else if (size_decoded == 0) {
                /* Did not fail, just didn't get anything.  Try again */
                free_pkt();
                packet_recv = mypacket_alloc(packet_recv);

                /* The 1000 is arbitrary */
                nodata++;
                if (nodata > 1000) {
                    context_close();
                    return -1;
                }

            } else {
                free_pkt();
                context_close();
                return -1;
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    clock_gettime(CLOCK_MONOTONIC, &img_recv->image_time);
    last_stream_index = packet_recv->stream_index;
    last_pts = packet_recv->pts;

    if (!first_image) {
        status = NETCAM_CONNECTED;
    }

    /* Skip resize/pix format for high pass-through */
    if (!(high_resolution && passthrough) &&
        (packet_recv->stream_index == video_stream_index)) {

        if ((imgsize.width  != frame->width) ||
            (imgsize.height != frame->height) ||
            (check_pixfmt() != 0)) {
            if (resize() < 0) {
                free_pkt();
                context_close();
                return -1;
            }
        }
    }

    pthread_mutex_lock(&mutex);
        idnbr++;
        if (passthrough) {
            pktarray_add();
        }
        if (!(high_resolution && passthrough) &&
            (packet_recv->stream_index == video_stream_index)) {
            xchg = img_latest;
            img_latest = img_recv;
            img_recv = xchg;
        }
    pthread_mutex_unlock(&mutex);

    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    free_pkt();

    if (format_context->streams[video_stream_index]->avg_frame_rate.den > 0) {
        src_fps = (int)(
            (format_context->streams[video_stream_index]->avg_frame_rate.num /
            format_context->streams[video_stream_index]->avg_frame_rate.den) +
            0.5);
        if (capture_rate < 1) {
            capture_rate = src_fps + 1;
            if (pts_adj == false) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:capture_rate not specified in params. Using %d")
                    ,cameratype.c_str(),capture_rate);
            }
        }
    } else {
        if (capture_rate < 1) {
            capture_rate = cfg_framerate;
            if (pts_adj == false) {
               MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:capture_rate not specified.")
                    ,cameratype.c_str());
               MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Using framerate %d")
                    ,cameratype.c_str(), capture_rate);
            }
        }
    }

    return 0;
}

int cls_netcam::ntc()
{
    if ((handler_stop) || (!first_image)) {
        return 0;
    }

    /* High Res pass through does not decode images into frames*/
    if (high_resolution && passthrough) {
        return 0;
    }

    if ((imgsize.width == 0) || (imgsize.height == 0) ||
        (frame->width == 0) || (frame->height == 0)){
            return 0;
    }

    if ((imgsize.width  != frame->width) ||
        (imgsize.height != frame->height)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures at %dx%d")
            , frame->width,frame->height);
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("resolution but config is %dx%d. If possible change")
            , imgsize.width,imgsize.height);
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("the netcam or config so that the image height and"));
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("width are the same to lower the CPU usage."));
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
    }

    return 0;
}

void cls_netcam::set_options()
{
    std::string tmp;
    int indx;
    ctx_params_item *itm;

    if ((service == "rtsp") ||
        (service == "rtsps") ||
        (service == "rtmp")) {
        util_parms_add_default(params,"rtsp_transport","tcp");
        util_parms_add_default(params,"input_format","");

    } else if ((service == "http") ||
               (service == "https")) {
        util_parms_add_default(params,"input_format","mjpeg");
        util_parms_add_default(params,"reconnect_on_network_error","1");
        util_parms_add_default(params,"reconnect_at_eof","1");
        util_parms_add_default(params,"reconnect","1");
        util_parms_add_default(params,"multiple_requests","1");
        util_parms_add_default(params,"reconnect_streamed","1");

    } else if (service == "v4l2") {
        util_parms_add_default(params,"input_format","video4linux2");

        tmp = std::to_string(cfg_framerate);
        util_parms_add_default(params,"framerate", tmp);

        tmp = std::to_string(cfg_width) + "x" +
            std::to_string(cfg_height);
        util_parms_add_default(params,"video_size", tmp);

    } else if (service == "file") {
        util_parms_add_default(params,"input_format","");

    } else {
        util_parms_add_default(params,"input_format","");
    }

    for (indx=0;indx<params->params_cnt;indx++) {
        itm = &params->params_array[indx];
        if ((itm->param_name != "decoder") &&
            (itm->param_name != "capture_rate") &&
            (itm->param_name != "interrupt") &&
            (itm->param_name != "input_format")) {
            av_dict_set(&opts
                , itm->param_name.c_str(), itm->param_value.c_str(), 0);
            if (status == NETCAM_NOTCONNECTED) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s:%s = %s")
                    ,cameratype.c_str()
                    ,itm->param_name.c_str(),itm->param_value.c_str());
            }
        } else if ((itm->param_name == "input_format") &&
            (itm->param_value != "")) {
            format_context->iformat = av_find_input_format(itm->param_value.c_str());
            if (status == NETCAM_NOTCONNECTED) {
                MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s:%s = %s")
                    ,cameratype.c_str()
                    ,itm->param_name.c_str(),itm->param_value.c_str());
            }

        }
    }
}

void cls_netcam::set_path ()
{
    ctx_url url;

    path = "";

    if (high_resolution) {
        url_parse(&url, cam->cfg->netcam_high_url);
    } else {
        url_parse(&url, cam->cfg->netcam_url);
    }

    if (cam->cfg->netcam_userpass != "") {
        url.userpass = cam->cfg->netcam_userpass;
    }

    if  (url.service == "v4l2") {
        path = url.path;
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up v4l2"));
    } else if (url.service == "file") {
        filedir = url.path;
        filelist_load();
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up file"));
    } else {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up %s "),url.service.c_str());
        if (url.userpass.length() > 0) {
            path = url.service + "://" +
                url.userpass + "@" + url.host +":"+
                std::to_string(url.port) + url.path;
        } else {
            path = url.service + "://" +
                url.host + ":" + std::to_string(url.port) + url.path;
        }
    }

    service = url.service;
}

void cls_netcam::set_parms ()
{
    int indx;

    params = new ctx_params;

    context_null();
    threadnbr = cam->cfg->device_id;
    cfg_width = cam->cfg->width;
    cfg_height = cam->cfg->height;
    cfg_framerate = cam->cfg->framerate;

    cam->imgs.width = cfg_width;
    cam->imgs.height = cfg_height;
    cam->imgs.size_norm = (cfg_width * cfg_height * 3) / 2;
    cam->imgs.motionsize = cfg_width * cfg_height;

    cam->imgs.width_high  = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high   = 0;

    if (high_resolution) {
        imgsize.width = 0;
        imgsize.height = 0;
        cameratype = _("High");
        cfg_params = cam->cfg->netcam_high_params;
        util_parms_parse(params,"netcam_high_params", cfg_params);
    } else {
        imgsize.width = cfg_width;
        imgsize.height = cfg_height;
        cameratype = _("Norm");
        cfg_params = cam->cfg->netcam_params;
        util_parms_parse(params,"netcam_params", cfg_params);
    }
    camera_name = cam->cfg->device_name;

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        , _("%s:Setting up camera(%s).")
        , cameratype.c_str(), camera_name.c_str());

    status = NETCAM_NOTCONNECTED;
    util_parms_add_default(params,"decoder","NULL");
    img_recv =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    img_recv->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    img_latest =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    img_latest->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    pktarray_size = 0;
    pktarray_index = -1;
    pktarray = nullptr;
    packet_recv = nullptr;
    first_image = true;
    src_fps =  -1; /* Default to neg so we know it has not been set */
    pts_adj = false;
    capture_rate = -1;
    video_stream_index = -1;
    audio_stream_index = -1;
    last_stream_index = -1;
    strm = nullptr;
    opts = nullptr;
    decoder = nullptr;
    idnbr = 0;
    swsframe_size = 0;
    hw_type = AV_HWDEVICE_TYPE_NONE;
    hw_pix_fmt = AV_PIX_FMT_NONE;
    connection_pts = 0;
    last_pts = 0;
    filenbr = 0;
    filelist.clear();
    filedir = "";
    cfg_idur = 3;

    for (indx=0;indx<params->params_cnt;indx++) {
        if (params->params_array[indx].param_name == "decoder") {
            decoder_nm = params->params_array[indx].param_value;
        }
        if (params->params_array[indx].param_name == "capture_rate") {
            if (params->params_array[indx].param_value == "pts") {
                pts_adj = true;
            } else {
                capture_rate = mtoi(params->params_array[indx].param_value);
            }
        }
        if (params->params_array[indx].param_name == "interrupt") {
            cfg_idur = mtoi(params->params_array[indx].param_value);
        }
    }

    /* If this is the norm and we have a highres, then disable passthru on the norm */
    if ((high_resolution == false) &&
        (cam->cfg->netcam_high_url != "")) {
        passthrough = false;
    } else {
        passthrough = cam->movie_passthrough;
    }

    threadname = _("Unknown");

    clock_gettime(CLOCK_MONOTONIC, &frame_curr_tm);
    clock_gettime(CLOCK_MONOTONIC, &frame_prev_tm);
    clock_gettime(CLOCK_MONOTONIC, &connection_tm);
    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    clock_gettime(CLOCK_MONOTONIC, &icur_tm);

    interrupted = false;

    set_path();
}

/* Make a static copy of the stream information for use in passthrough processing */
int cls_netcam::copy_stream()
{
    AVStream  *transfer_stream, *stream_in;
    int        retcd, indx;

    pthread_mutex_lock(&mutex_transfer);
        if (transfer_format != nullptr) {
            avformat_close_input(&transfer_format);
        }
        transfer_format = avformat_alloc_context();
        for (indx = 0; indx < (int)format_context->nb_streams; indx++) {
            if ((audio_stream_index == indx) ||
                (video_stream_index == indx)) {
                transfer_stream = avformat_new_stream(transfer_format, nullptr);
                stream_in = format_context->streams[indx];
                retcd = avcodec_parameters_copy(transfer_stream->codecpar, stream_in->codecpar);
                if (retcd < 0) {
                    MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                        ,_("%s:Unable to copy codec parameters")
                        , cameratype.c_str());
                    pthread_mutex_unlock(&mutex_transfer);
                    return -1;
                }
                transfer_stream->time_base = stream_in->time_base;
                transfer_stream->avg_frame_rate = stream_in->avg_frame_rate;
            }
        }
    pthread_mutex_unlock(&mutex_transfer);

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
    return 0;
}

int cls_netcam::open_context()
{
    int  retcd;
    char errstr[128];

    if (handler_stop) {
        return -1;
    }

    if (path == "") {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("No path passed to connect"));
        }
        return -1;
    }

    opts = nullptr;
    format_context = avformat_alloc_context();
    format_context->interrupt_callback.callback = netcam_interrupt;
    format_context->interrupt_callback.opaque = this;
    interrupted = false;

    clock_gettime(CLOCK_MONOTONIC, &ist_tm);

    idur = cfg_idur * 3; /*3 is arbritrary multiplier to give connect more time than other steps*/

    set_options();

    MOTPLS_LOG(DBG, TYPE_NETCAM, NO_ERRNO, _("Opening camera"));
    retcd = avformat_open_input(&format_context
        , path.c_str(), nullptr, &opts);
    if ((retcd < 0) || (interrupted) || (handler_stop) ) {
        if (status == NETCAM_NOTCONNECTED) {
            if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Unable to open camera(%s):%s")
                    , cameratype.c_str()
                    , camera_name.c_str(), errstr);
            } else if (interrupted) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Unable to open camera(%s):timeout")
                    , cameratype.c_str()
                    , camera_name.c_str());
            }
        }
        av_dict_free(&opts);
        context_close();
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    av_dict_free(&opts);
    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Opened camera(%s)"), cameratype.c_str()
        , camera_name.c_str());


    /* fill out stream information */
    retcd = avformat_find_stream_info(format_context, nullptr);
    if ((retcd < 0) || (interrupted) || (handler_stop) ) {
        if (status == NETCAM_NOTCONNECTED) {
            if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Unable to find stream info:%s")
                    ,cameratype.c_str(), errstr);
            } else if (interrupted) {
                MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Unable to find stream info:%s")
                    ,cameratype.c_str());
            }
        }
        context_close();
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */

    mythreadname_get(threadname);
    mythreadname_set("av", threadnbr, camera_name.c_str());
        retcd = open_codec();
    mythreadname_set(nullptr, 0, threadname.c_str());
    if ((retcd < 0) || (interrupted) || (handler_stop) ) {
        av_strerror(retcd, errstr, sizeof(errstr));
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to open codec context:%s")
                ,cameratype.c_str(), errstr);
        } else {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Connected and unable to open codec context:%s")
                ,cameratype.c_str(), errstr);
        }
        context_close();
        return -1;
    }

    if (codec_context->width <= 0 ||
        codec_context->height <= 0) {
        MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Camera image size is invalid")
            ,cameratype.c_str());
        context_close();
        return -1;
    }

    if (high_resolution) {
        imgsize.width = codec_context->width;
        imgsize.height = codec_context->height;
    }

    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    frame = av_frame_alloc();
    if (frame == nullptr) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Unable to allocate frame.")
                ,cameratype.c_str());
        }
        context_close();
        return -1;
    }

    if (passthrough) {
        retcd = copy_stream();
        if ((retcd < 0) || (interrupted)) {
            if (status == NETCAM_NOTCONNECTED) {
                MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Failed to copy stream for pass-through.")
                    ,cameratype.c_str());
            }
            passthrough = false;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ist_tm);
    /* Validate that the previous steps opened the camera */
    retcd = read_image();
    if ((retcd < 0) || (interrupted)) {
        if (status == NETCAM_NOTCONNECTED) {
            MOTPLS_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Failed to read first image")
                ,cameratype.c_str());
        }
        context_close();
        return -1;
    }

    connection_pts = AV_NOPTS_VALUE;

    return 0;
}

int cls_netcam::connect()
{

    if (open_context() < 0) {
        return -1;
    }

    if (ntc() < 0 ) {
        return -1;
    }

    if (read_image() < 0) {
        return -1;
    }

    /* We use the status for determining whether to grab a image from
     * the Motion loop(see "next" function).  When we are initially starting,
     * we open and close the context and during this process we do not want the
     * Motion loop to start quite yet on this first image so we do
     * not set the status to connected
     */
    if (!first_image) {
        status = NETCAM_CONNECTED;

        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Camera (%s) connected")
            , cameratype.c_str(),camera_name.c_str());

        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("%s:Netcam capture FPS is %d.")
            , cameratype.c_str(), capture_rate);

        if (src_fps > 0) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Camera source is %d FPS")
                , cameratype.c_str(), src_fps);
        } else {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Unable to determine the camera source FPS.")
                , cameratype.c_str());
        }

        if (capture_rate < src_fps) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Capture FPS less than camera FPS. Decoding errors will occur.")
                , cameratype.c_str());
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s:Capture FPS should be greater than camera FPS.")
                , cameratype.c_str());
        }

        if (audio_stream_index != -1) {
            /* The following is not technically precise but we want to convey in simple terms
            * that the capture rate must go faster to account for the additional packets read
            * for the audio stream.  The technically correct process is that our wait timer in
            * the handler is only triggered when the last packet is a video stream
            */
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:An audio stream was detected.  Capture_rate increased to compensate.")
                ,cameratype.c_str());
        }
    }

    return 0;
}

void cls_netcam::handler_wait()
{
    int64_t usec_ltncy;
    AVRational tbase;
    struct timespec tmp_tm;

    clock_gettime(CLOCK_MONOTONIC, &tmp_tm);

    if (capture_rate < 1) {
        capture_rate = 1;
    }
    tbase = format_context->streams[video_stream_index]->time_base;
    if (tbase.num == 0) {
        tbase.num = 1;
    }

    /* FPS calculation from last frame capture */
    frame_curr_tm = tmp_tm;
    usec_ltncy = (1000000 / capture_rate) -
        ((frame_curr_tm.tv_sec - frame_prev_tm.tv_sec) * 1000000) -
        ((frame_curr_tm.tv_nsec - frame_prev_tm.tv_nsec)/1000);

    /* Adjust to clock and pts timer */
    if (pts_adj == true) {
        if (connection_pts == AV_NOPTS_VALUE) {
            connection_pts = last_pts;
            clock_gettime(CLOCK_MONOTONIC, &connection_tm);
            return;
        }
        if (last_pts != AV_NOPTS_VALUE) {
            usec_ltncy +=
                + (av_rescale(last_pts, 1000000, tbase.den/tbase.num)
                  - av_rescale(connection_pts, 1000000, tbase.den/tbase.num))
                -(((tmp_tm.tv_sec - connection_tm.tv_sec) * 1000000) +
                  ((tmp_tm.tv_nsec - connection_tm.tv_nsec) / 1000));
        }
    }

    if ((usec_ltncy > 0) && (usec_ltncy < 1000000L)) {
        SLEEP(0, usec_ltncy * 1000);
    }
}

void cls_netcam::handler_reconnect()
{
    int retcd, slp_dur;

    if (service == "file") {
        filelist_load();
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Processing file: %s")
            ,cameratype.c_str(), path.c_str());
    } else if ((status == NETCAM_CONNECTED) ||
        (status == NETCAM_READINGIMAGE)) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Reconnecting with camera....")
            ,cameratype.c_str());
    }
    status = NETCAM_RECONNECTING;

    if (passthrough == true) {
        cam->event_stop = true;
    }

    retcd = connect();
    if (retcd < 0) {
        if (reconnect_count < 100) {
            reconnect_count++;
        } else {
            if (reconnect_count >= 500) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Camera did not reconnect.")
                    , cameratype.c_str());
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Checking for camera every 2 hours.")
                    ,cameratype.c_str());
                slp_dur = 7200;
            } else if (reconnect_count >= 200) {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Camera did not reconnect.")
                    , cameratype.c_str());
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Checking for camera every 10 minutes.")
                    ,cameratype.c_str());
                reconnect_count++;
                slp_dur = 600;
            } else {
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Camera did not reconnect.")
                    , cameratype.c_str());
                MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("%s:Checking for camera every 10 seconds.")
                    ,cameratype.c_str());
                reconnect_count++;
                slp_dur = 10;
            }
            cam->watchdog = slp_dur + (cam->cfg->watchdog_tmo * 3);

            while ((cam->handler_stop == false) && (slp_dur > 0)) {
                SLEEP(1, 0);
                slp_dur--;
            }

            cam->watchdog = (cam->cfg->watchdog_tmo * 3);

        }
    } else {
        reconnect_count = 0;
    }
}

void cls_netcam::handler()
{
    handler_running = true;

    mythreadname_set("nc", threadnbr, camera_name.c_str());

    MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Camera handler started")
        ,cameratype.c_str());

    while (handler_stop == false) {
        if (format_context == nullptr) {
            /* We have disconnected.  Try to reconnect */
            clock_gettime(CLOCK_MONOTONIC, &frame_prev_tm);
            handler_reconnect();
            continue;
        } else {
            /* We think we are connected...*/
            clock_gettime(CLOCK_MONOTONIC, &frame_prev_tm);
            if (read_image() < 0) {
                /* We are not connected or got bad image.*/
                if (handler_stop == false) {
                    handler_reconnect();
                }
                continue;
            }
            if (last_stream_index == video_stream_index) {
                handler_wait();
            }
        }
    }

    MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Camera handler stopped"),cameratype.c_str());
    handler_running = false;
    pthread_exit(nullptr);

}

void cls_netcam::handler_startup()
{
    int wait_counter, retcd;
    pthread_attr_t thread_attr;

    if (handler_running == false) {
        handler_running = true;
        handler_stop = false;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        retcd = pthread_create(&handler_thread, &thread_attr, &netcam_handler, this);
        if (retcd != 0) {
            MOTPLS_LOG(WRN, TYPE_ALL, NO_ERRNO,_("Unable to start camera thread."));
            handler_running = false;
            handler_stop = true;
            return;
        }
        pthread_attr_destroy(&thread_attr);
    }

    /* Now give a few tries to check that an image has been captured.
     * This ensures that by the time the setup routine exits, the
     * handler is completely set up and has images available
     */
    wait_counter = 60;
    while (wait_counter > 0) {
        pthread_mutex_lock(&mutex);
            if (img_latest->ptr != nullptr ) {
                wait_counter = -1;
            }
        pthread_mutex_unlock(&mutex);
        if (wait_counter > 0 ) {
            MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s:Waiting for first image from the handler.")
                ,cameratype.c_str());
            SLEEP(0,5000000);
            wait_counter--;
        }
    }

}

void cls_netcam::handler_shutdown()
{
    int waitcnt;

    idur = 0;
    handler_stop = true;

    if (handler_running == true) {
        MOTPLS_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s:Shutting down network camera.")
            ,cameratype.c_str());
        waitcnt = 0;
        while ((handler_running == true) && (waitcnt < cam->cfg->watchdog_tmo)){
            SLEEP(1,0)
            waitcnt++;
        }
        if (waitcnt == cam->cfg->watchdog_tmo) {
            MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                , _("Normal shutdown of camera failed"));
            if (cam->cfg->watchdog_kill > 0) {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    ,_("Waiting additional %d seconds (watchdog_kill).")
                    ,cam->cfg->watchdog_kill);
                waitcnt = 0;
                while ((handler_running == true) && (waitcnt < cam->cfg->watchdog_kill)){
                    SLEEP(1,0)
                    waitcnt++;
                }
                if (waitcnt == cam->cfg->watchdog_kill) {
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("No response to shutdown.  Killing it."));
                    MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                        , _("Memory leaks will occur."));
                    pthread_kill(handler_thread, SIGVTALRM);
                }
            } else {
                MOTPLS_LOG(ERR, TYPE_ALL, NO_ERRNO
                    , _("watchdog_kill set to terminate application."));
                exit(1);
            }
        }
        handler_running = false;
    }

    context_close();

    if (img_latest != nullptr) {
        myfree(img_latest->ptr);
        myfree(img_latest);
        img_latest = nullptr;
    }

    if (img_recv != nullptr) {
        myfree(img_recv->ptr);
        myfree(img_recv);
        img_recv   = nullptr;
    }

    mydelete(params);

    cam->device_status = STATUS_CLOSED;
    status = NETCAM_NOTCONNECTED;
    MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s:Shut down complete.")
        ,cameratype.c_str());

}

void cls_netcam::netcam_start()
{
    int retcd;

    if (cam->handler_stop == true) {
        return;
    }

    handler_running = false;
    handler_stop = false;

    if (high_resolution == false) {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("Norm: Opening Netcam"));
    } else {
        MOTPLS_LOG(NTC, TYPE_VIDEO, NO_ERRNO,_("High: Opening Netcam"));
    }
    cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbitrary multiplier to give startup more time*/
    set_parms();
    if (service == "file") {
        retcd = connect();
        while ((retcd != 0) &&
            (filenbr < (int)filelist.size())) {
            filelist_load();
            retcd = connect();
        }
        if (retcd != 0) {
            handler_shutdown();
            return;
        }
    } else {
        if (connect() != 0) {
            handler_shutdown();
            return;
        }
    }
    cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbitrary multiplier to give startup more time*/
    if (read_image() != 0) {
        MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("Failed trying to read first image"));
        handler_shutdown();
        return;
    }
    cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbitrary multiplier to give startup more time*/
    /* When running dual, there seems to be contamination across norm/high with codec functions. */
    context_close();       /* Close in this thread to open it again within handler thread */
    status = NETCAM_RECONNECTING;      /* Set as reconnecting to avoid excess messages when starting */
    first_image = false;             /* Set flag that we are not processing our first image */

    if (high_resolution) {
        cam->imgs.width_high = imgsize.width;
        cam->imgs.height_high = imgsize.height;
    }
    cam->watchdog = cam->cfg->watchdog_tmo * 3; /* 3 is arbitrary multiplier to give startup more time*/
    handler_startup();

    cam->device_status = STATUS_OPENED;

}

void cls_netcam::netcam_stop()
{
    handler_shutdown();
}

void cls_netcam::noimage()
{
    int slp_dur;

    if (handler_running == false ) {
        if (reconnect_count >= 500) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 2 hours."));
            slp_dur = 7200;
        } else if (reconnect_count >= 200) {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 10 minutes."));
            reconnect_count++;
            slp_dur = 600;
        } else {
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Camera did not reconnect."));
            MOTPLS_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("Checking for camera every 30 seconds."));
            reconnect_count++;
            slp_dur = 30;
        }
        netcam_stop();

        cam->watchdog = slp_dur + (cam->cfg->watchdog_tmo * 3);

        while ((cam->handler_stop == false) && (slp_dur > 0)) {
            SLEEP(1, 0);
            slp_dur--;
        }

        cam->watchdog = (cam->cfg->watchdog_tmo * 3);

        netcam_start();
    }
}

int cls_netcam::next(ctx_image_data *img_data)
{
    if ((status == NETCAM_RECONNECTING) ||
        (status == NETCAM_NOTCONNECTED)) {
        return CAPTURE_ATTEMPTED;
    }

    pthread_mutex_lock(&mutex);
        pktarray_resize();
        if (high_resolution == false) {
            memcpy(img_data->image_norm
                , img_latest->ptr
                , img_latest->used);
            img_data->idnbr_norm = idnbr;
        } else {
            img_data->idnbr_high = idnbr;
            if (cam->netcam_high->passthrough == false) {
                memcpy(img_data->image_high
                    , img_latest->ptr
                    , img_latest->used);
            }
        }
    pthread_mutex_unlock(&mutex);

    return CAPTURE_SUCCESS;
}

cls_netcam::cls_netcam(cls_camera *p_cam, bool p_is_high)
{
    cam = p_cam;
    high_resolution = p_is_high;
    reconnect_count = 0;
    cameratype = "";

    pthread_mutex_init(&mutex, nullptr);
    pthread_mutex_init(&mutex_pktarray, nullptr);
    pthread_mutex_init(&mutex_transfer, nullptr);

}

cls_netcam::~cls_netcam()
{
    handler_shutdown();

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex_pktarray);
    pthread_mutex_destroy(&mutex_transfer);

}

