AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/motion\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

if INC_MMAL_SRC
MMAL_SRC=raspicam/RaspiCamControl.c raspicam/RaspiCLI.c
endif

LIBS = @LIBINTL@ @LIBS@

bin_PROGRAMS = motion

motion_SOURCES = motion.c logger.c conf.c draw.c jpegutils.c video_loopback.c \
	video_v4l2.c video_common.c video_bktr.c netcam.c netcam_http.c netcam_ftp.c \
	netcam_jpeg.c netcam_wget.c netcam_rtsp.c track.c alg.c event.c picture.c \
	rotate.c translate.c md5.c stream.c ffmpeg.c \
	webu.c webu_html.c webu_stream.c webu_text.c mmalcam.c $(MMAL_SRC)

