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
 *    Copyright 2020 MotionMrDave@gmail.com
 *
*/


/***********************************************************
 *
 *  The functions:
 *      netcam_setup
 *      netcam_next
 *      netcam_cleanup
 *  are called from video_common.c which is on the main thread
 *
 ***********************************************************/

#include <stdio.h>
#include <regex.h>
#include <time.h>
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

    if ((buff->size - buff->used) >= numbytes)
        return;

    min_size_to_alloc = numbytes - (buff->size - buff->used);
    real_alloc = ((min_size_to_alloc / NETCAM_BUFFSIZE) * NETCAM_BUFFSIZE);

    if ((min_size_to_alloc - real_alloc) > 0)
        real_alloc += NETCAM_BUFFSIZE;

    new_size = buff->size + real_alloc;

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO
        ,_("expanding buffer from [%d/%d] to [%d/%d] bytes.")
        ,(int) buff->used, (int) buff->size
        ,(int) buff->used, new_size);

    buff->ptr =(char*) myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

/*
 * The following three routines (netcam_url_match, netcam_url_parse and
 * netcam_url_free are for 'parsing' (i.e. separating into the relevant
 * components) the URL provided by the user.  They make use of regular
 * expressions (which is outside the scope of this module, so detailed
 * comments are not provided).  netcam_url_parse is called from netcam_start,
 * and puts the "broken-up" components of the URL into the "url" element of
 * the netcam_context structure.
 *
 * Note that the routines are not "very clever", but they work sufficiently
 * well for the limited requirements of this module.  The expression:
 *   (http)://(((.*):(.*))@)?([^/:]|[-.a-z0-9]+)(:([0-9]+))?($|(/[^:]*))
 * requires
 *   1) a string which begins with 'http', followed by '://'
 *   2) optionally a '@' which is preceded by two strings
 *      (with 0 or more characters each) separated by a ':'
 *      [this is for an optional username:password]
 *   3) a string comprising alpha-numerics, '-' and '.' characters
 *      [this is for the hostname]
 *   4) optionally a ':' followed by one or more numeric characters
 *      [this is for an optional port number]
 *   5) finally, either an end of line or a series of segments,
 *      each of which begins with a '/', and contains anything
 *      except a ':'
 */

/**
 * netcam_url_match
 *
 *      Finds the matched part of a regular expression
 *
 * Parameters:
 *
 *      m          A structure containing the regular expression to be used
 *      input      The input string
 *
 * Returns:        The string which was matched
 *
 */
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

static void netcam_url_invalid(struct url_t *parse_url)
{

    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Invalid URL.  Can not parse values."));

    parse_url->host =(char*) malloc(5);
    parse_url->service =(char*) malloc(5);
    parse_url->path =(char*) malloc(10);
    parse_url->userpass =(char*) malloc(10);
    parse_url->port = 0;
    sprintf(parse_url->host, "%s","????");
    sprintf(parse_url->service, "%s","????");
    sprintf(parse_url->path, "%s","INVALID");
    sprintf(parse_url->userpass, "%s","INVALID");

}
/**
 * netcam_url_parse
 *
 *      parses a string containing a URL into it's components
 *
 * Parameters:
 *      parse_url          A structure which will receive the results
 *                         of the parsing
 *      text_url           The input string containing the URL
 *
 * Returns:                Nothing
 *
 */
static void netcam_url_parse(struct url_t *parse_url, const char *text_url)
{
    char *s;
    int i;

    const char *re = "(http|ftp|mjpg|mjpeg|rtsp|rtmp)://(((.*):(.*))@)?"
                     "([^/:]|[-_.a-z0-9]+)(:([0-9]+))?($|(/[^*]*))";
    regex_t pattbuf;
    regmatch_t matches[10];

    if (!strncmp(text_url, "file", 4))
        re = "(file)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "jpeg", 4))
        re = "(jpeg)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    if (!strncmp(text_url, "v4l2", 4))
        re = "(v4l2)://(((.*):(.*))@)?([/:])?(:([0-9]+))?($|(/[^*]*))";

    /*  Note that log messages are commented out to avoid leaking info related
     *  to user/host/pass etc.  Keeing them in the code for easier debugging if
     *  it is needed
     */

    //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Entry netcam_url_parse data %s",text_url);

    memset(parse_url, 0, sizeof(struct url_t));
    /*
     * regcomp compiles regular expressions into a form that is
     * suitable for regexec searches
     * regexec matches the URL string against the regular expression
     * and returns an array of pointers to strings matching each match
     * within (). The results that we need are finally placed in parse_url.
     */
    if (!regcomp(&pattbuf, re, REG_EXTENDED | REG_ICASE)) {
        if (regexec(&pattbuf, text_url, 10, matches, 0) != REG_NOMATCH) {
            for (i = 0; i < 10; i++) {
                if ((s = netcam_url_match(matches[i], text_url)) != NULL) {
                    //MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "Parse case %d data %s", i, s);
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
                        free(s);
                        break;
                    case 9:
                        parse_url->path = s;
                        break;
                        /* Other components ignored */
                    default:
                        free(s);
                        break;
                    }
                }
            }
        } else {
            netcam_url_invalid(parse_url);
        }
    } else {
        netcam_url_invalid(parse_url);
    }
    if (((!parse_url->port) && (parse_url->service)) ||
        ((parse_url->port > 65535) && (parse_url->service))) {
        if (mystreq(parse_url->service, "http"))
            parse_url->port = 80;
        else if (mystreq(parse_url->service, "ftp"))
            parse_url->port = 21;
        else if (mystreq(parse_url->service, "rtmp"))
            parse_url->port = 1935;
        else if (mystreq(parse_url->service, "rtsp"))
            parse_url->port = 554;
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Using port number %d"),parse_url->port);
    }

    regfree(&pattbuf);
}

/**
 * netcam_url_free
 *
 *      General cleanup of the URL structure, called from netcam_cleanup.
 *
 * Parameters:
 *
 *      parse_url       Structure containing the parsed data.
 *
 * Returns:             Nothing
 *
 */
static void netcam_url_free(struct url_t *parse_url)
{
    free(parse_url->service);
    parse_url->service = NULL;

    free(parse_url->userpass);
    parse_url->userpass = NULL;

    free(parse_url->host);
    parse_url->host = NULL;

    free(parse_url->path);
    parse_url->path = NULL;
}

static int netcam_check_pixfmt(struct ctx_netcam *netcam)
{
    /* Determine if the format is YUV420P */
    int retcd;

    retcd = -1;
    if (((enum AVPixelFormat)netcam->frame->format == MY_PIX_FMT_YUV420P) ||
        ((enum AVPixelFormat)netcam->frame->format == MY_PIX_FMT_YUVJ420P)) retcd = 0;

    return retcd;

}

static void netcam_pktarray_free(struct ctx_netcam *netcam)
{

    int indx;
    pthread_mutex_lock(&netcam->mutex_pktarray);
        if (netcam->pktarray_size > 0){
            for(indx = 0; indx < netcam->pktarray_size; indx++) {
                if (netcam->pktarray[indx].packet.data != NULL) {
                    mypacket_unref(netcam->pktarray[indx].packet);
                }
            }
        }
        free(netcam->pktarray);
        netcam->pktarray = NULL;
        netcam->pktarray_size = 0;
        netcam->pktarray_index = -1;
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}

static void netcam_null_context(struct ctx_netcam *netcam)
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

static void netcam_close_context(struct ctx_netcam *netcam)
{

    if (netcam->swsctx       != NULL) sws_freeContext(netcam->swsctx);
    if (netcam->swsframe_in  != NULL) myframe_free(netcam->swsframe_in);
    if (netcam->swsframe_out != NULL) myframe_free(netcam->swsframe_out);
    if (netcam->frame        != NULL) myframe_free(netcam->frame);
    if (netcam->pktarray     != NULL) netcam_pktarray_free(netcam);
    if (netcam->codec_context   != NULL) myavcodec_close(netcam->codec_context);
    if (netcam->format_context  != NULL) avformat_close_input(&netcam->format_context);
    if (netcam->transfer_format != NULL) avformat_close_input(&netcam->transfer_format);
    if (netcam->hw_device_ctx   != NULL) av_buffer_unref(&netcam->hw_device_ctx);
    netcam_null_context(netcam);

}

static void netcam_pktarray_resize(struct ctx_cam *cam, int is_highres)
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

    int64_t               idnbr_last, idnbr_first;
    int                   indx;
    struct ctx_netcam  *netcam;
    struct packet_item   *tmp;
    int                   newsize;

    if (is_highres){
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_high;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_high;
        netcam = cam->netcam_high;
    } else {
        idnbr_last = cam->imgs.image_ring[cam->imgs.ring_out].idnbr_norm;
        idnbr_first = cam->imgs.image_ring[cam->imgs.ring_in].idnbr_norm;
        netcam = cam->netcam;
    }

    if (!netcam->passthrough) return;

    /* The 30 is arbitrary */
    /* Double the size plus double last diff so we don't catch our tail */
    newsize =((idnbr_first - idnbr_last) * 1 ) + ((netcam->idnbr - idnbr_last ) * 2);
    if (newsize < 30) newsize = 30;

    pthread_mutex_lock(&netcam->mutex_pktarray);
        if ((netcam->pktarray_size < newsize) ||  (netcam->pktarray_size < 30)){
            tmp =(packet_item*) mymalloc(newsize * sizeof(struct packet_item));
            if (netcam->pktarray_size > 0 ){
                memcpy(tmp, netcam->pktarray, sizeof(struct packet_item) * netcam->pktarray_size);
            }
            for(indx = netcam->pktarray_size; indx < newsize; indx++) {
                av_init_packet(&tmp[indx].packet);
                tmp[indx].packet.data=NULL;
                tmp[indx].packet.size=0;
                tmp[indx].idnbr = 0;
                tmp[indx].iskey = FALSE;
                tmp[indx].iswritten = FALSE;
            }

            if (netcam->pktarray != NULL) free(netcam->pktarray);
            netcam->pktarray = tmp;
            netcam->pktarray_size = newsize;

            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Resized packet array to %d"), netcam->cameratype,newsize);
        }
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}

static void netcam_pktarray_add(struct ctx_netcam *netcam)
{

    int indx_next;
    int retcd;
    char errstr[128];

    pthread_mutex_lock(&netcam->mutex_pktarray);

        if (netcam->pktarray_size == 0){
            pthread_mutex_unlock(&netcam->mutex_pktarray);
            return;
        }

        /* Recall pktarray_size is one based but pktarray is zero based */
        if (netcam->pktarray_index == (netcam->pktarray_size-1) ){
            indx_next = 0;
        } else {
            indx_next = netcam->pktarray_index + 1;
        }

        netcam->pktarray[indx_next].idnbr = netcam->idnbr;

        mypacket_unref(netcam->pktarray[indx_next].packet);
        av_init_packet(&netcam->pktarray[indx_next].packet);
        netcam->pktarray[indx_next].packet.data = NULL;
        netcam->pktarray[indx_next].packet.size = 0;

        retcd = mycopy_packet(&netcam->pktarray[indx_next].packet, &netcam->packet_recv);
        if ((netcam->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_copy_packet: %s ,Interrupt: %s")
                ,netcam->cameratype
                ,errstr, netcam->interrupted ? _("True"):_("False"));
            mypacket_unref(netcam->pktarray[indx_next].packet);
            netcam->pktarray[indx_next].packet.data = NULL;
            netcam->pktarray[indx_next].packet.size = 0;
        }

        if (netcam->pktarray[indx_next].packet.flags & AV_PKT_FLAG_KEY) {
            netcam->pktarray[indx_next].iskey = TRUE;
        } else {
            netcam->pktarray[indx_next].iskey = FALSE;
        }
        netcam->pktarray[indx_next].iswritten = FALSE;
        clock_gettime(CLOCK_REALTIME, &netcam->pktarray[indx_next].timestamp_ts);

        netcam->pktarray_index = indx_next;
    pthread_mutex_unlock(&netcam->mutex_pktarray);

}

static int netcam_decode_sw(struct ctx_netcam *netcam)
{
    #if (MYFFVER >= 57041)
        int retcd;
        char errstr[128];

        retcd = avcodec_receive_frame(netcam->codec_context, netcam->frame);
        if ((netcam->interrupted) || (netcam->finish) || (retcd < 0) ){
            if (retcd == AVERROR(EAGAIN)){
                retcd = 0;
            } else if (retcd == AVERROR_INVALIDDATA) {
                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Ignoring packet with invalid data")
                    ,netcam->cameratype);
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: Rec frame error: %s")
                        ,netcam->cameratype, errstr);
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

static int netcam_decode_vaapi(struct ctx_netcam *netcam)
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
                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Ignoring packet with invalid data")
                    ,netcam->cameratype);
                retcd = 0;
            } else if (retcd < 0) {
                av_strerror(retcd, errstr, sizeof(errstr));
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: Rec frame error: %s")
                        ,netcam->cameratype, errstr);
                retcd = -1;
            } else {
                retcd = -1;
            }
            myframe_free(hw_frame);
            return retcd;
        }
        netcam->frame->format=AV_PIX_FMT_YUV420P;
        retcd = av_hwframe_transfer_data(netcam->frame, hw_frame, 0);
        if (retcd < 0) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error transferring HW decoded to system memory")
                ,netcam->cameratype);
            return -1;
        }
        myframe_free(hw_frame);

        return 1;
    #else
        (void)netcam;
        return -1;
    #endif
}

/* netcam_decode_video
 *
 * Return values:
 *   <0 error
 *   0 invalid but continue
 *   1 valid data
 */

static int netcam_decode_video(struct ctx_netcam *netcam)
{
    #if (MYFFVER >= 57041)
        int retcd;
        char errstr[128];

        /* The Invalid data problem comes frequently.  Usually at startup of rtsp cameras.
        * We now ignore those packets so this function would need to fail on a different error.
        * We should consider adding a maximum count of these errors and reset every time
        * we get a good image.
        */
        if (netcam->finish) return 0;   /* This just speeds up the shutdown time */

        retcd = avcodec_send_packet(netcam->codec_context, &netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Interrupted or finish on send")
                ,netcam->cameratype);
            return -1;
        }
        if (retcd == AVERROR_INVALIDDATA) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Send ignoring packet with invalid data")
                ,netcam->cameratype);
            return 0;
        }
        if (retcd < 0 && retcd != AVERROR_EOF){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error sending packet to codec: %s")
                ,netcam->cameratype, errstr);
            return -1;
        }

        if (netcam->hw_type == AV_HWDEVICE_TYPE_VAAPI ){
            retcd = netcam_decode_vaapi(netcam);
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

        if (netcam->finish) return 0;   /* This just speeds up the shutdown time */

        retcd = avcodec_decode_video2(netcam->codec_context, netcam->frame, &check, &netcam->packet_recv);
        if ((netcam->interrupted) || (netcam->finish)) return -1;

        if (retcd == AVERROR_INVALIDDATA) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Ignoring packet with invalid data"));
            return 0;
        }

        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Error decoding packet: %s"),errstr);
            return -1;
        }

        if (check == 0 || retcd == 0) return 0;

        return 1;
    #endif

}

static int netcam_decode_packet(struct ctx_netcam *netcam)
{

    int frame_size;
    int retcd;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    if (netcam->packet_recv.stream_index == netcam->audio_stream_index) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Error decoding video packet...it is audio")
            ,netcam->cameratype);
    }

    retcd = netcam_decode_video(netcam);
    if (retcd <= 0) return retcd;

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
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Error decoding video packet: Copying to buffer")
            ,netcam->cameratype);
        return -1;
    }

    netcam->img_recv->used = frame_size;

    return frame_size;
}

static void netcam_hwdecoders(struct ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        /* High Res pass through does not decode images into frames*/
        if (netcam->high_resolution && netcam->passthrough) return;

        if ((netcam->hw_type == AV_HWDEVICE_TYPE_NONE) && (netcam->first_image)) {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: HW Devices: ")
                , netcam->cameratype);
            while((netcam->hw_type = av_hwdevice_iterate_types(netcam->hw_type)) != AV_HWDEVICE_TYPE_NONE){
                if (netcam->hw_type == AV_HWDEVICE_TYPE_VAAPI) {
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: %s (available)")
                        , netcam->cameratype
                        , av_hwdevice_get_type_name(netcam->hw_type));
                } else {
                    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                        ,_("%s: %s (not implemented)")
                        , netcam->cameratype
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
            if (*p == AV_PIX_FMT_VAAPI) return *p;
        }

        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("Failed to get vaapi pix format"));
        return AV_PIX_FMT_NONE;
    #else
        (void)avctx;
        (void)pix_fmts;
        return AV_PIX_FMT_NONE;
    #endif
}

static void netcam_decoder_error(struct ctx_netcam *netcam, int retcd, const char* fnc_nm)
{
    char errstr[128];
    int indx;

    if (netcam->interrupted){
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Interrupted"),netcam->cameratype);
    } else {
        if (retcd < 0){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: %s: %s"),netcam->cameratype,fnc_nm, errstr);
        } else {
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: %s: Failed"), netcam->cameratype,fnc_nm);
        }
    }

    if (mystrne(netcam->decoder_nm,"NULL")) {
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Decoder %s did not work.")
            ,netcam->cameratype, netcam->decoder_nm);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Ignoring and removing the user requested decoder %s")
            ,netcam->cameratype, netcam->decoder_nm);

        for (indx = 0; indx < netcam->params->params_count; indx++) {
            if (mystreq(netcam->params->params_array[indx].param_name,"decoder") ) {
                free(netcam->params->params_array[indx].param_value);
                netcam->params->params_array[indx].param_value = (char*)mymalloc(5);
                snprintf(netcam->params->params_array[indx].param_value, 5, "%s","NULL");
                break;
            }
        }

        if (netcam->high_resolution) {
            util_parms_update(netcam->params, netcam->conf->netcam_high_params);
        } else {
            util_parms_update(netcam->params, netcam->conf->netcam_params);
        }

        free(netcam->decoder_nm);
        netcam->decoder_nm = (char*)mymalloc(5);
        snprintf(netcam->decoder_nm, 5, "%s","NULL");
    }

}

static int netcam_init_vaapi(struct ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57083)
        int retcd;

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Initializing vaapi decoder"),netcam->cameratype);

        netcam->hw_type = av_hwdevice_find_type_by_name("vaapi");
        if (netcam->hw_type == AV_HWDEVICE_TYPE_NONE){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO,_("%s: Unable to find vaapi hw device")
                , netcam->cameratype);
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

        netcam->hw_pix_fmt = AV_PIX_FMT_VAAPI;
        netcam->codec_context->get_format  = netcam_getfmt_vaapi;
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
        (void)netcam_getfmt_vaapi;
        return -1;
    #endif
}

static int netcam_init_swdecoder(struct ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57041)
        int retcd;

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Initializing decoder"),netcam->cameratype);

        if (mystrne(netcam->decoder_nm,"NULL")) {
            netcam->decoder = avcodec_find_decoder_by_name(netcam->decoder_nm);
            if (netcam->decoder == NULL) {
                netcam_decoder_error(netcam, 0, "avcodec_find_decoder_by_name");
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("%s: Using decoder %s")
                    ,netcam->cameratype, netcam->decoder_nm);
            }
        }
        if (netcam->decoder == NULL) {
            netcam->decoder = avcodec_find_decoder(netcam->strm->codecpar->codec_id);
        }
        if ((netcam->decoder == NULL) || (netcam->interrupted)){
            netcam_decoder_error(netcam, 0, "avcodec_find_decoder");
            return -1;
        }

        netcam->codec_context = avcodec_alloc_context3(netcam->decoder);
        if ((netcam->codec_context == NULL) || (netcam->interrupted)){
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

static int netcam_open_codec(struct ctx_netcam *netcam)
{
    #if ( MYFFVER >= 57041)

        int retcd;

        if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

        netcam_hwdecoders(netcam);

        retcd = av_find_best_stream(netcam->format_context
                    , AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if ((retcd < 0) || (netcam->interrupted)){
            netcam->audio_stream_index = -1;
        } else {
            netcam->audio_stream_index = retcd;
        }

        netcam->decoder = NULL;
        retcd = av_find_best_stream(netcam->format_context
                    , AVMEDIA_TYPE_VIDEO, -1, -1, &netcam->decoder, 0);
        if ((retcd < 0) || (netcam->interrupted)){
            netcam_decoder_error(netcam, retcd, "av_find_best_stream");
            return -1;
        }
        netcam->video_stream_index = retcd;
        netcam->strm = netcam->format_context->streams[netcam->video_stream_index];

        if (mystrceq(netcam->decoder_nm,"vaapi")){
            if (netcam_init_vaapi(netcam) < 0){
                netcam_decoder_error(netcam, retcd, "hwvaapi_init");
                return -1;
            }
        } else {
            netcam_init_swdecoder(netcam);
        }

        retcd = avcodec_open2(netcam->codec_context, netcam->decoder, NULL);
        if ((retcd < 0) || (netcam->interrupted)){
            netcam_decoder_error(netcam, retcd, "avcodec_open2");
            return -1;
        }

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Decoder opened"),netcam->cameratype);

        return 0;
    #else
        int retcd;

        (void)netcam_init_vaapi;

        if (netcam->finish) {
            /* This just speeds up the shutdown time */
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

static struct ctx_netcam *netcam_new_context(void)
{
    struct ctx_netcam *ret;

    /* Note that mymalloc will exit on any problem. */
    ret =(struct ctx_netcam*) mymalloc(sizeof(struct ctx_netcam));

    memset(ret, 0, sizeof(struct ctx_netcam));

    return ret;
}

static int netcam_interrupt(void *ctx)
{
    struct ctx_netcam *netcam = (struct ctx_netcam *)ctx;

    if (netcam->finish){
        netcam->interrupted = TRUE;
        return TRUE;
    }

    if (netcam->status == NETCAM_CONNECTED) {
        return FALSE;
    } else if (netcam->status == NETCAM_READINGIMAGE) {
        clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera reading (%s) timed out")
                , netcam->cameratype, netcam->camera_name);
            netcam->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * netcam_connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);
        if ((netcam->interruptcurrenttime.tv_sec - netcam->interruptstarttime.tv_sec ) > netcam->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera (%s) timed out")
                , netcam->cameratype, netcam->camera_name);
            netcam->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    }

    /* should not be possible to get here */
    return FALSE;
}

static int netcam_open_sws(struct ctx_netcam *netcam)
{

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    netcam->swsframe_in = myframe_alloc();
    if (netcam->swsframe_in == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s: Unable to allocate swsframe_in.")
                , netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_out = myframe_alloc();
    if (netcam->swsframe_out == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s: Unable to allocate swsframe_out.")
                , netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    if (netcam_check_pixfmt(netcam) != 0){
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            , _("%s: Pixel format %d will be converted.")
            , netcam->cameratype, netcam->frame->format);
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
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s: Unable to allocate scaling context.")
                , netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    netcam->swsframe_size = myimage_get_buffer_size(
            MY_PIX_FMT_YUV420P
            ,netcam->imgsize.width
            ,netcam->imgsize.height);
    if (netcam->swsframe_size <= 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                , _("%s: Error determining size of frame out")
                , netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    netcam_check_buffsize(netcam->img_recv, netcam->swsframe_size);
    netcam_check_buffsize(netcam->img_latest, netcam->swsframe_size);

    return 0;

}

static int netcam_resize(struct ctx_netcam *netcam)
{

    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    if (netcam->swsctx == NULL) {
        if (netcam_open_sws(netcam) < 0) return -1;
    }

    retcd=myimage_fill_arrays(
        netcam->swsframe_in
        ,(uint8_t*)netcam->img_recv->ptr
        ,(enum AVPixelFormat)netcam->frame->format
        ,netcam->frame->width
        ,netcam->frame->height);
    if (retcd < 0) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error allocating picture in: %s")
                , netcam->cameratype, errstr);
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
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error allocating picture out: %s")
                , netcam->cameratype, errstr);
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
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error resizing/reformatting: %s")
                , netcam->cameratype, errstr);
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
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Error putting frame into output buffer: %s")
                , netcam->cameratype, errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }
    netcam->img_recv->used = netcam->swsframe_size;

    av_free(buffer_out);

    return 0;

}

static int netcam_read_image(struct ctx_netcam *netcam)
{

    int  size_decoded, retcd, haveimage, errcnt;
    char errstr[128];
    netcam_buff *xchg;

    if (netcam->finish) return -1;   /* This just speeds up the shutdown time */

    av_init_packet(&netcam->packet_recv);
    netcam->packet_recv.data = NULL;
    netcam->packet_recv.size = 0;

    netcam->interrupted=FALSE;
    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);
    netcam->interruptduration = 10;

    netcam->status = NETCAM_READINGIMAGE;
    netcam->img_recv->used = 0;
    size_decoded = 0;
    errcnt = 0;
    haveimage = FALSE;

    while ((!haveimage) && (!netcam->interrupted)) {
        retcd = av_read_frame(netcam->format_context, &netcam->packet_recv);
        if (retcd < 0 ) errcnt++;
        if ((netcam->interrupted) || (errcnt > 1)) {
            if (netcam->interrupted) {
                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Interrupted")
                    ,netcam->cameratype);
            } else {
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: av_read_frame: %s" )
                    ,netcam->cameratype,errstr);
            }
            mypacket_unref(netcam->packet_recv);
            netcam_close_context(netcam);
            return -1;
        } else {
            errcnt = 0;
            if ((netcam->packet_recv.stream_index == netcam->video_stream_index) ||
                (netcam->packet_recv.stream_index == netcam->audio_stream_index)) {
                /* For a high resolution pass-through we don't decode the image */
                if ((netcam->high_resolution && netcam->passthrough) ||
                    (netcam->packet_recv.stream_index != netcam->video_stream_index)) {
                    if (netcam->packet_recv.data != NULL) {
                        size_decoded = 1;
                    }
                } else {
                    size_decoded = netcam_decode_packet(netcam);
                }
            }
            if (size_decoded > 0 ){
                haveimage = TRUE;
            } else if (size_decoded == 0){
                /* Did not fail, just didn't get anything.  Try again */
                mypacket_unref(netcam->packet_recv);
                av_init_packet(&netcam->packet_recv);
                netcam->packet_recv.data = NULL;
                netcam->packet_recv.size = 0;
            } else {
                mypacket_unref(netcam->packet_recv);
                netcam_close_context(netcam);
                return -1;
            }
        }
    }
    clock_gettime(CLOCK_REALTIME, &netcam->img_recv->image_time);

    if (!netcam->first_image) {
        netcam->status = NETCAM_CONNECTED;
    }

    /* Skip resize/pix format for high pass-through */
    if (!(netcam->high_resolution && netcam->passthrough) &&
        (netcam->packet_recv.stream_index == netcam->video_stream_index)) {

        if ((netcam->imgsize.width  != netcam->frame->width) ||
            (netcam->imgsize.height != netcam->frame->height) ||
            (netcam_check_pixfmt(netcam) != 0)) {
            if (netcam_resize(netcam) < 0){
                mypacket_unref(netcam->packet_recv);
                netcam_close_context(netcam);
                return -1;
            }
        }
    }

    pthread_mutex_lock(&netcam->mutex);
        netcam->idnbr++;
        if (netcam->passthrough) netcam_pktarray_add(netcam);
        if (!(netcam->high_resolution && netcam->passthrough) &&
            (netcam->packet_recv.stream_index == netcam->video_stream_index)) {
            xchg = netcam->img_latest;
            netcam->img_latest = netcam->img_recv;
            netcam->img_recv = xchg;
        }
    pthread_mutex_unlock(&netcam->mutex);

    mypacket_unref(netcam->packet_recv);

    if (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den > 0){
        netcam->src_fps = (
            (netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.num /
            netcam->format_context->streams[netcam->video_stream_index]->avg_frame_rate.den) +
            0.5);
        if (netcam->capture_rate < 1) {
            netcam->capture_rate = netcam->src_fps + 1;
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: capture_rate not specified in netcam_params. Using %d")
                    ,netcam->cameratype,netcam->capture_rate);
        }
    } else {
        if (netcam->capture_rate < 1) {
            netcam->capture_rate = netcam->conf->framerate;
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: capture_rate not specified in netcam_params. Using framerate %d")
                    ,netcam->cameratype, netcam->capture_rate);
        }
    }

    return 0;
}

static int netcam_ntc(struct ctx_netcam *netcam)
{

    if ((netcam->finish) || (!netcam->first_image)) return 0;

    /* High Res pass through does not decode images into frames*/
    if (netcam->high_resolution && netcam->passthrough) return 0;

    if ((netcam->imgsize.width  != netcam->frame->width) ||
        (netcam->imgsize.height != netcam->frame->height) ){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures at %dx%d")
            , netcam->frame->width,netcam->frame->height);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("resolution but config is %dx%d. If possible change")
            , netcam->imgsize.width,netcam->imgsize.height);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("the netcam or config so that the image height and"));
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("width are the same to lower the CPU usage."));
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
    }

    return 0;

}

static void netcam_set_options(struct ctx_netcam *netcam)
{

    int indx;
    char *tmpval;

    /* The log messages are a bit short in this function intentionally.
     * The function name is printed in each message so that is being
     * considered as part of the message.
     */

    tmpval = (char*)mymalloc(PATH_MAX);

    if (mystreq(netcam->service, "rtsp") ||
        mystreq(netcam->service, "rtmp")) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s: Setting rtsp/rtmp")
            ,netcam->cameratype);
        util_parms_add_default(netcam->params,"rtsp_transport","tcp");
        //util_parms_add_default(netcam->params,"allowed_media_types", "video");

    } else if (mystreq(netcam->service, "http")) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting input_format mjpeg"),netcam->cameratype);
        netcam->format_context->iformat = av_find_input_format("mjpeg");

    } else if (mystreq(netcam->service, "v4l2")) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting input_format video4linux2"),netcam->cameratype);
        netcam->format_context->iformat = av_find_input_format("video4linux2");

        sprintf(tmpval,"%d",netcam->conf->framerate);
        util_parms_add_default(netcam->params,"framerate", tmpval);

        sprintf(tmpval,"%dx%d",netcam->conf->width, netcam->conf->height);
        util_parms_add_default(netcam->params,"video_size", tmpval);

        /* Allow a bit more time for the v4l2 device to start up */
        //netcam->motapp-> ->watchdog = 60;
        netcam->interruptduration = 55;

    } else if (mystreq(netcam->service, "file")) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting up movie file"),netcam->cameratype);

    } else {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s: Setting up %s")
            ,netcam->cameratype, netcam->service);
    }

    free(tmpval);

    /* Write the options to the context, while skipping the Motion ones */
    for (indx = 0; indx < netcam->params->params_count; indx++) {
        if (mystrne(netcam->params->params_array[indx].param_name,"decoder") &&
            mystrne(netcam->params->params_array[indx].param_name,"capture_rate")) {
            av_dict_set(&netcam->opts
                , netcam->params->params_array[indx].param_name
                , netcam->params->params_array[indx].param_value
                , 0);
            if (netcam->status == NETCAM_NOTCONNECTED) {
                MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("%s: option: %s = %s")
                    ,netcam->cameratype
                    ,netcam->params->params_array[indx].param_name
                    ,netcam->params->params_array[indx].param_value
                );
            }
        }
    }

}


static void netcam_set_path (struct ctx_cam *cam, struct ctx_netcam *netcam )
{

    char   userpass[PATH_MAX];
    struct url_t url;
    int retcd;

    netcam->path = NULL;

    memset(&url, 0, sizeof(url));
    memset(userpass,0,PATH_MAX);

    if (netcam->high_resolution){
        netcam_url_parse(&url, cam->conf->netcam_high_url.c_str());
    } else {
        netcam_url_parse(&url, cam->conf->netcam_url.c_str());
    }

    if (cam->conf->netcam_userpass != "") {
        cam->conf->netcam_userpass.copy(userpass, PATH_MAX);
    } else if (url.userpass != NULL) {
        retcd = snprintf(userpass,PATH_MAX,"%s",url.userpass);
        if ((retcd <0) || (retcd>=PATH_MAX)){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO,_("Error getting userpass"));
        }
    }

    if (mystreq(url.service, "v4l2")) {
        netcam->path =(char*) mymalloc(strlen(url.path) + 1);
        sprintf(netcam->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up v4l2"));
    } else if (mystreq(url.service, "file")) {
        netcam->path =(char*) mymalloc(strlen(url.path) + 1);
        sprintf(netcam->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up file"));
    } else {
        if (mystreq(url.service, "mjpeg")) {
            sprintf(url.service, "%s","http");
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up http"));
        } else {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up %s "),url.service);
        }
        if (userpass != NULL) {
            netcam->path =(char*) mymalloc(strlen(url.service) + 3 + strlen(userpass)
                  + 1 + strlen(url.host) + 6 + strlen(url.path) + 2 );
            sprintf((char *)netcam->path, "%s://%s@%s:%d%s",
                    url.service, userpass, url.host, url.port, url.path);
        } else {
            netcam->path =(char*) mymalloc(strlen(url.service) + 3 + strlen(url.host)
                  + 6 + strlen(url.path) + 2);
            sprintf((char *)netcam->path, "%s://%s:%d%s", url.service,
                url.host, url.port, url.path);
        }
    }

    sprintf(netcam->service, "%s",url.service);

    netcam_url_free(&url);

}

static void netcam_set_parms (struct ctx_cam *cam, struct ctx_netcam *netcam )
{
    /* Set the parameters to be used with our camera */
    int indx, val_len;

    netcam->motapp = cam->motapp;
    netcam->conf = cam->conf;

    if (netcam->high_resolution) {
        netcam->imgsize.width = 0;
        netcam->imgsize.height = 0;
        snprintf(netcam->cameratype, 29, "%s",_("High"));
        netcam->params = (ctx_params*)mymalloc(sizeof(struct ctx_params));
        netcam->params->update_params = TRUE;
        util_parms_parse(netcam->params, cam->conf->netcam_high_params);
    } else {
        netcam->imgsize.width = cam->conf->width;
        netcam->imgsize.height = cam->conf->height;
        snprintf(netcam->cameratype, 29, "%s",_("Norm"));
        netcam->params = (ctx_params*)mymalloc(sizeof(struct ctx_params));
        netcam->params->update_params = TRUE;
        util_parms_parse(netcam->params, cam->conf->netcam_params);
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Setting up camera."),netcam->cameratype);

    netcam->status = NETCAM_NOTCONNECTED;
    cam->conf->camera_name.copy(netcam->camera_name,PATH_MAX);
    mycheck_passthrough(cam);
    util_parms_add_default(netcam->params,"decoder","NULL");
    netcam->img_recv =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_recv->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->img_latest =(netcam_buff_ptr) mymalloc(sizeof(netcam_buff));
    netcam->img_latest->ptr =(char*) mymalloc(NETCAM_BUFFSIZE);
    netcam->pktarray_size = 0;
    netcam->pktarray_index = -1;
    netcam->pktarray = NULL;
    netcam->handler_finished = TRUE;
    netcam->first_image = TRUE;
    netcam->reconnect_count = 0;
    netcam->src_fps =  -1; /* Default to neg so we know it has not been set */
    netcam->capture_rate = -1;

    for (indx = 0; indx < netcam->params->params_count; indx++) {
        if (mystreq(netcam->params->params_array[indx].param_name,"decoder")) {
            val_len = strlen(netcam->params->params_array[indx].param_value) + 1;
            netcam->decoder_nm = (char*)mymalloc(val_len);
            snprintf(netcam->decoder_nm, val_len
                , "%s",netcam->params->params_array[indx].param_value);
        }

        if (mystreq(netcam->params->params_array[indx].param_name,"capture_rate")) {
            netcam->capture_rate = atoi(netcam->params->params_array[indx].param_value);
        }

    }

    /* If this is the norm and we have a highres, then disable passthru on the norm */
    if ((!netcam->high_resolution) &&
        (cam->conf->netcam_high_url != "")) {
        netcam->passthrough = FALSE;
    } else {
        netcam->passthrough = mycheck_passthrough(cam);
    }

    snprintf(netcam->threadname, 15, "%s",_("Unknown"));

    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);
    clock_gettime(CLOCK_REALTIME, &netcam->interruptcurrenttime);

    netcam->interruptduration = 5;
    netcam->interrupted = FALSE;

    clock_gettime(CLOCK_REALTIME, &netcam->frame_curr_tm);
    clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);

    netcam_set_path(cam, netcam);

}

static int netcam_set_dimensions (struct ctx_cam *cam)
{

    cam->imgs.width = 0;
    cam->imgs.height = 0;
    cam->imgs.size_norm = 0;
    cam->imgs.motionsize = 0;

    cam->imgs.width_high  = 0;
    cam->imgs.height_high = 0;
    cam->imgs.size_high   = 0;

    if (cam->conf->width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8.")
            , cam->conf->width);
        cam->conf->width = cam->conf->width - (cam->conf->width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Adjusting width to next higher multiple of 8 (%d).")
            , cam->conf->width);
    }
    if (cam->conf->height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cam->conf->height);
        cam->conf->height = cam->conf->height - (cam->conf->height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cam->conf->height);
    }

    /* Fill in camera details into context structure. */
    cam->imgs.width = cam->conf->width;
    cam->imgs.height = cam->conf->height;
    cam->imgs.size_norm = (cam->conf->width * cam->conf->height * 3) / 2;
    cam->imgs.motionsize = cam->conf->width * cam->conf->height;

    return 0;
}

static int netcam_copy_stream(struct ctx_netcam *netcam)
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
                    if (retcd < 0){
                        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                            ,_("%s: Unable to copy codec parameters")
                            , netcam->cameratype);
                        pthread_mutex_unlock(&netcam->mutex_transfer);
                        return -1;
                    }
                    transfer_stream->time_base = stream_in->time_base;
                }
            }
        pthread_mutex_unlock(&netcam->mutex_transfer);

        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
        return 0;
    #else
        /* This is disabled in the mycheck_passthrough but we need it here for compiling */
        if (netcam != NULL) MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("ffmpeg too old"));
        return -1;
    #endif

}

static int netcam_open_context(struct ctx_netcam *netcam)
{

    int  retcd;
    char errstr[128];

    if (netcam->finish) return -1;

    if (netcam->path == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Null path passed to connect"));
        }
        return -1;
    }

    netcam->opts = NULL;
    netcam->format_context = avformat_alloc_context();
    netcam->format_context->interrupt_callback.callback = netcam_interrupt;
    netcam->format_context->interrupt_callback.opaque = netcam;
    netcam->interrupted = FALSE;

    clock_gettime(CLOCK_REALTIME, &netcam->interruptstarttime);

    netcam->interruptduration = 20;

    netcam_set_options(netcam);

    retcd = avformat_open_input(&netcam->format_context, netcam->path, NULL, &netcam->opts);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open camera(%s): %s")
                , netcam->cameratype, netcam->camera_name, errstr);
        }
        av_dict_free(&netcam->opts);
        if (netcam->interrupted) netcam_close_context(netcam);
        return -1;
    }
    av_dict_free(&netcam->opts);
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Opened camera(%s)"), netcam->cameratype, netcam->camera_name);


    /* fill out stream information */
    retcd = avformat_find_stream_info(netcam->format_context, NULL);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to find stream info: %s")
                ,netcam->cameratype, errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */

    mythreadname_get(netcam->threadname);
    mythreadname_set("av",netcam->threadnbr,netcam->camera_name);
        retcd = netcam_open_codec(netcam);
    mythreadname_set(NULL, 0, netcam->threadname);
    if ((retcd < 0) || (netcam->interrupted) || (netcam->finish) ){
        if (netcam->status == NETCAM_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open codec context: %s")
                ,netcam->cameratype, errstr);
        } else {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Connected and unable to open codec context: %s")
                ,netcam->cameratype, errstr);
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->codec_context->width <= 0 ||
        netcam->codec_context->height <= 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Camera image size is invalid"),netcam->cameratype);
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->high_resolution){
        netcam->imgsize.width = netcam->codec_context->width;
        netcam->imgsize.height = netcam->codec_context->height;
    }

    netcam->frame = myframe_alloc();
    if (netcam->frame == NULL) {
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to allocate frame."),netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    if (netcam->passthrough){
        retcd = netcam_copy_stream(netcam);
        if ((retcd < 0) || (netcam->interrupted)){
            if (netcam->status == NETCAM_NOTCONNECTED){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Failed to copy stream for pass-through.")
                    ,netcam->cameratype);
            }
            netcam->passthrough = FALSE;
        }
    }

    /* Validate that the previous steps opened the camera */
    retcd = netcam_read_image(netcam);
    if ((retcd < 0) || (netcam->interrupted)){
        if (netcam->status == NETCAM_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Failed to read first image"),netcam->cameratype);
        }
        netcam_close_context(netcam);
        return -1;
    }

    return 0;

}

static int netcam_connect(struct ctx_netcam *netcam)
{

    if (netcam_open_context(netcam) < 0) return -1;

    if (netcam_ntc(netcam) < 0 ) return -1;

    if (netcam_read_image(netcam) < 0) return -1;

    /* We use the status for determining whether to grab a image from
     * the Motion loop(see "next" function).  When we are initially starting,
     * we open and close the context and during this process we do not want the
     * Motion loop to start quite yet on this first image so we do
     * not set the status to connected
     */
    if (!netcam->first_image){
        netcam->status = NETCAM_CONNECTED;

        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Camera (%s) connected")
            , netcam->cameratype,netcam->camera_name);

        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("%s: Netcam capture FPS is %d.")
            , netcam->cameratype, netcam->capture_rate);

        if (netcam->src_fps > 0){
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s: Camera source is %d FPS")
                , netcam->cameratype, netcam->src_fps);
        } else {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                , _("%s: Unable to determine the camera source FPS.")
                , netcam->cameratype);
        }

        if (netcam->capture_rate < netcam->src_fps){
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                , _("%s: Capture FPS less than camera FPS. Decoding errors will occur.")
                , netcam->cameratype);
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
                , _("%s: Capture FPS should be greater than camera FPS.")
                , netcam->cameratype);
        }
    }

    return 0;
}

static void netcam_shutdown(struct ctx_netcam *netcam)
{

    if (netcam) {
        netcam_close_context(netcam);

        if (netcam->path != NULL) free(netcam->path);
        netcam->path    = NULL;

        if (netcam->img_latest != NULL){
            free(netcam->img_latest->ptr);
            free(netcam->img_latest);
        }
        netcam->img_latest = NULL;

        if (netcam->img_recv != NULL){
            free(netcam->img_recv->ptr);
            free(netcam->img_recv);
        }
        netcam->img_recv   = NULL;

        if (netcam->decoder_nm != NULL){
            free(netcam->decoder_nm);
        }
        netcam->decoder_nm = NULL;

        util_parms_free(netcam->params);
        if (netcam->params != NULL) {
            free(netcam->params);
        }
        netcam->params = NULL;
    }

}

static void netcam_handler_wait(struct ctx_netcam *netcam)
{
    long usec_maxrate, usec_delay;

    if (netcam->capture_rate < 1) netcam->capture_rate = 1;

    usec_maxrate = (1000000L / netcam->capture_rate);

    clock_gettime(CLOCK_REALTIME, &netcam->frame_curr_tm);

    usec_delay = usec_maxrate -
        ((netcam->frame_curr_tm.tv_sec - netcam->frame_prev_tm.tv_sec) * 1000000L) -
        ((netcam->frame_curr_tm.tv_nsec - netcam->frame_prev_tm.tv_nsec)/1000);

    if ((usec_delay > 0) && (usec_delay < 1000000L)){
        SLEEP(0, usec_delay * 1000);
    }

}

static void netcam_handler_reconnect(struct ctx_netcam *netcam)
{

    int retcd;

    if ((netcam->status == NETCAM_CONNECTED) ||
        (netcam->status == NETCAM_READINGIMAGE)){
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Reconnecting with camera...."),netcam->cameratype);
    }
    netcam->status = NETCAM_RECONNECTING;

    /*
    * The retry count of 100 is arbritrary.
    * We want to try many times quickly to not lose too much information
    * before we go into the long wait phase
    */
    retcd = netcam_connect(netcam);
    if (retcd < 0){
        if (netcam->reconnect_count < 100){
            netcam->reconnect_count++;
        } else if (netcam->reconnect_count == 100){
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera did not reconnect."), netcam->cameratype);
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Checking for camera every 10 seconds."),netcam->cameratype);
            netcam->reconnect_count++;
            SLEEP(10,0);
        } else {
            SLEEP(10,0);
        }
    } else {
        netcam->reconnect_count = 0;
    }

}

static void *netcam_handler(void *arg)
{

    struct ctx_netcam *netcam =(struct ctx_netcam *) arg;

    netcam->handler_finished = FALSE;

    mythreadname_set("nc",netcam->threadnbr, netcam->camera_name);

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)netcam->threadnbr));

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Camera handler thread [%d] started")
        ,netcam->cameratype, netcam->threadnbr);

    while (!netcam->finish) {
        if (!netcam->format_context) {      /* We must have disconnected.  Try to reconnect */
            clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);
            netcam_handler_reconnect(netcam);
            continue;
        } else {            /* We think we are connected...*/
            clock_gettime(CLOCK_REALTIME, &netcam->frame_prev_tm);
            if (netcam_read_image(netcam) < 0) {
                if (!netcam->finish) {   /* Nope.  We are not or got bad image.  Reconnect*/
                    netcam_handler_reconnect(netcam);
                }
                continue;
            }
            netcam_handler_wait(netcam);
        }
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Loop finished."),netcam->cameratype);
    netcam_shutdown(netcam);

    /* Our thread is finished - decrement motion's thread count. */
    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->motapp->threads_running--;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Exiting"),netcam->cameratype);
    netcam->handler_finished = TRUE;

    pthread_exit(NULL);
}

static int netcam_start_handler(struct ctx_netcam *netcam)
{

    int retcd, wait_counter;
    pthread_attr_t handler_attribute;

    pthread_mutex_init(&netcam->mutex, NULL);
    pthread_mutex_init(&netcam->mutex_pktarray, NULL);
    pthread_mutex_init(&netcam->mutex_transfer, NULL);

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&netcam->motapp->global_lock);
        netcam->threadnbr = ++netcam->motapp->threads_running;
    pthread_mutex_unlock(&netcam->motapp->global_lock);

    retcd = pthread_create(&netcam->thread_id, &handler_attribute, &netcam_handler, netcam);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("%s: Error starting handler thread"),netcam->cameratype);
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
            if (netcam->img_latest->ptr != NULL ) wait_counter = -1;
        pthread_mutex_unlock(&netcam->mutex);

        if (wait_counter > 0 ){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Waiting for first image from the handler."),netcam->cameratype);
            SLEEP(0,5000000);
            wait_counter--;
        }
    }

    return 0;

}

int netcam_setup(struct ctx_cam *cam)
{

    int retcd;
    int indx_cam, indx_max;
    struct ctx_netcam *netcam;

    cam->netcam = NULL;
    cam->netcam_high = NULL;

    if (netcam_set_dimensions(cam) < 0 ) return -1;

    indx_cam = 1;
    indx_max = 1;
    if (cam->conf->netcam_high_url != "") indx_max = 2;

    while (indx_cam <= indx_max){
        if (indx_cam == 1){
            cam->netcam = netcam_new_context();
            if (cam->netcam == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp context"));
                return -1;
            }
            netcam = cam->netcam;
            netcam->high_resolution = FALSE;           /* Set flag for this being the normal resolution camera */
        } else {
            cam->netcam_high = netcam_new_context();
            if (cam->netcam_high == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp high context"));
                return -1;
            }
            netcam = cam->netcam_high;
            netcam->high_resolution = TRUE;            /* Set flag for this being the high resolution camera */
        }

        netcam_null_context(netcam);

        netcam_set_parms(cam, netcam);

        if (netcam_connect(netcam) < 0) return -1;

        retcd = netcam_read_image(netcam);
        if (retcd < 0){
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Failed trying to read first image - retval:%d"), retcd);
            netcam->status = NETCAM_NOTCONNECTED;
            return -1;
        }
        /* When running dual, there seems to be contamination across norm/high with codec functions. */
        netcam_close_context(netcam);       /* Close in this thread to open it again within handler thread */
        netcam->status = NETCAM_RECONNECTING;      /* Set as reconnecting to avoid excess messages when starting */
        netcam->first_image = FALSE;             /* Set flag that we are not processing our first image */

        /* For normal resolution, we resize the image to the config parms so we do not need
         * to set the dimension parameters here (it is done in the set_parms).  For high res
         * we must get the dimensions from the first image captured
         */
        if (netcam->high_resolution){
            cam->imgs.width_high = netcam->imgsize.width;
            cam->imgs.height_high = netcam->imgsize.height;
        }

        if (netcam_start_handler(netcam) < 0 ) return -1;

        indx_cam++;
    }

    return 0;

}

int netcam_next(struct ctx_cam *cam, struct ctx_image_data *img_data)
{

    /* This is called from the motion loop thread */

    if ((cam->netcam->status == NETCAM_RECONNECTING) ||
        (cam->netcam->status == NETCAM_NOTCONNECTED)){
            return 1;
        }
    pthread_mutex_lock(&cam->netcam->mutex);
        netcam_pktarray_resize(cam, FALSE);
        memcpy(img_data->image_norm
               , cam->netcam->img_latest->ptr
               , cam->netcam->img_latest->used);
        img_data->idnbr_norm = cam->netcam->idnbr;
    pthread_mutex_unlock(&cam->netcam->mutex);

    if (cam->netcam_high){
        if ((cam->netcam_high->status == NETCAM_RECONNECTING) ||
            (cam->netcam_high->status == NETCAM_NOTCONNECTED)) return 1;

        pthread_mutex_lock(&cam->netcam_high->mutex);
            netcam_pktarray_resize(cam, TRUE);
            if (!(cam->netcam_high->high_resolution && cam->netcam_high->passthrough)) {
                memcpy(img_data->image_high
                       ,cam->netcam_high->img_latest->ptr
                       ,cam->netcam_high->img_latest->used);
            }
            img_data->idnbr_high = cam->netcam_high->idnbr;
        pthread_mutex_unlock(&cam->netcam_high->mutex);
    }

    /* Rotate images if requested */
    rotate_map(cam, img_data);

    return 0;
}

void netcam_cleanup(struct ctx_cam *cam, int init_retry_flag)
{

     /*
     * If the init_retry_flag is not set this function was
     * called while retrying the initial connection and there is
     * no camera-handler started yet and thread_running must
     * not be decremented.  This is called from motion_loop thread
     */
    int wait_counter;
    int indx_cam, indx_max;
    struct ctx_netcam *netcam;

    indx_cam = 1;
    indx_max = 1;
    if (cam->netcam_high) indx_max = 2;

    while (indx_cam <= indx_max) {
        if (indx_cam == 1){
            netcam = cam->netcam;
        } else {
            netcam = cam->netcam_high;
        }

        if (netcam){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Shutting down network camera."),netcam->cameratype);

            /* Throw the finish flag in context and wait a bit for it to finish its work and close everything
             * This is shutting down the thread so for the moment, we are not worrying about the
             * cross threading and protecting these variables with mutex's
            */
            netcam->finish = TRUE;
            netcam->interruptduration = 0;
            wait_counter = 0;
            while ((!netcam->handler_finished) && (wait_counter < 10)) {
                SLEEP(1,0);
                wait_counter++;
            }
            if (!netcam->handler_finished) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: No response from handler thread."),netcam->cameratype);
                /* Last resort.  Kill the thread. Not safe for posix but if no response, what to do...*/
                /* pthread_kill(netcam->thread_id); */
                pthread_cancel(netcam->thread_id);
                pthread_kill(netcam->thread_id, SIGVTALRM); /* This allows the cancel to be processed */
                if (!init_retry_flag){
                    pthread_mutex_lock(&netcam->motapp->global_lock);
                        netcam->motapp->threads_running--;
                    pthread_mutex_unlock(&netcam->motapp->global_lock);
                }
            }
            /* If we never connect we don't have a handler but we still need to clean up some */
            netcam_shutdown(netcam);

            pthread_mutex_destroy(&netcam->mutex);
            pthread_mutex_destroy(&netcam->mutex_pktarray);
            pthread_mutex_destroy(&netcam->mutex_transfer);

            free(netcam);
            netcam = NULL;
            if (indx_cam == 1){
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("Norm: Shut down complete."));
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("High: Shut down complete."));
            }
        }
        indx_cam++;
    }
    cam->netcam = NULL;
    cam->netcam_high = NULL;
    cam->running_cam = FALSE;
}

