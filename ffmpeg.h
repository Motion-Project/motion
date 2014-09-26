#ifndef _INCLUDE_FFMPEG_H_
#define _INCLUDE_FFMPEG_H_

#ifdef HAVE_FFMPEG

#include <errno.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdarg.h>

#endif /* HAVE_FFMPEG */

#define TIMELAPSE_CODEC "mpeg1_tl"

struct ffmpeg {
#ifdef HAVE_FFMPEG
    AVFormatContext *oc;
    AVStream *video_st;
    AVCodecContext *c;

    AVFrame *picture;       /* contains default image pointers */
    uint8_t *video_outbuf;
    int video_outbuf_size;

    void *udata;            /* U & V planes for greyscale images */
    int vbr;                /* variable bitrate setting */
    char codec[20];         /* codec name */
#else
    int dummy;
#endif
};

/* Initialize FFmpeg stuff. Needs to be called before ffmpeg_open. */
void ffmpeg_init(void);

struct ffmpeg *ffmpeg_open(
    char *ffmpeg_video_codec,
    char *filename,
    unsigned char *y,    /* YUV420 Y plane */
    unsigned char *u,    /* YUV420 U plane */
    unsigned char *v,    /* YUV420 V plane */
    int width,
    int height,
    int rate,            /* framerate, fps */
    int bps,             /* bitrate; bits per second */
    int vbr              /* variable bitrate */
    );

/* Puts the image pointed to by the picture member of struct ffmpeg. */
int ffmpeg_put_image(struct ffmpeg *);

/* Puts the image defined by u, y and v (YUV420 format). */
int ffmpeg_put_other_image(
    struct ffmpeg *ffmpeg,
    unsigned char *y,
    unsigned char *u,
    unsigned char *v
    );

/* Closes the mpeg file. */
void ffmpeg_close(struct ffmpeg *);

/* Setup an avcodec log handler. */
void ffmpeg_avcodec_log(void *, int, const char *, va_list);

#ifdef HAVE_FFMPEG
AVFrame *my_frame_alloc(void);
void my_frame_free(AVFrame *frame);
int ffmpeg_put_frame(struct ffmpeg *, AVFrame *);
void ffmpeg_cleanups(struct ffmpeg *);
AVFrame *ffmpeg_prepare_frame(struct ffmpeg *, unsigned char *,
                              unsigned char *, unsigned char *);
#endif

#endif /* _INCLUDE_FFMPEG_H_ */
