/*
 *
 * ffmpeg.c
 *
 * This software is distributed under the GNU Public License version 2
 * See also the file 'COPYING'.
 *
 * The contents of this file has been derived from output_example.c
 * and apiexample.c from the FFmpeg distribution.
 *
 * This file has been modified so that only major versions greater than
 * 53 are supported.
 */


#ifdef HAVE_FFMPEG

#include "ffmpeg.h"
#include "motion.h"

#define AVSTREAM_CODEC_PTR(avs_ptr) (avs_ptr->codec)


/****************************************************************************
 *  The section below is the "my" section of functions.
 *  These are designed to be extremely simple version specific
 *  variants of the libav functions.
 ****************************************************************************/
#if (LIBAVFORMAT_VERSION_MAJOR >= 55) || ((LIBAVFORMAT_VERSION_MAJOR == 54) && (LIBAVFORMAT_VERSION_MINOR > 6))

#define MY_FLAG_READ       AVIO_FLAG_READ
#define MY_FLAG_WRITE      AVIO_FLAG_WRITE
#define MY_FLAG_READ_WRITE AVIO_FLAG_READ_WRITE

#else  //Older versions

#define MY_FLAG_READ       URL_RDONLY
#define MY_FLAG_WRITE      URL_WRONLY
#define MY_FLAG_READ_WRITE URL_RDWR

#endif
/*********************************************/
#if (LIBAVFORMAT_VERSION_MAJOR >= 56)

#define MY_CODEC_ID_MSMPEG4V2 AV_CODEC_ID_MSMPEG4V2
#define MY_CODEC_ID_FLV1      AV_CODEC_ID_FLV1
#define MY_CODEC_ID_FFV1      AV_CODEC_ID_FFV1
#define MY_CODEC_ID_NONE      AV_CODEC_ID_NONE

#else

#define MY_CODEC_ID_MSMPEG4V2 CODEC_ID_MSMPEG4V2
#define MY_CODEC_ID_FLV1      CODEC_ID_FLV1
#define MY_CODEC_ID_FFV1      CODEC_ID_FFV1
#define MY_CODEC_ID_NONE      CODEC_ID_NONE

#endif
/*********************************************/
AVFrame *my_frame_alloc(void){
    AVFrame *pic;
#if (LIBAVFORMAT_VERSION_MAJOR >= 55)
    pic = av_frame_alloc();
#else
    pic = avcodec_alloc_frame();
#endif
    return pic;
}
/*********************************************/
void my_frame_free(AVFrame *frame){
#if (LIBAVFORMAT_VERSION_MAJOR >= 55)
    av_frame_free(&frame);
#else
    //avcodec_free_frame(&frame);
    //av_frame_free(&netcam->rtsp->frame);
    av_freep(&frame);
#endif
}
/****************************************************************************
 ****************************************************************************
 ****************************************************************************/
/**
 * ffmpeg_init
 *      Initializes for libavformat.
 *
 * Returns
 *      Function returns nothing.
 */
void ffmpeg_init()
{
    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "%s: ffmpeg LIBAVCODEC_BUILD %d"
               " LIBAVFORMAT_BUILD %d", LIBAVCODEC_BUILD,
               LIBAVFORMAT_BUILD);
    av_register_all();
    av_log_set_callback((void *)ffmpeg_avcodec_log);
    av_log_set_level(AV_LOG_ERROR);
}
/**
 * get_oformat
 *      Obtains the output format used for the specified codec. For mpeg4 codecs,
 *      the format is avi; for mpeg1 codec, the format is mpeg. The filename has
 *      to be passed, because it gets the appropriate extension appended onto it.
 *
 *  Returns
 *      AVOutputFormat pointer or NULL if any error happens.
 */
static AVOutputFormat *get_oformat(const char *codec, char *filename)
{
    const char *ext;
    AVOutputFormat *of = NULL;
    /*
     * Here, we use guess_format to automatically setup the codec information.
     * If we are using msmpeg4, manually set that codec here.
     * We also dynamically add the file extension to the filename here.
     */
    if (strcmp(codec, TIMELAPSE_CODEC) == 0){
        ext = ".avi";
        of = av_guess_format("avi", NULL, NULL);
    } else if (strcmp(codec, "mpeg4") == 0) {
        ext = ".avi";
        of = av_guess_format("avi", NULL, NULL);
    } else if (strcmp(codec, "msmpeg4") == 0) {
        ext = ".avi";
        of = av_guess_format("avi", NULL, NULL);
        /* Manually override the codec id. */
        if (of) of->video_codec = MY_CODEC_ID_MSMPEG4V2;
    } else if (strcmp(codec, "swf") == 0) {
        ext = ".swf";
        of = av_guess_format("swf", NULL, NULL);
    } else if (strcmp(codec, "flv") == 0) {
        ext = ".flv";
        of = av_guess_format("flv", NULL, NULL);
        of->video_codec = MY_CODEC_ID_FLV1;
    } else if (strcmp(codec, "ffv1") == 0) {
        ext = ".avi";
        of = av_guess_format("avi", NULL, NULL);
        if (of) of->video_codec = MY_CODEC_ID_FFV1;
    } else if (strcmp(codec, "mov") == 0) {
        ext = ".mov";
        of = av_guess_format("mov", NULL, NULL);
	} else if (strcmp (codec, "ogg") == 0){
      ext = ".ogg";
      of = av_guess_format ("ogg", NULL, NULL);
    } else {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: ffmpeg_video_codec option value"
                   " %s is not supported", codec);
        return NULL;
    }

    if (!of) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Could not guess format for %s",
                   codec);
        return NULL;
    }

    /* The 4 allows for ".avi" or ".mpg" to be appended. */
    strncat(filename, ext, 4);

    return of;
}
/**
 * ffmpeg_open
 *      Opens an mpeg file using the new libavformat method. Both mpeg1
 *      and mpeg4 are supported. However, if the current ffmpeg version doesn't allow
 *      mpeg1 with non-standard framerate, the open will fail. Timelapse is a special
 *      case and is tested separately.
 *
 *  Returns
 *      A new allocated ffmpeg struct or NULL if any error happens.
 */
struct ffmpeg *ffmpeg_open(char *ffmpeg_video_codec, char *filename,
                           unsigned char *y, unsigned char *u, unsigned char *v,
                           int width, int height, int rate, int bps, int vbr)
{
    AVCodecContext *c;
    AVCodec *codec;
    struct ffmpeg *ffmpeg;
    int ret;
    /*
     * Allocate space for our ffmpeg structure. This structure contains all the
     * codec and image information we need to generate movies.
     * FIXME when motion exits we should close the movie to ensure that
     * ffmpeg is freed.
     */
    ffmpeg = mymalloc(sizeof(struct ffmpeg));
    memset(ffmpeg, 0, sizeof(struct ffmpeg));

    ffmpeg->vbr = vbr;

    /* Store codec name in ffmpeg->codec, with buffer overflow check. */
    snprintf(ffmpeg->codec, sizeof(ffmpeg->codec), "%s", ffmpeg_video_codec);

    /* Allocation the output media context. */
    ffmpeg->oc = avformat_alloc_context();

    if (!ffmpeg->oc) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Memory error while allocating"
                   " output media context");
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    /* Setup output format */
    ffmpeg->oc->oformat = get_oformat(ffmpeg_video_codec, filename);
    if (!ffmpeg->oc->oformat) {
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    snprintf(ffmpeg->oc->filename, sizeof(ffmpeg->oc->filename), "%s", filename);

    /* Create a new video stream and initialize the codecs. */

    /*
     * Now that all the parameters are set, we can open the video
     * codec and allocate the necessary encode buffers.
     */

    ffmpeg->video_st = NULL;
    if (ffmpeg->oc->oformat->video_codec != MY_CODEC_ID_NONE) {

        codec = avcodec_find_encoder(ffmpeg->oc->oformat->video_codec);
        if (!codec) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Codec %s not found", ffmpeg_video_codec);
            ffmpeg_cleanups(ffmpeg);
            return NULL;
        }

        ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, codec);
        if (!ffmpeg->video_st) {
            MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: av_new_stream - could"
                       " not alloc stream");
            ffmpeg_cleanups(ffmpeg);
            return NULL;
        }
    } else {
        /* We did not get a proper video codec. */
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Failed to obtain a proper"
                   " video codec");
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    ffmpeg->c     = c = AVSTREAM_CODEC_PTR(ffmpeg->video_st);
    c->codec_id   = ffmpeg->oc->oformat->video_codec;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    if (strcmp(ffmpeg_video_codec, "ffv1") == 0)
        c->strict_std_compliance = -2;

    /* Uncomment to allow non-standard framerates. */
    //c->strict_std_compliance = -1;

    /* Set default parameters */
    c->bit_rate = bps;
    c->width    = width;
    c->height   = height;
    /* Frame rate = 1/time_base, so we set 1/rate, not rate/1 */
    c->time_base.num = 1;
    c->time_base.den = rate;

    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s FPS %d", rate);

    if (vbr)
        c->flags |= CODEC_FLAG_QSCALE;

    c->gop_size = 12;

    /* Some formats want stream headers to be separate. */
    if (!strcmp(ffmpeg->oc->oformat->name, "mp4") ||
        !strcmp(ffmpeg->oc->oformat->name, "mov") ||
        !strcmp(ffmpeg->oc->oformat->name, "3gp")) {
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* Dump the format settings.  This shows how the various streams relate to each other. */
    //dump_format(ffmpeg->oc, 0, filename, 1);

    /* Set the picture format - need in ffmpeg starting round April-May 2005 */
    c->pix_fmt = PIX_FMT_YUV420P;

    /* Get a mutex lock. */
    pthread_mutex_lock(&global_lock);

    /* Open the codec */
    ret = avcodec_open2(c, codec, NULL /* options */ );

    if (ret < 0) {
        /* Release the lock. */
        pthread_mutex_unlock(&global_lock);
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: avcodec_open - could not open codec %s",
                   ffmpeg_video_codec);
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    /* Release the lock. */
    pthread_mutex_unlock(&global_lock);

    ffmpeg->video_outbuf = NULL;

    if (!(ffmpeg->oc->oformat->flags & AVFMT_RAWPICTURE)) {
        /*
         * Allocate output buffer
         * XXX: API change will be done
         * ffmpeg->video_outbuf_size = 200000
         */
        ffmpeg->video_outbuf_size = ffmpeg->c->width * 512;
        ffmpeg->video_outbuf = mymalloc(ffmpeg->video_outbuf_size);
    }

    /* Allocate the encoded raw picture. */
    ffmpeg->picture = my_frame_alloc();

    if (!ffmpeg->picture) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: avcodec_alloc_frame -"
                   " could not alloc frame");
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    /* Set variable bitrate if requested. */
    if (ffmpeg->vbr)
        ffmpeg->picture->quality = ffmpeg->vbr;

    /* Set the frame data. */
    ffmpeg->picture->data[0] = y;
    ffmpeg->picture->data[1] = u;
    ffmpeg->picture->data[2] = v;
    ffmpeg->picture->linesize[0] = ffmpeg->c->width;
    ffmpeg->picture->linesize[1] = ffmpeg->c->width / 2;
    ffmpeg->picture->linesize[2] = ffmpeg->c->width / 2;

    /* Open the output file, if needed. */
    if (!(ffmpeg->oc->oformat->flags & AVFMT_NOFILE)) {
        char file_proto[256];

        snprintf(file_proto, sizeof(file_proto), "%s", filename);

        if (avio_open(&ffmpeg->oc->pb, file_proto, MY_FLAG_WRITE) < 0) {
            /* Path did not exist? */
            if (errno == ENOENT) {
                /* Create path for file (don't use file_proto)... */
                if (create_path(filename) == -1) {
                    ffmpeg_cleanups(ffmpeg);
                    return NULL;
                }

                if (avio_open(&ffmpeg->oc->pb, file_proto, MY_FLAG_WRITE) < 0) {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: url_fopen -"
                               " error opening file %s", filename);
                    ffmpeg_cleanups(ffmpeg);
                    return NULL;
                }
                /* Permission denied */
            } else if (errno ==  EACCES) {
                MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO,
                           "%s: url_fopen - error opening file %s"
                           " ... check access rights to target directory",
                           filename);
                ffmpeg_cleanups(ffmpeg);
                return NULL;
            } else {
                MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error opening file %s",
                           filename);
                ffmpeg_cleanups(ffmpeg);
                return NULL;
            }
        }
    }

    /* Write the stream header, if any. */
    avformat_write_header(ffmpeg->oc, NULL);
    return ffmpeg;
}
/**
 * ffmpeg_cleanups
 *      Clean up ffmpeg struct if something was wrong.
 *
 * Returns
 *      Function returns nothing.
 */
void ffmpeg_cleanups(struct ffmpeg *ffmpeg)
{

    /* Close each codec */
    if (ffmpeg->video_st) {
        pthread_mutex_lock(&global_lock);
        if (ffmpeg->video_st->codec->priv_data != NULL)
            avcodec_close(AVSTREAM_CODEC_PTR(ffmpeg->video_st));
        pthread_mutex_unlock(&global_lock);
        av_freep(&ffmpeg->picture);
        free(ffmpeg->video_outbuf);
    }

    avcodec_close(ffmpeg->c);
    avformat_free_context(ffmpeg->oc);
    free(ffmpeg);
}
/**
 * ffmpeg_close
 *      Closes a video file.
 *
 * Returns
 *      Function returns nothing.
 */
void ffmpeg_close(struct ffmpeg *ffmpeg)
{

    /* Write the trailer, if any. */
    av_write_trailer(ffmpeg->oc);

    /* Close each codec */
    if (ffmpeg->video_st) {
        pthread_mutex_lock(&global_lock);
        avcodec_close(AVSTREAM_CODEC_PTR(ffmpeg->video_st));
        pthread_mutex_unlock(&global_lock);
        av_freep(&ffmpeg->picture);
        free(ffmpeg->video_outbuf);
    }

    avcodec_close(ffmpeg->c);
    if (!(ffmpeg->oc->oformat->flags & AVFMT_NOFILE)) {
        /* Close the output file. */
        avio_close(ffmpeg->oc->pb);
    }

    avformat_free_context(ffmpeg->oc);
    free(ffmpeg);

}
/**
 * ffmpeg_put_image
 *      Puts the image pointed to by ffmpeg->picture.
 *
 * Returns
 *      value returned by ffmpeg_put_frame call.
 */
int ffmpeg_put_image(struct ffmpeg *ffmpeg)
{
    return ffmpeg_put_frame(ffmpeg, ffmpeg->picture);
}

/**
 * ffmpeg_put_other_image
 *      Puts an arbitrary picture defined by y, u and v.
 *
 * Returns
 *      Number of bytes written by ffmpeg_put_frame
 *      -1 if any error happens in ffmpeg_put_frame
 *       0 if error allocating picture.
 */
int ffmpeg_put_other_image(struct ffmpeg *ffmpeg, unsigned char *y,
                            unsigned char *u, unsigned char *v)
{
    AVFrame *picture;
    int ret = 0;

    /* Allocate the encoded raw picture. */
    picture = ffmpeg_prepare_frame(ffmpeg, y, u, v);

    if (picture) {
        ret = ffmpeg_put_frame(ffmpeg, picture);
        if (!ret)
            av_free(picture);
    }

    return ret;
}

/**
 * ffmpeg_put_frame
 *      Encodes and writes a video frame using the av_write_frame API. This is
 *      a helper function for ffmpeg_put_image and ffmpeg_put_other_image.
 *
 *  Returns
 *      Number of bytes written or -1 if any error happens.
 */
int ffmpeg_put_frame(struct ffmpeg *ffmpeg, AVFrame *pic)
{
/**
 * Since the logic,return values and conditions changed so
 * dramatically between versions, the encoding of the frame
 * is 100% blocked based upon Libav/FFMpeg version
 */
#if (LIBAVFORMAT_VERSION_MAJOR >= 55) || ((LIBAVFORMAT_VERSION_MAJOR == 54) && (LIBAVFORMAT_VERSION_MINOR > 6))
    int retcd;
    int got_packet_ptr;
    AVPacket pkt;

    if (ffmpeg->oc->oformat->flags & AVFMT_RAWPICTURE) {
        av_init_packet(&pkt); /* Init static structure. */
           pkt.stream_index = ffmpeg->video_st->index;
           pkt.flags |= AV_PKT_FLAG_KEY;
           pkt.data = (uint8_t *)pic;
           pkt.size = sizeof(AVPicture);
           retcd = av_write_frame(ffmpeg->oc, &pkt);
        av_free_packet(&pkt);
    } else {
        av_init_packet(&pkt);
        pkt.data = NULL;
        retcd = avcodec_encode_video2(AVSTREAM_CODEC_PTR(ffmpeg->video_st),
                                        &pkt, pic, &got_packet_ptr);
        if (retcd < 0 ){
            MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error encoding video");
            //Packet is freed upon failure of encoding
            return -1;
        }
        //No error returned, now check packet encoding status.
        if (got_packet_ptr == 0){
            MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error encoding video");
            av_free_packet(&pkt);
            return -1;
        }

        pkt.pts = AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->pts;
        if (AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->key_frame)
                pkt.flags |= AV_PKT_FLAG_KEY;

        retcd = av_write_frame(ffmpeg->oc, &pkt);
        av_free_packet(&pkt);
    }

    if (retcd != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error while writing video frame");
        ffmpeg_cleanups(ffmpeg);
        return -1;
    }

    return retcd;

#else  //  Old versions of Libav/FFmpeg
    int retcd;
    AVPacket pkt;

    av_init_packet(&pkt); /* Init static structure. */
    pkt.stream_index = ffmpeg->video_st->index;
    if (ffmpeg->oc->oformat->flags & AVFMT_RAWPICTURE) {
        // Raw video case.
        pkt.size = sizeof(AVPicture);
        pkt.data = (uint8_t *)pic;
        pkt.flags |= AV_PKT_FLAG_KEY;
    } else {
        retcd = avcodec_encode_video(AVSTREAM_CODEC_PTR(ffmpeg->video_st),
                                        ffmpeg->video_outbuf,
                                        ffmpeg->video_outbuf_size, pic);
        if (retcd <= 0 ){
            MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error encoding video");
            av_free_packet(&pkt);
            return -1;
        }
        pkt.size = retcd;
        pkt.data = ffmpeg->video_outbuf;
        pkt.pts = AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->pts;
        if (AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
    }

    retcd = av_write_frame(ffmpeg->oc, &pkt);
    av_free_packet(&pkt);

    if (retcd != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error while writing video frame");
        ffmpeg_cleanups(ffmpeg);
        return -1;
    }

    return retcd;

#endif
}

/**
 * ffmpeg_prepare_frame
 *      Allocates and prepares a picture frame by setting up the U, Y and V pointers in
 *      the frame according to the passed pointers.
 *
 * Returns
 *      NULL If the allocation fails.
 *
 *      The returned AVFrame pointer must be freed after use.
 */
AVFrame *ffmpeg_prepare_frame(struct ffmpeg *ffmpeg, unsigned char *y,
                              unsigned char *u, unsigned char *v)
{
    AVFrame *picture;

    picture = my_frame_alloc();

    if (!picture) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Could not alloc frame");
        return NULL;
    }

    /* Take care of variable bitrate setting. */
    if (ffmpeg->vbr)
        picture->quality = ffmpeg->vbr;

    /* Setup pointers and line widths. */
    picture->data[0] = y;
    picture->data[1] = u;
    picture->data[2] = v;
    picture->linesize[0] = ffmpeg->c->width;
    picture->linesize[1] = ffmpeg->c->width / 2;
    picture->linesize[2] = ffmpeg->c->width / 2;

    return picture;
}
/**
 * ffmpeg_avcodec_log
 *      Handle any logging output from the ffmpeg library avcodec.
 *
 * Parameters
 *      *ignoreme  A pointer we will ignore
 *      errno_flag The error number value
 *      fmt        Text message to be used for log entry in printf() format.
 *      ap         List of variables to be used in formatted message text.
 *
 * Returns
 *      Function returns nothing.
 */
void ffmpeg_avcodec_log(void *ignoreme ATTRIBUTE_UNUSED, int errno_flag, const char *fmt, va_list vl)
{
    char buf[1024];

    /* Flatten the message coming in from avcodec. */
    vsnprintf(buf, sizeof(buf), fmt, vl);

    /* If the debug_level is correct then send the message to the motion logging routine. */
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s: %s - flag %d",
               buf, errno_flag);
}

#endif /* HAVE_FFMPEG */
