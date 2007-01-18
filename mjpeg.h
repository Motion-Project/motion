#ifndef _INCLUDE_MJPEG_H_
#define _INCLUDE_MJPEG_H_

#ifdef HAVE_FFMPEG
#include <avcodec.h>
#endif

struct mjpeg {
#ifdef HAVE_FFMPEG
	AVFrame *pictureIn;	/* contains default image pointers */
	AVCodec *mjpegDecoder;
	AVCodecContext *mjpegDecContext;
#else
        int dummy;
#endif
};


struct mjpeg * MJPEGStartDecoder(unsigned int width, unsigned int height);
unsigned char * MJPEGDecodeFrame(unsigned char *mjpegFrame, int mjpegFrameLen, unsigned char *outbuf, int outbufSize, struct mjpeg *MJPEG_ST);
void MJPEGStopDecoder(struct mjpeg *MJPEG_ST);
int getPixFmt(void);

#endif
