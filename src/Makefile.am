AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/motion\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

if INC_MMAL_SRC
MMAL_SRC=raspicam/RaspiCamControl.c raspicam/RaspiCLI.c
endif

LIBS = @LIBINTL@ @LIBS@

bin_PROGRAMS = motion

motion_SOURCES = motion.cpp logger.cpp conf.cpp draw.cpp jpegutils.cpp video_loopback.cpp \
	video_v4l2.cpp video_common.cpp dbse.cpp rotate.cpp movie.cpp exif.cpp util.cpp \
	netcam.cpp track.cpp alg.cpp event.cpp picture.cpp motion_loop.cpp \
	webu.cpp webu_html.cpp webu_stream.cpp webu_text.cpp mmalcam.cpp $(MMAL_SRC)

