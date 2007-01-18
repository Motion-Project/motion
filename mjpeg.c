#include "mjpeg.h"
#include "motion.h"

#ifdef HAVE_FFMPEG

/* EXPERIMENTAL : some mutex needs to be set around ! */

void MJPEGinit(){
	avcodec_init();
	avcodec_register_all();
}


struct mjpeg * MJPEGStartDecoder(unsigned int width, unsigned int height){

	struct mjpeg *MJPEG_ST;

	MJPEG_ST = mymalloc(sizeof(struct mjpeg));
	memset(MJPEG_ST, 0, sizeof(struct mjpeg));

	MJPEGinit();	
		
	MJPEG_ST->mjpegDecoder = avcodec_find_decoder(CODEC_ID_MJPEG);
	if (!MJPEG_ST->mjpegDecoder){
		motion_log(LOG_ERR,1,"Could not find MJPEG decoder");
		return NULL;
	}

	MJPEG_ST->mjpegDecContext = avcodec_alloc_context();
	MJPEG_ST->pictureIn = avcodec_alloc_frame();

	MJPEG_ST->mjpegDecContext->codec_id = CODEC_ID_MJPEG;
	MJPEG_ST->mjpegDecContext->width = width;
	MJPEG_ST->mjpegDecContext->height = height;

	/* open it */
	if (avcodec_open(MJPEG_ST->mjpegDecContext, MJPEG_ST->mjpegDecoder) < 0){
		motion_log(LOG_ERR,1,"Could not open MJPEG Decoder");
		return NULL;
	}
    
	return MJPEG_ST;
}


int getPixFmt() {
	return 0;
//	return mjpegDecContext->pix_fmt;
}

unsigned char * MJPEGDecodeFrame(unsigned char *mjpegFrame, int mjpegFrameLen, unsigned char *outbuf, 
								int outbufSize, struct mjpeg *MJPEG_ST){
	int got_picture;

	memset(outbuf, 0, outbufSize);

	int len = avcodec_decode_video(MJPEG_ST->mjpegDecContext, MJPEG_ST->pictureIn, &got_picture, 
					(uint8_t *) mjpegFrame, mjpegFrameLen);

	if ((!got_picture) || (len == -1)){
		motion_log(LOG_ERR,1,"mjpeg decoder: expected picture but didn't get it...");
		return NULL;
	}

	// int wrap = pictureIn->linesize[0];
	int xsize = MJPEG_ST->mjpegDecContext->width;
	int ysize = MJPEG_ST->mjpegDecContext->height;
	int pic_size = avpicture_get_size(MJPEG_ST->mjpegDecContext->pix_fmt, xsize, ysize);
	
	if (pic_size != outbufSize) {
		motion_log(LOG_ERR,1,"outbuf size mismatch. pic_size %i  bufsize: %i",pic_size,outbufSize);
		return NULL;
	}
    
	int size = avpicture_layout((AVPicture *)MJPEG_ST->pictureIn, MJPEG_ST->mjpegDecContext->pix_fmt, 
					xsize, ysize, outbuf, outbufSize);
	
	if (size != outbufSize) {
		motion_log(LOG_ERR,1,"mjpeg decoder: avpicture_layout error: size %i",size);
		return NULL;
	}

	return outbuf;
}

void MJPEGStopDecoder(struct mjpeg *MJPEG_ST){
	int got_picture;

	// See if there is a last frame ... needed ?!
	avcodec_decode_video(MJPEG_ST->mjpegDecContext, MJPEG_ST->pictureIn, &got_picture, NULL, 0);

	if (MJPEG_ST->mjpegDecContext){
		avcodec_close(MJPEG_ST->mjpegDecContext);
		av_free(MJPEG_ST->mjpegDecContext);
		MJPEG_ST->mjpegDecContext = 0;
	}

	if (MJPEG_ST->pictureIn)
		av_free(MJPEG_ST->pictureIn);

	MJPEG_ST->pictureIn = 0;
}

#endif
