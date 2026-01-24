#/*
# *    This file is part of Motion.
# *
# *    Motion is free software: you can redistribute it and/or modify
# *    it under the terms of the GNU General Public License as published by
# *    the Free Software Foundation, either version 3 of the License, or
# *    (at your option) any later version.
# *
# *    Motion is distributed in the hope that it will be useful,
# *    but WITHOUT ANY WARRANTY; without even the implied warranty of
# *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *    GNU General Public License for more details.
# *
# *    You should have received a copy of the GNU General Public License
# *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
# *
# */


AM_CPPFLAGS= \
	-Dconfigdir=\"$(localstatedir)/lib/@PACKAGE@\" \
	-Dsysconfdir=\"$(sysconfdir)/motion\" \
	-DLOCALEDIR=\"$(localedir)\"

LDADD = $(LIBINTL)

bin_PROGRAMS = motion motion-setup

motion_SOURCES = \
	alg.hpp            alg.cpp \
	alg_sec.hpp        alg_sec.cpp \
	conf.hpp           conf.cpp \
	conf_file.hpp      conf_file.cpp \
	conf_profile.hpp   conf_profile.cpp \
	dbse.hpp           dbse.cpp \
	draw.hpp           draw.cpp \
	jpegutils.hpp      jpegutils.cpp \
	json_parse.hpp     json_parse.cpp \
	libcam.hpp         libcam.cpp \
	logger.hpp         logger.cpp \
	motion.hpp         motion.cpp \
	allcam.hpp         allcam.cpp \
	schedule.hpp       schedule.cpp \
	camera.hpp         camera.cpp \
	movie.hpp          movie.cpp \
	netcam.hpp         netcam.cpp \
	parm_registry.hpp  parm_registry.cpp \
	parm_structs.hpp \
	picture.hpp        picture.cpp \
	rotate.hpp         rotate.cpp \
	sound.hpp          sound.cpp \
	thumbnail.hpp      thumbnail.cpp \
	util.hpp           util.cpp \
	video_v4l2.hpp     video_v4l2.cpp \
	video_convert.hpp  video_convert.cpp \
	video_loopback.hpp video_loopback.cpp \
	webu.hpp           webu.cpp \
	webu_ans.hpp       webu_ans.cpp \
	webu_auth.hpp      webu_auth.cpp \
	webu_file.hpp      webu_file.cpp \
	webu_json.hpp      webu_json.cpp \
	webu_stream.hpp    webu_stream.cpp \
	webu_getimg.hpp    webu_getimg.cpp \
	webu_mpegts.hpp    webu_mpegts.cpp

motion_setup_SOURCES = \
	motion_setup.cpp \
	webu_auth.hpp      webu_auth.cpp

