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
#include "rotate.h"    /* already includes motion.h */
#include "netcam_rtsp.h"

#ifdef HAVE_FFMPEG

#include "ffmpeg.h"

static int netcam_rtsp_resize(netcam_context_ptr netcam);
static int netcam_rtsp_open_sws(netcam_context_ptr netcam);

/**
 * netcam_check_pixfmt
 *
 * Determine whether pix_format is YUV420P
 */
static int netcam_check_pixfmt(netcam_context_ptr netcam){
    int retcd;

    retcd = -1;

    if ((netcam->rtsp->codec_context->pix_fmt == MY_PIX_FMT_YUV420P) ||
        (netcam->rtsp->codec_context->pix_fmt == MY_PIX_FMT_YUVJ420P)) retcd = 0;

    return retcd;

}
/**
 * netcam_rtsp_null_context
 *
 * Null all the context
 */
static void netcam_rtsp_null_context(netcam_context_ptr netcam){

    netcam->rtsp->swsctx         = NULL;
    netcam->rtsp->swsframe_in    = NULL;
    netcam->rtsp->swsframe_out   = NULL;
    netcam->rtsp->frame          = NULL;
    netcam->rtsp->codec_context  = NULL;
    netcam->rtsp->format_context = NULL;

    netcam->rtsp->active = 0;
}
/**
 * netcam_rtsp_close_context
 *
 * Close all the context that could be open
 */
static void netcam_rtsp_close_context(netcam_context_ptr netcam){

    if (netcam->rtsp->swsctx       != NULL) sws_freeContext(netcam->rtsp->swsctx);
    if (netcam->rtsp->swsframe_in  != NULL) my_frame_free(netcam->rtsp->swsframe_in);
    if (netcam->rtsp->swsframe_out != NULL) my_frame_free(netcam->rtsp->swsframe_out);
    if (netcam->rtsp->frame        != NULL) my_frame_free(netcam->rtsp->frame);
    if (netcam->rtsp->codec_context    != NULL) my_avcodec_close(netcam->rtsp->codec_context);
    if (netcam->rtsp->format_context   != NULL) avformat_close_input(&netcam->rtsp->format_context);

    netcam_rtsp_null_context(netcam);
}

static int rtsp_decode_video(AVPacket *packet, AVFrame *frame, AVCodecContext *ctx_codec){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    int retcd;
    char errstr[128];

    if (packet) {
        retcd = avcodec_send_packet(ctx_codec, packet);
        assert(retcd != AVERROR(EAGAIN));
        if (retcd < 0 && retcd != AVERROR_EOF){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error sending packet to codec: %s", errstr);
            return -1;
        }
    }

    retcd = avcodec_receive_frame(ctx_codec, frame);

    if (retcd == AVERROR(EAGAIN)) return 0;

    /*
     * At least one netcam (Wansview K1) is known to always send a bogus
     * packet at the start of the stream. Just grin and bear it...
     *
     * TODO: This error-tolerance should be limited/conditionalized, or
     * else Motion could end up accepting bogus video data indefinitely
     */
    if (retcd == AVERROR_INVALIDDATA) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Ignoring packet with invalid data");
        return 0;
    }

    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error receiving frame from codec: %s", errstr);
        return -1;
    }
    return 1;

#else

    AVPacket empty_packet;
    int retcd;
    int check = 0;
    char errstr[128];

    if (!packet) {
        av_init_packet(&empty_packet);
        empty_packet.data = NULL;
        empty_packet.size = 0;
        packet = &empty_packet;
    }

    retcd = avcodec_decode_video2(ctx_codec, frame, &check, packet);

    /*
     * See note above
     */
    if (retcd == AVERROR_INVALIDDATA) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Ignoring packet with invalid data");
        return 0;
    }

    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error decoding packet: %s",errstr);
        return -1;
    }

    if (check == 0 || retcd == 0) return 0;
    return 1;

#endif

}
/**
 * rtsp_decode_packet
 *
 * This routine takes in the packet from the read and decodes it into
 * the frame.  It then takes the frame and copies it into the netcam
 * buffer
 *
 * Parameters:
 *      packet    The packet that was read from av_read, or NULL
 *      buffer    The buffer that is the final destination
 *      frame     The frame into which we decode the packet
 *
 *
 * Returns:
 *      Error      Negative value
 *      No result  0(zero)
 *      Success    The size of the frame decoded
 */
static int rtsp_decode_packet(AVPacket *packet, netcam_buff_ptr buffer, AVFrame *frame, AVCodecContext *ctx_codec){

    int frame_size;
    int retcd;

    retcd = rtsp_decode_video(packet, frame, ctx_codec);
    if (retcd <= 0) return retcd;

    frame_size = my_image_get_buffer_size(ctx_codec->pix_fmt, ctx_codec->width, ctx_codec->height);

    netcam_check_buffsize(buffer, frame_size);

    retcd = my_image_copy_to_buffer(frame, (uint8_t *)buffer->ptr,ctx_codec->pix_fmt,ctx_codec->width,ctx_codec->height, frame_size);
    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error decoding video packet: Copying to buffer");
        return -1;
    }

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
static int netcam_open_codec(netcam_context_ptr netcam){

    int retcd;
    char errstr[128];
    AVStream *st;
    AVCodec *decoder = NULL;

    retcd = av_find_best_stream(netcam->rtsp->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Could not find stream in input!: %s",errstr);
        return (retcd < 0) ? retcd : -1;
    }
    netcam->rtsp->video_stream_index = retcd;
    st = netcam->rtsp->format_context->streams[netcam->rtsp->video_stream_index];

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    decoder = avcodec_find_decoder(st->codecpar->codec_id);
    if (decoder == NULL) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to find codec!");
        return -1;
    }

    netcam->rtsp->codec_context = avcodec_alloc_context3(decoder);
    if (netcam->rtsp->codec_context == NULL) {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to allocate decoder!");
        return -1;
    }

    if ((retcd = avcodec_parameters_to_context(netcam->rtsp->codec_context, st->codecpar)) < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to copy decoder parameters!: %s", errstr);
        return -1;
    }

#else

    netcam->rtsp->codec_context = st->codec;
    decoder = avcodec_find_decoder(netcam->rtsp->codec_context->codec_id);
    if (decoder == NULL) {
         MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to find codec!");
         return -1;
     }

#endif

    retcd = avcodec_open2(netcam->rtsp->codec_context, decoder, NULL);
    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to open codec!: %s", errstr);
        return (retcd < 0) ? retcd : -1;
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
*       ctx   We pass in the netcam context to use it to look for the
*             readingframe flag as well as the time that we started
*             the read attempt.
*
* Returns:
*       Failure    -1(which triggers an interrupt)
*       Success     0(zero which indicates to let process continue)
*
*/
static int netcam_interrupt_rtsp(void *ctx){
    netcam_context_ptr netcam = (netcam_context_ptr)ctx;
    struct rtsp_context *rtsp = netcam->rtsp;

    if (netcam->finish) {
        /* netcam_cleanup() wants us to stop */
        rtsp->interrupted = 1;
        return 1;
    }

    if (rtsp->status == RTSP_CONNECTED) {
        return 0;
    } else if (rtsp->status == RTSP_READINGIMAGE) {
        struct timeval interrupttime;
        if (gettimeofday(&interrupttime, NULL) < 0) {
            MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "%s: get interrupt time failed");
        }
        if ((interrupttime.tv_sec - rtsp->startreadtime.tv_sec ) > 10){
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: Camera timed out for %s", rtsp->netcam_url);
            rtsp->interrupted = 1;
            return 1;
        } else{
            return 0;
        }
    } else {
        /* This is for NOTCONNECTED and RECONNECTING status.  We give these
         * options more time because all the ffmpeg calls that are inside the
         * rtsp_connect function will use the same start time.  Otherwise we
         * would need to reset the time before each call to a ffmpeg function.
        */
        struct timeval interrupttime;
        if (gettimeofday(&interrupttime, NULL) < 0) {
            MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "%s: get interrupt time failed");
        }
        if ((interrupttime.tv_sec - rtsp->startreadtime.tv_sec ) > 30){
            MOTION_LOG(WRN, TYPE_NETCAM, NO_ERRNO, "%s: Camera timed out for %s", rtsp->netcam_url);
            rtsp->interrupted = 1;
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

    if (gettimeofday(&curtime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: gettimeofday");
    }
    netcam->rtsp->startreadtime = curtime;
    netcam->rtsp->interrupted = 0;
    netcam->rtsp->status = RTSP_READINGIMAGE;

    /* First, check whether the codec has any frames ready to go
     * before we feed it new packets
     */
    size_decoded = rtsp_decode_packet(NULL, buffer, netcam->rtsp->frame, netcam->rtsp->codec_context);

    while (size_decoded == 0 && av_read_frame(netcam->rtsp->format_context, &packet) >= 0) {
        if (packet.stream_index == netcam->rtsp->video_stream_index)
            size_decoded = rtsp_decode_packet(&packet, buffer, netcam->rtsp->frame, netcam->rtsp->codec_context);

        my_packet_unref(packet);
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;
    }
    netcam->rtsp->status = RTSP_CONNECTED;

    if ((size_decoded <= 0) || (netcam->rtsp->interrupted == 1)) {
        // something went wrong, end of stream? interrupted?
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    if ((netcam->width  != (unsigned)netcam->rtsp->codec_context->width) ||
        (netcam->height != (unsigned)netcam->rtsp->codec_context->height) ||
        (netcam_check_pixfmt(netcam) != 0) ){
        if (netcam_rtsp_resize(netcam) < 0)
          return -1;
    }

    netcam_image_read_complete(netcam);

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
static int netcam_rtsp_resize_ntc(netcam_context_ptr netcam){

    if ((netcam->width  != (unsigned)netcam->rtsp->codec_context->width) ||
        (netcam->height != (unsigned)netcam->rtsp->codec_context->height) ||
        (netcam_check_pixfmt(netcam) != 0) ){
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: ****************************************************************");
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: The network camera is sending pictures in a different");
        if ((netcam->width  != (unsigned)netcam->rtsp->codec_context->width) ||
            (netcam->height != (unsigned)netcam->rtsp->codec_context->height)) {
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
static int netcam_rtsp_open_context(netcam_context_ptr netcam){

    int  retcd;
    char errstr[128];
    char optsize[10], optfmt[8], optfps[5];


    if (netcam->rtsp->path == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Null path passed to connect");
        }
        return -1;
    }

    // open the network connection
    AVDictionary *opts = 0;
    netcam->rtsp->format_context = avformat_alloc_context();
    netcam->rtsp->format_context->interrupt_callback.callback = netcam_interrupt_rtsp;
    netcam->rtsp->format_context->interrupt_callback.opaque = netcam;

    netcam->rtsp->interrupted = 0;
    if (gettimeofday(&netcam->rtsp->startreadtime, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: gettimeofday");
    }

    if (strncmp(netcam->rtsp->path, "http", 4) == 0 ){
        netcam->rtsp->format_context->iformat = av_find_input_format("mjpeg");
    } else if (strncmp(netcam->rtsp->path, "rtsp", 4) == 0 ){
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
    } else {
        netcam->rtsp->format_context->iformat = av_find_input_format("video4linux2");

        if (netcam->cnt->conf.v4l2_palette == 8) {
            sprintf(optfmt, "%s","mjpeg");
            av_dict_set(&opts, "input_format", optfmt, 0);
        } else if (netcam->cnt->conf.v4l2_palette == 21){
            sprintf(optfmt, "%s","h264");
            av_dict_set(&opts, "input_format", optfmt, 0);
        } else{
            sprintf(optfmt, "%s","default");
        }

        sprintf(optfps, "%d",netcam->cnt->conf.frame_limit);
        av_dict_set(&opts, "framerate", optfps, 0);

        sprintf(optsize, "%dx%d",netcam->cnt->conf.width,netcam->cnt->conf.height);
        av_dict_set(&opts, "video_size", optsize, 0);

        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: v4l2 input_format %s",optfmt);
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: v4l2 framerate %s", optfps);
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: v4l2 video_size %s", optsize);
        }
     }

    retcd = avformat_open_input(&netcam->rtsp->format_context, netcam->rtsp->path, NULL, &opts);
    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open input(%s): %s", netcam->rtsp->netcam_url, errstr);
        }
        av_dict_free(&opts);
        //The format context gets freed upon any error from open_input.
        return (retcd < 0) ? retcd : -1;
    }
    av_dict_free(&opts);

    // fill out stream information
    retcd = avformat_find_stream_info(netcam->rtsp->format_context, NULL);
    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to find stream info: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    /* there is no way to set the avcodec thread names, but they inherit
     * our thread name - so temporarily change our thread name to the
     * desired name */
    {
        char newtname[16];
        char curtname[16] = "unknown";
#if (!defined(BSD) || defined(__APPLE__))
        pthread_getname_np(pthread_self(), curtname, sizeof(curtname));
#endif
        snprintf(newtname, sizeof(newtname), "av%d%s%s",
                 netcam->cnt->threadnr,
                 netcam->cnt->conf.camera_name ? ":" : "",
                 netcam->cnt->conf.camera_name ? netcam->cnt->conf.camera_name : "");
        MOTION_PTHREAD_SETNAME(newtname);

    retcd = netcam_open_codec(netcam);

        MOTION_PTHREAD_SETNAME(curtname);
    }

    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open codec context: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    if (netcam->rtsp->codec_context->width <= 0 ||
        netcam->rtsp->codec_context->height <= 0)
    {
        MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Camera image size is invalid");
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->frame = my_frame_alloc();
    if (netcam->rtsp->frame == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate frame.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    if (netcam_rtsp_open_sws(netcam) < 0) return -1;

    /*
     *  Validate that the previous steps opened the camera
     */
    retcd = netcam_read_rtsp_image(netcam);
    if ((retcd < 0) || (netcam->rtsp->interrupted == 1)){
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to read first image");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->active = 1;

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
static int netcam_rtsp_open_sws(netcam_context_ptr netcam){

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
        ,MY_PIX_FMT_YUV420P
        ,SWS_BICUBIC,NULL,NULL,NULL);
    if (netcam->rtsp->swsctx == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate scaling context.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    netcam->rtsp->swsframe_size = my_image_get_buffer_size(
            MY_PIX_FMT_YUV420P
            ,netcam->width
            ,netcam->height);
    if (netcam->rtsp->swsframe_size <= 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error determining size of frame out");
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }

    /* the image buffers must be big enough to hold the final frame after resizing */
    netcam_check_buffsize(netcam->receiving, netcam->rtsp->swsframe_size);
    netcam_check_buffsize(netcam->latest, netcam->rtsp->swsframe_size);

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
static int netcam_rtsp_resize(netcam_context_ptr netcam){

    int      retcd;
    char     errstr[128];
    uint8_t *buffer_out;

    retcd=my_image_fill_arrays(
        netcam->rtsp->swsframe_in
        ,(uint8_t*)netcam->receiving->ptr
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

    retcd=my_image_fill_arrays(
        netcam->rtsp->swsframe_out
        ,buffer_out
        ,MY_PIX_FMT_YUV420P
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

    retcd=my_image_copy_to_buffer(
         netcam->rtsp->swsframe_out
        ,(uint8_t *)netcam->receiving->ptr
        ,MY_PIX_FMT_YUV420P
        ,netcam->width
        ,netcam->height
        ,netcam->rtsp->swsframe_size);
    if (retcd < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error putting frame into output buffer: %s", errstr);
        }
        netcam_rtsp_close_context(netcam);
        return -1;
    }
    netcam->receiving->used = netcam->rtsp->swsframe_size;

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

    if (netcam_rtsp_resize_ntc(netcam) < 0 ) return -1;

    if (netcam_read_rtsp_image(netcam) < 0) return -1;

    netcam->rtsp->status = RTSP_CONNECTED;

    MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: Camera connected");

    return 0;

#else  /* No FFmpeg/Libav */
    if (netcam)
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

    if (netcam->rtsp->status == RTSP_CONNECTED ||
        netcam->rtsp->status == RTSP_READINGIMAGE) {
        netcam_rtsp_close_context(netcam);
        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,"%s: netcam shut down");
    }

    free(netcam->rtsp->path);
    free(netcam->rtsp->user);
    free(netcam->rtsp->pass);

    free(netcam->rtsp);
    netcam->rtsp = NULL;

#else  /* No FFmpeg/Libav */
    /* Stop compiler warnings */
    if (netcam)
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
    if (strcmp(url->service, "v4l2") == 0) {
        ptr = mymalloc(strlen(url->path));
        sprintf((char *)ptr, "%s",url->path);
    } else if ((netcam->rtsp->user != NULL) && (netcam->rtsp->pass != NULL)) {
        ptr = mymalloc(strlen(url->service) + strlen(netcam->connect_host)
              + 5 + strlen(url->path) + 5
              + strlen(netcam->rtsp->user) + strlen(netcam->rtsp->pass) + 4 );
        sprintf((char *)ptr, "%s://%s:%s@%s:%d%s",
                url->service,netcam->rtsp->user,netcam->rtsp->pass,
                netcam->connect_host, netcam->connect_port, url->path);
    } else {
        ptr = mymalloc(strlen(url->service) + strlen(netcam->connect_host)
              + 5 + strlen(url->path) + 5);
        sprintf((char *)ptr, "%s://%s:%d%s", url->service,
            netcam->connect_host, netcam->connect_port, url->path);
    }
    netcam->rtsp->path = (char *)ptr;

    netcam_url_free(url);

    /*
     * Keep a pointer to the original URL for logging purposes
     * (we don't want to put passwords into the log)
     */
    netcam->rtsp->netcam_url = cnt->conf.netcam_url;

    /*
     * Now we need to set some flags
     */
    netcam->rtsp->status = RTSP_NOTCONNECTED;

    /*
     * Warn and fix dimensions as needed.
     */
    if (netcam->cnt->conf.width % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Image width (%d) requested is not modulo 8.", netcam->cnt->conf.width);
        netcam->cnt->conf.width = netcam->cnt->conf.width - (netcam->cnt->conf.width % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Adjusting width to next higher multiple of 8 (%d).", netcam->cnt->conf.width);
    }
    if (netcam->cnt->conf.height % 8) {
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Image height (%d) requested is not modulo 8.", netcam->cnt->conf.height);
        netcam->cnt->conf.height = netcam->cnt->conf.height - (netcam->cnt->conf.height % 8) + 8;
        MOTION_LOG(CRT, TYPE_NETCAM, NO_ERRNO, "%s: Adjusting height to next higher multiple of 8 (%d).", netcam->cnt->conf.height);
    }

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
    /* Stop compiler warnings */
    if ((url) || (netcam))
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
    /* This function is running from the motion_loop thread - generally the
     * rest of the functions in this file are running from the
     * netcam_handler_loop thread - this means you generally cannot access
     * or call anything else without taking care of thread safety.
     * The netcam mutex *only* protects netcam->latest, it cannot be
     * used to safely call other netcam functions. */

    pthread_mutex_lock(&netcam->mutex);
    memcpy(image, netcam->latest->ptr, netcam->latest->used);
    pthread_mutex_unlock(&netcam->mutex);

    if (netcam->cnt->rotate_data.degrees > 0 || netcam->cnt->rotate_data.axis != FLIP_TYPE_NONE)
        /* Rotate as specified */
        rotate_map(netcam->cnt, image);

    return 0;
}
