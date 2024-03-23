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
# */


ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
dist_pkgsysconf_DATA = \
	data/motionplus-dist.conf \
	data/camera1-dist.conf \
	data/camera2-dist.conf \
	data/camera3-dist.conf \
	data/sound1-dist.conf

dist_man_MANS = man/motionplus.1

dist_doc_DATA = \
	doc/motionplus_guide.html \
	doc/motionplus_stylesheet.css \
	doc/motionplus_build.html \
	doc/motionplus_config.html

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.hpp.in config.hpp.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f po/en@boldquot.header po/en@quot.header po/insert-header.sin
	@rm -f po/Makevars.template po/quot.sed po/remove-potcdate.sin
	@rm -f po/Rules-quot po/stamp-po po/*.gmo po/motionplus.pot po/boldquot.sed
	@rm -f Makefile.in src/Makefile.in po/Makefile.in.in man/Makefile.in
	@rm -f data/motionplus.service data/motionplus-dist.conf
	@rm -f data/camera1-dist.conf data/camera2-dist.conf
	@rm -f data/camera3-dist.conf data/sound1-dist.conf

###################################################################
## Clean out the tilde crumb files left by gettext autotool processing
###################################################################
all-local:
	@rm -f po/*.po\~

maintainer-check:
	./configure --with-developer-flags                   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-opencv  && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mysql   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-sqlite3 && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-pgsql   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-v4l2    && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-webp    && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-alsa    && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-fftw3   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		&& $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		--without-sqlite3 \
		--without-pgsql \
		&& $(MAKE) clean && $(MAKE)
