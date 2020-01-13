/***********************************************************
 *  In the top section are the functions that are used
 *  when processing the camera feed.  Since these functions
 *  are internal to the RTSP module, and many require FFmpeg
 *  structures in their declarations, they are within the
 *  HAVE_FFMPEG block that eliminates them entirely when
 *  FFmpeg is not present.
 *
 *  The functions:
 *      netcam_rtsp_setup
 *      netcam_rtsp_next
 *      netcam_rtsp_cleanup
 *  are called from video_common.c therefore must be defined even
 *  if FFmpeg is not present.  They must also not have FFmpeg
 *  structures in the declarations.  Simple error
 *  messages are raised if called when no FFmpeg is found.
 *
 *  Additional note:  Although this module is called netcam_rtsp,
 *  it actually handles more camera types than just rtsp.
 *  Within its current construct, it could be set up to handle
 *  whatever types of capture devices that ffmpeg can use.
 *  As of this writing it includes rtsp, http, files and v4l2.
 *
 ***********************************************************/

#include <stdio.h>
#include "translate.h"
#include "rotate.h"    /* already includes motion.h */
#include "netcam_rtsp.h"
#include "video_v4l2.h"  /* Needed to validate palette for v4l2 via netcam */

#ifdef HAVE_FFMPEG

#include "ffmpeg.h"

static int netcam_rtsp_check_pixfmt(struct rtsp_context *rtsp_data){
    /* Determine if the format is YUV420P */
    int retcd;

    retcd = -1;
    if ((rtsp_data->codec_context->pix_fmt == MY_PIX_FMT_YUV420P) ||
        (rtsp_data->codec_context->pix_fmt == MY_PIX_FMT_YUVJ420P)) retcd = 0;

    return retcd;

}

static void netcam_rtsp_pktarray_free(struct rtsp_context *rtsp_data){

    int indx;
    pthread_mutex_lock(&rtsp_data->mutex_pktarray);
        if (rtsp_data->pktarray_size > 0){
            for(indx = 0; indx < rtsp_data->pktarray_size; indx++) {
                if (rtsp_data->pktarray[indx].packet.data != NULL) {
                    my_packet_unref(rtsp_data->pktarray[indx].packet);
                }
            }
        }
        free(rtsp_data->pktarray);
        rtsp_data->pktarray = NULL;
        rtsp_data->pktarray_size = 0;
        rtsp_data->pktarray_index = -1;
    pthread_mutex_unlock(&rtsp_data->mutex_pktarray);

}

static void netcam_rtsp_null_context(struct rtsp_context *rtsp_data){

    rtsp_data->swsctx          = NULL;
    rtsp_data->swsframe_in     = NULL;
    rtsp_data->swsframe_out    = NULL;
    rtsp_data->frame           = NULL;
    rtsp_data->codec_context   = NULL;
    rtsp_data->format_context  = NULL;
    rtsp_data->transfer_format = NULL;

}

static void netcam_rtsp_close_context(struct rtsp_context *rtsp_data){

    if (rtsp_data->swsctx       != NULL) sws_freeContext(rtsp_data->swsctx);
    if (rtsp_data->swsframe_in  != NULL) my_frame_free(rtsp_data->swsframe_in);
    if (rtsp_data->swsframe_out != NULL) my_frame_free(rtsp_data->swsframe_out);
    if (rtsp_data->frame        != NULL) my_frame_free(rtsp_data->frame);
    if (rtsp_data->pktarray     != NULL) netcam_rtsp_pktarray_free(rtsp_data);
    if (rtsp_data->codec_context    != NULL) my_avcodec_close(rtsp_data->codec_context);
    if (rtsp_data->format_context   != NULL) avformat_close_input(&rtsp_data->format_context);
    if (rtsp_data->transfer_format != NULL) avformat_close_input(&rtsp_data->transfer_format);
    netcam_rtsp_null_context(rtsp_data);

}

static void netcam_rtsp_pktarray_resize(struct context *cnt, int is_highres){
    /* This is called from netcam_rtsp_next and is on the motion loop thread
     * The rtsp_data->mutex is locked around the call to this function.
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
    struct rtsp_context  *rtsp_data;
    struct packet_item   *tmp;
    int                   newsize;

    if (is_highres){
        idnbr_last = cnt->imgs.image_ring[cnt->imgs.image_ring_out].idnbr_high;
        idnbr_first = cnt->imgs.image_ring[cnt->imgs.image_ring_in].idnbr_high;
        rtsp_data = cnt->rtsp_high;
    } else {
        idnbr_last = cnt->imgs.image_ring[cnt->imgs.image_ring_out].idnbr_norm;
        idnbr_first = cnt->imgs.image_ring[cnt->imgs.image_ring_in].idnbr_norm;
        rtsp_data = cnt->rtsp;
    }

    if (!rtsp_data->passthrough) return;

    /* The 30 is arbitrary */
    /* Double the size plus double last diff so we don't catch our tail */
    newsize =((idnbr_first - idnbr_last) * 2 ) + ((rtsp_data->idnbr - idnbr_last ) * 2);
    if (newsize < 30) newsize = 30;

    pthread_mutex_lock(&rtsp_data->mutex_pktarray);
        if ((rtsp_data->pktarray_size < newsize) ||  (rtsp_data->pktarray_size < 30)){
            tmp = mymalloc(newsize * sizeof(struct packet_item));
            if (rtsp_data->pktarray_size > 0 ){
                memcpy(tmp, rtsp_data->pktarray, sizeof(struct packet_item) * rtsp_data->pktarray_size);
            }
            for(indx = rtsp_data->pktarray_size; indx < newsize; indx++) {
                av_init_packet(&tmp[indx].packet);
                tmp[indx].packet.data=NULL;
                tmp[indx].packet.size=0;
                tmp[indx].idnbr = 0;
                tmp[indx].iskey = FALSE;
                tmp[indx].iswritten = FALSE;
            }

            if (rtsp_data->pktarray != NULL) free(rtsp_data->pktarray);
            rtsp_data->pktarray = tmp;
            rtsp_data->pktarray_size = newsize;

            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Resized packet array to %d"), rtsp_data->cameratype,newsize);
        }
    pthread_mutex_unlock(&rtsp_data->mutex_pktarray);

}

static void netcam_rtsp_pktarray_add(struct rtsp_context *rtsp_data){

    int indx_next;
    int retcd;
    char errstr[128];

    pthread_mutex_lock(&rtsp_data->mutex_pktarray);

        if (rtsp_data->pktarray_size == 0){
            pthread_mutex_unlock(&rtsp_data->mutex_pktarray);
            return;
        }

        /* Recall pktarray_size is one based but pktarray is zero based */
        if (rtsp_data->pktarray_index == (rtsp_data->pktarray_size-1) ){
            indx_next = 0;
        } else {
            indx_next = rtsp_data->pktarray_index + 1;
        }

        rtsp_data->pktarray[indx_next].idnbr = rtsp_data->idnbr;

        my_packet_unref(rtsp_data->pktarray[indx_next].packet);
        av_init_packet(&rtsp_data->pktarray[indx_next].packet);
        rtsp_data->pktarray[indx_next].packet.data = NULL;
        rtsp_data->pktarray[indx_next].packet.size = 0;

        retcd = my_copy_packet(&rtsp_data->pktarray[indx_next].packet, &rtsp_data->packet_recv);
        if ((rtsp_data->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_copy_packet: %s ,Interrupt: %s")
                ,rtsp_data->cameratype
                ,errstr, rtsp_data->interrupted ? _("True"):_("False"));
            my_packet_unref(rtsp_data->pktarray[indx_next].packet);
            rtsp_data->pktarray[indx_next].packet.data = NULL;
            rtsp_data->pktarray[indx_next].packet.size = 0;
        }

        if (rtsp_data->pktarray[indx_next].packet.flags & AV_PKT_FLAG_KEY) {
            rtsp_data->pktarray[indx_next].iskey = TRUE;
        } else {
            rtsp_data->pktarray[indx_next].iskey = FALSE;
        }
        rtsp_data->pktarray[indx_next].iswritten = FALSE;
        rtsp_data->pktarray[indx_next].timestamp_tv.tv_sec = rtsp_data->img_recv->image_time.tv_sec;
        rtsp_data->pktarray[indx_next].timestamp_tv.tv_usec = rtsp_data->img_recv->image_time.tv_usec;
        rtsp_data->pktarray_index = indx_next;
    pthread_mutex_unlock(&rtsp_data->mutex_pktarray);

}


/* netcam_rtsp_decode_video
 *
 * Return values:
 *   <0 error
 *   0 invalid but continue
 *   1 valid data
 */
static int netcam_rtsp_decode_video(struct rtsp_context *rtsp_data){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))

    int retcd;
    char errstr[128];

    /* The Invalid data problem comes frequently.  Usually at startup of rtsp cameras.
     * We now ignore those packets so this function would need to fail on a different error.
     * We should consider adding a maximum count of these errors and reset every time
     * we get a good image.
     */
    if (rtsp_data->finish) return 0;   /* This just speeds up the shutdown time */

    retcd = avcodec_send_packet(rtsp_data->codec_context, &rtsp_data->packet_recv);
    if ((rtsp_data->interrupted) || (rtsp_data->finish)) return -1;
    if (retcd == AVERROR_INVALIDDATA) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Ignoring packet with invalid data"));
        return 0;
    }
    if (retcd < 0 && retcd != AVERROR_EOF){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Error sending packet to codec: %s"), errstr);
        return -1;
    }

    retcd = avcodec_receive_frame(rtsp_data->codec_context, rtsp_data->frame);
    if ((rtsp_data->interrupted) || (rtsp_data->finish)) return -1;

    if (retcd == AVERROR(EAGAIN)) return 0;

    if (retcd == AVERROR_INVALIDDATA) {
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Ignoring packet with invalid data"));
        return 0;
    }

    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Error receiving frame from codec: %s"), errstr);
        return -1;
    }

    return 1;

#else

    int retcd;
    int check = 0;
    char errstr[128];

    if (rtsp_data->finish) return 0;   /* This just speeds up the shutdown time */

    retcd = avcodec_decode_video2(rtsp_data->codec_context, rtsp_data->frame, &check, &rtsp_data->packet_recv);
    if ((rtsp_data->interrupted) || (rtsp_data->finish)) return -1;

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

static int netcam_rtsp_decode_packet(struct rtsp_context *rtsp_data){

    int frame_size;
    int retcd;

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    retcd = netcam_rtsp_decode_video(rtsp_data);
    if (retcd <= 0) return retcd;

    frame_size = my_image_get_buffer_size(rtsp_data->codec_context->pix_fmt
                                          ,rtsp_data->codec_context->width
                                          ,rtsp_data->codec_context->height);

    netcam_check_buffsize(rtsp_data->img_recv, frame_size);
    netcam_check_buffsize(rtsp_data->img_latest, frame_size);

    retcd = my_image_copy_to_buffer(rtsp_data->frame
                                    ,(uint8_t *)rtsp_data->img_recv->ptr
                                    ,rtsp_data->codec_context->pix_fmt
                                    ,rtsp_data->codec_context->width
                                    ,rtsp_data->codec_context->height
                                    ,frame_size);
    if ((retcd < 0) || (rtsp_data->interrupted)) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("Error decoding video packet: Copying to buffer"));
        return -1;
    }

    rtsp_data->img_recv->used = frame_size;

    return frame_size;
}

static void netcam_rtsp_decoder_error(struct rtsp_context *rtsp_data, int retcd, const char* fnc_nm){

    char errstr[128];

    if (retcd < 0){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: %s: %s,Interrupt %s")
            ,rtsp_data->cameratype,fnc_nm, errstr, rtsp_data->interrupted ? _("True"):_("False"));
    } else {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: %s: Failed,Interrupt %s"),rtsp_data->cameratype
            ,fnc_nm, rtsp_data->interrupted ? _("True"):_("False"));
    }

    if (rtsp_data->decoder_nm != NULL){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Ignoring user requested decoder %s"),rtsp_data->cameratype
            ,rtsp_data->decoder_nm);
        free(rtsp_data->cnt->netcam_decoder);
        rtsp_data->cnt->netcam_decoder = NULL;
        rtsp_data->decoder_nm = NULL;
    }

}

static int netcam_rtsp_open_codec(struct rtsp_context *rtsp_data){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    int retcd;
    AVStream *st;
    AVCodec *decoder = NULL;

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    retcd = av_find_best_stream(rtsp_data->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if ((retcd < 0) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, retcd, "av_find_best_stream");
        return -1;
    }
    rtsp_data->video_stream_index = retcd;
    st = rtsp_data->format_context->streams[rtsp_data->video_stream_index];

    if (rtsp_data->decoder_nm != NULL){
        decoder = avcodec_find_decoder_by_name(rtsp_data->decoder_nm);
        if (decoder == NULL) {
            netcam_rtsp_decoder_error(rtsp_data, 0, "avcodec_find_decoder_by_name");
        } else {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,_("%s: Using decoder %s")
                ,rtsp_data->cameratype,rtsp_data->decoder_nm);
        }
    }

    if (decoder == NULL) {
        decoder = avcodec_find_decoder(st->codecpar->codec_id);
    }

    if ((decoder == NULL) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, 0, "avcodec_find_decoder");
        return -1;
    }

    rtsp_data->codec_context = avcodec_alloc_context3(decoder);
    if ((rtsp_data->codec_context == NULL) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, 0, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_parameters_to_context(rtsp_data->codec_context, st->codecpar);
    if ((retcd < 0) || (rtsp_data->interrupted)) {
        netcam_rtsp_decoder_error(rtsp_data, retcd, "avcodec_alloc_context3");
        return -1;
    }

    retcd = avcodec_open2(rtsp_data->codec_context, decoder, NULL);
    if ((retcd < 0) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, retcd, "avcodec_open2");
        return -1;
    }

    return 0;
#else

    int retcd;
    AVStream *st;
    AVCodec *decoder = NULL;

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    retcd = av_find_best_stream(rtsp_data->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if ((retcd < 0) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, retcd, "av_find_best_stream");
        return -1;
    }
    rtsp_data->video_stream_index = retcd;
    st = rtsp_data->format_context->streams[rtsp_data->video_stream_index];

    rtsp_data->codec_context = st->codec;
    decoder = avcodec_find_decoder(rtsp_data->codec_context->codec_id);
    if ((decoder == NULL) || (rtsp_data->interrupted)) {
        netcam_rtsp_decoder_error(rtsp_data, 0, "avcodec_find_decoder");
        return -1;
     }
    retcd = avcodec_open2(rtsp_data->codec_context, decoder, NULL);
    if ((retcd < 0) || (rtsp_data->interrupted)){
        netcam_rtsp_decoder_error(rtsp_data, retcd, "avcodec_open2");
        return -1;
    }

    return 0;
#endif


}

static struct rtsp_context *rtsp_new_context(void){
    struct rtsp_context *ret;

    /* Note that mymalloc will exit on any problem. */
    ret = mymalloc(sizeof(struct rtsp_context));

    memset(ret, 0, sizeof(struct rtsp_context));

    return ret;
}

static int netcam_rtsp_interrupt(void *ctx){
    struct rtsp_context *rtsp_data = ctx;

    if (rtsp_data->finish){
        rtsp_data->interrupted = TRUE;
        return TRUE;
    }

    if (rtsp_data->status == RTSP_CONNECTED) {
        return FALSE;
    } else if (rtsp_data->status == RTSP_READINGIMAGE) {
        if (gettimeofday(&rtsp_data->interruptcurrenttime, NULL) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
        }
        if ((rtsp_data->interruptcurrenttime.tv_sec - rtsp_data->interruptstarttime.tv_sec ) > rtsp_data->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera reading (%s) timed out")
                , rtsp_data->cameratype, rtsp_data->camera_name);
            rtsp_data->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * rtsp_connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        if (gettimeofday(&rtsp_data->interruptcurrenttime, NULL) < 0) {
            MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
        }
        if ((rtsp_data->interruptcurrenttime.tv_sec - rtsp_data->interruptstarttime.tv_sec ) > rtsp_data->interruptduration){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera (%s) timed out")
                , rtsp_data->cameratype, rtsp_data->camera_name);
            rtsp_data->interrupted = TRUE;
            return TRUE;
        } else{
            return FALSE;
        }
    }

    /* should not be possible to get here */
    return FALSE;
}

static int netcam_rtsp_resize(struct rtsp_context *rtsp_data){

    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    retcd=my_image_fill_arrays(
        rtsp_data->swsframe_in
        ,(uint8_t*)rtsp_data->img_recv->ptr
        ,rtsp_data->codec_context->pix_fmt
        ,rtsp_data->codec_context->width
        ,rtsp_data->codec_context->height);
    if (retcd < 0) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error allocating picture in: %s"), errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    buffer_out=(uint8_t *)av_malloc(rtsp_data->swsframe_size*sizeof(uint8_t));

    retcd=my_image_fill_arrays(
        rtsp_data->swsframe_out
        ,buffer_out
        ,MY_PIX_FMT_YUV420P
        ,rtsp_data->imgsize.width
        ,rtsp_data->imgsize.height);
    if (retcd < 0) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error allocating picture out: %s"), errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    retcd = sws_scale(
        rtsp_data->swsctx
        ,(const uint8_t* const *)rtsp_data->swsframe_in->data
        ,rtsp_data->swsframe_in->linesize
        ,0
        ,rtsp_data->codec_context->height
        ,rtsp_data->swsframe_out->data
        ,rtsp_data->swsframe_out->linesize);
    if (retcd < 0) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error resizing/reformatting: %s"), errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    retcd=my_image_copy_to_buffer(
         rtsp_data->swsframe_out
        ,(uint8_t *)rtsp_data->img_recv->ptr
        ,MY_PIX_FMT_YUV420P
        ,rtsp_data->imgsize.width
        ,rtsp_data->imgsize.height
        ,rtsp_data->swsframe_size);
    if (retcd < 0) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Error putting frame into output buffer: %s"), errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }
    rtsp_data->img_recv->used = rtsp_data->swsframe_size;

    av_free(buffer_out);

    return 0;

}

static int netcam_rtsp_read_image(struct rtsp_context *rtsp_data){

    int  size_decoded;
    int  retcd;
    int  haveimage;
    char errstr[128];
    netcam_buff *xchg;

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    av_init_packet(&rtsp_data->packet_recv);
    rtsp_data->packet_recv.data = NULL;
    rtsp_data->packet_recv.size = 0;

    rtsp_data->interrupted=FALSE;
    if (gettimeofday(&rtsp_data->interruptstarttime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }
    rtsp_data->interruptduration = 10;

    rtsp_data->status = RTSP_READINGIMAGE;
    rtsp_data->img_recv->used = 0;
    size_decoded = 0;
    haveimage = FALSE;

    while ((!haveimage) && (!rtsp_data->interrupted)) {
        retcd = av_read_frame(rtsp_data->format_context, &rtsp_data->packet_recv);
        if ((rtsp_data->interrupted) || (retcd < 0)) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: av_read_frame: %s ,Interrupt: %s")
                ,rtsp_data->cameratype
                ,errstr, rtsp_data->interrupted ? _("True"):_("False"));
            my_packet_unref(rtsp_data->packet_recv);
            netcam_rtsp_close_context(rtsp_data);
            return -1;
        }

        if (rtsp_data->packet_recv.stream_index == rtsp_data->video_stream_index){
            /* For a high resolution pass-through we don't decode the image */
            if (rtsp_data->high_resolution && rtsp_data->passthrough){
                if (rtsp_data->packet_recv.data != NULL) size_decoded = 1;
            } else {
                size_decoded = netcam_rtsp_decode_packet(rtsp_data);
            }
        }

        if (size_decoded > 0 ){
            haveimage = TRUE;
        } else if (size_decoded == 0){
            /* Did not fail, just didn't get anything.  Try again */
            my_packet_unref(rtsp_data->packet_recv);
            av_init_packet(&rtsp_data->packet_recv);
            rtsp_data->packet_recv.data = NULL;
            rtsp_data->packet_recv.size = 0;
        } else {
            my_packet_unref(rtsp_data->packet_recv);
            netcam_rtsp_close_context(rtsp_data);
            return -1;
        }
    }
    if (gettimeofday(&rtsp_data->img_recv->image_time, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }

    /* Skip status change on our first image to keep the "next" function waiting
     * until the handler thread gets going
     */
    if (!rtsp_data->first_image) rtsp_data->status = RTSP_CONNECTED;

    /* Skip resize/pix format for high pass-through */
    if (!(rtsp_data->high_resolution && rtsp_data->passthrough)){
        if ((rtsp_data->imgsize.width  != rtsp_data->codec_context->width) ||
            (rtsp_data->imgsize.height != rtsp_data->codec_context->height) ||
            (netcam_rtsp_check_pixfmt(rtsp_data) != 0) ){
            if (netcam_rtsp_resize(rtsp_data) < 0){
                my_packet_unref(rtsp_data->packet_recv);
                netcam_rtsp_close_context(rtsp_data);
                return -1;
            }
        }
    }

    pthread_mutex_lock(&rtsp_data->mutex);
        rtsp_data->idnbr++;
        if (rtsp_data->passthrough) netcam_rtsp_pktarray_add(rtsp_data);
        if (!(rtsp_data->high_resolution && rtsp_data->passthrough)) {
            xchg = rtsp_data->img_latest;
            rtsp_data->img_latest = rtsp_data->img_recv;
            rtsp_data->img_recv = xchg;
        }
    pthread_mutex_unlock(&rtsp_data->mutex);

    my_packet_unref(rtsp_data->packet_recv);

    if (rtsp_data->format_context->streams[rtsp_data->video_stream_index]->avg_frame_rate.den > 0){
        rtsp_data->src_fps = (
            (rtsp_data->format_context->streams[rtsp_data->video_stream_index]->avg_frame_rate.num /
            rtsp_data->format_context->streams[rtsp_data->video_stream_index]->avg_frame_rate.den) +
            0.5);
    }

    return 0;
}

static int netcam_rtsp_ntc(struct rtsp_context *rtsp_data){

    if ((rtsp_data->finish) || (!rtsp_data->first_image)) return 0;

    if ((rtsp_data->imgsize.width  != rtsp_data->codec_context->width) ||
        (rtsp_data->imgsize.height != rtsp_data->codec_context->height) ||
        (netcam_rtsp_check_pixfmt(rtsp_data) != 0) ){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        if ((rtsp_data->imgsize.width  != rtsp_data->codec_context->width) ||
            (rtsp_data->imgsize.height != rtsp_data->codec_context->height)) {
            if (netcam_rtsp_check_pixfmt(rtsp_data) != 0) {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures in a different"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("size than specified in the config and also a "));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("different picture format.  The picture is being"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("transcoded to YUV420P and into the size requested"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("in the config file.  If possible change netcam to"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("be in YUV420P format and the size requested in the"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("config to possibly lower CPU usage."));
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The network camera is sending pictures in a different"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("size than specified in the configuration file."));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The picture is being transcoded into the size "));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("requested in the configuration.  If possible change"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("netcam or configuration to indicate the same size"));
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("to possibly lower CPU usage."));
            }
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("Netcam: %d x %d => Config: %d x %d")
            ,rtsp_data->codec_context->width,rtsp_data->codec_context->height
            ,rtsp_data->imgsize.width,rtsp_data->imgsize.height);
        } else {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("The image sent is being "));
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("trancoded to YUV420P.  If possible change netcam "));
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, _("picture format to YUV420P to possibly lower CPU usage."));
        }
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "******************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "");
    }

    return 0;

}

static int netcam_rtsp_open_sws(struct rtsp_context *rtsp_data){

    if (rtsp_data->finish) return -1;   /* This just speeds up the shutdown time */

    rtsp_data->swsframe_in = my_frame_alloc();
    if (rtsp_data->swsframe_in == NULL) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate swsframe_in."));
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    rtsp_data->swsframe_out = my_frame_alloc();
    if (rtsp_data->swsframe_out == NULL) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate swsframe_out."));
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    rtsp_data->swsctx = sws_getContext(
         rtsp_data->codec_context->width
        ,rtsp_data->codec_context->height
        ,rtsp_data->codec_context->pix_fmt
        ,rtsp_data->imgsize.width
        ,rtsp_data->imgsize.height
        ,MY_PIX_FMT_YUV420P
        ,SWS_BICUBIC,NULL,NULL,NULL);
    if (rtsp_data->swsctx == NULL) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to allocate scaling context."));
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    rtsp_data->swsframe_size = my_image_get_buffer_size(
            MY_PIX_FMT_YUV420P
            ,rtsp_data->imgsize.width
            ,rtsp_data->imgsize.height);
    if (rtsp_data->swsframe_size <= 0) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Error determining size of frame out"));
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    netcam_check_buffsize(rtsp_data->img_recv, rtsp_data->swsframe_size);
    netcam_check_buffsize(rtsp_data->img_latest, rtsp_data->swsframe_size);

    return 0;

}

static void netcam_rtsp_set_http(struct rtsp_context *rtsp_data){

    rtsp_data->format_context->iformat = av_find_input_format("mjpeg");
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Setting http input_format mjpeg"),rtsp_data->cameratype);

}

static void netcam_rtsp_set_rtsp(struct rtsp_context *rtsp_data){

    if (rtsp_data->rtsp_uses_tcp) {
        av_dict_set(&rtsp_data->opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&rtsp_data->opts, "allowed_media_types", "video", 0);
        if (rtsp_data->status == RTSP_NOTCONNECTED)
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Setting rtsp transport to tcp"),rtsp_data->cameratype);
    } else {
        av_dict_set(&rtsp_data->opts, "rtsp_transport", "udp", 0);
        av_dict_set(&rtsp_data->opts, "max_delay", "500000", 0);  /* 100000 is the default */
        if (rtsp_data->status == RTSP_NOTCONNECTED)
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Setting rtsp transport to udp"),rtsp_data->cameratype);
    }
}

static void netcam_rtsp_set_file(struct rtsp_context *rtsp_data){

    /* This is a place holder for the moment.  We will add into
     * this function any options that must be set for ffmpeg to
     * read a particular file.  To date, it does not need any
     * additional options and works fine with defaults.
     */
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Setting attributes to read file"),rtsp_data->cameratype);

}

static void netcam_rtsp_set_v4l2(struct rtsp_context *rtsp_data){

    char optsize[10], optfmt[10], optfps[10];
    char *fourcc;

    rtsp_data->format_context->iformat = av_find_input_format("video4linux2");

    fourcc=malloc(5*sizeof(char));

    v4l2_palette_fourcc(rtsp_data->v4l2_palette, fourcc);

    if (strcmp(fourcc,"MJPG") == 0) {
        if (v4l2_palette_valid(rtsp_data->path,rtsp_data->v4l2_palette)){
            sprintf(optfmt, "%s","mjpeg");
            av_dict_set(&rtsp_data->opts, "input_format", optfmt, 0);
        } else {
            sprintf(optfmt, "%s","default");
        }
    } else if (strcmp(fourcc,"H264") == 0){
        if (v4l2_palette_valid(rtsp_data->path,rtsp_data->v4l2_palette)){
            sprintf(optfmt, "%s","h264");
            av_dict_set(&rtsp_data->opts, "input_format", optfmt, 0);
        } else {
            sprintf(optfmt, "%s","default");
        }
    } else {
        sprintf(optfmt, "%s","default");
    }

    if (strcmp(optfmt,"default") != 0) {
        if (v4l2_parms_valid(rtsp_data->path
                             ,rtsp_data->v4l2_palette
                             ,rtsp_data->framerate
                             ,rtsp_data->imgsize.width
                             ,rtsp_data->imgsize.height)) {
            sprintf(optfps, "%d",rtsp_data->framerate);
            av_dict_set(&rtsp_data->opts, "framerate", optfps, 0);

            sprintf(optsize, "%dx%d",rtsp_data->imgsize.width,rtsp_data->imgsize.height);
            av_dict_set(&rtsp_data->opts, "video_size", optsize, 0);
        } else {
            sprintf(optfps, "%s","default");
            sprintf(optsize, "%s","default");
        }
    } else {
        sprintf(optfps, "%s","default");
        sprintf(optsize, "%s","default");
    }

    if (rtsp_data->status == RTSP_NOTCONNECTED){
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Requested v4l2_palette option: %d")
            ,rtsp_data->cameratype,rtsp_data->v4l2_palette);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Requested FOURCC code: %s"),rtsp_data->cameratype,fourcc);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 input_format: %s"),rtsp_data->cameratype,optfmt);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 framerate: %s"),rtsp_data->cameratype, optfps);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Setting v4l2 video_size: %s"),rtsp_data->cameratype, optsize);
    }

    free(fourcc);

}

static void netcam_rtsp_set_path (struct context *cnt, struct rtsp_context *rtsp_data ) {

    char        *userpass = NULL;
    struct url_t url;

    rtsp_data->path = NULL;

    memset(&url, 0, sizeof(url));

    if (rtsp_data->high_resolution){
        netcam_url_parse(&url, cnt->conf.netcam_highres);
    } else {
        netcam_url_parse(&url, cnt->conf.netcam_url);
    }

    if (cnt->conf.netcam_proxy) {
        MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO
            ,_("Proxies not supported using for %s"),url.service);
    }

    if (cnt->conf.netcam_userpass != NULL) {
        userpass = mystrdup(cnt->conf.netcam_userpass);
    } else if (url.userpass != NULL) {
        userpass = mystrdup(url.userpass);
    }

    if (strcmp(url.service, "v4l2") == 0) {
        rtsp_data->path = mymalloc(strlen(url.path) + 1);
        sprintf(rtsp_data->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up v4l2 via ffmpeg netcam"));
    } else if (strcmp(url.service, "file") == 0) {
        rtsp_data->path = mymalloc(strlen(url.path) + 1);
        sprintf(rtsp_data->path, "%s",url.path);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("Setting up file via ffmpeg netcam"));
    } else {
        if (!strcmp(url.service, "mjpeg")) {
            sprintf(url.service, "%s","http");
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up http via ffmpeg netcam"));
        } else {
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("Setting up %s via ffmpeg netcam"),url.service);
        }
        if (userpass != NULL) {
            rtsp_data->path = mymalloc(strlen(url.service) + 3 + strlen(userpass)
                  + 1 + strlen(url.host) + 6 + strlen(url.path) + 2 );
            sprintf((char *)rtsp_data->path, "%s://%s@%s:%d%s",
                    url.service, userpass, url.host, url.port, url.path);
        } else {
            rtsp_data->path = mymalloc(strlen(url.service) + 3 + strlen(url.host)
                  + 6 + strlen(url.path) + 2);
            sprintf((char *)rtsp_data->path, "%s://%s:%d%s", url.service,
                url.host, url.port, url.path);
        }
    }

    sprintf(rtsp_data->service, "%s",url.service);

    netcam_url_free(&url);
    if (userpass) free (userpass);

}

static void netcam_rtsp_set_parms (struct context *cnt, struct rtsp_context *rtsp_data ) {
    /* Set the parameters to be used with our camera */

    if (rtsp_data->high_resolution) {
        rtsp_data->imgsize.width = 0;
        rtsp_data->imgsize.height = 0;
        snprintf(rtsp_data->cameratype,29, "%s",_("High resolution"));
    } else {
        rtsp_data->imgsize.width = cnt->conf.width;
        rtsp_data->imgsize.height = cnt->conf.height;
        snprintf(rtsp_data->cameratype,29, "%s",_("Normal resolution"));
    }
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("Setting up %s stream."),rtsp_data->cameratype);

    util_check_passthrough(cnt); /* In case it was turned on via webcontrol */
    rtsp_data->status = RTSP_NOTCONNECTED;
    rtsp_data->rtsp_uses_tcp =cnt->conf.netcam_use_tcp;
    rtsp_data->v4l2_palette = cnt->conf.v4l2_palette;
    rtsp_data->framerate = cnt->conf.framerate;
    rtsp_data->src_fps =  cnt->conf.framerate; /* Default to conf fps */
    rtsp_data->conf = &cnt->conf;
    rtsp_data->camera_name = cnt->conf.camera_name;
    rtsp_data->img_recv = mymalloc(sizeof(netcam_buff));
    rtsp_data->img_recv->ptr = mymalloc(NETCAM_BUFFSIZE);
    rtsp_data->img_latest = mymalloc(sizeof(netcam_buff));
    rtsp_data->img_latest->ptr = mymalloc(NETCAM_BUFFSIZE);
    rtsp_data->pktarray_size = 0;
    rtsp_data->pktarray_index = -1;
    rtsp_data->pktarray = NULL;
    rtsp_data->handler_finished = TRUE;
    rtsp_data->first_image = TRUE;
    rtsp_data->reconnect_count = 0;
    rtsp_data->decoder_nm = cnt->netcam_decoder;
    rtsp_data->cnt = cnt;

    snprintf(rtsp_data->threadname, 15, "%s",_("Unknown"));

    if (gettimeofday(&rtsp_data->interruptstarttime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }
    if (gettimeofday(&rtsp_data->interruptcurrenttime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }
    /* If this is the norm and we have a highres, then disable passthru on the norm */
    if ((!rtsp_data->high_resolution) &&
        (cnt->conf.netcam_highres)) {
        rtsp_data->passthrough = FALSE;
    } else {
        rtsp_data->passthrough = util_check_passthrough(cnt);
    }
    rtsp_data->interruptduration = 5;
    rtsp_data->interrupted = FALSE;

    if (gettimeofday(&rtsp_data->frame_curr_tm, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }
    if (gettimeofday(&rtsp_data->frame_prev_tm, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }

    netcam_rtsp_set_path(cnt, rtsp_data);

}

static int netcam_rtsp_set_dimensions (struct context *cnt) {

    cnt->imgs.width = 0;
    cnt->imgs.height = 0;
    cnt->imgs.size_norm = 0;
    cnt->imgs.motionsize = 0;

    cnt->imgs.width_high  = 0;
    cnt->imgs.height_high = 0;
    cnt->imgs.size_high   = 0;

    if (cnt->conf.width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image width (%d) requested is not modulo 8."), cnt->conf.width);
        cnt->conf.width = cnt->conf.width - (cnt->conf.width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting width to next higher multiple of 8 (%d)."), cnt->conf.width);
    }
    if (cnt->conf.height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Image height (%d) requested is not modulo 8."), cnt->conf.height);
        cnt->conf.height = cnt->conf.height - (cnt->conf.height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
            ,_("Adjusting height to next higher multiple of 8 (%d)."), cnt->conf.height);
    }

    /* Fill in camera details into context structure. */
    cnt->imgs.width = cnt->conf.width;
    cnt->imgs.height = cnt->conf.height;
    cnt->imgs.size_norm = (cnt->conf.width * cnt->conf.height * 3) / 2;
    cnt->imgs.motionsize = cnt->conf.width * cnt->conf.height;

    return 0;
}

static int netcam_rtsp_copy_stream(struct rtsp_context *rtsp_data){
    /* Make a static copy of the stream information for use in passthrough processing */
#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    AVStream  *transfer_stream, *stream_in;
    int        retcd;

    pthread_mutex_lock(&rtsp_data->mutex_transfer);
        if (rtsp_data->transfer_format != NULL) avformat_close_input(&rtsp_data->transfer_format);
        rtsp_data->transfer_format = avformat_alloc_context();
        transfer_stream = avformat_new_stream(rtsp_data->transfer_format, NULL);
        stream_in = rtsp_data->format_context->streams[rtsp_data->video_stream_index];
        retcd = avcodec_parameters_copy(transfer_stream->codecpar, stream_in->codecpar);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("Unable to copy codec parameters"));
            pthread_mutex_unlock(&rtsp_data->mutex_transfer);
            return -1;
        }
        transfer_stream->time_base         = stream_in->time_base;
    pthread_mutex_unlock(&rtsp_data->mutex_transfer);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
    return 0;
#elif (LIBAVFORMAT_VERSION_MAJOR >= 55)

    AVStream  *transfer_stream, *stream_in;
    int        retcd;

    pthread_mutex_lock(&rtsp_data->mutex_transfer);
        if (rtsp_data->transfer_format != NULL) avformat_close_input(&rtsp_data->transfer_format);
        rtsp_data->transfer_format = avformat_alloc_context();
        transfer_stream = avformat_new_stream(rtsp_data->transfer_format, NULL);
        stream_in = rtsp_data->format_context->streams[rtsp_data->video_stream_index];
        retcd = avcodec_copy_context(transfer_stream->codec, stream_in->codec);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Unable to copy codec parameters"));
            pthread_mutex_unlock(&rtsp_data->mutex_transfer);
            return -1;
        }
        transfer_stream->time_base         = stream_in->time_base;
    pthread_mutex_unlock(&rtsp_data->mutex_transfer);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO, _("Stream copied for pass-through"));
    return 0;
#else
    /* This is disabled in the util_check_passthrough but we need it here for compiling */
    if (rtsp_data != NULL) MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("ffmpeg too old"));
    return -1;
#endif

}

static int netcam_rtsp_open_context(struct rtsp_context *rtsp_data){

    int  retcd;
    char errstr[128];

    if (rtsp_data->finish) return -1;

    if (rtsp_data->path == NULL) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("Null path passed to connect"));
        }
        return -1;
    }

    rtsp_data->opts = NULL;
    rtsp_data->format_context = avformat_alloc_context();
    rtsp_data->format_context->interrupt_callback.callback = netcam_rtsp_interrupt;
    rtsp_data->format_context->interrupt_callback.opaque = rtsp_data;
    rtsp_data->interrupted = FALSE;

    if (gettimeofday(&rtsp_data->interruptstarttime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }

    rtsp_data->interruptduration = 20;

    if (strncmp(rtsp_data->service, "http", 4) == 0 ){
        netcam_rtsp_set_http(rtsp_data);
    } else if (strncmp(rtsp_data->service, "rtsp", 4) == 0 ){
        netcam_rtsp_set_rtsp(rtsp_data);
    } else if (strncmp(rtsp_data->service, "rtmp", 4) == 0 ){
        netcam_rtsp_set_rtsp(rtsp_data);
    } else if (strncmp(rtsp_data->service, "v4l2", 4) == 0 ){
        netcam_rtsp_set_v4l2(rtsp_data);
    } else if (strncmp(rtsp_data->service, "file", 4) == 0 ){
        netcam_rtsp_set_file(rtsp_data);
    } else {
        av_dict_free(&rtsp_data->opts);
        MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Invalid camera service"), rtsp_data->cameratype);
        return -1;
    }
    /*
     * There is not many av functions above this (av_dict_free?) but we are not getting clean
     * interrupts or shutdowns via valgrind and they all point to issues with the avformat_open_input
     * right below so we make sure that we are not in a interrupt / finish situation before calling it
     */
    if ((rtsp_data->interrupted) || (rtsp_data->finish) ){
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open camera(%s)")
                , rtsp_data->cameratype, rtsp_data->camera_name);
        }
        av_dict_free(&rtsp_data->opts);
        if (rtsp_data->interrupted) netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    retcd = avformat_open_input(&rtsp_data->format_context, rtsp_data->path, NULL, &rtsp_data->opts);
    if ((retcd < 0) || (rtsp_data->interrupted) || (rtsp_data->finish) ){
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open camera(%s): %s")
                , rtsp_data->cameratype, rtsp_data->camera_name, errstr);
        }
        av_dict_free(&rtsp_data->opts);
        if (rtsp_data->interrupted) netcam_rtsp_close_context(rtsp_data);
        return -1;
    }
    av_dict_free(&rtsp_data->opts);
    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Opened camera(%s)"), rtsp_data->cameratype, rtsp_data->camera_name);

    /* fill out stream information */
    retcd = avformat_find_stream_info(rtsp_data->format_context, NULL);
    if ((retcd < 0) || (rtsp_data->interrupted) || (rtsp_data->finish) ){
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to find stream info: %s")
                ,rtsp_data->cameratype, errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */

    util_threadname_get(rtsp_data->threadname);

    util_threadname_set("av",rtsp_data->threadnbr,rtsp_data->camera_name);

    retcd = netcam_rtsp_open_codec(rtsp_data);

    util_threadname_set(NULL, 0, rtsp_data->threadname);

    if ((retcd < 0) || (rtsp_data->interrupted) || (rtsp_data->finish) ){
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to open codec context: %s")
                ,rtsp_data->cameratype, errstr);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    if (rtsp_data->codec_context->width <= 0 ||
        rtsp_data->codec_context->height <= 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Camera image size is invalid"),rtsp_data->cameratype);
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    if (rtsp_data->high_resolution){
        rtsp_data->imgsize.width = rtsp_data->codec_context->width;
        rtsp_data->imgsize.height = rtsp_data->codec_context->height;
    } else {
        if (netcam_rtsp_open_sws(rtsp_data) < 0) return -1;
    }

    rtsp_data->frame = my_frame_alloc();
    if (rtsp_data->frame == NULL) {
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Unable to allocate frame."),rtsp_data->cameratype);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    if (rtsp_data->passthrough){
        retcd = netcam_rtsp_copy_stream(rtsp_data);
        if ((retcd < 0) || (rtsp_data->interrupted)){
            if (rtsp_data->status == RTSP_NOTCONNECTED){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: Failed to copy stream for pass-through.")
                    ,rtsp_data->cameratype);
            }
            rtsp_data->passthrough = FALSE;
        }
    }

    /* Validate that the previous steps opened the camera */
    retcd = netcam_rtsp_read_image(rtsp_data);
    if ((retcd < 0) || (rtsp_data->interrupted)){
        if (rtsp_data->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Failed to read first image"),rtsp_data->cameratype);
        }
        netcam_rtsp_close_context(rtsp_data);
        return -1;
    }

    return 0;

}

static int netcam_rtsp_connect(struct rtsp_context *rtsp_data){

    if (netcam_rtsp_open_context(rtsp_data) < 0) return -1;

    if (netcam_rtsp_ntc(rtsp_data) < 0 ) return -1;

    if (netcam_rtsp_read_image(rtsp_data) < 0) return -1;

    /* We use the status for determining whether to grab a image from
     * the Motion loop(see "next" function).  When we are initially starting,
     * we open and close the context and during this process we do not want the
     * Motion loop to start quite yet on this first image so we do
     * not set the status to connected
     */
    if (!rtsp_data->first_image) rtsp_data->status = RTSP_CONNECTED;

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Camera (%s) connected")
        , rtsp_data->cameratype,rtsp_data->camera_name);

    return 0;
}

static void netcam_rtsp_shutdown(struct rtsp_context *rtsp_data){

    if (rtsp_data) {
        netcam_rtsp_close_context(rtsp_data);

        if (rtsp_data->path != NULL) free(rtsp_data->path);

        if (rtsp_data->img_latest != NULL){
            free(rtsp_data->img_latest->ptr);
            free(rtsp_data->img_latest);
        }
        if (rtsp_data->img_recv != NULL){
            free(rtsp_data->img_recv->ptr);
            free(rtsp_data->img_recv);
        }

        rtsp_data->path    = NULL;
        rtsp_data->img_latest = NULL;
        rtsp_data->img_recv   = NULL;
    }

}

static void netcam_rtsp_handler_wait(struct rtsp_context *rtsp_data){
    /* This function slows down the handler loop to try to
     * get in sync with the main motion loop in the capturing
     * of images while also trying to not go so slow that the
     * connection to the  network camera is lost and we end up
     * with lots of reconnects or fragmented images
     */

    int framerate;
    long usec_maxrate, usec_delay;

    framerate = rtsp_data->conf->framerate;
    if (framerate < 2) framerate = 2;

    if (strcmp(rtsp_data->service,"file") == 0) {
        /* For file processing, we try to match exactly the motion loop rate */
        usec_maxrate = (1000000L / framerate);
    } else {
        /* We set the capture rate to be a bit faster than the frame rate.  This
         * should provide the motion loop with a picture whenever it wants one.
         */
        if (framerate < rtsp_data->src_fps) framerate = rtsp_data->src_fps;
        usec_maxrate = (1000000L / (framerate + 3));
    }

    if (gettimeofday(&rtsp_data->frame_curr_tm, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
    }

    usec_delay = usec_maxrate -
        ((rtsp_data->frame_curr_tm.tv_sec - rtsp_data->frame_prev_tm.tv_sec) * 1000000L) -
        (rtsp_data->frame_curr_tm.tv_usec - rtsp_data->frame_prev_tm.tv_usec);
    if ((usec_delay > 0) && (usec_delay < 1000000L)){
        SLEEP(0, usec_delay * 1000);
    }

}

static void netcam_rtsp_handler_reconnect(struct rtsp_context *rtsp_data){

    int retcd;

    if ((rtsp_data->status == RTSP_CONNECTED) ||
        (rtsp_data->status == RTSP_READINGIMAGE)){
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
            ,_("%s: Reconnecting with camera...."),rtsp_data->cameratype);
    }
    rtsp_data->status = RTSP_RECONNECTING;

    /*
    * The retry count of 100 is arbritrary.
    * We want to try many times quickly to not lose too much information
    * before we go into the long wait phase
    */
    retcd = netcam_rtsp_connect(rtsp_data);
    if (retcd < 0){
        if (rtsp_data->reconnect_count < 100){
            rtsp_data->reconnect_count++;
        } else if (rtsp_data->reconnect_count == 100){
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Camera did not reconnect."), rtsp_data->cameratype);
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Checking for camera every 10 seconds."),rtsp_data->cameratype);
            rtsp_data->reconnect_count++;
            SLEEP(10,0);
        } else {
            SLEEP(10,0);
        }
    } else {
        rtsp_data->reconnect_count = 0;
    }

}

static void *netcam_rtsp_handler(void *arg){

    struct rtsp_context *rtsp_data = arg;

    rtsp_data->handler_finished = FALSE;

    util_threadname_set("nc",rtsp_data->threadnbr, rtsp_data->camera_name);

    pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)rtsp_data->threadnbr));

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Camera handler thread [%d] started")
        ,rtsp_data->cameratype, rtsp_data->threadnbr);

    while (!rtsp_data->finish) {
        if (!rtsp_data->format_context) {      /* We must have disconnected.  Try to reconnect */
            if (gettimeofday(&rtsp_data->frame_prev_tm, NULL) < 0) {
                MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
            }
            netcam_rtsp_handler_reconnect(rtsp_data);
            continue;
        } else {            /* We think we are connected...*/
            if (gettimeofday(&rtsp_data->frame_prev_tm, NULL) < 0) {
                MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "gettimeofday");
            }
            if (netcam_rtsp_read_image(rtsp_data) < 0) {
                if (!rtsp_data->finish) {   /* Nope.  We are not or got bad image.  Reconnect*/
                    netcam_rtsp_handler_reconnect(rtsp_data);
                }
                continue;
            }
            netcam_rtsp_handler_wait(rtsp_data);
        }
    }

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("%s: Handler loop finished."),rtsp_data->cameratype);
    netcam_rtsp_shutdown(rtsp_data);

    /* Our thread is finished - decrement motion's thread count. */
    pthread_mutex_lock(&global_lock);
        threads_running--;
    pthread_mutex_unlock(&global_lock);

    MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
        ,_("netcam camera handler: finish set, exiting"));
    rtsp_data->handler_finished = TRUE;

    pthread_exit(NULL);
}

static int netcam_rtsp_start_handler(struct rtsp_context *rtsp_data){

    int retcd;
    int wait_counter;
    pthread_attr_t handler_attribute;

    pthread_mutex_init(&rtsp_data->mutex, NULL);
    pthread_mutex_init(&rtsp_data->mutex_pktarray, NULL);
    pthread_mutex_init(&rtsp_data->mutex_transfer, NULL);

    pthread_attr_init(&handler_attribute);
    pthread_attr_setdetachstate(&handler_attribute, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&global_lock);
        rtsp_data->threadnbr = ++threads_running;
    pthread_mutex_unlock(&global_lock);

    retcd = pthread_create(&rtsp_data->thread_id, &handler_attribute, &netcam_rtsp_handler, rtsp_data);
    if (retcd < 0) {
        MOTION_LOG(ALR, TYPE_NETCAM, SHOW_ERRNO
            ,_("%s: Error starting handler thread"),rtsp_data->cameratype);
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
        pthread_mutex_lock(&rtsp_data->mutex);
            if (rtsp_data->img_latest->ptr != NULL ) wait_counter = -1;
        pthread_mutex_unlock(&rtsp_data->mutex);

        if (wait_counter > 0 ){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Waiting for first image from the handler."),rtsp_data->cameratype);
            SLEEP(0,5000000);
            wait_counter--;
        }
    }
    /* Warn the user about a mismatch of camera FPS vs handler capture rate*/
    if (rtsp_data->conf->framerate < rtsp_data->src_fps){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("Requested frame rate %d FPS is less than camera frame rate %d FPS")
            , rtsp_data->conf->framerate,rtsp_data->src_fps);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("Increasing capture rate to %d FPS to match camera.")
            , rtsp_data->src_fps);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
            , _("To lower CPU, change camera FPS to lower rate and decrease I frame interval.")
            , rtsp_data->src_fps);

    }

    return 0;

}

/*********************************************************
 *  This ends the section of functions that rely upon FFmpeg
 ***********************************************************/
#endif /* End HAVE_FFMPEG */


int netcam_rtsp_setup(struct context *cnt){
#ifdef HAVE_FFMPEG

    int retcd;
    int indx_cam, indx_max;
    struct rtsp_context *rtsp_data;

    cnt->rtsp = NULL;
    cnt->rtsp_high = NULL;

    if (netcam_rtsp_set_dimensions(cnt) < 0 ) return -1;

    indx_cam = 1;
    indx_max = 1;
    if (cnt->conf.netcam_highres) indx_max = 2;

    while (indx_cam <= indx_max){
        if (indx_cam == 1){
            cnt->rtsp = rtsp_new_context();
            if (cnt->rtsp == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp context"));
                return -1;
            }
            rtsp_data = cnt->rtsp;
            rtsp_data->high_resolution = FALSE;           /* Set flag for this being the normal resolution camera */
        } else {
            cnt->rtsp_high = rtsp_new_context();
            if (cnt->rtsp_high == NULL) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("unable to create rtsp high context"));
                return -1;
            }
            rtsp_data = cnt->rtsp_high;
            rtsp_data->high_resolution = TRUE;            /* Set flag for this being the high resolution camera */
        }

        netcam_rtsp_null_context(rtsp_data);

        netcam_rtsp_set_parms(cnt, rtsp_data);

        if (netcam_rtsp_connect(rtsp_data) < 0) return -1;

        retcd = netcam_rtsp_read_image(rtsp_data);
        if (retcd < 0){
            MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO
                ,_("Failed trying to read first image - retval:%d"), retcd);
            rtsp_data->status = RTSP_NOTCONNECTED;
            return -1;
        }
        /* When running dual, there seems to be contamination across norm/high with codec functions. */
        netcam_rtsp_close_context(rtsp_data);       /* Close in this thread to open it again within handler thread */
        rtsp_data->status = RTSP_RECONNECTING;      /* Set as reconnecting to avoid excess messages when starting */
        rtsp_data->first_image = FALSE;             /* Set flag that we are not processing our first image */

        /* For normal resolution, we resize the image to the config parms so we do not need
         * to set the dimension parameters here (it is done in the set_parms).  For high res
         * we must get the dimensions from the first image captured
         */
        if (rtsp_data->high_resolution){
            cnt->imgs.width_high = rtsp_data->imgsize.width;
            cnt->imgs.height_high = rtsp_data->imgsize.height;
        }

        if (netcam_rtsp_start_handler(rtsp_data) < 0 ) return -1;

        indx_cam++;
    }

    return 0;

#else  /* No FFmpeg/Libav */
    /* Stop compiler warnings */
    if (cnt)
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("FFmpeg/Libav not found on computer.  No RTSP support"));
    return -1;
#endif /* End #ifdef HAVE_FFMPEG */
}

int netcam_rtsp_next(struct context *cnt, struct image_data *img_data){
#ifdef HAVE_FFMPEG
    /* This is called from the motion loop thread */

    if ((cnt->rtsp->status == RTSP_RECONNECTING) ||
        (cnt->rtsp->status == RTSP_NOTCONNECTED)){
            return 1;
        }
    pthread_mutex_lock(&cnt->rtsp->mutex);
        netcam_rtsp_pktarray_resize(cnt, FALSE);
        memcpy(img_data->image_norm
               , cnt->rtsp->img_latest->ptr
               , cnt->rtsp->img_latest->used);
        img_data->idnbr_norm = cnt->rtsp->idnbr;
    pthread_mutex_unlock(&cnt->rtsp->mutex);

    if (cnt->rtsp_high){
        if ((cnt->rtsp_high->status == RTSP_RECONNECTING) ||
            (cnt->rtsp_high->status == RTSP_NOTCONNECTED)) return 1;

        pthread_mutex_lock(&cnt->rtsp_high->mutex);
            netcam_rtsp_pktarray_resize(cnt, TRUE);
            if (!(cnt->rtsp_high->high_resolution && cnt->rtsp_high->passthrough)) {
                memcpy(img_data->image_high
                       ,cnt->rtsp_high->img_latest->ptr
                       ,cnt->rtsp_high->img_latest->used);
            }
            img_data->idnbr_high = cnt->rtsp_high->idnbr;
        pthread_mutex_unlock(&cnt->rtsp_high->mutex);
    }

    /* Rotate images if requested */
    rotate_map(cnt, img_data);

    return 0;

#else  /* No FFmpeg/Libav */
    /* Stop compiler warnings */
    if ((cnt) || (img_data))
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("FFmpeg/Libav not found on computer.  No RTSP support"));
    return -1;
#endif /* End #ifdef HAVE_FFMPEG */
}

void netcam_rtsp_cleanup(struct context *cnt, int init_retry_flag){
#ifdef HAVE_FFMPEG
     /*
     * If the init_retry_flag is not set this function was
     * called while retrying the initial connection and there is
     * no camera-handler started yet and thread_running must
     * not be decremented.
     */
    int wait_counter;
    int indx_cam, indx_max;
    struct rtsp_context *rtsp_data;

    indx_cam = 1;
    indx_max = 1;
    if (cnt->rtsp_high) indx_max = 2;

    while (indx_cam <= indx_max) {
        if (indx_cam == 1){
            rtsp_data = cnt->rtsp;
        } else {
            rtsp_data = cnt->rtsp_high;
        }

        if (rtsp_data){
            MOTION_LOG(INF, TYPE_NETCAM, NO_ERRNO
                ,_("%s: Shutting down network camera."),rtsp_data->cameratype);

            /* Throw the finish flag in context and wait a bit for it to finish its work and close everything
             * This is shutting down the thread so for the moment, we are not worrying about the
             * cross threading and protecting these variables with mutex's
            */
            rtsp_data->finish = TRUE;
            rtsp_data->interruptduration = 0;
            wait_counter = 0;
            while ((!rtsp_data->handler_finished) && (wait_counter < 10)) {
                SLEEP(1,0);
                wait_counter++;
            }
            if (!rtsp_data->handler_finished) {
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO
                    ,_("%s: No response from handler thread."),rtsp_data->cameratype);
                /* Last resort.  Kill the thread. Not safe for posix but if no response, what to do...*/
                /* pthread_kill(rtsp_data->thread_id); */
                pthread_cancel(rtsp_data->thread_id);
                pthread_kill(rtsp_data->thread_id, SIGVTALRM); /* This allows the cancel to be processed */
                if (!init_retry_flag){
                    pthread_mutex_lock(&global_lock);
                        threads_running--;
                    pthread_mutex_unlock(&global_lock);
                }
            }
            /* If we never connect we don't have a handler but we still need to clean up some */
            netcam_rtsp_shutdown(rtsp_data);

            pthread_mutex_destroy(&rtsp_data->mutex);
            pthread_mutex_destroy(&rtsp_data->mutex_pktarray);
            pthread_mutex_destroy(&rtsp_data->mutex_transfer);

            free(rtsp_data);
            rtsp_data = NULL;
            if (indx_cam == 1){
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("Normal resolution: Shut down complete."));
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO
                    ,_("High resolution: Shut down complete."));
            }
        }
        indx_cam++;
    }
    cnt->rtsp = NULL;
    cnt->rtsp_high = NULL;

#else  /* No FFmpeg/Libav */
    /* Stop compiler warnings */
    if ((cnt) || (init_retry_flag))
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, _("FFmpeg/Libav not found on computer.  No RTSP support"));
    return;
#endif /* End #ifdef HAVE_FFMPEG */

}

