AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/motion\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

if INC_MMAL_SRC
MMAL_SRC=raspicam/RaspiCamControl.c raspicam/RaspiCLI.c
endif

LIBS = @LIBINTL@ @LIBS@

bin_PROGRAMS = motion

motion_SOURCES = motion.cpp motion_loop.cpp logger.cpp conf.cpp conf_edit.cpp util.cpp alg.cpp \
	video_v4l2.cpp video_common.cpp video_loopback.cpp netcam.cpp jpegutils.cpp exif.cpp \
	rotate.cpp draw.cpp event.cpp movie.cpp  picture.cpp dbse.cpp track.cpp \
	webu.cpp webu_html.cpp webu_stream.cpp webu_text.cpp mmalcam.cpp $(MMAL_SRC)

