#include <stdio.h>
#include "netcam_rtsp.h"
#include "motion.h"

#ifdef have_av_get_media_type_string

/****************************************************
 * Duplicated static functions - FIXME
 ****************************************************/

/**
 * netcam_check_buffsize
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
    
    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: expanding buffer from [%d/%d] to [%d/%d] bytes.",
               (int) buff->used, (int) buff->size,
               (int) buff->used, new_size);

    buff->ptr = myrealloc(buff->ptr, new_size,
                          "netcam_check_buf_size");
    buff->size = new_size;
}

/****************************************************
 * End Duplicated static functions - FIXME
 ****************************************************/

static int decode_packet(AVPacket *packet, netcam_buff_ptr buffer, AVFrame *frame, AVCodecContext *cc)
{
  int check = 0;
  int ret = avcodec_decode_video2(cc, frame, &check, packet);
  
  if (ret < 0) {
    MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Error decoding video packet");
    return 0;
  }
  
  if (check == 0) {
    // no frame could be decoded...keep trying
    return 0;
  }

  int frame_size = av_image_get_buffer_size(cc->pix_fmt, cc->width, cc->height, 1);

  /* Assure there's enough room in the buffer. */
  netcam_check_buffsize(buffer, frame_size);

  av_image_copy_to_buffer((uint8_t *)buffer->ptr, frame_size,
			  (const uint8_t **)(frame->data), frame->linesize,
			  cc->pix_fmt, cc->width, cc->height, 1);

  buffer->used = frame_size;
  
  return frame_size;
}

static int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
		MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Could not find stream %s in input!", av_get_media_type_string(type));
        return ret;
    } else {
        *stream_idx = ret;
        st = fmt_ctx->streams[*stream_idx];
        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec) {
    		MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to find %s codec!", av_get_media_type_string(type));
            return ret;
        }
        if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
    		MOTION_LOG(ERR, TYPE_NETCAM, NO_ERRNO, "%s: Failed to open %s codec!", av_get_media_type_string(type));
            return ret;
        }
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
struct rtsp_context *rtsp_new_context(void)
{
  struct rtsp_context *ret;
  
  /* Note that mymalloc will exit on any problem. */
  ret = mymalloc(sizeof(struct rtsp_context));

  memset(ret, 0, sizeof(struct rtsp_context));
  
  return ret;
}

/**
* rtsp_free_context
*
*      Free the resources allocated for this context.
*
* Parameters
*
*      ctxt    Pointer to the rtsp_context structure.
*
* Returns:     Nothing
*
*/
static void rtsp_free_context(struct rtsp_context *ctxt)
{
  if (ctxt == NULL)
    return;
  
  if (ctxt->path != NULL)
    free(ctxt->path);
  
  if (ctxt->user)
    free(ctxt->user);
  
  if (ctxt->pass)
    free(ctxt->pass);
  
  if (ctxt->format_context != NULL) {
    	avformat_close_input(&ctxt->format_context);
  }
  
  if (ctxt->codec_context != NULL) {
    	avcodec_close(ctxt->codec_context);
  }
  
  free(ctxt);
}

int rtsp_connect(netcam_context_ptr netcam)
{
  if (netcam->rtsp == NULL) {
    netcam->rtsp = rtsp_new_context();

    if (netcam->rtsp == NULL) {
      MOTION_LOG(ALR, TYPE_NETCAM, NO_ERRNO, "%s: unable to create context(%s)", netcam->rtsp->path);
      return -1;
    }
  }

  // open the network connection
  AVDictionary *opts = 0;
  av_dict_set(&opts, "rtsp_transport", "tcp", 0);

  int ret = avformat_open_input(&netcam->rtsp->format_context, netcam->rtsp->path, NULL, &opts);
  if (ret < 0) {
    MOTION_LOG(ALR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open input(%s): %d - %s", netcam->rtsp->path, ret, av_err2str(ret));
    rtsp_free_context(netcam->rtsp);
    netcam->rtsp = NULL;
    return -1;
  }

  // fill out stream information
  ret = avformat_find_stream_info(netcam->rtsp->format_context, NULL);
  if (ret < 0) {
    MOTION_LOG(ALR, TYPE_NETCAM, NO_ERRNO, "%s: unable to find stream info: %d", ret);
    rtsp_free_context(netcam->rtsp);
    netcam->rtsp = NULL;
    return -1;
  }

  ret = open_codec_context(&netcam->rtsp->video_stream_index, netcam->rtsp->format_context, AVMEDIA_TYPE_VIDEO);
  if (ret < 0) {
    MOTION_LOG(ALR, TYPE_NETCAM, NO_ERRNO, "%s: unable to open codec context: %d", ret);
    rtsp_free_context(netcam->rtsp);
    netcam->rtsp = NULL;
    return -1;
  }
  
  netcam->rtsp->codec_context = netcam->rtsp->format_context->streams[netcam->rtsp->video_stream_index]->codec;
  
  // start up the feed
  av_read_play(netcam->rtsp->format_context);

  return 0;
}

int netcam_read_rtsp_image(netcam_context_ptr netcam)
{
  if (netcam->rtsp == NULL) {
    if (rtsp_connect(netcam) < 0) {
      return -1;
    }
  }

  AVCodecContext *cc = netcam->rtsp->codec_context;
  AVFormatContext *fc = netcam->rtsp->format_context;
  netcam_buff_ptr buffer;

  /* Point to our working buffer. */
  buffer = netcam->receiving;
  buffer->used = 0;

  AVFrame *frame = avcodec_alloc_frame();

  AVPacket packet;
  
  av_init_packet(&packet);

  packet.data = NULL;
  packet.size = 0;

  int size_decoded = 0;
  static int usual_size_decoded = 0;

  while (size_decoded == 0 && av_read_frame(fc, &packet) >= 0) {

    if(packet.stream_index != netcam->rtsp->video_stream_index) {
      // not our packet, skip
      continue;
    }

    size_decoded = decode_packet(&packet, buffer, frame, cc);
  }

  if (size_decoded == 0) {
    // something went wrong, end of stream?
    MOTION_LOG(ERR, TYPE_NETCAM, SHOW_ERRNO, "%s: invalid frame!");
    return -1;
  }

  if (size_decoded != usual_size_decoded) {
    MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "%s: unusual frame size of %d!", size_decoded);
    usual_size_decoded = size_decoded;
  }

  // at this point, we are finished with the packet and frame, so free them.
  av_free_packet(&packet);
  av_free(frame);
  
  struct timeval curtime;
  
  if (gettimeofday(&curtime, NULL) < 0) {
    MOTION_LOG(WRN, TYPE_NETCAM, SHOW_ERRNO, "%s: gettimeofday");
  }
  
  netcam->receiving->image_time = curtime;
  
  /*
   * Calculate our "running average" time for this netcam's
   * frame transmissions (except for the first time).
   * Note that the average frame time is held in microseconds.
   */
  if (netcam->last_image.tv_sec) {
    netcam->av_frame_time = ((9.0 * netcam->av_frame_time) + 1000000.0 *
			     (curtime.tv_sec - netcam->last_image.tv_sec) +
			     (curtime.tv_usec- netcam->last_image.tv_usec)) / 10.0;
    
    MOTION_LOG(DBG, TYPE_NETCAM, NO_ERRNO, "%s: Calculated frame time %f",
	       netcam->av_frame_time);
  }
  
  netcam->last_image = curtime;
  
  netcam_buff *xchg;
  
  /*
   * read is complete - set the current 'receiving' buffer atomically
   * as 'latest', and make the buffer previously in 'latest' become
   * the new 'receiving'.
   */
  pthread_mutex_lock(&netcam->mutex);

  xchg = netcam->latest;
  netcam->latest = netcam->receiving;
  netcam->receiving = xchg;
  netcam->imgcnt++;
  
  /*
   * We have a new frame ready.  We send a signal so that
   * any thread (e.g. the motion main loop) waiting for the
   * next frame to become available may proceed.
   */
  pthread_cond_signal(&netcam->pic_ready);
  
  pthread_mutex_unlock(&netcam->mutex);
  
  return 0;
}


void netcam_shutdown_rtsp(netcam_context_ptr netcam)
{
  if (netcam->rtsp != NULL) {
    rtsp_free_context(netcam->rtsp);
    netcam->rtsp = NULL;
  }
}

#endif
