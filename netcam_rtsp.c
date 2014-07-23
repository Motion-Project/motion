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
 *  are called from netcam.c therefore must be defined even
 *  if FFmpeg is not present.  They must also not have FFmpeg
 *  structures are in the declarations.  Simple error
 *  messages are raised if called when no FFmpeg is found.
 *
 ***********************************************************/

#include <stdio.h>
#include "netcam_rtsp.h"
#include "motion.h"

#ifdef HAVE_FFMPEG

#include "ffmpeg.h"

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

    cc->err_recognition = 3;
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
 * open_codec_context
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
static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type){
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
    static int        usual_size_decoded;

    /* Point to our working buffer. */
    buffer = netcam->receiving;
    buffer->used = 0;

    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    size_decoded = 0;
    usual_size_decoded = 0;

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
        my_frame_free(netcam->rtsp->frame);
        avcodec_close(netcam->rtsp->codec_context);
        avformat_close_input(&netcam->rtsp->format_context);
        return -1;
    }

    if (size_decoded != usual_size_decoded) {
        if (usual_size_decoded !=0) {
            MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO, "%s: unusual frame size of %d!", size_decoded);
        }
        usual_size_decoded = size_decoded;
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

/***********************************************************
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

    int ret;
    char errstr[128];

    if (netcam->rtsp->path == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Null path passed to connect (%s)", netcam->rtsp->path);
        }
        return -1;
    }

    // open the network connection
    AVDictionary *opts = 0;
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

    netcam->rtsp->format_context = avformat_alloc_context();
    netcam->rtsp->format_context->interrupt_callback.callback = netcam_interrupt_rtsp;
    netcam->rtsp->format_context->interrupt_callback.opaque = netcam->rtsp;

    ret = avformat_open_input(&netcam->rtsp->format_context, netcam->rtsp->path, NULL, &opts);
    if (ret < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(ret, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open input(%s): %s", netcam->rtsp->path,errstr);
        }
        av_dict_free(&opts);
        //The format context gets freed upon any error from open_input.
        return ret;
    }
    av_dict_free(&opts);

    // fill out stream information
    ret = avformat_find_stream_info(netcam->rtsp->format_context, NULL);
    if (ret < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(ret, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to find stream info: %s", errstr);
        }
        avformat_close_input(&netcam->rtsp->format_context);
        return -1;
    }

    ret = open_codec_context(&netcam->rtsp->video_stream_index, netcam->rtsp->format_context, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(ret, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open codec context: %s", errstr);
        }
        avformat_close_input(&netcam->rtsp->format_context);
        return -1;
    }

    netcam->rtsp->codec_context = netcam->rtsp->format_context->streams[netcam->rtsp->video_stream_index]->codec;

    netcam->rtsp->frame = my_frame_alloc();
    if (netcam->rtsp->frame == NULL) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to allocate frame.  Fatal error.  Check FFmpeg/Libav configuration");
        }
        avcodec_close(netcam->rtsp->codec_context);
        avformat_close_input(&netcam->rtsp->format_context);
        return -1;
    }

    // start up the feed
    ret = av_read_play(netcam->rtsp->format_context);
    if (ret < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            av_strerror(ret, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open the camera: %s", errstr);
        }
        my_frame_free(netcam->rtsp->frame);
        avcodec_close(netcam->rtsp->codec_context);
        avformat_close_input(&netcam->rtsp->format_context);
        return -1;
    }

    /*
     *  Get an image from the feed, this serves
     *  two purposes.
     *  1.  Get the dimensions of the pic
     *  2.  Validate that the previous steps opened the camera
     */

    ret = netcam_read_rtsp_image(netcam);
    if (ret < 0) {
        if (netcam->rtsp->status == RTSP_NOTCONNECTED){
            MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to read first image");
        }
        return -1;
    }

    netcam->width = netcam->rtsp->codec_context->width;
    netcam->height = netcam->rtsp->codec_context->height;

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
        my_frame_free(netcam->rtsp->frame);
        avcodec_close(netcam->rtsp->codec_context);
        avformat_close_input(&netcam->rtsp->format_context);

        MOTION_LOG(NTC, TYPE_NETCAM, NO_ERRNO,"%s: rtsp shut down");
    }

    if (netcam->rtsp->path != NULL) free(netcam->rtsp->path);
    if (netcam->rtsp->user != NULL) free(netcam->rtsp->user);
    if (netcam->rtsp->pass != NULL) free(netcam->rtsp->pass);

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

    av_register_all();
    avformat_network_init();

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
