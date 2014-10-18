/***********************************************************
 *  In the top section are the functions that are used
 *  when processing the RTSP camera feed.  Since these functions
 *  are internal to the RTSP module, and many require FFmpeg
 *  structures in their declarations, they are within the
 *  HAVE_FFMPEG block that eliminates them entirely when
 *  FFmpeg is not present.
 *
 *  The functions:
 *      netcam_setup_rtsp
 *      netcam_connect_rtsp
 *      netcam_shutdown_rtsp
 *      netcam_next_rtsp
 *  are called from netcam.c therefore must be defined even
 *  if FFmpeg is not present.  They must also not have FFmpeg
 *  structures in the declarations.  Simple error
 *  messages are raised if called when no FFmpeg is found.
 *
 ***********************************************************/

#include <stdio.h>
#include "netcam_rtsp.h"
#include "rotate.h"    /* already includes motion.h */

#ifdef HAVE_FFMPEG

#include "ffmpeg.h"

/**
 * netcam_check_pixfmt
 *
 * Determine whether pix_format is YUV420P
 */
int netcam_check_pixfmt(netcam_context_ptr netcam){
    int retcd;

    retcd = -1;

    if ((netcam->rtsp->codec_context->pix_fmt == PIX_FMT_YUV420P) ||
        (netcam->rtsp->codec_context->pix_fmt == PIX_FMT_YUVJ420P)) retcd = 0;

    return retcd;

}
/**
 * netcam_rtsp_null_context
 *
 * Null all the context
 */
void netcam_rtsp_null_context(netcam_context_ptr netcam){

    netcam->rtsp->swsctx         = NULL;
    netcam->rtsp->swsframe_in    = NULL;
    netcam->rtsp->swsframe_out   = NULL;
    netcam->rtsp->frame        = NULL;
    netcam->rtsp->codec_context     = NULL;
    netcam->rtsp->format_context    = NULL;

}
/**
 * netcam_rtsp_close_context
 *
 * Close all the context that could be open
 */
void netcam_rtsp_close_context(netcam_context_ptr netcam){

    if (netcam->rtsp->swsctx       != NULL) sws_freeContext(netcam->rtsp->swsctx);
    if (netcam->rtsp->swsframe_in  != NULL) my_frame_free(netcam->rtsp->swsframe_in);
    if (netcam->rtsp->swsframe_out != NULL) my_frame_free(netcam->rtsp->swsframe_out);
    if (netcam->rtsp->frame        != NULL) my_frame_free(netcam->rtsp->frame);
    if (netcam->rtsp->codec_context    != NULL) avcodec_close(netcam->rtsp->codec_context);
    if (netcam->rtsp->format_context   != NULL) avformat_close_input(&netcam->rtsp->format_context);

    netcam_rtsp_null_context(netcam);
}

/**
 * netcam_buffsize_rtsp
 *
 * This routine checks whether there is enough room in a buffer to copy
 * some additional data.  If there is not enough room, it will re-allocate
 * the buffer and adjust it's size.
 *
 * Parameters:
 *      buff            Pointer to a netcam_image_buffer structure.
 *      numbytes        The number of bytes to be copied.
 *
 * Returns:             Nothing
 */
static void netcam_buffsize_rtsp(netcam_buff_ptr buff, size_t numbytes){

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

    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: expanding buffer from [%d/%d] to [%d/%d] bytes.",
               (int) buff->used, (int) buff->size,
               (int) buff->used, new_size);

    buff->ptr = myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

/**
 * decode_packet
 *
 * This routine takes in the packet from the read and decodes it into
 * the frame.  It then takes the frame and copies it into the netcam
 * buffer
 *
 * Parameters:
 *      packet    The packet that was read from av_read
 *      buffer    The buffer that is the final destination
 *      frame     The frame into which we decode the packet
 *
 *
 * Returns:
 *      Failure    0(zero)
 *      Success    The size of the frame decoded
 */
static int decode_packet(AVPacket *packet, netcam_buff_ptr buffer, AVFrame *frame, AVCodecContext *cc){
    int check = 0;
    int frame_size = 0;
    int ret = 0;

    ret = avcodec_decode_video2(cc, frame, &check, packet);
    if (ret < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error decoding video packet");
        return 0;
    }

    if (check == 0) {
        return 0;
    }

    frame_size = avpicture_get_size(cc->pix_fmt, cc->width, cc->height);

    netcam_buffsize_rtsp(buffer, frame_size);

    avpicture_layout((const AVPicture*)frame,cc->pix_fmt,cc->width,cc->height
                    ,(unsigned char *)buffer->ptr,frame_size );

    buffer->used = frame_size;

    return frame_size;
}

/**
 * netcam_open_codec
 *
 * This routine opens the codec context for the indicated stream
 *
 * Parameters:
 *      stream_idx  The index of the stream that was found as "best"
 *      fmt_ctx     The format context that was created upon opening the stream
 *      type        The type of media type (This is a constant)
 *
 *
 * Returns:
 *      Failure    Error code from FFmpeg (Negative number)
 *      Success    0(Zero)
 */
static int netcam_open_codec(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type){
    int ret;
    char errstr[128];
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        av_strerror(ret, errstr, sizeof(errstr));
		MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Could not find stream in input!: %s",errstr);
        return ret;
    }

    *stream_idx = ret;
    st = fmt_ctx->streams[*stream_idx];

    /* find decoder for the stream */
    dec_ctx = st->codec;
    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (dec == NULL) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to find codec!");
        return -1;
    }

    /* Open the codec  */
    ret = avcodec_open2(dec_ctx, dec, NULL);
    if (ret < 0) {
        av_strerror(ret, errstr, sizeof(errstr));
    	MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to open codec!: %s", errstr);
        return ret;
    }

    return 0;
}

/**
* rtsp_new_context
*
*      Create a new RTSP context structure.
*
* Parameters
*
*       None
*
* Returns:     Pointer to the newly-created structure, NULL if error.
*
*/
struct rtsp_context *rtsp_new_context(void){
    struct rtsp_context *ret;

    /* Note that mymalloc will exit on any problem. */
    ret = mymalloc(sizeof(struct rtsp_context));

    memset(ret, 0, sizeof(struct rtsp_context));

    return ret;
}
/**
* netcam_interrupt_rtsp
*
*    This function is called during the FFmpeg blocking functions.
*    These include the opening of the format context as well as the
*    reading of the packets from the stream.  Since this is called
*    during all blocking functions, the process uses the readingframe
*    flag to determine whether to timeout the process.
*
* Parameters
*
*       ctx   We pass in the rtsp context to use it to look for the
*             readingframe flag as well as the time that we started
*             the read attempt.
*
* Returns:
*       Failure    -1(which triggers an interupt)
*       Success     0(zero which indicates to let process continue)
*
*/
static int netcam_interrupt_rtsp(void *ctx){
    struct rtsp_context *rtsp = (struct rtsp_context *)ctx;

    if (rtsp->readingframe != 1) {
        return 0;
    } else {
        struct timeval interrupttime;
        if (gettimeofday(&interrupttime, NULL) < 0) {
            MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "%s: get interrupt time failed");
        }
        if ((interrupttime.tv_sec - rtsp->startreadtime.tv_sec ) > 10){
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: Reading picture timed out for %s",rtsp->path);
            return 1;
        } else{
            return 0;
        }
    }

    //should not be possible to get here
    return 0;
}
/**
* netcam_read_rtsp_image
*
*    This function reads the packet from the camera.
*    It is called extensively so only absolutely essential
*    functions and allocations are performed.
*
* Parameters
*
*       netcam  The netcam context to read from
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_read_rtsp_image(netcam_context_ptr netcam){
    struct timeval    curtime;
    netcam_buff_ptr    buffer;
    AVPacket           packet;
    int                size_decoded;

    /* Point to our working buffer. */
    buffer = netcam->receiving;
    buffer->used = 0;

    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    size_decoded = 0;

    if (gettimeofday(&curtime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: gettimeofday");
    }
    netcam->rtsp->startreadtime = curtime;

    netcam->rtsp->readingframe = 1;
    while (size_decoded == 0 && av_read_frame(netcam->rtsp->format_context, &packet) >= 0) {
        if(packet.stream_index != netcam->rtsp->video_stream_index) {
            av_free_packet(&packet);
            av_init_packet(&packet);
            packet.data = NULL;
            packet.size = 0;
            // not our packet, skip
           continue;
        }
        size_decoded = decode_packet(&packet, buffer, netcam->rtsp->frame, netcam->rtsp->codec_context);

        av_free_packet(&packet);
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
    }
    netcam->rtsp->readingframe = 0;

    // at this point, we are finished with the packet
    av_free_packet(&packet);

    if (size_decoded == 0) {
        // something went wrong, end of stream? Interupted?
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    /*
     * read is complete - set the current 'receiving' buffer atomically
     * as 'latest', and make the buffer previously in 'latest' become
     * the new 'receiving' and signal pic_ready.
     */
    netcam->receiving->image_time = curtime;
    netcam->last_image = curtime;
    netcam_buff *xchg;

    pthread_mutex_lock(&netcam->mutex);
        xchg = netcam->latest;
        netcam->latest = netcam->receiving;
        netcam->receiving = xchg;
        netcam->imgcnt++;
        pthread_cond_signal(&netcam->pic_ready);
    pthread_mutex_unlock(&netcam->mutex);

    return 0;
}
/**
* netcam_rtsp_resize_ntc
*
*    This function notifies the user of the need to transcode
*    the netcam image which uses a lot of CPU resources
*
* Parameters
*
*       netcam  The netcam context to read from
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_rtsp_resize_ntc(netcam_context_ptr netcam){

    if ((netcam->width  != netcam->rtsp->codec_context->width) ||
        (netcam->height != netcam->rtsp->codec_context->height) ||
        (netcam_check_pixfmt(netcam) != 0) ){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ****************************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: The network camera is sending pictures in a different");
        if ((netcam->width  != netcam->rtsp->codec_context->width) ||
            (netcam->height != netcam->rtsp->codec_context->height)) {
            if (netcam_check_pixfmt(netcam) != 0) {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: size than specified in the config and also a ");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: different picture format.  The picture is being");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: transcoded to YUV420P and into the size requested");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: in the config file.  If possible change netcam to");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: be in YUV420P format and the size requested in the");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: config to possibly lower CPU usage.");
            } else {
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: size than specified in the configuration file.");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: The picture is being transcoded into the size ");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: requested in the configuration.  If possible change");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: netcam or configuration to indicate the same size");
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: to possibly lower CPU usage.");
            }
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: Netcam: %d x %d => Config: %d x %d"
            ,netcam->rtsp->codec_context->width,netcam->rtsp->codec_context->height
            ,netcam->width,netcam->height);
        } else {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: format than YUV420P.  The image sent is being ");
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: trancoded to YUV420P.  If possible change netcam ");
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: picture format to YUV420P to possibly lower CPU usage.");
        }
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ****************************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ");
    }

    return 0;

}
/**
* netcam_rtsp_open_context
*
*    This function opens the format context for the camera.
*
* Parameters
*
*       netcam  The netcam context to read from
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_rtsp_open_context(netcam_context_ptr netcam){

    int  retcd;
    char errstr[128];

    if (netcam->rtsp->path == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Null path passed to connect (%s)", netcam->rtsp->path);
        }
        return -1;
    }

    // open the network connection
    AVDictionary *opts = 0;
    netcam->rtsp->format_context = avformat_alloc_context();
    netcam->rtsp->format_context->interrupt_callback.callback = netcam_interrupt_rtsp;
    netcam->rtsp->format_context->interrupt_callback.opaque = netcam->rtsp;

    if (strncmp(netcam->rtsp->path, "http", 4) == 0 ){
        netcam->rtsp->format_context->iformat = av_find_input_format("mjpeg");
    } else {
        if (netcam->cnt->conf.rtsp_uses_tcp) {
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            if (netcam->rtsp->status == RTSP_NOTCONNECTED)
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: Using tcp transport");
        } else {
            av_dict_set(&opts, "rtsp_transport", "udp", 0);
            av_dict_set(&opts, "max_delay", "500000", 0);  //100000 is the default
            if (netcam->rtsp->status == RTSP_NOTCONNECTED)
                MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: Using udp transport");
        }
    }

    retcd = avformat_open_input(&netcam->rtsp->format_context, netcam->rtsp->path, NULL, &opts);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open input(%s): %s", netcam->rtsp->path,errstr);
        }
        av_dict_free(&opts);
        //The format context gets freed upon any error from open_input.
        return retcd;
    }
    av_dict_free(&opts);

    // fill out stream information
    retcd = avformat_find_stream_info(netcam->rtsp->format_context, NULL);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to find stream info: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    retcd = netcam_open_codec(&netcam->rtsp->video_stream_index, netcam->rtsp->format_context, AVMEDIA_TYPE_VIDEO);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open codec context: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->codec_context = netcam->rtsp->format_context->streams[netcam->rtsp->video_stream_index]->codec;

    netcam->rtsp->frame = my_frame_alloc();
    if (netcam->rtsp->frame == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate frame.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    /*
     *  Validate that the previous steps opened the camera
     */
    retcd = netcam_read_rtsp_image(netcam);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to read first image");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    return 0;

}
/**
* netcam_rtsp_open_sws
*
*    This function opens the rescaling context components.
*
* Parameters
*
*       netcam  The netcam context to read from
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_rtsp_open_sws(netcam_context_ptr netcam){

    netcam->width  = ((netcam->cnt->conf.width / 8) * 8);
    netcam->height = ((netcam->cnt->conf.height / 8) * 8);


    netcam->rtsp->swsframe_in = my_frame_alloc();
    if (netcam->rtsp->swsframe_in == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate frame.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->swsframe_out = my_frame_alloc();
    if (netcam->rtsp->swsframe_out == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate frame.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    /*
     *  The scaling context is used to change dimensions to config file and
     *  also if the format sent by the camera is not YUV420.
     */
    netcam->rtsp->swsctx = sws_getContext(
         netcam->rtsp->codec_context->width
        ,netcam->rtsp->codec_context->height
        ,netcam->rtsp->codec_context->pix_fmt
        ,netcam->width
        ,netcam->height
        ,PIX_FMT_YUV420P
        ,SWS_BICUBIC,NULL,NULL,NULL);
    if (netcam->rtsp->swsctx == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate scaling context.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->swsframe_size = avpicture_get_size(
            PIX_FMT_YUV420P
            ,netcam->width
            ,netcam->height);
        if (netcam->rtsp->swsframe_size <= 0) {
            if (netcam->rtsp->status == RTSP_NOTCONNECTED){
                MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error determining size of frame out");
            }
            netcam_rtsp_close_context(netcam);
            return -1;
        }

    return 0;

}
/**
* netcam_rtsp_resize
*
*    This function reencodes the image to yuv420p with the desired size
*
* Parameters
*
*       netcam  The netcam context to read from
*       image   The destination image.
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_rtsp_resize(unsigned char *image , netcam_context_ptr netcam){

    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    retcd = avpicture_fill(
        (AVPicture*)netcam->rtsp->swsframe_in
        ,(uint8_t*)netcam->latest->ptr
        ,netcam->rtsp->codec_context->pix_fmt
        ,netcam->rtsp->codec_context->width
        ,netcam->rtsp->codec_context->height);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error allocating picture in: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }


    buffer_out=(uint8_t *)av_malloc(netcam->rtsp->swsframe_size*sizeof(uint8_t));

    retcd = avpicture_fill(
        (AVPicture*)netcam->rtsp->swsframe_out
        ,buffer_out
        ,PIX_FMT_YUV420P
        ,netcam->width
        ,netcam->height);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error allocating picture out: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    retcd = sws_scale(
        netcam->rtsp->swsctx
        ,(const uint8_t* const *)netcam->rtsp->swsframe_in->data
        ,netcam->rtsp->swsframe_in->linesize
        ,0
        ,netcam->rtsp->codec_context->height
        ,netcam->rtsp->swsframe_out->data
        ,netcam->rtsp->swsframe_out->linesize);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error resizing/reformatting: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    retcd = avpicture_layout(
        (const AVPicture*)netcam->rtsp->swsframe_out
        ,PIX_FMT_YUV420P
        ,netcam->width
        ,netcam->height
        ,(unsigned char *)image
        ,netcam->rtsp->swsframe_size );
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error putting frame into output buffer: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    av_free(buffer_out);

    return 0;

}
/*********************************************************
 *  This ends the section of functions that rely upon FFmpeg
 ***********************************************************/
#endif /* End HAVE_FFMPEG */

/**
* netcam_connect_rtsp
*
*    This function initiates the connection to the rtsp camera.
*
* Parameters
*
*       netcam  The netcam context to open.
*
* Returns:
*       Failure    -1
*       Success     0(zero)
*
*/
int netcam_connect_rtsp(netcam_context_ptr netcam){
#ifdef HAVE_FFMPEG

    if (netcam_rtsp_open_context(netcam) < 0) return -1;

    if (netcam_rtsp_open_sws(netcam) < 0) return -1;

    if (netcam_rtsp_resize_ntc(netcam) < 0 ) return -1;

    if (netcam_read_rtsp_image(netcam) < 0) return -1;

    netcam->rtsp->status = RTSP_CONNECTED;

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: Camera connected");

    return 0;

#else  /* No FFmpeg/Libav */
    netcam->rtsp->status = RTSP_NOTCONNECTED;
    netcam->rtsp->format_context = NULL;
    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: FFmpeg/Libav not found on computer.  No RTSP support");
    return -1;
#endif /* End #ifdef HAVE_FFMPEG */
}

/**
* netcam_shutdown_rtsp
*
*    This function closes and frees all the items for rtsp
*
* Parameters
*
*       netcam  The netcam context to free.
*
* Returns:
*       Failure    nothing
*       Success    nothing
*
*/
void netcam_shutdown_rtsp(netcam_context_ptr netcam){
#ifdef HAVE_FFMPEG

    if (netcam->rtsp->status == RTSP_CONNECTED) {
        netcam_rtsp_close_context(netcam);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,"%s: netcam shut down");
    }

    free(netcam->rtsp->path);
    free(netcam->rtsp->user);
    free(netcam->rtsp->pass);

    free(netcam->rtsp);
    netcam->rtsp = NULL;

#else  /* No FFmpeg/Libav */
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: FFmpeg/Libav not found on computer.  No RTSP support");
#endif /* End #ifdef HAVE_FFMPEG */
}

/**
* netcam_setup_rtsp
*
*    This function sets up all the necessary items for the
*    rtsp camera.
*
* Parameters
*
*       netcam  The netcam context to free.
*       url     The URL of the camera
*
* Returns:
*       Failure    -1
*       Success    0(zero)
*
*/
int netcam_setup_rtsp(netcam_context_ptr netcam, struct url_t *url){
#ifdef HAVE_FFMPEG

  struct context *cnt = netcam->cnt;
  const char *ptr;
  int ret = -1;

  netcam->caps.streaming = NCS_RTSP;

  netcam->rtsp = rtsp_new_context();

  netcam_rtsp_null_context(netcam);

  if (netcam->rtsp == NULL) {
    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to create rtsp context");
    netcam_shutdown_rtsp(netcam);
    return -1;
  }

  /*
   * Allocate space for a working string to contain the path.
   * The extra 5 is for "://", ":" and string terminator.
   */

  // force port to a sane value
  if (netcam->connect_port > 65536) {
    netcam->connect_port = 65536;
  } else if (netcam->connect_port < 0) {
    netcam->connect_port = 0;
  }

    if (cnt->conf.netcam_userpass != NULL) {
        ptr = cnt->conf.netcam_userpass;
    } else {
        ptr = url->userpass;  /* Don't set this one NULL, gets freed. */
    }

    if (ptr != NULL) {
        char *cptr;
        if ((cptr = strchr(ptr, ':')) == NULL) {
            netcam->rtsp->user = mystrdup(ptr);
        } else {
            netcam->rtsp->user = mymalloc((cptr - ptr)+2);  //+2 for string terminator
            memcpy(netcam->rtsp->user, ptr,(cptr - ptr));
            netcam->rtsp->pass = mystrdup(cptr + 1);
        }
    }

    /*
     *  Need a method to query the path and
     *  determine the authentication type
     */
    if ((netcam->rtsp->user != NULL) && (netcam->rtsp->pass != NULL)) {
        ptr = mymalloc(strlen(url->service) + strlen(netcam->connect_host)
	          + 5 + strlen(url->path) + 5
              + strlen(netcam->rtsp->user) + strlen(netcam->rtsp->pass) + 4 );
        sprintf((char *)ptr, "%s://%s:%s@%s:%d%s",
                url->service,netcam->rtsp->user,netcam->rtsp->pass,
                netcam->connect_host, netcam->connect_port, url->path);
    }
    else {
        ptr = mymalloc(strlen(url->service) + strlen(netcam->connect_host)
	          + 5 + strlen(url->path) + 5);
        sprintf((char *)ptr, "%s://%s:%d%s", url->service,
	        netcam->connect_host, netcam->connect_port, url->path);
    }
    netcam->rtsp->path = (char *)ptr;

    netcam_url_free(url);

    /*
     * Now we need to set some flags
     */
    netcam->rtsp->readingframe = 0;
    netcam->rtsp->status = RTSP_NOTCONNECTED;

    /*
     * Warn and fix dimensions as needed.
     */
    if (netcam->cnt->conf.width % 16) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Image width (%d) requested is not modulo 16.", netcam->cnt->conf.width);
        netcam->cnt->conf.width = netcam->cnt->conf.width - (netcam->cnt->conf.width % 16) + 16;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Adjusting width to next higher multiple of 16 (%d).", netcam->cnt->conf.width);
    }
    if (netcam->cnt->conf.height % 16) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Image height (%d) requested is not modulo 16.", netcam->cnt->conf.height);
        netcam->cnt->conf.height = netcam->cnt->conf.height - (netcam->cnt->conf.height % 16) + 16;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Adjusting height to next higher multiple of 16 (%d).", netcam->cnt->conf.height);
    }
    
    av_register_all();
    avformat_network_init();
    avcodec_register_all();

    /*
     * The RTSP context should be all ready to attempt a connection with
     * the server, so we try ....
     */
    ret = netcam_connect_rtsp(netcam);
    if (ret < 0){
        return ret;
    }

    netcam->get_image = netcam_read_rtsp_image;

  return 0;

#else  /* No FFmpeg/Libav */
    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: FFmpeg/Libav not found on computer.  No RTSP support");
    return -1;
#endif /* End #ifdef HAVE_FFMPEG */
}

/**
* netcam_next_rtsp
*
*    This function moves the picture to the image buffer.
*    If the picture is not in the correct format for size
*    it will put it into the requested format
*
* Parameters
*
*       netcam  The netcam context to free.
*       url     The URL of the camera
*
* Returns:
*       Failure    -1
*       Success    0(zero)
*
*/
int netcam_next_rtsp(unsigned char *image , netcam_context_ptr netcam){
#ifdef HAVE_FFMPEG

    if ((netcam->width  != netcam->rtsp->codec_context->width) ||
        (netcam->height != netcam->rtsp->codec_context->height) ||
        (netcam_check_pixfmt(netcam) != 0) ){
        netcam_rtsp_resize(image ,netcam);
    } else {
        memcpy(image, netcam->latest->ptr, netcam->latest->used);
    }
    if (netcam->cnt->rotate_data.degrees > 0)
        /* Rotate as specified */
        rotate_map(netcam->cnt, image);

    return 0;
#else  /* No FFmpeg/Libav */
    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: FFmpeg/Libav not found on computer.  No RTSP support");
    return -1;
#endif /* End #ifdef HAVE_FFMPEG */
}
