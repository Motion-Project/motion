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
# *    Copyright 2020-2023 MotionMrDave@gmail.com
# */


AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/motionplus\" -DLOCALEDIR=\"$(localedir)\"

LDADD = $(LIBINTL)

bin_PROGRAMS = motionplus

motionplus_SOURCES = \
	alg.hpp            alg.cpp \
	alg_sec.hpp        alg_sec.cpp \
	conf.hpp           conf.cpp \
	dbse.hpp           dbse.cpp \
	draw.hpp           draw.cpp \
	event.hpp          event.cpp \
	exif.hpp           exif.cpp \
	jpegutils.hpp      jpegutils.cpp \
	libcam.hpp         libcam.cpp \
	logger.hpp         logger.cpp \
	motionplus.hpp     motionplus.cpp \
	motion_loop.hpp    motion_loop.cpp \
	movie.hpp          movie.cpp \
	netcam.hpp         netcam.cpp \
	picture.hpp        picture.cpp \
	rotate.hpp         rotate.cpp \
	sound.hpp          sound.cpp \
	util.hpp           util.cpp \
	video_v4l2.hpp     video_v4l2.cpp \
	video_common.hpp   video_common.cpp \
	video_loopback.hpp video_loopback.cpp \
	webu.hpp           webu.cpp \
	webu_file.hpp      webu_file.cpp \
	webu_html.hpp      webu_html.cpp \
	webu_json.hpp      webu_json.cpp \
	webu_post.hpp      webu_post.cpp \
	webu_stream.hpp    webu_stream.cpp \
	webu_mpegts.hpp    webu_mpegts.cpp

