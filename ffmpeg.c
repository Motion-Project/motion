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
 */

#ifdef HAVE_FFMPEG

#include "ffmpeg.h"
#include "motion.h"

#if LIBAVCODEC_BUILD > 4680
/*
 * FFmpeg after build 4680 doesn't have support for mpeg1 videos with
 * non-standard framerates. Previous builds contained a broken hack
 * that padded with B frames to obtain the correct framerate.
 */
#    define FFMPEG_NO_NONSTD_MPEG1
#    ifdef __GNUC__
/* #warning is a non-standard gcc extension */
#        warning **************************************************
#        warning Your version of FFmpeg is newer than version 0.4.8
#        warning Newer versions of ffmpeg do not support MPEG1 with
#        warning non-standard framerate. MPEG1 will be disabled for
#        warning normal video output. You can still use mpeg4 and
#        warning and mpeg4ms which are both better in terms of size
#        warning and quality. MPEG1 is always used for timelapse.
#        warning Please read the Motion Guide for more information.
#        warning Note that this is not an error message!
#        warning **************************************************
#    endif /* __GNUC__ */
#endif /* LIBAVCODEC_BUILD > 4680 */

#if defined LIBAVFORMAT_VERSION_MAJOR && defined LIBAVFORMAT_VERSION_MINOR 
#   if LIBAVFORMAT_VERSION_MAJOR < 53 && LIBAVFORMAT_VERSION_MINOR < 45
#       define GUESS_NO_DEPRECATED 
#   endif
#endif

#if LIBAVFORMAT_BUILD >= 4616
/*
 * The API for av_write_frame changed with FFmpeg version 0.4.9pre1.
 * It now uses an AVPacket struct instead of direct parameters to the
 * function.
 */
#    define FFMPEG_AVWRITEFRAME_NEWAPI
#endif /* LIBAVFORMAT_BUILD >= 4616 */

#if LIBAVFORMAT_BUILD >= 4629
/*
 * In this build/header version, the codec member of struct AVStream
 * was changed to a pointer so changes to AVCodecContext shouldn't
 * break binary compatibility with AVStream.
 */
#    define AVSTREAM_CODEC_PTR(avs_ptr) (avs_ptr->codec)
#else
#    define AVSTREAM_CODEC_PTR(avs_ptr) (&avs_ptr->codec)
#endif /* LIBAVFORMAT_BUILD >= 4629 */

// AV_VERSION_INT(a, b, c) (a<<16 | b<<8 | c) 
// (54*2^16 | 6*2^8 | 100)
#if LIBAVFORMAT_BUILD >= 3540580
#define FF_API_NEW_AVIO
#define URL_RDONLY  AVIO_FLAG_READ       /**< read-only */
#define URL_WRONLY  AVIO_FLAG_WRITE      /**< write-only */
#define URL_RDWR    AVIO_FLAG_READ_WRITE /**< read-write pseudo flag */
#endif


/*
 * Name of custom file protocol for appending to existing files instead
 * of truncating.
 */
#define APPEND_PROTO "appfile"

/* Some forward-declarations. */
int ffmpeg_put_frame(struct ffmpeg *, AVFrame *);
void ffmpeg_cleanups(struct ffmpeg *);
AVFrame *ffmpeg_prepare_frame(struct ffmpeg *, unsigned char *,
                              unsigned char *, unsigned char *);

/* This is the trailer used to end mpeg1 videos. */
static unsigned char mpeg1_trailer[] = {0x00, 0x00, 0x01, 0xb7};


// FFMPEG API changed in 0.8
#if defined FF_API_NEW_AVIO

// TODO 

	
#else

/**
 * file_open_append
 *      Append version of the file open function used in libavformat when opening
 *      an ordinary file. The original file open function truncates an existing
 *      file, but this version appends to it instead.
 *
 *  Returns 0 on success and AVERROR(ENOENT) on error.
 *
 */
static int file_open_append(URLContext *h, const char *filename, int flags)
{
    const char *colon;
    const char *mode;
    FILE *fh;
    size_t bufsize = 0;

    /* Skip past the protocol part of filename. */
    colon = strchr(filename, ':');

    if (colon)
        filename = colon + 1;


    if (flags & URL_RDWR) {
        mode = "ab+";
        bufsize = BUFSIZE_1MEG;
    } else if (flags & URL_WRONLY) {
        mode = "ab";
        bufsize = BUFSIZE_1MEG;
    } else {
        mode = "rb";
    }

    fh = myfopen(filename, mode, bufsize);
    if (fh == NULL)
        return AVERROR(ENOENT);

    h->priv_data = (void *)fh;
    return 0;
}

/*
 * URLProtocol entry for the append file protocol, which we use for mpeg1 videos
 * in order to get append behavior with url_fopen.
 *
 * Libavformat uses protocols for achieving flexibility when handling files
 * and other resources. A call to url_fopen will eventually be redirected to
 * a protocol-specific open function.
 *
 * The remaining functions (for writing, seeking etc.) are set in ffmpeg_init.
 */
URLProtocol mpeg1_file_protocol = {
    .name     = APPEND_PROTO,
    .url_open = file_open_append
};


#ifdef HAVE_FFMPEG_NEW

/* file_procotol has been removed from avio.h */
#ifdef FFMPEG_NEW_INCLUDES
#include <libavutil/avstring.h>
#else
#include "avstring.h"
#endif

/**
 * file_open
 *
 */
static int file_open(URLContext *h, const char *filename, int flags)
{
    const char *mode;
    FILE *fh;
    size_t bufsize = 0;

    av_strstart(filename, "file:", &filename);

    if (flags & URL_RDWR) {
        mode = "wb+";
        bufsize = BUFSIZE_1MEG;
    } else if (flags & URL_WRONLY) {
        mode = "wb";
        bufsize = BUFSIZE_1MEG;
    } else {
        mode = "rb";
    }
    fh = myfopen(filename, mode, bufsize);
    if (fh == NULL)
        return AVERROR(ENOENT);
    h->priv_data = (void *)fh;
    return 0;
}

/**
 * file_read
 */
static int file_read(URLContext *h, unsigned char *buf, int size)
{
    FILE *fh = (FILE *)h->priv_data;
    return fread(buf, 1, size, fh);
}

/**
 * file_write
 */
static int file_write(URLContext *h, unsigned char *buf, int size)
{
    FILE *fh = (FILE *)h->priv_data;
    return fwrite(buf, 1, size, fh);
}

/**
 * file_seek
 */
static int64_t file_seek(URLContext *h, int64_t pos, int whence)
{
    FILE *fh = (FILE *)h->priv_data;
    if (fseek(fh, pos, whence))
        return -1;
    return ftell(fh);
}

/**
 * file_close
 */
static int file_close(URLContext *h)
{
    FILE *fh = (FILE *)h->priv_data;
    return myfclose(fh);
}

URLProtocol file_protocol = {
    "file",
    file_open,
    file_read,
    file_write,
    file_seek,
    file_close,
#if LIBAVFORMAT_BUILD >= (52<<16 | 31<<8)
    NULL,
    NULL,
    NULL,
#endif
};

#endif // HAVE_FFMPEG_NEW

#endif // FF_API_NEW_AVIO

/**
 * mpeg1_write_trailer
 *      We set AVOutputFormat->write_trailer to this function for mpeg1. That way,
 *      the mpeg1 video gets a proper trailer when it is closed.
 *
 *  Returns 0
 *
 */
static int mpeg1_write_trailer(AVFormatContext *s)
{
#if defined FF_API_NEW_AVIO
    avio_write(s->pb, mpeg1_trailer, 4);
    avio_flush(s->pb);
#elif LIBAVFORMAT_BUILD >= (52<<16)
    put_buffer(s->pb, mpeg1_trailer, 4);
    put_flush_packet(s->pb);
#else
    put_buffer(&s->pb, mpeg1_trailer, 4);
    put_flush_packet(&s->pb);
#endif /* FF_API_NEW_AVIO -- LIBAVFORMAT_BUILD >= (52<<16) */

    return 0; /* success */
}

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

#if LIBAVCODEC_BUILD > 4680
    av_log_set_callback((void *)ffmpeg_avcodec_log);
    av_log_set_level(AV_LOG_ERROR);
#endif

#if defined FF_API_NEW_AVIO
#else
    /*
     * Copy the functions to use for the append file protocol from the standard
     * file protocol.
     */
    mpeg1_file_protocol.url_read  = file_protocol.url_read;
    mpeg1_file_protocol.url_write = file_protocol.url_write;
    mpeg1_file_protocol.url_seek  = file_protocol.url_seek;
    mpeg1_file_protocol.url_close = file_protocol.url_close;

/* Register the append file protocol. */
#ifdef have_av_register_protocol2
    av_register_protocol2(&mpeg1_file_protocol, sizeof(mpeg1_file_protocol));
#elif defined have_av_register_protocol        
    av_register_protocol(&mpeg1_file_protocol);
#else
#   warning av_register_protocolXXX missing
#endif

#endif // FF_API_NEW_AVIO

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
     * We also dynamically add the file extension to the filename here. This was
     * done to support both mpeg1 and mpeg4 codecs since they have different extensions.
     */
    if ((strcmp(codec, TIMELAPSE_CODEC) == 0)
#ifndef FFMPEG_NO_NONSTD_MPEG1
        || (strcmp(codec, "mpeg1") == 0)
#endif
    ) {
        ext = ".mpg";
        /*
         * We use "mpeg1video" for raw mpeg1 format. Using "mpeg" would
         * result in a muxed output file, which isn't appropriate here.
         */
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);        
#else
        of = av_guess_format("mpeg1video", NULL, NULL);
#endif 
        /* But we want the trailer to be correctly written. */
        if (of)
            of->write_trailer = mpeg1_write_trailer;

#ifdef FFMPEG_NO_NONSTD_MPEG1
    } else if (strcmp(codec, "mpeg1") == 0) {
        MOTION_LOG(WRN, TYPE_ENCODER, NO_ERRNO, "%s: *** mpeg1 support for normal"
                   " videos has been disabled ***");
        return NULL;
#endif
    } else if (strcmp(codec, "mpeg4") == 0) {
        ext = ".avi";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else        
        of = av_guess_format("avi", NULL, NULL);
#endif        
    } else if (strcmp(codec, "msmpeg4") == 0) {
        ext = ".avi";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else        
        of = av_guess_format("avi", NULL, NULL);
#endif
        /* Manually override the codec id. */
        if (of)
            of->video_codec = CODEC_ID_MSMPEG4V2;

    } else if (strcmp(codec, "swf") == 0) {
        ext = ".swf";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else        
        of = av_guess_format("swf", NULL, NULL);
#endif        
    } else if (strcmp(codec, "flv") == 0) {
        ext = ".flv";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else        
        of = av_guess_format("flv", NULL, NULL);
#endif        
        of->video_codec = CODEC_ID_FLV1;
    } else if (strcmp(codec, "ffv1") == 0) {
        ext = ".avi";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else
        of = av_guess_format("avi", NULL, NULL);
#endif
        /*
         * Use the FFMPEG Lossless Video codec (experimental!).
         * Requires strict_std_compliance to be <= -2
         */
        if (of)
            of->video_codec = CODEC_ID_FFV1;

    } else if (strcmp(codec, "mov") == 0) {
        ext = ".mov";
#ifdef GUESS_NO_DEPRECATED
        of = guess_format("mpeg1video", NULL, NULL);
#else        
        of = av_guess_format("mov", NULL, NULL);
#endif
	    }
  else if (strcmp (codec, "ogg") == 0)
    {
      ext = ".ogg";
#ifdef GUESS_NO_DEPRECATED
      of = guess_format ("ogg", NULL, NULL);
#else
      of = av_guess_format ("ogg", NULL, NULL);
#endif
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
    int is_mpeg1;
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
#ifdef have_avformat_alloc_context
    ffmpeg->oc = avformat_alloc_context();
#elif defined have_av_avformat_alloc_context
    ffmpeg->oc = av_alloc_format_context();
#else
    ffmpeg->oc = av_mallocz(sizeof(AVFormatContext));
#endif

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
    ffmpeg->video_st = NULL;
    if (ffmpeg->oc->oformat->video_codec != CODEC_ID_NONE) {
#if defined FF_API_NEW_AVIO 
        ffmpeg->video_st = avformat_new_stream(ffmpeg->oc, NULL /* Codec */);
#else
        ffmpeg->video_st = av_new_stream(ffmpeg->oc, 0);
#endif
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
#if LIBAVCODEC_VERSION_MAJOR < 53    
    c->codec_type = CODEC_TYPE_VIDEO;
#else
    c->codec_type = AVMEDIA_TYPE_VIDEO;
#endif    
    is_mpeg1      = c->codec_id == CODEC_ID_MPEG1VIDEO;

    if (strcmp(ffmpeg_video_codec, "ffv1") == 0)
        c->strict_std_compliance = -2;

    /* Uncomment to allow non-standard framerates. */
    //c->strict_std_compliance = -1;

    /* Set default parameters */
    c->bit_rate = bps;
    c->width    = width;
    c->height   = height;
#if LIBAVCODEC_BUILD >= 4754
    /* Frame rate = 1/time_base, so we set 1/rate, not rate/1 */
    c->time_base.num = 1;
    c->time_base.den = rate;
#else
    c->frame_rate      = rate;
    c->frame_rate_base = 1;
#endif /* LIBAVCODEC_BUILD >= 4754 */

    MOTION_LOG(INF, TYPE_ENCODER, NO_ERRNO, "%s FPS %d",
               rate);

    if (vbr)
        c->flags |= CODEC_FLAG_QSCALE;

    /*
     * Set codec specific parameters.
     * Set intra frame distance in frames depending on codec.
     */
    c->gop_size = is_mpeg1 ? 10 : 12;

    /* Some formats want stream headers to be separate. */
    if (!strcmp(ffmpeg->oc->oformat->name, "mp4") ||
        !strcmp(ffmpeg->oc->oformat->name, "mov") ||
        !strcmp(ffmpeg->oc->oformat->name, "3gp")) {
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

#if defined FF_API_NEW_AVIO
// pass the options to avformat_write_header directly
#else
    /* Set the output parameters (must be done even if no parameters). */
    if (av_set_parameters(ffmpeg->oc, NULL) < 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: av_set_parameters error:"
                   " Invalid output format parameters");
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }
#endif

    /* Dump the format settings.  This shows how the various streams relate to each other. */
    //dump_format(ffmpeg->oc, 0, filename, 1);

    /*
     * Now that all the parameters are set, we can open the video
     * codec and allocate the necessary encode buffers.
     */
    codec = avcodec_find_encoder(c->codec_id);

    if (!codec) {
        MOTION_LOG(ERR, TYPE_ENCODER, NO_ERRNO, "%s: Codec %s not found",
                   ffmpeg_video_codec);
        ffmpeg_cleanups(ffmpeg);
        return NULL;
    }

    /* Set the picture format - need in ffmpeg starting round April-May 2005 */
    c->pix_fmt = PIX_FMT_YUV420P;

    /* Get a mutex lock. */
    pthread_mutex_lock(&global_lock);

    /* Open the codec */
#if defined FF_API_NEW_AVIO
    ret = avcodec_open2(c, codec, NULL /* options */ );
#else
    ret = avcodec_open(c, codec);
#endif

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
    ffmpeg->picture = avcodec_alloc_frame();

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

        /*
         * Use append file protocol for mpeg1, to get the append behavior from
         * url_fopen, but no protocol (=> default) for other codecs.
         */
        if (is_mpeg1)
#if defined FF_API_NEW_AVIO
            snprintf(file_proto, sizeof(file_proto), "%s", filename);
#else
            snprintf(file_proto, sizeof(file_proto), APPEND_PROTO ":%s", filename);
#endif
        else
            snprintf(file_proto, sizeof(file_proto), "%s", filename);


#if defined FF_API_NEW_AVIO
        if (avio_open(&ffmpeg->oc->pb, file_proto, URL_WRONLY) < 0) {
#else
        if (url_fopen(&ffmpeg->oc->pb, file_proto, URL_WRONLY) < 0) {
#endif
            /* Path did not exist? */
            if (errno == ENOENT) {
                /* Create path for file (don't use file_proto)... */
                if (create_path(filename) == -1) {
                    ffmpeg_cleanups(ffmpeg);
                    return NULL;
                }

#if defined FF_API_NEW_AVIO
                if (avio_open(&ffmpeg->oc->pb, file_proto, URL_WRONLY) < 0) {
#else
                /* And retry opening the file (use file_proto). */
                if (url_fopen(&ffmpeg->oc->pb, file_proto, URL_WRONLY) < 0) {
#endif
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
#if defined FF_API_NEW_AVIO
    avformat_write_header(ffmpeg->oc, NULL);
#else
    av_write_header(ffmpeg->oc);
#endif // FF_API_NEW_AVIO
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
    unsigned int i;

    /* Close each codec */
    if (ffmpeg->video_st) {
        pthread_mutex_lock(&global_lock);
#if LIBAVCODEC_BUILD > 4680
        if (ffmpeg->video_st->codec->priv_data != NULL)
#endif
            avcodec_close(AVSTREAM_CODEC_PTR(ffmpeg->video_st));
        pthread_mutex_unlock(&global_lock);
        av_freep(&ffmpeg->picture);
        free(ffmpeg->video_outbuf);
    }

    /* Free the streams */
    for (i = 0; i < ffmpeg->oc->nb_streams; i++)
        av_freep(&ffmpeg->oc->streams[i]);

    /* Free the stream */
    av_free(ffmpeg->oc);
#if LIBAVFORMAT_BUILD >= 4629
    av_free(ffmpeg->c);
#endif
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
    unsigned int i;

    /* Close each codec */
    if (ffmpeg->video_st) {
        pthread_mutex_lock(&global_lock);
        avcodec_close(AVSTREAM_CODEC_PTR(ffmpeg->video_st));
        pthread_mutex_unlock(&global_lock);
        av_freep(&ffmpeg->picture);
        free(ffmpeg->video_outbuf);
    }

    /* Write the trailer, if any. */
    av_write_trailer(ffmpeg->oc);

    /* Free the streams. */
    for (i = 0; i < ffmpeg->oc->nb_streams; i++)
        av_freep(&ffmpeg->oc->streams[i]);

    if (!(ffmpeg->oc->oformat->flags & AVFMT_NOFILE)) {
        /* Close the output file. */
#if defined FF_API_NEW_AVIO
        avio_close(ffmpeg->oc->pb);
#elif LIBAVFORMAT_BUILD >= (52<<16)
        url_fclose(ffmpeg->oc->pb);
#else
        url_fclose(&ffmpeg->oc->pb);
#endif /* FF_API_NEW_AVIO -- LIBAVFORMAT_BUILD >= (52<<16) */
    }


    /* Free the stream. */
    av_free(ffmpeg->oc);
#if LIBAVFORMAT_BUILD >= 4629
    av_free(ffmpeg->c);
#endif
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
    int out_size, ret, got_packet_ptr;

#ifdef FFMPEG_AVWRITEFRAME_NEWAPI
    AVPacket pkt;

    av_init_packet(&pkt); /* Init static structure. */
    pkt.stream_index = ffmpeg->video_st->index;
#endif /* FFMPEG_AVWRITEFRAME_NEWAPI */

    if (ffmpeg->oc->oformat->flags & AVFMT_RAWPICTURE) {
        /* Raw video case. The API will change slightly in the near future for that. */
#ifdef FFMPEG_AVWRITEFRAME_NEWAPI
#   if LIBAVCODEC_VERSION_MAJOR < 53        
        pkt.flags |= PKT_FLAG_KEY;
#   else
        pkt.flags |= AV_PKT_FLAG_KEY;  
#   endif        
        pkt.data = (uint8_t *)pic;
        pkt.size = sizeof(AVPicture);
        ret = av_write_frame(ffmpeg->oc, &pkt);
#else
        ret = av_write_frame(ffmpeg->oc, ffmpeg->video_st->index,
            (uint8_t *)pic, sizeof(AVPicture));
#endif /* FFMPEG_AVWRITEFRAME_NEWAPI */
    } else {
        /* Encodes the image. */
#if defined FF_API_NEW_AVIO
        pkt.data = ffmpeg->video_outbuf;
        pkt.size = ffmpeg->video_outbuf_size;

        out_size = avcodec_encode_video2(AVSTREAM_CODEC_PTR(ffmpeg->video_st), 
                                        &pkt, pic, &got_packet_ptr);
        if (out_size < 0)
            // Error encondig 
            out_size = 0;
        else
            out_size = pkt.size;
#else
        out_size = avcodec_encode_video(AVSTREAM_CODEC_PTR(ffmpeg->video_st),
                                        ffmpeg->video_outbuf,
                                        ffmpeg->video_outbuf_size, pic);
#endif
        /* If zero size, it means the image was buffered. */
        if (out_size != 0) {
            /*
             * Writes the compressed frame in the media file.
             * XXX: in case of B frames, the pts is not yet valid.
             */
#ifdef FFMPEG_AVWRITEFRAME_NEWAPI
            pkt.pts = AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->pts;

            if (AVSTREAM_CODEC_PTR(ffmpeg->video_st)->coded_frame->key_frame)
#   if LIBAVCODEC_VERSION_MAJOR < 53                
                pkt.flags |= PKT_FLAG_KEY;
#   else
                pkt.flags |= AV_PKT_FLAG_KEY;
#   endif                

            pkt.data = ffmpeg->video_outbuf;
            pkt.size = out_size;
            ret = av_write_frame(ffmpeg->oc, &pkt);
#else
            ret = av_write_frame(ffmpeg->oc, ffmpeg->video_st->index,
                                 ffmpeg->video_outbuf, out_size);
#endif /* FFMPEG_AVWRITEFRAME_NEWAPI */

        } else {
            ret = 0;
        }
    }

    if (ret != 0) {
        MOTION_LOG(ERR, TYPE_ENCODER, SHOW_ERRNO, "%s: Error while writing"
                   " video frame");
        ffmpeg_cleanups(ffmpeg);
        return -1;
    }

    return ret;
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

    picture = avcodec_alloc_frame();

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
 * ffmpeg_deinterlace
 *      Make the image suitable for deinterlacing using ffmpeg, then deinterlace the picture.
 *
 * Parameters
 *      img     image in YUV420P format
 *      width   image width in pixels
 *      height  image height in pixels
 *
 * Returns
 *      Function returns nothing.
 *      img     contains deinterlaced image
 */
void ffmpeg_deinterlace(unsigned char *img, int width, int height)
{
    AVPicture picture;
    int width2 = width / 2;

    picture.data[0] = img;
    picture.data[1] = img + width * height;
    picture.data[2] = picture.data[1] + (width * height) / 4;
    picture.linesize[0] = width;
    picture.linesize[1] = width2;
    picture.linesize[2] = width2;

    /* We assume using 'PIX_FMT_YUV420P' always */
    avpicture_deinterlace(&picture, &picture, PIX_FMT_YUV420P, width, height);

#ifndef __SSE_MATH__
    __asm__ __volatile__ ( "emms");
#endif

    return;
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
