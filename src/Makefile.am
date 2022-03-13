#/*
# *    This file is part of MotionPlus.
# *
# *    MotionPlus is free software: you can redistribute it and/or modify
# *    it under the terms of the GNU General Public License as published by
# *    the Free Software Foundation, either version 3 of the License, or
# *    (at your option) any later version.
# *
# *    MotionPlus is distributed in the hope that it will be useful,
# *    but WITHOUT ANY WARRANTY; without even the implied warranty of
# *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *    GNU General Public License for more details.
# *
# *    You should have received a copy of the GNU General Public License
# *    along with MotionPlus.  If not, see <https://www.gnu.org/licenses/>.
# *
# *    Copyright 2020-2022 MotionMrDave@gmail.com
# */


AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/motionplus\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

if INC_MMAL_SRC
MMAL_SRC=raspicam/RaspiCamControl.c raspicam/RaspiCLI.c
endif

LIBS = @LIBINTL@ @LIBS@

bin_PROGRAMS = motionplus

motionplus_SOURCES = motionplus.cpp motion_loop.cpp logger.cpp conf.cpp util.cpp alg.cpp alg_sec.cpp\
	video_v4l2.cpp video_common.cpp video_loopback.cpp netcam.cpp jpegutils.cpp exif.cpp \
	rotate.cpp draw.cpp event.cpp movie.cpp  picture.cpp dbse.cpp \
	webu.cpp webu_html.cpp webu_stream.cpp webu_json.cpp webu_post.cpp \
	mmalcam.cpp $(MMAL_SRC)

