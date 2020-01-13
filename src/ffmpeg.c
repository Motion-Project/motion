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

#include "translate.h"
#include "motion.h"

#ifdef HAVE_FFMPEG

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
static void ffmpeg_free_nal(struct ffmpeg *ffmpeg){
    if (ffmpeg->nal_info) {
        free(ffmpeg->nal_info);
        ffmpeg->nal_info = NULL;
        ffmpeg->nal_info_len = 0;
    }
}

static int ffmpeg_encode_nal(struct ffmpeg *ffmpeg){
    // h264_v4l2m2m has NAL units separated from the first frame, which makes
    // some players very unhappy.
    if ((ffmpeg->pkt.pts == 0) && (!(ffmpeg->pkt.flags & AV_PKT_FLAG_KEY))) {
        ffmpeg_free_nal(ffmpeg);
        ffmpeg->nal_info_len = ffmpeg->pkt.size;
        ffmpeg->nal_info = malloc(ffmpeg->nal_info_len);
        if (ffmpeg->nal_info) {
            memcpy(ffmpeg->nal_info, &ffmpeg->pkt.data[0], ffmpeg->nal_info_len);
            return 1;
        } else
            ffmpeg->nal_info_len = 0;
    } else if (ffmpeg->nal_info) {
        int old_size = ffmpeg->pkt.size;
        av_grow_packet(&ffmpeg->pkt, ffmpeg->nal_info_len);
        memmove(&ffmpeg->pkt.data[ffmpeg->nal_info_len], &ffmpeg->pkt.data[0], old_size);
        memcpy(&ffmpeg->pkt.data[0], ffmpeg->nal_info, ffmpeg->nal_info_len);
        ffmpeg_free_nal(ffmpeg);
    }
    return 0;
}

static int ffmpeg_timelapse_exists(const char *fname){
    FILE *file;
    file = fopen(fname, "r");
    if (file)
    {
        fclose(file);
        return 1;
    }
    return 0;
}

static int ffmpeg_timelapse_append(struct ffmpeg *ffmpeg, AVPacket pkt){
    FILE *file;

    file = fopen(ffmpeg->filename, "a");
    if (!file) return -1;

    fwrite(pkt.data,1,pkt.size,file);

    fclose(file);

    return 0;
}

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
/* TODO Determine if this is even needed for old versions. Per
 * documentation for version 58, 'av_lockmgr_register This function does nothing'
 */
static int ffmpeg_lockmgr_cb(void **arg, enum AVLockOp op){
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

static void ffmpeg_free_context(struct ffmpeg *ffmpeg){

        if (ffmpeg->picture != NULL){
            my_frame_free(ffmpeg->picture);
            ffmpeg->picture = NULL;
        }

        if (ffmpeg->ctx_codec != NULL){
            my_avcodec_close(ffmpeg->ctx_codec);
            ffmpeg->ctx_codec = NULL;
        }

        if (ffmpeg->oc != NULL){
            avformat_free_context(ffmpeg->oc);
            ffmpeg->oc = NULL;
        }

}

static int ffmpeg_get_oformat(struct ffmpeg *ffmpeg){

    size_t codec_name_len = strcspn(ffmpeg->codec_name, ":");
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
        ffmpeg_free_context(ffmpeg);
        return -1;
    }
    memcpy(codec_name, ffmpeg->codec_name, codec_name_len);
    codec_name[codec_name_len] = 0;

    /* Only the newer codec and containers can handle the really fast FPS */
    if (((strcmp(codec_name, "msmpeg4") == 0) ||
        (strcmp(codec_name, "mpeg4") == 0) ||
        (strcmp(codec_name, "swf") == 0) ) && (ffmpeg->fps >50)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("The frame rate specified is too high for the ffmpeg movie type specified. "
            "Choose a different ffmpeg container or lower framerate."));
        ffmpeg_free_context(ffmpeg);
        free(codec_name);
        return -1;
    }

    retcd = snprintf(basename,PATH_MAX,"%s",ffmpeg->filename);
    if ((retcd < 0) || (retcd >= PATH_MAX)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting base file name"));
        ffmpeg_free_context(ffmpeg);
        free(codec_name);
        return -1;
    }

    if (ffmpeg->tlapse == TIMELAPSE_APPEND){
        ffmpeg->oc->oformat = av_guess_format ("mpeg2video", NULL, NULL);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_MPEG2VIDEO;
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.mpg",basename);
        if ((!ffmpeg->oc->oformat) ||
            (retcd < 0) || (retcd >= PATH_MAX)){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error setting timelapse append for codec %s"), codec_name);
            ffmpeg_free_context(ffmpeg);
            free(codec_name);
            return -1;
        }
        free(codec_name);
        return 0;
    }

    if (strcmp(codec_name, "mpeg4") == 0) {
        ffmpeg->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.avi",basename);
    }

    if (strcmp(codec_name, "msmpeg4") == 0) {
        ffmpeg->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.avi",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_MSMPEG4V2;
    }

    if (strcmp(codec_name, "swf") == 0) {
        ffmpeg->oc->oformat = av_guess_format("swf", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.swf",basename);
    }

    if (strcmp(codec_name, "flv") == 0) {
        ffmpeg->oc->oformat = av_guess_format("flv", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.flv",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_FLV1;
    }

    if (strcmp(codec_name, "ffv1") == 0) {
        ffmpeg->oc->oformat = av_guess_format("avi", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.avi",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_FFV1;
    }

    if (strcmp(codec_name, "mov") == 0) {
        ffmpeg->oc->oformat = av_guess_format("mov", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.mov",basename);
    }

    if (strcmp(codec_name, "mp4") == 0) {
        ffmpeg->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.mp4",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_H264;
    }

    if (strcmp(codec_name, "mkv") == 0) {
        ffmpeg->oc->oformat = av_guess_format("matroska", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.mkv",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_H264;
    }

    if (strcmp(codec_name, "hevc") == 0) {
        ffmpeg->oc->oformat = av_guess_format("mp4", NULL, NULL);
        retcd = snprintf(ffmpeg->filename,PATH_MAX,"%s.mp4",basename);
        if (ffmpeg->oc->oformat) ffmpeg->oc->oformat->video_codec = MY_CODEC_ID_HEVC;
    }

    //Check for valid results
    if ((retcd < 0) || (retcd >= PATH_MAX)){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error setting file name"));
        ffmpeg_free_context(ffmpeg);
        free(codec_name);
        return -1;
    }

    if (!ffmpeg->oc->oformat) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("codec option value %s is not supported"), codec_name);
        ffmpeg_free_context(ffmpeg);
        free(codec_name);
        return -1;
    }

    if (ffmpeg->oc->oformat->video_codec == MY_CODEC_ID_NONE) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get the codec"));
        ffmpeg_free_context(ffmpeg);
        free(codec_name);
        return -1;
    }

    free(codec_name);
    return 0;
}

static int ffmpeg_encode_video(struct ffmpeg *ffmpeg){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    //ffmpeg version 3.1 and after
    int retcd = 0;
    char errstr[128];

    retcd = avcodec_send_frame(ffmpeg->ctx_codec, ffmpeg->picture);
    if (retcd < 0 ){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error sending frame for encoding:%s"),errstr);
        return -1;
    }
    retcd = avcodec_receive_packet(ffmpeg->ctx_codec, &ffmpeg->pkt);
    if (retcd == AVERROR(EAGAIN)){
        //Buffered packet.  Throw special return code
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO
            ,_("Receive packet threw EAGAIN returning -2 code :%s"),errstr);
        my_packet_unref(ffmpeg->pkt);
        return -2;
    }
    if (retcd < 0 ){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error receiving encoded packet video:%s"),errstr);
        //Packet is freed upon failure of encoding
        return -1;
    }

    if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M){
        if (ffmpeg_encode_nal(ffmpeg)) {
            // Throw special return code
            return -2;
        }
    }

    return 0;

#elif (LIBAVFORMAT_VERSION_MAJOR >= 55) || ((LIBAVFORMAT_VERSION_MAJOR == 54) && (LIBAVFORMAT_VERSION_MINOR > 6))

    int retcd = 0;
    char errstr[128];
    int got_packet_ptr;

    retcd = avcodec_encode_video2(ffmpeg->ctx_codec, &ffmpeg->pkt, ffmpeg->picture, &got_packet_ptr);
    if (retcd < 0 ){
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video:%s"),errstr);
        //Packet is freed upon failure of encoding
        return -1;
    }
    if (got_packet_ptr == 0){
        //Buffered packet.  Throw special return code
        my_packet_unref(ffmpeg->pkt);
        return -2;
    }

    /* This kills compiler warnings.  Nal setting is only for recent ffmpeg versions*/
    if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M){
        if (ffmpeg_encode_nal(ffmpeg)) {
            // Throw special return code
            return -2;
        }
    }

    return 0;

#else

    int retcd = 0;
    uint8_t *video_outbuf;
    int video_outbuf_size;

    video_outbuf_size = (ffmpeg->ctx_codec->width +16) * (ffmpeg->ctx_codec->height +16) * 1;
    video_outbuf = mymalloc(video_outbuf_size);

    retcd = avcodec_encode_video(ffmpeg->video_st->codec, video_outbuf, video_outbuf_size, ffmpeg->picture);
    if (retcd < 0 ){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error encoding video"));
        my_packet_unref(ffmpeg->pkt);
        return -1;
    }
    if (retcd == 0 ){
        // No bytes encoded => buffered=>special handling
        my_packet_unref(ffmpeg->pkt);
        return -2;
    }

    // Encoder did not provide metadata, set it up manually
    ffmpeg->pkt.size = retcd;
    ffmpeg->pkt.data = video_outbuf;

    if (ffmpeg->picture->key_frame == 1)
      ffmpeg->pkt.flags |= AV_PKT_FLAG_KEY;

    ffmpeg->pkt.pts = ffmpeg->picture->pts;
    ffmpeg->pkt.dts = ffmpeg->pkt.pts;

    free(video_outbuf);

    /* This kills compiler warnings.  Nal setting is only for recent ffmpeg versions*/
    if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M){
        if (ffmpeg_encode_nal(ffmpeg)) {
            // Throw special return code
            return -2;
        }
    }

    return 0;

#endif

}

static int ffmpeg_set_pts(struct ffmpeg *ffmpeg, const struct timeval *tv1){

    int64_t pts_interval;

    if (ffmpeg->tlapse != TIMELAPSE_NONE) {
        ffmpeg->last_pts++;
        ffmpeg->picture->pts = ffmpeg->last_pts;
    } else {
        pts_interval = ((1000000L * (tv1->tv_sec - ffmpeg->start_time.tv_sec)) + tv1->tv_usec - ffmpeg->start_time.tv_usec);
        if (pts_interval < 0){
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            ffmpeg_reset_movie_start_time(ffmpeg, tv1);
            pts_interval = 0;
        }
        if (ffmpeg->last_pts < 0) {
            // This is the very first frame, ensure PTS is zero
            ffmpeg->picture->pts = 0;
        } else
            ffmpeg->picture->pts = av_rescale_q(pts_interval,(AVRational){1, 1000000L},ffmpeg->video_st->time_base) + ffmpeg->base_pts;

        if (ffmpeg->test_mode == TRUE){
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                ,_("PTS %"PRId64" Base PTS %"PRId64" ms interval %"PRId64" timebase %d-%d")
                ,ffmpeg->picture->pts,ffmpeg->base_pts,pts_interval
                ,ffmpeg->video_st->time_base.num,ffmpeg->video_st->time_base.den);
        }

        if (ffmpeg->picture->pts <= ffmpeg->last_pts){
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (ffmpeg->test_mode == TRUE){
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        ffmpeg->last_pts = ffmpeg->picture->pts;
    }
    return 0;
}

static int ffmpeg_set_pktpts(struct ffmpeg *ffmpeg, const struct timeval *tv1){

    int64_t pts_interval;

    if (ffmpeg->tlapse != TIMELAPSE_NONE) {
        ffmpeg->last_pts++;
        ffmpeg->pkt.pts = ffmpeg->last_pts;
    } else {
        pts_interval = ((1000000L * (tv1->tv_sec - ffmpeg->start_time.tv_sec)) + tv1->tv_usec - ffmpeg->start_time.tv_usec);
        if (pts_interval < 0){
            /* This can occur when we have pre-capture frames.  Reset start time of video. */
            ffmpeg_reset_movie_start_time(ffmpeg, tv1);
            pts_interval = 0;
        }
        ffmpeg->pkt.pts = av_rescale_q(pts_interval,(AVRational){1, 1000000L},ffmpeg->video_st->time_base) + ffmpeg->base_pts;

        if (ffmpeg->test_mode == TRUE){
            MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                       ,_("PTS %"PRId64" Base PTS %"PRId64" ms interval %"PRId64" timebase %d-%d Change %d")
                       ,ffmpeg->pkt.pts
                       ,ffmpeg->base_pts,pts_interval
                       ,ffmpeg->video_st->time_base.num
                       ,ffmpeg->video_st->time_base.den
                       ,(ffmpeg->pkt.pts-ffmpeg->last_pts) );
        }

        if (ffmpeg->pkt.pts <= ffmpeg->last_pts){
            //We have a problem with our motion loop timing and sending frames or the rounding into the PTS.
            if (ffmpeg->test_mode == TRUE){
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("BAD TIMING!! Frame skipped."));
            }
            return -1;
        }
        ffmpeg->last_pts = ffmpeg->pkt.pts;
        ffmpeg->pkt.dts=ffmpeg->pkt.pts;
    }
    return 0;
}

static int ffmpeg_set_quality(struct ffmpeg *ffmpeg){

    ffmpeg->opts = 0;
    if (ffmpeg->quality > 100) ffmpeg->quality = 100;
    if (ffmpeg->ctx_codec->codec_id == MY_CODEC_ID_H264 ||
        ffmpeg->ctx_codec->codec_id == MY_CODEC_ID_HEVC){
        if (ffmpeg->quality <= 0)
            ffmpeg->quality = 45; // default to 45% quality
        av_dict_set(&ffmpeg->opts, "preset", "ultrafast", 0);
        av_dict_set(&ffmpeg->opts, "tune", "zerolatency", 0);
        /* This next if statement needs validation.  Are mpeg4omx
         * and v4l2m2m even MY_CODEC_ID_H264 or MY_CODEC_ID_HEVC
         * such that it even would be possible to be part of this
         * if block to start with? */
        if ((ffmpeg->preferred_codec == USER_CODEC_H264OMX) ||
            (ffmpeg->preferred_codec == USER_CODEC_MPEG4OMX) ||
            (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M)) {
            // bit_rate = ffmpeg->width * ffmpeg->height * ffmpeg->fps * quality_factor
            ffmpeg->quality = (int)(((int64_t)ffmpeg->width * ffmpeg->height * ffmpeg->fps * ffmpeg->quality) >> 7);
            // Clip bit rate to min
            if (ffmpeg->quality < 4000) // magic number
                ffmpeg->quality = 4000;
            ffmpeg->ctx_codec->profile = FF_PROFILE_H264_HIGH;
            ffmpeg->ctx_codec->bit_rate = ffmpeg->quality;
        } else {
            // Control other H264 encoders quality via CRF
            char crf[10];
            ffmpeg->quality = (int)(( (100-ffmpeg->quality) * 51)/100);
            snprintf(crf, 10, "%d", ffmpeg->quality);
            av_dict_set(&ffmpeg->opts, "crf", crf, 0);
        }
    } else {
        /* The selection of 8000 is a subjective number based upon viewing output files */
        if (ffmpeg->quality > 0){
            ffmpeg->quality =(int)(((100-ffmpeg->quality)*(100-ffmpeg->quality)*(100-ffmpeg->quality) * 8000) / 1000000) + 1;
            ffmpeg->ctx_codec->flags |= MY_CODEC_FLAG_QSCALE;
            ffmpeg->ctx_codec->global_quality=ffmpeg->quality;
        }
    }
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
        ,_("%s codec vbr/crf/bit_rate: %d"), ffmpeg->codec->name, ffmpeg->quality);

    return 0;
}

struct blacklist_t
{
    const char *codec_name;
    const char *reason;
};

static const char *ffmpeg_codec_is_blacklisted(const char *codec_name){

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

static int ffmpeg_set_codec_preferred(struct ffmpeg *ffmpeg){
    size_t codec_name_len = strcspn(ffmpeg->codec_name, ":");

    ffmpeg->codec = NULL;
    if (ffmpeg->codec_name[codec_name_len]) {
        const char *blacklist_reason = ffmpeg_codec_is_blacklisted(&ffmpeg->codec_name[codec_name_len+1]);
        if (blacklist_reason) {
            MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                ,_("Preferred codec %s has been blacklisted: %s")
                ,&ffmpeg->codec_name[codec_name_len+1], blacklist_reason);
        } else {
            ffmpeg->codec = avcodec_find_encoder_by_name(&ffmpeg->codec_name[codec_name_len+1]);
            if ((ffmpeg->oc->oformat) && (ffmpeg->codec != NULL)) {
                    ffmpeg->oc->oformat->video_codec = ffmpeg->codec->id;
            } else if (ffmpeg->codec == NULL) {
                MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO
                    ,_("Preferred codec %s not found")
                    ,&ffmpeg->codec_name[codec_name_len+1]);
            }
        }
    }
    if (!ffmpeg->codec)
        ffmpeg->codec = avcodec_find_encoder(ffmpeg->oc->oformat->video_codec);
    if (!ffmpeg->codec) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Codec %s not found"), ffmpeg->codec_name);
        ffmpeg_free_context(ffmpeg);
        return -1;
    }

    if (strcmp(ffmpeg->codec->name, "h264_v4l2m2m") == 0){
        ffmpeg->preferred_codec = USER_CODEC_V4L2M2M;
    } else if (strcmp(ffmpeg->codec->name, "h264_omx") == 0){
        ffmpeg->preferred_codec = USER_CODEC_H264OMX;
    } else if (strcmp(ffmpeg->codec->name, "mpeg4_omx") == 0){
        ffmpeg->preferred_codec = USER_CODEC_MPEG4OMX;
    } else {
        ffmpeg->preferred_codec = USER_CODEC_DEFAULT;
    }

    if (ffmpeg->codec_name[codec_name_len])
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO,_("Using codec %s"), ffmpeg->codec->name);

    return 0;

}

static int ffmpeg_set_codec(struct ffmpeg *ffmpeg){

    int retcd;
    char errstr[128];
    int chkrate;

    retcd = ffmpeg_set_codec_preferred(ffmpeg);
    if (retcd != 0) return retcd;

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    //If we provide the codec to this, it results in a memory leak.  ffmpeg ticket: 5714
    ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, NULL);
    if (!ffmpeg->video_st) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
        ffmpeg_free_context(ffmpeg);
        return -1;
    }
    ffmpeg->ctx_codec = avcodec_alloc_context3(ffmpeg->codec);
    if (ffmpeg->ctx_codec == NULL) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate decoder!"));
        ffmpeg_free_context(ffmpeg);
        return -1;
    }
#else
    ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, ffmpeg->codec);
    if (!ffmpeg->video_st) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
        ffmpeg_free_context(ffmpeg);
        return -1;
    }
    ffmpeg->ctx_codec = ffmpeg->video_st->codec;
#endif


    if (ffmpeg->tlapse != TIMELAPSE_NONE) {
        ffmpeg->ctx_codec->gop_size = 1;
    } else {
        if (ffmpeg->fps <= 5){
            ffmpeg->ctx_codec->gop_size = 1;
        } else if (ffmpeg->fps > 30){
            ffmpeg->ctx_codec->gop_size = 15;
        } else {
            ffmpeg->ctx_codec->gop_size = (ffmpeg->fps / 2);
        }
        ffmpeg->gop_cnt = ffmpeg->ctx_codec->gop_size - 1;
    }

    /*  For certain containers, setting the fps to very low numbers results in
    **  a very poor quality playback.  We can set the FPS to a higher number and
    **  then let the PTS display the frames correctly.
    */
    if ((ffmpeg->tlapse == TIMELAPSE_NONE) && (ffmpeg->fps <= 5)){
        if ((strcmp(ffmpeg->codec_name, "msmpeg4") == 0) ||
            (strcmp(ffmpeg->codec_name, "flv")     == 0) ||
            (strcmp(ffmpeg->codec_name, "mov") == 0) ||
            (strcmp(ffmpeg->codec_name, "mp4") == 0) ||
            (strcmp(ffmpeg->codec_name, "hevc") == 0) ||
            (strcmp(ffmpeg->codec_name, "mpeg4")   == 0)) {
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("Low fps. Encoding %d frames into a %d frames container."), ffmpeg->fps, 10);
            ffmpeg->fps = 10;
        }
    }

    ffmpeg->ctx_codec->codec_id      = ffmpeg->oc->oformat->video_codec;
    ffmpeg->ctx_codec->codec_type    = AVMEDIA_TYPE_VIDEO;
    ffmpeg->ctx_codec->bit_rate      = ffmpeg->bps;
    ffmpeg->ctx_codec->width         = ffmpeg->width;
    ffmpeg->ctx_codec->height        = ffmpeg->height;
    ffmpeg->ctx_codec->time_base.num = 1;
    ffmpeg->ctx_codec->time_base.den = ffmpeg->fps;
    if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M){
        ffmpeg->ctx_codec->pix_fmt   = AV_PIX_FMT_NV21;
    } else {
        ffmpeg->ctx_codec->pix_fmt   = MY_PIX_FMT_YUV420P;
    }
    ffmpeg->ctx_codec->max_b_frames  = 0;
    if (strcmp(ffmpeg->codec_name, "ffv1") == 0){
      ffmpeg->ctx_codec->strict_std_compliance = -2;
      ffmpeg->ctx_codec->level = 3;
    }
    ffmpeg->ctx_codec->flags |= MY_CODEC_FLAG_GLOBAL_HEADER;

    if ((strcmp(ffmpeg->codec->name, "h264_omx") == 0) ||
        (strcmp(ffmpeg->codec->name, "mpeg4_omx") == 0)) {
        /* h264_omx & ffmpeg combination locks up on Raspberry Pi.
         * To use h264_omx encoder, we need to disable zerocopy.
         * More information: https://github.com/Motion-Project/motion/issues/433
         */
        av_dict_set(&ffmpeg->opts, "zerocopy", "0", 0);
    }

    retcd = ffmpeg_set_quality(ffmpeg);
    if (retcd < 0){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to set quality"));
        return -1;
    }

    retcd = avcodec_open2(ffmpeg->ctx_codec, ffmpeg->codec, &ffmpeg->opts);
    if (retcd < 0) {
        if (ffmpeg->codec->supported_framerates) {
            const AVRational *fps = ffmpeg->codec->supported_framerates;
            while (fps->num) {
                MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO
                    ,_("Reported FPS Supported %d/%d"), fps->num, fps->den);
                fps++;
            }
        }
        chkrate = 1;
        while ((chkrate < 36) && (retcd != 0)) {
            ffmpeg->ctx_codec->time_base.den = chkrate;
            retcd = avcodec_open2(ffmpeg->ctx_codec, ffmpeg->codec, &ffmpeg->opts);
            chkrate++;
        }
        if (retcd < 0){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not open codec %s"),errstr);
            av_dict_free(&ffmpeg->opts);
            ffmpeg_free_context(ffmpeg);
            return -1;
        }

    }
    av_dict_free(&ffmpeg->opts);

    return 0;
}

static int ffmpeg_set_stream(struct ffmpeg *ffmpeg){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    int retcd;
    char errstr[128];

    retcd = avcodec_parameters_from_context(ffmpeg->video_st->codecpar,ffmpeg->ctx_codec);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Failed to copy decoder parameters!: %s"), errstr);
        ffmpeg_free_context(ffmpeg);
        return -1;
    }
#endif

    ffmpeg->video_st->time_base = (AVRational){1, ffmpeg->fps};

    return 0;

}

/*Special allocation of video buffer for v4l2m2m codec*/
static int ffmpeg_alloc_video_buffer(AVFrame *frame, int align)
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


static int ffmpeg_set_picture(struct ffmpeg *ffmpeg){

    int retcd;
    char errstr[128];

    ffmpeg->picture = my_frame_alloc();
    if (!ffmpeg->picture) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc frame"));
        ffmpeg_free_context(ffmpeg);
        return -1;
    }

    /* Take care of variable bitrate setting. */
    if (ffmpeg->quality)
        ffmpeg->picture->quality = ffmpeg->quality;

    ffmpeg->picture->linesize[0] = ffmpeg->ctx_codec->width;
    ffmpeg->picture->linesize[1] = ffmpeg->ctx_codec->width / 2;
    ffmpeg->picture->linesize[2] = ffmpeg->ctx_codec->width / 2;

    ffmpeg->picture->format = ffmpeg->ctx_codec->pix_fmt;
    ffmpeg->picture->width  = ffmpeg->ctx_codec->width;
    ffmpeg->picture->height = ffmpeg->ctx_codec->height;

    if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M) {
        retcd = ffmpeg_alloc_video_buffer(ffmpeg->picture, 32);
        if (retcd) {
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("could not alloc buffers %s"), errstr);
            ffmpeg_free_context(ffmpeg);
            return -1;
        }
    }

    return 0;

}

static int ffmpeg_set_outputfile(struct ffmpeg *ffmpeg){

    int retcd;
    char errstr[128];

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
    snprintf(ffmpeg->oc->filename, sizeof(ffmpeg->oc->filename), "%s", ffmpeg->filename);
#endif

    /* Open the output file, if needed. */
    if ((ffmpeg_timelapse_exists(ffmpeg->filename) == 0) || (ffmpeg->tlapse != TIMELAPSE_APPEND)) {
        if (!(ffmpeg->oc->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&ffmpeg->oc->pb, ffmpeg->filename, MY_FLAG_WRITE) < 0) {
                if (errno == ENOENT) {
                    if (create_path(ffmpeg->filename) == -1) {
                        ffmpeg_free_context(ffmpeg);
                        return -1;
                    }
                    if (avio_open(&ffmpeg->oc->pb, ffmpeg->filename, MY_FLAG_WRITE) < 0) {
                        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                            ,_("error opening file %s"), ffmpeg->filename);
                        ffmpeg_free_context(ffmpeg);
                        return -1;
                    }
                    /* Permission denied */
                } else if (errno ==  EACCES) {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Permission denied. %s"),ffmpeg->filename);
                    ffmpeg_free_context(ffmpeg);
                    return -1;
                } else {
                    MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO
                        ,_("Error opening file %s"), ffmpeg->filename);
                    ffmpeg_free_context(ffmpeg);
                    return -1;
                }
            }
        }

        /* Write the stream header,  For the TIMELAPSE_APPEND
         * we write the data via standard file I/O so we close the
         * items here
         */
        retcd = avformat_write_header(ffmpeg->oc, NULL);
        if (retcd < 0){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Could not write ffmpeg header %s"),errstr);
            ffmpeg_free_context(ffmpeg);
            return -1;
        }
        if (ffmpeg->tlapse == TIMELAPSE_APPEND) {
            av_write_trailer(ffmpeg->oc);
            avio_close(ffmpeg->oc->pb);
        }

    }

    return 0;

}

static int ffmpeg_flush_codec(struct ffmpeg *ffmpeg){

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
    //ffmpeg version 3.1 and after

    int retcd;
    int recv_cd = 0;
    char errstr[128];

    if (ffmpeg->passthrough){
        return 0;
    }

    retcd = 0;
    recv_cd = 0;
    if (ffmpeg->tlapse == TIMELAPSE_NONE) {
        retcd = avcodec_send_frame(ffmpeg->ctx_codec, NULL);
        if (retcd < 0 ){
            av_strerror(retcd, errstr, sizeof(errstr));
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                ,_("Error entering draining mode:%s"),errstr);
            return -1;
        }
        while (recv_cd != AVERROR_EOF){
            av_init_packet(&ffmpeg->pkt);
            ffmpeg->pkt.data = NULL;
            ffmpeg->pkt.size = 0;
            recv_cd = avcodec_receive_packet(ffmpeg->ctx_codec, &ffmpeg->pkt);
            if (recv_cd != AVERROR_EOF){
                if (recv_cd < 0){
                    av_strerror(recv_cd, errstr, sizeof(errstr));
                    MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                        ,_("Error draining codec:%s"),errstr);
                    my_packet_unref(ffmpeg->pkt);
                    return -1;
                }
                // v4l2_m2m encoder uses pts 0 and size 0 to indicate AVERROR_EOF
                if ((ffmpeg->pkt.pts == 0) || (ffmpeg->pkt.size == 0)) {
                    recv_cd = AVERROR_EOF;
                    my_packet_unref(ffmpeg->pkt);
                    continue;
                }
                retcd = av_write_frame(ffmpeg->oc, &ffmpeg->pkt);
                if (retcd < 0) {
                    MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
                        ,_("Error writing draining video frame"));
                    return -1;
                }
            }
            my_packet_unref(ffmpeg->pkt);
        }
    }
    return 0;
#else
    /* Dummy to kill warnings.  No draining in older ffmpeg versions */
    if (ffmpeg) {
        return 0;
    } else{
        return 0;
    }
#endif

}

static int ffmpeg_put_frame(struct ffmpeg *ffmpeg, const struct timeval *tv1){
    int retcd;

    av_init_packet(&ffmpeg->pkt);
    ffmpeg->pkt.data = NULL;
    ffmpeg->pkt.size = 0;

    retcd = ffmpeg_set_pts(ffmpeg, tv1);
    if (retcd < 0) {
        //If there is an error, it has already been reported.
        my_packet_unref(ffmpeg->pkt);
        return 0;
    }

    retcd = ffmpeg_encode_video(ffmpeg);
    if (retcd != 0){
        if (retcd != -2){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while encoding picture"));
        }
        my_packet_unref(ffmpeg->pkt);
        return retcd;
    }

    if (ffmpeg->tlapse == TIMELAPSE_APPEND) {
        retcd = ffmpeg_timelapse_append(ffmpeg, ffmpeg->pkt);
    } else {
        retcd = av_write_frame(ffmpeg->oc, &ffmpeg->pkt);
    }
    my_packet_unref(ffmpeg->pkt);

    if (retcd < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error while writing video frame"));
        return -1;
    }
    return retcd;

}

static void ffmpeg_passthru_reset(struct ffmpeg *ffmpeg){
    /* Reset the written flag at start of each event */
    int indx;

    pthread_mutex_lock(&ffmpeg->rtsp_data->mutex_pktarray);
        for(indx = 0; indx < ffmpeg->rtsp_data->pktarray_size; indx++) {
            ffmpeg->rtsp_data->pktarray[indx].iswritten = FALSE;
        }
    pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_pktarray);

}

static void ffmpeg_passthru_write(struct ffmpeg *ffmpeg, int indx){
    /* Write the packet in the buffer at indx to file */
    char errstr[128];
    int retcd;

    av_init_packet(&ffmpeg->pkt);
    ffmpeg->pkt.data = NULL;
    ffmpeg->pkt.size = 0;


    ffmpeg->rtsp_data->pktarray[indx].iswritten = TRUE;

    retcd = my_copy_packet(&ffmpeg->pkt, &ffmpeg->rtsp_data->pktarray[indx].packet);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("av_copy_packet: %s"),errstr);
        my_packet_unref(ffmpeg->pkt);
        return;
    }

    retcd = ffmpeg_set_pktpts(ffmpeg, &ffmpeg->rtsp_data->pktarray[indx].timestamp_tv);
    if (retcd < 0) {
        my_packet_unref(ffmpeg->pkt);
        return;
    }

    ffmpeg->pkt.stream_index = 0;

    retcd = av_write_frame(ffmpeg->oc, &ffmpeg->pkt);
    my_packet_unref(ffmpeg->pkt);
    if (retcd < 0) {
        av_strerror(retcd, errstr, sizeof(errstr));
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO
            ,_("Error while writing video frame: %s"),errstr);
        return;
    }

}

static int ffmpeg_passthru_put(struct ffmpeg *ffmpeg, struct image_data *img_data){

    int idnbr_image, idnbr_lastwritten, idnbr_stop, idnbr_firstkey;
    int indx, indx_lastwritten, indx_firstkey;

    if (ffmpeg->rtsp_data == NULL) return -1;

    if ((ffmpeg->rtsp_data->status == RTSP_NOTCONNECTED  ) ||
        (ffmpeg->rtsp_data->status == RTSP_RECONNECTING  ) ){
        return 0;
    }

    if (ffmpeg->high_resolution){
        idnbr_image = img_data->idnbr_high;
    } else {
        idnbr_image = img_data->idnbr_norm;
    }

    pthread_mutex_lock(&ffmpeg->rtsp_data->mutex_pktarray);
        idnbr_lastwritten = 0;
        idnbr_firstkey = idnbr_image;
        idnbr_stop = 0;
        indx_lastwritten = -1;
        indx_firstkey = -1;

        for(indx = 0; indx < ffmpeg->rtsp_data->pktarray_size; indx++) {
            if ((ffmpeg->rtsp_data->pktarray[indx].iswritten) &&
                (ffmpeg->rtsp_data->pktarray[indx].idnbr > idnbr_lastwritten)){
                idnbr_lastwritten=ffmpeg->rtsp_data->pktarray[indx].idnbr;
                indx_lastwritten = indx;
            }
            if ((ffmpeg->rtsp_data->pktarray[indx].idnbr >  idnbr_stop) &&
                (ffmpeg->rtsp_data->pktarray[indx].idnbr <= idnbr_image)){
                idnbr_stop=ffmpeg->rtsp_data->pktarray[indx].idnbr;
            }
            if ((ffmpeg->rtsp_data->pktarray[indx].iskey) &&
                (ffmpeg->rtsp_data->pktarray[indx].idnbr <= idnbr_firstkey)){
                    idnbr_firstkey=ffmpeg->rtsp_data->pktarray[indx].idnbr;
                    indx_firstkey = indx;
            }
        }

        if (idnbr_stop == 0){
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_pktarray);
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
            if ((!ffmpeg->rtsp_data->pktarray[indx].iswritten) &&
                (ffmpeg->rtsp_data->pktarray[indx].packet.size > 0) &&
                (ffmpeg->rtsp_data->pktarray[indx].idnbr >  idnbr_lastwritten) &&
                (ffmpeg->rtsp_data->pktarray[indx].idnbr <= idnbr_image)) {
                ffmpeg_passthru_write(ffmpeg, indx);
            }
            if (ffmpeg->rtsp_data->pktarray[indx].idnbr == idnbr_stop) break;
            indx++;
            if (indx == ffmpeg->rtsp_data->pktarray_size ) indx = 0;
        }
    pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_pktarray);
    return 0;
}

static int ffmpeg_passthru_codec(struct ffmpeg *ffmpeg){

    int retcd;
    AVStream    *stream_in;

    if (ffmpeg->rtsp_data == NULL){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("RTSP context not available."));
        return -1;
    }

    pthread_mutex_lock(&ffmpeg->rtsp_data->mutex_transfer);

        if ((ffmpeg->rtsp_data->status == RTSP_NOTCONNECTED  ) ||
            (ffmpeg->rtsp_data->status == RTSP_RECONNECTING  ) ){
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("rtsp camera not ready for pass-through."));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }

        if (strcmp(ffmpeg->codec_name, "mp4") != 0){
            MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
                ,_("pass-through mode enabled.  Changing to MP4 container."));
            ffmpeg->codec_name = "mp4";
        }

        retcd = ffmpeg_get_oformat(ffmpeg);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }

#if (LIBAVFORMAT_VERSION_MAJOR >= 58) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR >= 41))
        stream_in = ffmpeg->rtsp_data->transfer_format->streams[0];
        ffmpeg->oc->oformat->video_codec = stream_in->codecpar->codec_id;

        ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, NULL);
        if (!ffmpeg->video_st) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }

        retcd = avcodec_parameters_copy(ffmpeg->video_st->codecpar, stream_in->codecpar);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy codec parameters"));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }
        ffmpeg->video_st->codecpar->codec_tag  = 0;

#elif (LIBAVFORMAT_VERSION_MAJOR >= 55)

        stream_in = ffmpeg->rtsp_data->transfer_format->streams[0];

        ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, stream_in->codec->codec);
        if (!ffmpeg->video_st) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not alloc stream"));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }

        retcd = avcodec_copy_context(ffmpeg->video_st->codec, stream_in->codec);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Unable to copy codec parameters"));
            pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
            return -1;
        }
        ffmpeg->video_st->codec->flags     |= MY_CODEC_FLAG_GLOBAL_HEADER;
        ffmpeg->video_st->codec->codec_tag  = 0;
#else
        /* This is disabled in the util_check_passthrough but we need it here for compiling */
        pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
        MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("Pass-through disabled.  ffmpeg too old"));
        return -1;
#endif

        ffmpeg->video_st->time_base         = stream_in->time_base;
    pthread_mutex_unlock(&ffmpeg->rtsp_data->mutex_transfer);
    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, _("Pass-through stream opened"));
    return 0;

}

void ffmpeg_avcodec_log(void *ignoreme ATTRIBUTE_UNUSED, int errno_flag ATTRIBUTE_UNUSED, const char *fmt, va_list vl){

    char buf[1024];
    char *end;

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

static void ffmpeg_put_pix_nv21(struct ffmpeg *ffmpeg, struct image_data *img_data){
    unsigned char *image,*imagecr, *imagecb;
    int cr_len, x, y;

    if (ffmpeg->high_resolution){
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    cr_len = ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height / 4;
    imagecr = image + (ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height);
    imagecb = image + (ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height) + cr_len;

    memcpy(ffmpeg->picture->data[0], image, ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height);
    for (y = 0; y < ffmpeg->ctx_codec->height; y++) {
        for (x = 0; x < ffmpeg->ctx_codec->width/4; x++) {
            ffmpeg->picture->data[1][y*ffmpeg->ctx_codec->width/2 + x*2] = *imagecb;
            ffmpeg->picture->data[1][y*ffmpeg->ctx_codec->width/2 + x*2 + 1] = *imagecr;
            imagecb++;
            imagecr++;
        }
    }

}

static void ffmpeg_put_pix_yuv420(struct ffmpeg *ffmpeg, struct image_data *img_data){
    unsigned char *image;

    if (ffmpeg->high_resolution){
        image = img_data->image_high;
    } else {
        image = img_data->image_norm;
    }

    // Usual setup for image pointers
    ffmpeg->picture->data[0] = image;
    ffmpeg->picture->data[1] = image + (ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height);
    ffmpeg->picture->data[2] = ffmpeg->picture->data[1] + ((ffmpeg->ctx_codec->width * ffmpeg->ctx_codec->height) / 4);

}


#endif /* HAVE_FFMPEG */

/****************************************************************************
 ****************************************************************************
 ****************************************************************************/

void ffmpeg_global_init(void){
#ifdef HAVE_FFMPEG


    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO
        ,_("ffmpeg libavcodec version %d.%d.%d"
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
    av_log_set_callback((void *)ffmpeg_avcodec_log);

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
    /* TODO: Determine if this is even needed for older versions */
    int ret;
    ret = av_lockmgr_register(ffmpeg_lockmgr_cb);
    if (ret < 0)
    {
        MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO, _("av_lockmgr_register failed (%d)"), ret);
        exit(1);
    }
#endif

#else /* No FFMPEG */

    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("No ffmpeg functionality included"));

#endif /* HAVE_FFMPEG */
}

void ffmpeg_global_deinit(void) {
#ifdef HAVE_FFMPEG

    avformat_network_deinit();

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
    /* TODO Determine if this is even needed for old versions */
    if (av_lockmgr_register(NULL) < 0)
    {
        MOTION_LOG(EMG, TYPE_ALL, SHOW_ERRNO
            ,_("av_lockmgr_register reset failed on cleanup"));
    }
#endif


#else /* No FFMPEG */

    MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("No ffmpeg functionality included"));

#endif /* HAVE_FFMPEG */
}

int ffmpeg_open(struct ffmpeg *ffmpeg){

#ifdef HAVE_FFMPEG

    int retcd;

    ffmpeg->oc = avformat_alloc_context();
    if (!ffmpeg->oc) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not allocate output context"));
        ffmpeg_free_context(ffmpeg);
        return -1;
    }

    if (ffmpeg->passthrough) {
        retcd = ffmpeg_passthru_codec(ffmpeg);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not setup passthru!"));
            ffmpeg_free_context(ffmpeg);
            return -1;
        }

        ffmpeg_passthru_reset(ffmpeg);

    } else {
        retcd = ffmpeg_get_oformat(ffmpeg);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not get codec!"));
            ffmpeg_free_context(ffmpeg);
            return -1;
        }

        retcd = ffmpeg_set_codec(ffmpeg);
        if (retcd < 0 ) {
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Failed to allocate codec!"));
            return -1;
        }

        retcd = ffmpeg_set_stream(ffmpeg);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
            return -1;
        }

        retcd = ffmpeg_set_picture(ffmpeg);
        if (retcd < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
            return -1;
        }
    }



    retcd = ffmpeg_set_outputfile(ffmpeg);
    if (retcd < 0){
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Could not set the stream"));
        return -1;
    }

    return 0;

#else /* No FFMPEG */

    if (ffmpeg) {
        MOTION_LOG(NTC, TYPE_ENCODER, NO_ERRNO, _("No ffmpeg functionality included"));
    }
    return -1;

#endif /* HAVE_FFMPEG */

}

void ffmpeg_close(struct ffmpeg *ffmpeg){
#ifdef HAVE_FFMPEG

    if (ffmpeg != NULL) {

        if (ffmpeg_flush_codec(ffmpeg) < 0){
            MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, _("Error flushing codec"));
        }
        if (ffmpeg->oc->pb != NULL){
            if (ffmpeg->tlapse != TIMELAPSE_APPEND) {
                av_write_trailer(ffmpeg->oc);
            }
            if (!(ffmpeg->oc->oformat->flags & AVFMT_NOFILE)) {
                if (ffmpeg->tlapse != TIMELAPSE_APPEND) {
                    avio_close(ffmpeg->oc->pb);
                }
            }
        }
        ffmpeg_free_context(ffmpeg);
        ffmpeg_free_nal(ffmpeg);
    }

#else
    if (ffmpeg != NULL) free(ffmpeg);
#endif // HAVE_FFMPEG
}


int ffmpeg_put_image(struct ffmpeg *ffmpeg, struct image_data *img_data, const struct timeval *tv1){
#ifdef HAVE_FFMPEG
    int retcd = 0;
    int cnt = 0;


    if (ffmpeg->passthrough) {
        retcd = ffmpeg_passthru_put(ffmpeg, img_data);
        return retcd;
    }

    if (ffmpeg->picture) {

        if (ffmpeg->preferred_codec == USER_CODEC_V4L2M2M) {
            ffmpeg_put_pix_nv21(ffmpeg, img_data);
        } else {
            ffmpeg_put_pix_yuv420(ffmpeg, img_data);
        }

        ffmpeg->gop_cnt ++;
        if (ffmpeg->gop_cnt == ffmpeg->ctx_codec->gop_size ){
            ffmpeg->picture->pict_type = AV_PICTURE_TYPE_I;
            ffmpeg->picture->key_frame = 1;
            ffmpeg->gop_cnt = 0;
        } else {
            ffmpeg->picture->pict_type = AV_PICTURE_TYPE_P;
            ffmpeg->picture->key_frame = 0;
        }

        /* A return code of -2 is thrown by the put_frame
         * when a image is buffered.  For timelapse, we absolutely
         * never want a frame buffered so we keep sending back the
         * the same pic until it flushes or fails in a different way
         */
        retcd = ffmpeg_put_frame(ffmpeg, tv1);
        while ((retcd == -2) && (ffmpeg->tlapse != TIMELAPSE_NONE)) {
            retcd = ffmpeg_put_frame(ffmpeg, tv1);
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

#else
    if (ffmpeg && img_data && tv1) {
        MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO, _("No ffmpeg support"));
    }
    return 0;
#endif // HAVE_FFMPEG
}

void ffmpeg_reset_movie_start_time(struct ffmpeg *ffmpeg, const struct timeval *tv1){
#ifdef HAVE_FFMPEG
    int64_t one_frame_interval = av_rescale_q(1,(AVRational){1, ffmpeg->fps},ffmpeg->video_st->time_base);
    if (one_frame_interval <= 0)
        one_frame_interval = 1;
    ffmpeg->base_pts = ffmpeg->last_pts + one_frame_interval;

    ffmpeg->start_time.tv_sec = tv1->tv_sec;
    ffmpeg->start_time.tv_usec = tv1->tv_usec;

#else
    if (ffmpeg && tv1) {
        MOTION_LOG(DBG, TYPE_ENCODER, NO_ERRNO, _("No ffmpeg support"));
    }
#endif // HAVE_FFMPEG
}
