AM_CPPFLAGS = -Dsysconfdir=\"$(sysconfdir)\"

bin_PROGRAMS = motion

motion_SOURCES = alg.c conf.c draw.c event.c ffmpeg.c logger.c md5.c motion.c \
	picture.c rotate.c stream.c track.c translate.c

if MMAL
motion_SOURCES += mmal/mmalcam.c
motion_SOURCES += \
	mmal/raspicam/RaspiCLI.c \
	mmal/raspicam/RaspiCamControl.c
endif

motion_SOURCES += \
	netcam/netcam.c \
	netcam/netcam_ftp.c \
	netcam/netcam_http.c \
	netcam/netcam_jpeg.c \
	netcam/netcam_rtsp.c \
	netcam/netcam_wget.c

motion_SOURCES += \
	video/jpegutils.c \
	video/video_common.c
motion_SOURCES += video/video_bktr.c
if V4L2
motion_SOURCES += video/video_loopback.c
endif
motion_SOURCES += video/video_v4l2.c

motion_SOURCES += \
	webu/webu.c \
	webu/webu_html.c \
	webu/webu_stream.c \
	webu/webu_text.c
