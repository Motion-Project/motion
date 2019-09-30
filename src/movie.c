/*
 *
 * movie.c
 *
 * This software is distributed under the GNU Public License version 2
 * See also the file 'COPYING'.
 *
 * The contents of this file has been derived from output_example.c
 * and apiexample.c from the FFmpeg distribution.
 *
 * This file has been modified so that only major versions greater than
 * 53 are supported.
 * Note that while the conditions are based upon LIBAVFORMAT, not all of the changes are
 * specific to libavformat.h.  Some changes could be related to other components of ffmpeg.
 * This is for simplicity.  The avformat version has historically changed at the same time
 * as the other components so it is easier to have a single version number to track rather
 * than the particular version numbers which are associated with each component.
 * The libav variant also has different apis with the same major/minor version numbers.
 * As such, it is occasionally necessary to look at the microversion number.  Numbers
 * greater than 100 for micro version indicate ffmpeg whereas numbers less than 100
 * indicate libav
*/

#include "motion.h"

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
    #define MY_CODEC_ID_MPEG2VIDEO AV_CODEC_ID_MPEG2VIDEO
    #define MY_CODEC_ID_H264      AV_CODEC_ID_H264
    #define MY_CODEC_ID_HEVC      AV_CODEC_ID_HEVC
#else
    #define MY_CODEC_ID_MSMPEG4V2 CODEC_ID_MSMPEG4V2
    #define MY_CODEC_ID_FLV1      CODEC_ID_FLV1
    #define MY_CODEC_ID_FFV1      CODEC_ID_FFV1
    #define MY_CODEC_ID_NONE      CODEC_ID_NONE
    #define MY_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
    #define MY_CODEC_ID_H264      CODEC_ID_H264
    #define MY_CODEC_ID_HEVC      CODEC_ID_H264
#endif

/*********************************************/
#if (LIBAVCODEC_VERSION_MAJOR >= 57)
    #define MY_CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
    #define MY_CODEC_FLAG_QSCALE        AV_CODEC_FLAG_QSCALE
#else
    #define MY_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
    #define MY_CODEC_FLAG_QSCALE        CODEC_FLAG_QSCALE
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
        av_freep(&frame);
    #endif
}
/*********************************************/
int my_image_get_buffer_size(enum MyPixelFormat pix_fmt, int width, int height){
    int retcd = 0;
    #if (LIBAVFORMAT_VERSION_MAJOR >= 57)
        int align = 1;
        retcd = av_image_get_buffer_size(pix_fmt, width, height, align);
    #else
        retcd = avpicture_get_size(pix_fmt, width, height);
    #endif
    return retcd;
}
/*********************************************/
int my_image_copy_to_buffer(AVFrame *frame, uint8_t *buffer_ptr, enum MyPixelFormat pix_fmt,int width, int height,int dest_size){
    int retcd = 0;
    #if (LIBAVFORMAT_VERSION_MAJOR >= 57)
        int align = 1;
        retcd = av_image_copy_to_buffer((uint8_t *)buffer_ptr,dest_size
            ,(const uint8_t * const*)frame,frame->linesize,pix_fmt,width,height,align);
    #else
        retcd = avpicture_layout((const AVPicture*)frame,pix_fmt,width,height
            ,(unsigned char *)buffer_ptr,dest_size);
    #endif
    return retcd;
}
/*********************************************/
int my_image_fill_arrays(AVFrame *frame,uint8_t *buffer_ptr,enum MyPixelFormat pix_fmt,int width,int height){
    int retcd = 0;
    #if (LIBAVFORMAT_VERSION_MAJOR >= 57)
        int align = 1;
        retcd = av_image_fill_arrays(
            frame->data
            ,frame->linesize
            ,buffer_ptr
            ,pix_fmt
            ,width
            ,height
            ,align
        );
    #else
        retcd = avpicture_fill(
            (AVPicture *)frame
            ,buffer_ptr
            ,pix_fmt
            ,width
            ,height);
    #endif
    return retcd;
}
/*********************************************/
void my_packet_unref(AVPacket pkt){
    #if (LIBAVFORMAT_VERSION_MAJOR >= 57)
        av_packet_unref(&pkt);
    #else
        av_free_packet(&pkt);
    #endif
}
/*********************************************/
void my_avcodec_close(AVCodecContext *codec_context){
    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        avcodec_free_context(&codec_context);
    #else
        avcodec_close(codec_context);
    #endif
}
/*********************************************/
int my_copy_packet(AVPacket *dest_pkt, AVPacket *src_pkt){
    #if (LIBAVFORMAT_VERSION_MAJOR >= 55)
        return av_packet_ref(dest_pkt, src_pkt);
    #else
        /* Old versions of libav do not support copying packet
        * We therefore disable the pass through recording and
        * for this function, simply do not do anything
        */
        if (dest_pkt == src_pkt ){
            return 0;
        } else {
            return 0;
        }
    #endif
}
/*********************************************/

/****************************************************************************
 ****************************************************************************
 ****************************************************************************/
/*********************************************/
static void movie_free_nal(struct ctx_movie *movie){
    if (movie->nal_info) {
        free(movie->nal_info);
        movie->nal_info = NULL;
        movie->nal_info_len = 0;
    }
}

static void movie_encode_nal(struct ctx_movie *movie){
    // h264_v4l2m2m has NAL units separated from the first frame, which makes
    // some players very unhappy.
    if ((movie->pkt.pts == 0) && (!(movie->pkt.flags & AV_PKT_FLAG_KEY))) {
        movie_free_nal(movie);
        movie->nal_info_len = movie->pkt.size;
        movie->nal_info = malloc(movie->nal_info_len);
        if (movie->nal_info)
            memcpy(movie->nal_info, &movie->pkt.data[0], movie->nal_info_len);
        else
            movie->nal_info_len = 0;
    } else if (movie->nal_info) {
        int old_size = movie->pkt.size;
        av_grow_packet(&movie->pkt, movie->nal_info_len);
        memmove(&movie->pkt.data[movie->nal_info_len], &movie->pkt.data[0], old_size);
        memcpy(&movie->pkt.data[0], movie->nal_info, movie->nal_info_len);
        movie_free_nal(movie);
    }
}

static int movie_timelapse_exists(const char *fname){
    FILE *file;
    file = fopen(fname, "r");
    if (file)
    {
        fclose(file);
        return 1;
    }
    return 0;
}

static int movie_timelapse_append(struct ctx_movie *movie, AVPacket pkt){
    FILE *file;

    file = fopen(movie->filename, "a");
    if (!file) return -1;

    fwrite(pkt.data,1,pkt.size,file);

    fclose(file);

    return 0;
}

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
    /* TODO Determine if this is even needed for old versions. Per
    * documentation for version 58, 'av_lockmgr_register This function does nothing'
    */
    static int movie_lockmgr_cb(void **arg, enum AVLockOp op){
        pthread_mutex_t *mutex = *arg;
        int err;

        switch (op) {
        case AV_LOCK_CREATE:
            mutex = malloc(sizeof(*mutex));
            if (!mutex)
                return AVERROR(ENOMEM);
            if ((err = pthread_mutex_init(mutex, NULL))) {
                free(mutex);
                return AVERROR(err);
            }
            *arg = mutex;
            return 0;
        case AV_LOCK_OBTAIN:
            if ((err = pthread_mutex_lock(mutex)))
                return AVERROR(err);

            return 0;
        case AV_LOCK_RELEASE:
            if ((err = pthread_mutex_unlock(mutex)))
                return AVERROR(err);

            return 0;
        case AV_LOCK_DESTROY:
            if (mutex)
                pthread_mutex_destroy(mutex);
            free(mutex);
            *arg = NULL;
            return 0;
        }
        return 1;
    }
#endif

static void movie_free_context(struct ctx_movie *movie){

        if (movie->picture != NULL){
            my_frame_free(movie->picture);
            movie->picture = NULL;
        }

        if (movie->ctx_codec != NULL){
            my_avcodec_close(movie->ctx_codec);
            movie->ctx_codec = NULL;
        }

        if (movie->oc != NULL){
            avformat_free_context(movie->oc);
            movie->oc = NULL;
        }

}

static int movie_get_oformat(struct ctx_movie *movie){

    size_t codec_name_len = strcspn(movie->codec_name, ":");
    char *codec_name = malloc(codec_name_len + 1);
    char basename[PATH_MAX];
    int retcd;

    /* TODO:  Rework the extenstion asssignment along with the code in event.c
     * If extension is ever something other than three bytes,
     * preceded by . then lots of things will fail
     */
    if (codec_name == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Failed to allocate memory for codec name"));
        movie_free_context(movie);
        return -1;
    }
    memcpy(codec_name, movie->codec_name, codec_name_len);
    codec_name[codec_name_len] = 0;

    /* Only the newer codec and containers can handle the really fast FPS */
    if (((strcmp(codec_name, "msmpeg4") == 0) ||
        (strcmp(codec_name, "mpeg4") == 0) ||
        (strcmp(codec_name, "swf") == 0) ) && (movie->fps >50)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("The frame rate specified is too high for the movie movie type specified. "
            "Choose a different movie container or lower framerate."));
        movie_free_context(movie);
        free(codec_name);
        return -1;
    }

    retcd = snprintf(basename,PATH_MAX,"%s",movie->filename);
    if ((retcd < 0) || (retcd >= PATH_MAX)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting base file name"));
        movie_free_context(movie);
        free(codec_name);
        return -1;
    }

    if (movie->tlapse == TIMELAPSE_APPEND){
        movie->oc->oformat = av_guess_format ("mpeg2video", NULL, NULL);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_MPEG2VIDEO;
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mpg",basename);
        if ((!movie->oc->oformat) ||
            (retcd < 0) || (retcd >= PATH_MAX)){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting timelapse append for codec %s"), codec_name);
            movie_free_context(movie);
            free(codec_name);
            return -1;
        }
        free(codec_name);
        return 0;
    }

    if (strcmp(codec_name, "mpeg4") == 0) {
        movie->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.avi",basename);
    }

    if (strcmp(codec_name, "msmpeg4") == 0) {
        movie->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.avi",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_MSMPEG4V2;
    }

    if (strcmp(codec_name, "swf") == 0) {
        movie->oc->oformat = av_guess_format("swf", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.swf",basename);
    }

    if (strcmp(codec_name, "flv") == 0) {
        movie->oc->oformat = av_guess_format("flv", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.flv",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_FLV1;
    }

    if (strcmp(codec_name, "ffv1") == 0) {
        movie->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.avi",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_FFV1;
    }

    if (strcmp(codec_name, "mov") == 0) {
        movie->oc->oformat = av_guess_format("mov", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mov",basename);
    }

    if (strcmp(codec_name, "mp4") == 0) {
        movie->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mp4",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_H264;
    }

    if (strcmp(codec_name, "mkv") == 0) {
        movie->oc->oformat = av_guess_format("matroska", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mkv",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_H264;
    }

    if (strcmp(codec_name, "hevc") == 0) {
        movie->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(movie->filename,PATH_MAX,"%s.mp4",basename);
        if (movie->oc->oformat) movie->oc->oformat->video_codec = MY_CODEC_ID_HEVC;
    }

    //Check for valid results
    if ((retcd < 0) || (retcd >= PATH_MAX)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting file name"));
        movie_free_context(movie);
        free(codec_name);
        return -1;
    }

    if (!movie->oc->oformat) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("codec option value %s is not supported"), codec_name);
        movie_free_context(movie);
        free(codec_name);
        return -1;
    }

    if (movie->oc->oformat->video_codec == MY_CODEC_ID_NONE) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get the codec"));
        movie_free_context(movie);
        free(codec_name);
        return -1;
    }

    free(codec_name);
    return 0;
}

static int movie_encode_video(struct ctx_movie *movie){

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        //ffmpeg version 3.1 and after
        int retcd = 0;
        char errstr[128];

        retcd = avcodec_send_frame(movie->ctx_codec, movie->picture);
        if (retcd < 0 ){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error sending frame for encoding:%s"),errstr);
            return -1;
        }
        retcd = avcodec_receive_packet(movie->ctx_codec, &movie->pkt);
        if (retcd == AVERROR(EAGAIN)){
            //Buffered packet.  Throw special return code
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO
                ,_("Receive packet threw EAGAIN returning -2 code :%s"),errstr);
            my_packet_unref(movie->pkt);
            return -2;
        }
        if (retcd < 0 ){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error receiving encoded packet video:%s"),errstr);
            //Packet is freed upon failure of encoding
            return -1;
        }

        if (movie->preferred_codec == USER_CODEC_V4L2M2M){
            movie_encode_nal(movie);
        }

        return 0;

    #elif (LIBAVFORMAT_VERSION_MAJOR >= 55) || ((LIBAVFORMAT_VERSION_MAJOR == 54) && (LIBAVFORMAT_VERSION_MINOR > 6))

        int retcd = 0;
        char errstr[128];
        int got_packet_ptr;

        retcd = avcodec_encode_video2(movie->ctx_codec, &movie->pkt, movie->picture, &got_packet_ptr);
        if (retcd < 0 ){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video:%s"),errstr);
            //Packet is freed upon failure of encoding
            return -1;
        }
        if (got_packet_ptr == 0){
            //Buffered packet.  Throw special return code
            my_packet_unref(movie->pkt);
            return -2;
        }

        /* This kills compiler warnings.  Nal setting is only for recent movie versions*/
        if (movie->preferred_codec == USER_CODEC_V4L2M2M){
            movie_encode_nal(movie);
        }

        return 0;

    #else

        int retcd = 0;
        uint8_t *video_outbuf;
        int video_outbuf_size;

        video_outbuf_size = (movie->ctx_codec->width +16) * (movie->ctx_codec->height +16) * 1;
        video_outbuf = mymalloc(video_outbuf_size);

        retcd = avcodec_encode_video(movie->video_st->codec, video_outbuf, video_outbuf_size, movie->picture);
        if (retcd < 0 ){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video"));
            my_packet_unref(movie->pkt);
            return -1;
        }
        if (retcd == 0 ){
            // No bytes encoded => buffered=>special handling
            my_packet_unref(movie->pkt);
            return -2;
        }

        // Encoder did not provide metadata, set it up manually
        movie->pkt.size = retcd;
        movie->pkt.data = video_outbuf;

        if (movie->picture->key_frame == 1)
        movie->pkt.flags |= AV_PKT_FLAG_KEY;

        movie->pkt.pts = movie->picture->pts;
        movie->pkt.dts = movie->pkt.pts;

        free(video_outbuf);

        /* This kills compiler warnings.  Nal setting is only for recent movie versions*/
        if (movie->preferred_codec == USER_CODEC_V4L2M2M){
            movie_encode_nal(movie);
        }

        return 0;

    #endif

}

static int movie_set_pts(struct ctx_movie *movie, const struct timespec *ts1){

    int64_t pts_interval;

    if (movie->tlapse != TIMELAPSE_NONE) {
        movie->last_pts++;
        movie->picture->pts = movie->last_pts;
    } else {
        pts_interval = ((1000000L * (ts1->tv_sec - movie->start_time.tv_sec)) + (ts1->tv_nsec/1000) - (movie->start_time.tv_nsec/1000));
        if (pts_interval < 0){
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            movie_reset_start_time(movie, ts1);
            pts_interval = 0;
        }
        if (movie->last_pts < 0) {
            // This is the very first frame, ensure PTS is zero
            movie->picture->pts = 0;
        } else
            movie->picture->pts = av_rescale_q(pts_interval,(AVRational){1, 1000000L},movie->video_st->time_base) + movie->base_pts;

        if (movie->test_mode == TRUE){
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                ,_("PTS %"PRId64" Base PTS %"PRId64" ms interval %"PRId64" timebase %d-%d")
                ,movie->picture->pts,movie->base_pts,pts_interval
                ,movie->video_st->time_base.num,movie->video_st->time_base.den);
        }

        if (movie->picture->pts <= movie->last_pts){
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (movie->test_mode == TRUE){
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        movie->last_pts = movie->picture->pts;
    }
    return 0;
}

static int movie_set_pktpts(struct ctx_movie *movie, const struct timespec *ts1){

    int64_t pts_interval;

    if (movie->tlapse != TIMELAPSE_NONE) {
        movie->last_pts++;
        movie->pkt.pts = movie->last_pts;
    } else {
        pts_interval = ((1000000L * (ts1->tv_sec - movie->start_time.tv_sec)) + (ts1->tv_nsec/1000) - (movie->start_time.tv_nsec/1000));
        if (pts_interval < 0){
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            movie_reset_start_time(movie, ts1);
            pts_interval = 0;
        }
        movie->pkt.pts = av_rescale_q(pts_interval,(AVRational){1, 1000000L},movie->video_st->time_base) + movie->base_pts;

        if (movie->test_mode == TRUE){
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                       ,_("PTS %"PRId64" Base PTS %"PRId64" ms interval %"PRId64" timebase %d-%d Change %d")
                       ,movie->pkt.pts
                       ,movie->base_pts,pts_interval
                       ,movie->video_st->time_base.num
                       ,movie->video_st->time_base.den
                       ,(movie->pkt.pts-movie->last_pts) );
        }

        if (movie->pkt.pts <= movie->last_pts){
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (movie->test_mode == TRUE){
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        movie->last_pts = movie->pkt.pts;
        movie->pkt.dts=movie->pkt.pts;
    }
    return 0;
}

static int movie_set_quality(struct ctx_movie *movie){

    movie->opts = 0;
    if (movie->quality > 100) movie->quality = 100;
    if (movie->ctx_codec->codec_id == MY_CODEC_ID_H264 ||
        movie->ctx_codec->codec_id == MY_CODEC_ID_HEVC){
        if (movie->quality <= 0)
            movie->quality = 45; // default to 45% quality
        av_dict_set(&movie->opts, "preset", "ultrafast", 0);
        av_dict_set(&movie->opts, "tune", "zerolatency", 0);
        /* This next if statement needs validation.  Are mpeg4omx
         * and v4l2m2m even MY_CODEC_ID_H264 or MY_CODEC_ID_HEVC
         * such that it even would be possible to be part of this
         * if block to start with? */
        if ((movie->preferred_codec == USER_CODEC_H264OMX) ||
            (movie->preferred_codec == USER_CODEC_MPEG4OMX) ||
            (movie->preferred_codec == USER_CODEC_V4L2M2M)) {
            // bit_rate = movie->width * movie->height * movie->fps * quality_factor
            movie->quality = (int)(((int64_t)movie->width * movie->height * movie->fps * movie->quality) >> 7);
            // Clip bit rate to min
            if (movie->quality < 4000) // magic number
                movie->quality = 4000;
            movie->ctx_codec->profile = FF_PROFILE_H264_HIGH;
            movie->ctx_codec->bit_rate = movie->quality;
        } else {
            // Control other H264 encoders quality via CRF
            char crf[10];
            movie->quality = (int)(( (100-movie->quality) * 51)/100);
            snprintf(crf, 10, "%d", movie->quality);
            av_dict_set(&movie->opts, "crf", crf, 0);
        }
    } else {
        /* The selection of 8000 is a subjective number based upon viewing output files */
        if (movie->quality > 0){
            movie->quality =(int)(((100-movie->quality)*(100-movie->quality)*(100-movie->quality) * 8000) / 1000000) + 1;
            movie->ctx_codec->flags |= MY_CODEC_FLAG_QSCALE;
            movie->ctx_codec->global_quality=movie->quality;
        }
    }
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("%s codec vbr/crf/bit_rate: %d"), movie->codec->name, movie->quality);

    return 0;
}

struct blacklist_t
{
    const char *codec_name;
    const char *reason;
};

static const char *movie_codec_is_blacklisted(const char *codec_name){

    static struct blacklist_t blacklisted_codec[] =
    {
    #if (LIBAVFORMAT_VERSION_MAJOR < 58) || ( (LIBAVFORMAT_VERSION_MAJOR == 58) && ( (LIBAVFORMAT_VERSION_MINOR < 29) || ((LIBAVFORMAT_VERSION_MINOR == 29) && (LIBAVFORMAT_VERSION_MICRO <= 100)) ) )
            /* h264_omx & ffmpeg combination locks up on Raspberry Pi.
            * Newer versions of ffmpeg allow zerocopy to be disabled to workaround
            * this issue.
            * To use h264_omx encoder on older versions of ffmpeg:
            * - disable input_zerocopy in ffmpeg omx.c:omx_encode_init function.
            * - remove the "h264_omx" from this blacklist.
            * More information: https://github.com/Motion-Project/motion/issues/433
            */
            {"h264_omx", "Codec causes lock up on your FFMpeg version"},
    #endif
    #if (LIBAVFORMAT_VERSION_MAJOR < 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR < 41))
            {"h264_v4l2m2m", "FFMpeg version is too old"},
    #endif
    };
    size_t i;

    for (i = 0; i < sizeof(blacklisted_codec)/sizeof(blacklisted_codec[0]); i++) {
        if (strcmp(codec_name, blacklisted_codec[i].codec_name) == 0)
            return blacklisted_codec[i].reason;
    }
    return NULL;
}

static int movie_set_codec_preferred(struct ctx_movie *movie){
    size_t codec_name_len = strcspn(movie->codec_name, ":");

    movie->codec = NULL;
    if (movie->codec_name[codec_name_len]) {
        const char *blacklist_reason = movie_codec_is_blacklisted(&movie->codec_name[codec_name_len+1]);
        if (blacklist_reason) {
            MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                ,_("Preferred codec %s has been blacklisted: %s")
                ,&movie->codec_name[codec_name_len+1], blacklist_reason);
        } else {
            movie->codec = avcodec_find_encoder_by_name(&movie->codec_name[codec_name_len+1]);
            if ((movie->oc->oformat) && (movie->codec != NULL)) {
                    movie->oc->oformat->video_codec = movie->codec->id;
            } else if (movie->codec == NULL) {
                MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                    ,_("Preferred codec %s not found")
                    ,&movie->codec_name[codec_name_len+1]);
            }
        }
    }
    if (!movie->codec)
        movie->codec = avcodec_find_encoder(movie->oc->oformat->video_codec);
    if (!movie->codec) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Codec %s not found"), movie->codec_name);
        movie_free_context(movie);
        return -1;
    }

    if (strcmp(movie->codec->name, "h264_v4l2m2m") == 0){
        movie->preferred_codec = USER_CODEC_V4L2M2M;
    } else if (strcmp(movie->codec->name, "h264_omx") == 0){
        movie->preferred_codec = USER_CODEC_H264OMX;
    } else if (strcmp(movie->codec->name, "mpeg4_omx") == 0){
        movie->preferred_codec = USER_CODEC_MPEG4OMX;
    } else {
        movie->preferred_codec = USER_CODEC_DEFAULT;
    }

    if (movie->codec_name[codec_name_len])
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO,_("Using codec %s"), movie->codec->name);

    return 0;

}

static int movie_set_codec(struct ctx_movie *movie){

    int retcd;
    char errstr[128];
    int chkrate;

    retcd = movie_set_codec_preferred(movie);
    if (retcd != 0) return retcd;

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        //If we provide the codec to this, it results in a memory leak.  movie ticket: 5714
        movie->video_st = avformat_new_stream(movie->oc, NULL);
        if (!movie->video_st) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            movie_free_context(movie);
            return -1;
        }
        movie->ctx_codec = avcodec_alloc_context3(movie->codec);
        if (movie->ctx_codec == NULL) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate decoder!"));
            movie_free_context(movie);
            return -1;
        }
    #else
        movie->video_st = avformat_new_stream(movie->oc, movie->codec);
        if (!movie->video_st) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            movie_free_context(movie);
            return -1;
        }
        movie->ctx_codec = movie->video_st->codec;
    #endif


    if (movie->tlapse != TIMELAPSE_NONE) {
        movie->ctx_codec->gop_size = 1;
    } else {
        if (movie->fps <= 5){
            movie->ctx_codec->gop_size = 1;
        } else if (movie->fps > 30){
            movie->ctx_codec->gop_size = 15;
        } else {
            movie->ctx_codec->gop_size = (movie->fps / 2);
        }
        movie->gop_cnt = movie->ctx_codec->gop_size - 1;
    }

    /*  For certain containers, setting the fps to very low numbers results in
    **  a very poor quality playback.  We can set the FPS to a higher number and
    **  then let the PTS display the frames correctly.
    */
    if ((movie->tlapse == TIMELAPSE_NONE) && (movie->fps <= 5)){
        if ((strcmp(movie->codec_name, "msmpeg4") == 0) ||
            (strcmp(movie->codec_name, "flv")     == 0) ||
            (strcmp(movie->codec_name, "mov") == 0) ||
            (strcmp(movie->codec_name, "mp4") == 0) ||
            (strcmp(movie->codec_name, "hevc") == 0) ||
            (strcmp(movie->codec_name, "mpeg4")   == 0)) {
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Low fps. Encoding %d frames into a %d frames container.", movie->fps, 10);
            movie->fps = 10;
        }
    }

    movie->ctx_codec->codec_id      = movie->oc->oformat->video_codec;
    movie->ctx_codec->codec_type    = AVMEDIA_TYPE_VIDEO;
    movie->ctx_codec->bit_rate      = movie->bps;
    movie->ctx_codec->width         = movie->width;
    movie->ctx_codec->height        = movie->height;
    movie->ctx_codec->time_base.num = 1;
    movie->ctx_codec->time_base.den = movie->fps;
    if (movie->preferred_codec == USER_CODEC_V4L2M2M){
        movie->ctx_codec->pix_fmt   = AV_PIX_FMT_NV21;
    } else {
        movie->ctx_codec->pix_fmt   = MY_PIX_FMT_YUV420P;
    }
    movie->ctx_codec->max_b_frames  = 0;
    if (strcmp(movie->codec_name, "ffv1") == 0){
      movie->ctx_codec->strict_std_compliance = -2;
      movie->ctx_codec->level = 3;
    }
    movie->ctx_codec->flags |= MY_CODEC_FLAG_GLOBAL_HEADER;

    if ((strcmp(movie->codec->name, "h264_omx") == 0) ||
        (strcmp(movie->codec->name, "mpeg4_omx") == 0)) {
        /* h264_omx & ffmpeg combination locks up on Raspberry Pi.
         * To use h264_omx encoder, we need to disable zerocopy.
         * More information: https://github.com/Motion-Project/motion/issues/433
         */
        av_dict_set(&movie->opts, "zerocopy", "0", 0);
    }

    retcd = movie_set_quality(movie);
    if (retcd < 0){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to set quality"));
        return -1;
    }

    retcd = avcodec_open2(movie->ctx_codec, movie->codec, &movie->opts);
    if (retcd < 0) {
        if (movie->codec->supported_framerates) {
            const AVRational *fps = movie->codec->supported_framerates;
            while (fps->num) {
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                    ,_("Reported FPS Supported %d/%d"), fps->num, fps->den);
                fps++;
            }
        }
        chkrate = 1;
        while ((chkrate < 36) && (retcd != 0)) {
            movie->ctx_codec->time_base.den = chkrate;
            retcd = avcodec_open2(movie->ctx_codec, movie->codec, &movie->opts);
            chkrate++;
        }
        if (retcd < 0){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not open codec %s"),errstr);
            av_dict_free(&movie->opts);
            movie_free_context(movie);
            return -1;
        }

    }
    av_dict_free(&movie->opts);

    return 0;
}

static int movie_set_stream(struct ctx_movie *movie){

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        int retcd;
        char errstr[128];

        retcd = avcodec_parameters_from_context(movie->video_st->codecpar,movie->ctx_codec);
        if (retcd < 0) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Failed to copy decoder parameters!: %s"), errstr);
            movie_free_context(movie);
            return -1;
        }
    #endif

    movie->video_st->time_base = (AVRational){1, movie->fps};

    return 0;

}

/*Special allocation of video buffer for v4l2m2m codec*/
static int movie_alloc_video_buffer(AVFrame *frame, int align)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    int ret, i, padded_height;
    int plane_padding = FFMAX(16 + 16/*STRIDE_ALIGN*/, align);

    if (!desc)
        return AVERROR(EINVAL);

    if ((ret = av_image_check_size(frame->width, frame->height, 0, NULL)) < 0)
        return ret;

    if (!frame->linesize[0]) {
        if (align <= 0)
            align = 32; /* STRIDE_ALIGN. Should be av_cpu_max_align() */

        for(i=1; i<=align; i+=i) {
            ret = av_image_fill_linesizes(frame->linesize, frame->format,
                                          FFALIGN(frame->width, i));
            if (ret < 0)
                return ret;
            if (!(frame->linesize[0] & (align-1)))
                break;
        }

        for (i = 0; i < 4 && frame->linesize[i]; i++)
            frame->linesize[i] = FFALIGN(frame->linesize[i], align);
    }

    padded_height = FFALIGN(frame->height, 32);
    if ((ret = av_image_fill_pointers(frame->data, frame->format, padded_height,
                                      NULL, frame->linesize)) < 0)
        return ret;

    frame->buf[0] = av_buffer_alloc(ret + 4*plane_padding);
    if (!frame->buf[0]) {
        ret = AVERROR(ENOMEM);
        av_frame_unref(frame);
        return ret;
    }
    frame->buf[1] = av_buffer_alloc(ret + 4*plane_padding);
    if (!frame->buf[1]) {
        ret = AVERROR(ENOMEM);
        av_frame_unref(frame);
        return ret;
    }

    frame->data[0] = frame->buf[0]->data;
    frame->data[1] = frame->buf[1]->data;
    frame->data[2] = frame->data[1] + ((frame->width * padded_height) / 4);

    frame->extended_data = frame->data;

    return 0;
}


static int movie_set_picture(struct ctx_movie *movie){

    int retcd;
    char errstr[128];

    movie->picture = my_frame_alloc();
    if (!movie->picture) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc frame"));
        movie_free_context(movie);
        return -1;
    }

    /* Take care of variable bitrate setting. */
    if (movie->quality)
        movie->picture->quality = movie->quality;

    movie->picture->linesize[0] = movie->ctx_codec->width;
    movie->picture->linesize[1] = movie->ctx_codec->width / 2;
    movie->picture->linesize[2] = movie->ctx_codec->width / 2;

    movie->picture->format = movie->ctx_codec->pix_fmt;
    movie->picture->width  = movie->ctx_codec->width;
    movie->picture->height = movie->ctx_codec->height;

    if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
        retcd = movie_alloc_video_buffer(movie->picture, 32);
        if (retcd) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc buffers %s"), errstr);
            movie_free_context(movie);
            return -1;
        }
    }

    return 0;

}

static int movie_set_outputfile(struct ctx_movie *movie){

    int retcd;
    char errstr[128];

    #if (LIBAVFORMAT_VERSION_MAJOR < 58)
        snprintf(movie->oc->filename, sizeof(movie->oc->filename), "%s", movie->filename);
    #endif

    /* Open the output file, if needed. */
    if ((movie_timelapse_exists(movie->filename) == 0) || (movie->tlapse != TIMELAPSE_APPEND)) {
        if (!(movie->oc->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&movie->oc->pb, movie->filename, MY_FLAG_WRITE) < 0) {
                if (errno == ENOENT) {
                    if (create_path(movie->filename) == -1) {
                        movie_free_context(movie);
                        return -1;
                    }
                    if (avio_open(&movie->oc->pb, movie->filename, MY_FLAG_WRITE) < 0) {
                        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                            ,_("error opening file %s"), movie->filename);
                        movie_free_context(movie);
                        return -1;
                    }
                    /* Permission denied */
                } else if (errno ==  EACCES) {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Permission denied. %s"),movie->filename);
                    movie_free_context(movie);
                    return -1;
                } else {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Error opening file %s"), movie->filename);
                    movie_free_context(movie);
                    return -1;
                }
            }
        }

        /* Write the stream header,  For the TIMELAPSE_APPEND
         * we write the data via standard file I/O so we close the
         * items here
         */
        retcd = avformat_write_header(movie->oc, NULL);
        if (retcd < 0){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Could not write movie header %s"),errstr);
            movie_free_context(movie);
            return -1;
        }
        if (movie->tlapse == TIMELAPSE_APPEND) {
            av_write_trailer(movie->oc);
            avio_close(movie->oc->pb);
        }

    }

    return 0;

}

static int movie_flush_codec(struct ctx_movie *movie){

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        //ffmpeg version 3.1 and after

        int retcd;
        int recv_cd = 0;
        char errstr[128];

        if (movie->passthrough){
            return 0;
        }

        retcd = 0;
        recv_cd = 0;
        if (movie->tlapse == TIMELAPSE_NONE) {
            retcd = avcodec_send_frame(movie->ctx_codec, NULL);
            if (retcd < 0 ){
                av_strerror(retcd, errstr, sizeof(errstr));
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    ,_("Error entering draining mode:%s"),errstr);
                return -1;
            }
            while (recv_cd != AVERROR_EOF){
                av_init_packet(&movie->pkt);
                movie->pkt.data = NULL;
                movie->pkt.size = 0;
                recv_cd = avcodec_receive_packet(movie->ctx_codec, &movie->pkt);
                if (recv_cd != AVERROR_EOF){
                    if (recv_cd < 0){
                        av_strerror(recv_cd, errstr, sizeof(errstr));
                        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                            ,_("Error draining codec:%s"),errstr);
                        my_packet_unref(movie->pkt);
                        return -1;
                    }
                    // v4l2_m2m encoder uses pts 0 and size 0 to indicate AVERROR_EOF
                    if ((movie->pkt.pts == 0) || (movie->pkt.size == 0)) {
                        recv_cd = AVERROR_EOF;
                        my_packet_unref(movie->pkt);
                        continue;
                    }
                    retcd = av_write_frame(movie->oc, &movie->pkt);
                    if (retcd < 0) {
                        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                            ,_("Error writing draining video frame"));
                        return -1;
                    }
                }
                my_packet_unref(movie->pkt);
            }
        }
        return 0;
    #else
        /* Dummy to kill warnings.  No draining in older ffmpeg versions */
        if (movie) {
            return 0;
        } else{
            return 0;
        }
    #endif

}

static int movie_put_frame(struct ctx_movie *movie, const struct timespec *ts1){
    int retcd;

    av_init_packet(&movie->pkt);
    movie->pkt.data = NULL;
    movie->pkt.size = 0;

    retcd = movie_set_pts(movie, ts1);
    if (retcd < 0) {
        //If there is an error, it has already been reported.
        my_packet_unref(movie->pkt);
        return 0;
    }

    retcd = movie_encode_video(movie);
    if (retcd != 0){
        if (retcd != -2){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while encoding picture"));
        }
        my_packet_unref(movie->pkt);
        return retcd;
    }

    if (movie->tlapse == TIMELAPSE_APPEND) {
        retcd = movie_timelapse_append(movie, movie->pkt);
    } else {
        retcd = av_write_frame(movie->oc, &movie->pkt);
    }
    my_packet_unref(movie->pkt);

    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while writing video frame"));
        return -1;
    }
    return retcd;

}

static void movie_passthru_reset(struct ctx_movie *movie){
    /* Reset the written flag at start of each event */
    int indx;

    pthread_mutex_lock(&movie->netcam_data->mutex_pktarray);
        for(indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            movie->netcam_data->pktarray[indx].iswritten = FALSE;
        }
    pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);

}

static void movie_passthru_write(struct ctx_movie *movie, int indx){
    /* Write the packet in the buffer at indx to file */
    char errstr[128];
    int retcd;

    av_init_packet(&movie->pkt);
    movie->pkt.data = NULL;
    movie->pkt.size = 0;


    movie->netcam_data->pktarray[indx].iswritten = TRUE;

    retcd = my_copy_packet(&movie->pkt, &movie->netcam_data->pktarray[indx].packet);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "av_copy_packet: %s",errstr);
        my_packet_unref(movie->pkt);
        return;
    }

    retcd = movie_set_pktpts(movie, &movie->netcam_data->pktarray[indx].timestamp_ts);
    if (retcd < 0) {
        my_packet_unref(movie->pkt);
        return;
    }

    movie->pkt.stream_index = 0;

    retcd = av_write_frame(movie->oc, &movie->pkt);
    my_packet_unref(movie->pkt);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error while writing video frame: %s"),errstr);
        return;
    }

}

static int movie_passthru_put(struct ctx_movie *movie, struct ctx_image_data *img_data){

    int idnbr_image, idnbr_lastwritten, idnbr_stop, idnbr_firstkey;
    int indx, indx_lastwritten, indx_firstkey;

    if (movie->netcam_data == NULL) return -1;

    if ((movie->netcam_data->status == NETCAM_NOTCONNECTED  ) ||
        (movie->netcam_data->status == NETCAM_RECONNECTING  ) ){
        return 0;
    }

    if (movie->high_resolution){
        idnbr_image = img_data->idnbr_high;
    } else {
        idnbr_image = img_data->idnbr_norm;
    }

    pthread_mutex_lock(&movie->netcam_data->mutex_pktarray);
        idnbr_lastwritten = 0;
        idnbr_firstkey = idnbr_image;
        idnbr_stop = 0;
        indx_lastwritten = -1;
        indx_firstkey = -1;

        for(indx = 0; indx < movie->netcam_data->pktarray_size; indx++) {
            if ((movie->netcam_data->pktarray[indx].iswritten) &&
                (movie->netcam_data->pktarray[indx].idnbr > idnbr_lastwritten)){
                idnbr_lastwritten=movie->netcam_data->pktarray[indx].idnbr;
                indx_lastwritten = indx;
            }
            if ((movie->netcam_data->pktarray[indx].idnbr >  idnbr_stop) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_image)){
                idnbr_stop=movie->netcam_data->pktarray[indx].idnbr;
            }
            if ((movie->netcam_data->pktarray[indx].iskey) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_firstkey)){
                    idnbr_firstkey=movie->netcam_data->pktarray[indx].idnbr;
                    indx_firstkey = indx;
            }
        }

        if (idnbr_stop == 0){
            pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);
            return 0;
        }

        if (indx_lastwritten != -1){
            indx = indx_lastwritten;
        } else if (indx_firstkey != -1) {
            indx = indx_firstkey;
        } else {
            indx = 0;
        }

        while (TRUE){
            if ((!movie->netcam_data->pktarray[indx].iswritten) &&
                (movie->netcam_data->pktarray[indx].packet.size > 0) &&
                (movie->netcam_data->pktarray[indx].idnbr >  idnbr_lastwritten) &&
                (movie->netcam_data->pktarray[indx].idnbr <= idnbr_image)) {
                movie_passthru_write(movie, indx);
            }
            if (movie->netcam_data->pktarray[indx].idnbr == idnbr_stop) break;
            indx++;
            if (indx == movie->netcam_data->pktarray_size ) indx = 0;
        }
    pthread_mutex_unlock(&movie->netcam_data->mutex_pktarray);
    return 0;
}

static int movie_passthru_codec(struct ctx_movie *movie){

    int retcd;
    AVStream    *stream_in;

    if (movie->netcam_data == NULL){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("RTSP context not available."));
        return -1;
    }

    pthread_mutex_lock(&movie->netcam_data->mutex_transfer);

        if ((movie->netcam_data->status == NETCAM_NOTCONNECTED  ) ||
            (movie->netcam_data->status == NETCAM_RECONNECTING  ) ){
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("rtsp camera not ready for pass-through."));
            pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
            return -1;
        }

        if (strcmp(movie->codec_name, "mp4") != 0){
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("pass-through mode enabled.  Changing to MP4 container."));
            movie->codec_name = "mp4";
        }

        retcd = movie_get_oformat(movie);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
            pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
            return -1;
        }

    #if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
            stream_in = movie->netcam_data->transfer_format->streams[0];
            movie->oc->oformat->video_codec = stream_in->codecpar->codec_id;

            movie->video_st = avformat_new_stream(movie->oc, NULL);
            if (!movie->video_st) {
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
                pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
                return -1;
            }

            retcd = avcodec_parameters_copy(movie->video_st->codecpar, stream_in->codecpar);
            if (retcd < 0){
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy codec parameters"));
                pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
                return -1;
            }
            movie->video_st->codecpar->codec_tag  = 0;

    #elif (LIBAVFORMAT_VERSION_MAJOR >= 55)

            stream_in = movie->netcam_data->transfer_format->streams[0];

            movie->video_st = avformat_new_stream(movie->oc, stream_in->codec->codec);
            if (!movie->video_st) {
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
                pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
                return -1;
            }

            retcd = avcodec_copy_context(movie->video_st->codec, stream_in->codec);
            if (retcd < 0){
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy codec parameters"));
                pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
                return -1;
            }
            movie->video_st->codec->flags     |= MY_CODEC_FLAG_GLOBAL_HEADER;
            movie->video_st->codec->codec_tag  = 0;
    #else
            /* This is disabled in the util_check_passthrough but we need it here for compiling */
            pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("Pass-through disabled.  ffmpeg too old"));
            return -1;
    #endif

        movie->video_st->time_base         = stream_in->time_base;
    pthread_mutex_unlock(&movie->netcam_data->mutex_transfer);
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "Pass-through stream opened");
    return 0;

}

void movie_avcodec_log(void *ignoreme, int errno_flag, const char *fmt, va_list vl){

    char buf[1024];
    char *end;

    (void)ignoreme;
    (void)errno_flag;

    /* Valgrind occasionally reports use of uninitialized values in here when we interrupt
     * some rtsp functions.  The offending value is either fmt or vl and seems to be from a
     * debug level of av functions.  To address it we flatten the message after we know
     * the log level.  Now we put the avcodec messages to INF level since their error
     * are not necessarily our errors.
     */
    if (errno_flag <= AV_LOG_WARNING){
        /* Flatten the message coming in from avcodec. */
        vsnprintf(buf, sizeof(buf), fmt, vl);
        end = buf + strlen(buf);
        if (end > buf && end[-1] == '\n')
        {
            *--end = 0;
        }

        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s", buf);
    }
}

static void movie_put_pix_nv21(struct ctx_movie *movie, struct ctx_image_data *img_data){
    unsigned char *image,*imagecr, *imagecb;
    int cr_len, x, y;

    if (movie->high_resolution){
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    cr_len = movie->ctx_codec->width * movie->ctx_codec->height / 4;
    imagecr = image + (movie->ctx_codec->width * movie->ctx_codec->height);
    imagecb = image + (movie->ctx_codec->width * movie->ctx_codec->height) + cr_len;

    memcpy(movie->picture->data[0], image, movie->ctx_codec->width * movie->ctx_codec->height);
    for (y = 0; y < movie->ctx_codec->height; y++) {
        for (x = 0; x < movie->ctx_codec->width/4; x++) {
            movie->picture->data[1][y*movie->ctx_codec->width/2 + x*2] = *imagecb;
            movie->picture->data[1][y*movie->ctx_codec->width/2 + x*2 + 1] = *imagecr;
            imagecb++;
            imagecr++;
        }
    }

}

static void movie_put_pix_yuv420(struct ctx_movie *movie, struct ctx_image_data *img_data){
    unsigned char *image;

    if (movie->high_resolution){
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    // Usual setup for image pointers
    movie->picture->data[0] = image;
    movie->picture->data[1] = image + (movie->ctx_codec->width * movie->ctx_codec->height);
    movie->picture->data[2] = movie->picture->data[1] + ((movie->ctx_codec->width * movie->ctx_codec->height) / 4);

}


/****************************************************************************
 ****************************************************************************
 ****************************************************************************/

void movie_global_init(void){

    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
        ,_("movie libavcodec version %d.%d.%d"
        " libavformat version %d.%d.%d")
        , LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO
        , LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);

    #if (LIBAVFORMAT_VERSION_MAJOR < 58)
        /* TODO: Determine if this is even needed for older versions */
        av_register_all();
        avcodec_register_all();
    #endif


    avformat_network_init();
    avdevice_register_all();
    av_log_set_callback((void *)movie_avcodec_log);

    #if (LIBAVFORMAT_VERSION_MAJOR < 58)
        /* TODO: Determine if this is even needed for older versions */
        int ret;
        ret = av_lockmgr_register(movie_lockmgr_cb);
        if (ret < 0)
        {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, _("av_lockmgr_register failed (%d)"), ret);
            exit(1);
        }
    #endif

}

void movie_global_deinit(void) {

    avformat_network_deinit();

    #if (LIBAVFORMAT_VERSION_MAJOR < 58)
        /* TODO Determine if this is even needed for old versions */
        if (av_lockmgr_register(NULL) < 0)
        {
            MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
                ,_("av_lockmgr_register reset failed on cleanup"));
        }
    #endif


}

int movie_open(struct ctx_movie *movie){

    int retcd;

    movie->oc = avformat_alloc_context();
    if (!movie->oc) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        movie_free_context(movie);
        return -1;
    }

    if (movie->passthrough) {
        retcd = movie_passthru_codec(movie);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not setup passthru!"));
            movie_free_context(movie);
            return -1;
        }

        movie_passthru_reset(movie);

    } else {
        retcd = movie_get_oformat(movie);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
            movie_free_context(movie);
            return -1;
        }

        retcd = movie_set_codec(movie);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate codec!"));
            return -1;
        }

        retcd = movie_set_stream(movie);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
            return -1;
        }

        retcd = movie_set_picture(movie);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
            return -1;
        }
    }



    retcd = movie_set_outputfile(movie);
    if (retcd < 0){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    return 0;

}

void movie_close(struct ctx_movie *movie){

    if (movie != NULL) {

        if (movie_flush_codec(movie) < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error flushing codec"));
        }
        if (movie->oc->pb != NULL){
            if (movie->tlapse != TIMELAPSE_APPEND) {
                av_write_trailer(movie->oc);
            }
            if (!(movie->oc->oformat->flags & AVFMT_NOFILE)) {
                if (movie->tlapse != TIMELAPSE_APPEND) {
                    avio_close(movie->oc->pb);
                }
            }
        }
        movie_free_context(movie);
        movie_free_nal(movie);
    }

}


int movie_put_image(struct ctx_movie *movie, struct ctx_image_data *img_data, const struct timespec *ts1){

    int retcd = 0;
    int cnt = 0;


    if (movie->passthrough) {
        retcd = movie_passthru_put(movie, img_data);
        return retcd;
    }

    if (movie->picture) {

        if (movie->preferred_codec == USER_CODEC_V4L2M2M) {
            movie_put_pix_nv21(movie, img_data);
        } else {
            movie_put_pix_yuv420(movie, img_data);
        }

        movie->gop_cnt ++;
        if (movie->gop_cnt == movie->ctx_codec->gop_size ){
            movie->picture->pict_type = AV_PICTURE_TYPE_I;
            movie->picture->key_frame = 1;
            movie->gop_cnt = 0;
        } else {
            movie->picture->pict_type = AV_PICTURE_TYPE_P;
            movie->picture->key_frame = 0;
        }

        /* A return code of -2 is thrown by the put_frame
         * when a image is buffered.  For timelapse, we absolutely
         * never want a frame buffered so we keep sending back the
         * the same pic until it flushes or fails in a different way
         */
        retcd = movie_put_frame(movie, ts1);
        while ((retcd == -2) && (movie->tlapse != TIMELAPSE_NONE)) {
            retcd = movie_put_frame(movie, ts1);
            cnt++;
            if (cnt > 50){
                MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                    ,_("Excessive attempts to clear buffered packet"));
                retcd = -1;
            }
        }
        //non timelapse buffered is ok
        if (retcd == -2){
            retcd = 0;
            MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO, _("Buffered packet"));
        }
    }

    return retcd;

}

void movie_reset_start_time(struct ctx_movie *movie, const struct timespec *ts1){
    int64_t one_frame_interval = av_rescale_q(1,(AVRational){1, movie->fps},movie->video_st->time_base);
    if (one_frame_interval <= 0)
        one_frame_interval = 1;
    movie->base_pts = movie->last_pts + one_frame_interval;

    movie->start_time.tv_sec = ts1->tv_sec;
    movie->start_time.tv_nsec = ts1->tv_nsec;

}

static const char* movie_init_codec(struct ctx_cam *cam){

    /* The following section allows for testing of all the various containers
    * that Motion permits. The container type is pre-pended to the name of the
    * file so that we can determine which container type created what movie.
    * The intent for this is be used for developer testing when the ffmpeg libs
    * change or the code inside our movie module changes.  For each event, the
    * container type will change.  This way, you can turn on emulate motion, then
    * specify a maximum movie time and let Motion run for days creating all the
    * different types of movies checking for crashes, warnings, etc.
    */
    const char *codec;
    int codenbr;

    codec = cam->conf.movie_codec;
    if (mystreq(codec, "ogg")) {
        MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO, "The ogg container is no longer supported.  Changing to mpeg4");
        codec = "mpeg4";
    }
    if (mystreq(codec, "test")) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, "Running test of the various output formats.");
        codenbr = cam->event_nr % 10;
        if (codenbr == 1)      codec = "mpeg4";
        else if (codenbr == 2) codec = "msmpeg4";
        else if (codenbr == 3) codec = "swf";
        else if (codenbr == 4) codec = "flv";
        else if (codenbr == 5) codec = "ffv1";
        else if (codenbr == 6) codec = "mov";
        else if (codenbr == 7) codec = "mp4";
        else if (codenbr == 8) codec = "mkv";
        else if (codenbr == 9) codec = "hevc";
        else                   codec = "msmpeg4";
    }

    return codec;

}

int movie_init_norm(struct ctx_cam *cam, struct timespec *ts1){
    char stamp[PATH_MAX];
    const char *moviepath;
    const char *codec;
    int retcd;

    if (cam->conf.movie_filename){
        moviepath = cam->conf.movie_filename;
    } else {
        moviepath = "%v-%Y%m%d%H%M%S";
    }
    mystrftime(cam, stamp, sizeof(stamp), moviepath, ts1, NULL, 0);

    codec = movie_init_codec(cam);

    cam->movie_norm =(struct ctx_movie*) mymalloc(sizeof(struct ctx_movie));
    if (mystreq(codec, "test")) {
        snprintf(cam->movie_norm->filename, PATH_MAX - 4, "%.*s/%s_%.*s"
            , (int)(PATH_MAX-6-strlen(stamp)-strlen(codec))
            , cam->conf.target_dir, codec
            , (int)(PATH_MAX-6-strlen(cam->conf.target_dir)-strlen(codec))
            , stamp);
    } else {
        snprintf(cam->movie_norm->filename, PATH_MAX - 4, "%.*s/%.*s"
            , (int)(PATH_MAX-5-strlen(stamp))
            , cam->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cam->conf.target_dir))
            , stamp);
    }
    if (cam->imgs.size_high > 0){
        cam->movie_norm->width  = cam->imgs.width_high;
        cam->movie_norm->height = cam->imgs.height_high;
        cam->movie_norm->high_resolution = TRUE;
        cam->movie_norm->netcam_data = cam->netcam_high;
    } else {
        cam->movie_norm->width  = cam->imgs.width;
        cam->movie_norm->height = cam->imgs.height;
        cam->movie_norm->high_resolution = FALSE;
        cam->movie_norm->netcam_data = cam->netcam;
    }
    cam->movie_norm->tlapse = TIMELAPSE_NONE;
    cam->movie_norm->fps = cam->lastrate;
    cam->movie_norm->bps = cam->conf.movie_bps;
    cam->movie_norm->quality = cam->conf.movie_quality;
    cam->movie_norm->start_time.tv_sec = ts1->tv_sec;
    cam->movie_norm->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_norm->last_pts = -1;
    cam->movie_norm->base_pts = 0;
    cam->movie_norm->gop_cnt = 0;
    cam->movie_norm->codec_name = codec;
    if (mystreq(cam->conf.movie_codec, "test")) {
        cam->movie_norm->test_mode = TRUE;
    } else {
        cam->movie_norm->test_mode = FALSE;
    }
    cam->movie_norm->motion_images = 0;
    cam->movie_norm->passthrough = cam->movie_passthrough;

    retcd = movie_open(cam->movie_norm);

    return retcd;

}

int movie_init_motion(struct ctx_cam *cam, struct timespec *ts1){
    char stamp[PATH_MAX];
    const char *moviepath;
    const char *codec;
    int retcd;

    if (cam->conf.movie_filename){
        moviepath = cam->conf.movie_filename;
    } else {
        moviepath = "%v-%Y%m%d%H%M%S";
    }
    mystrftime(cam, stamp, sizeof(stamp), moviepath, ts1, NULL, 0);

    codec = movie_init_codec(cam);

    cam->movie_motion =(struct ctx_movie*)mymalloc(sizeof(struct ctx_movie));
    if (mystreq(codec, "test")) {
        snprintf(cam->movie_motion->filename, PATH_MAX - 4, "%.*s/%s_%.*sm"
            , (int)(PATH_MAX-6-strlen(stamp)-strlen(codec))
            , cam->conf.target_dir, codec
            , (int)(PATH_MAX-6-strlen(cam->conf.target_dir)-strlen(codec))
            , stamp);
    } else {
        snprintf(cam->movie_motion->filename, PATH_MAX - 4, "%.*s/%.*sm"
            , (int)(PATH_MAX-5-strlen(stamp))
            , cam->conf.target_dir
            , (int)(PATH_MAX-5-strlen(cam->conf.target_dir))
            , stamp);
    }

    cam->movie_motion->width  = cam->imgs.width;
    cam->movie_motion->height = cam->imgs.height;
    cam->movie_motion->netcam_data = NULL;
    cam->movie_motion->tlapse = TIMELAPSE_NONE;
    cam->movie_motion->fps = cam->lastrate;
    cam->movie_motion->bps = cam->conf.movie_bps;
    cam->movie_motion->quality = cam->conf.movie_quality;
    cam->movie_motion->start_time.tv_sec = ts1->tv_sec;
    cam->movie_motion->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_motion->last_pts = -1;
    cam->movie_motion->base_pts = 0;
    cam->movie_motion->gop_cnt = 0;
    cam->movie_motion->codec_name = codec;
    if (mystreq(cam->conf.movie_codec, "test")) {
        cam->movie_motion->test_mode = TRUE;
    } else {
        cam->movie_motion->test_mode = FALSE;
    }
    cam->movie_motion->motion_images = TRUE;
    cam->movie_motion->passthrough = FALSE;
    cam->movie_motion->high_resolution = FALSE;
    cam->movie_motion->netcam_data = NULL;

    retcd = movie_open(cam->movie_motion);

    return retcd;

}

int movie_init_timelapse(struct ctx_cam *cam, struct timespec *ts1){

    char tmp[PATH_MAX];
    const char *timepath;
    const char *codec_mpg = "mpg";
    const char *codec_mpeg = "mpeg4";
    int retcd;

    cam->movie_timelapse =(struct ctx_movie*)mymalloc(sizeof(struct ctx_movie));

    if (cam->conf.timelapse_filename){
        timepath = cam->conf.timelapse_filename;
    } else {
        timepath = "%Y%m%d-timelapse";
    }
    mystrftime(cam, tmp, sizeof(tmp), timepath, ts1, NULL, 0);

    snprintf(cam->movie_timelapse->filename, PATH_MAX - 4, "%.*s/%.*s"
        , (int)(PATH_MAX-5-strlen(tmp))
        , cam->conf.target_dir
        , (int)(PATH_MAX-5-strlen(cam->conf.target_dir))
        , tmp);
    if ((cam->imgs.size_high > 0) && (!cam->movie_passthrough)){
        cam->movie_timelapse->width  = cam->imgs.width_high;
        cam->movie_timelapse->height = cam->imgs.height_high;
        cam->movie_timelapse->high_resolution = TRUE;
    } else {
        cam->movie_timelapse->width  = cam->imgs.width;
        cam->movie_timelapse->height = cam->imgs.height;
        cam->movie_timelapse->high_resolution = FALSE;
    }
    cam->movie_timelapse->fps = cam->conf.timelapse_fps;
    cam->movie_timelapse->bps = cam->conf.movie_bps;
    cam->movie_timelapse->quality = cam->conf.movie_quality;
    cam->movie_timelapse->start_time.tv_sec = ts1->tv_sec;
    cam->movie_timelapse->start_time.tv_nsec = ts1->tv_nsec;
    cam->movie_timelapse->last_pts = -1;
    cam->movie_timelapse->base_pts = 0;
    cam->movie_timelapse->test_mode = FALSE;
    cam->movie_timelapse->gop_cnt = 0;
    cam->movie_timelapse->motion_images = FALSE;
    cam->movie_timelapse->passthrough = FALSE;
    cam->movie_timelapse->netcam_data = NULL;

    if (mystreq(cam->conf.timelapse_codec,"mpg") ||
        mystreq(cam->conf.timelapse_codec,"swf") ){

        if (mystreq(cam->conf.timelapse_codec,"swf")) {
            MOTION_LOG(WRN, TYPE_EVENTS, NO_ERRNO
                ,_("The swf container for timelapse no longer supported.  Using mpg container."));
        }

        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpg codec."));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be appended to file"));

        cam->movie_timelapse->tlapse = TIMELAPSE_APPEND;
        cam->movie_timelapse->codec_name = codec_mpg;
        retcd = movie_open(cam->movie_timelapse);
    } else {
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Timelapse using mpeg4 codec."));
        MOTION_LOG(NTC, TYPE_EVENTS, NO_ERRNO, _("Events will be trigger new files"));

        cam->movie_timelapse->tlapse = TIMELAPSE_NEW;
        cam->movie_timelapse->codec_name = codec_mpeg;
        retcd = movie_open(cam->movie_timelapse);
    }

    return retcd;

}
