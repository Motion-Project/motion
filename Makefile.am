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

libstatedir = @localstatedir@/lib/@PACKAGE@

dist_libstate_DATA = \
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
## Clean tilde crumbs and add prefix in config file.
###################################################################
all-local:
	@rm -f po/*.po\~
	@sed -e 's|$${prefix}|$(prefix)|' data/motionplus-dist.conf > data/motionplus-dist.conf.tmp && mv -f data/motionplus-dist.conf.tmp data/motionplus-dist.conf

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
	@rm -f data/motionplus-dist.service data/motionplus-dist.conf
	@rm -f data/camera1-dist.conf data/camera2-dist.conf
	@rm -f data/camera3-dist.conf data/sound1-dist.conf

###################################################################
## Testing options for maintainer
###################################################################
maintainer-clang:
	./configure --with-clang-flags CC=clang CXX=clang++ LD=clang++  && $(MAKE) clean && $(MAKE)

maintainer-check:
	./configure --with-developer-flags                   && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-libcam  && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-opencv  && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-mysql   && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-sqlite3 && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-pgsql   && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-v4l2    && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-webp    && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-alsa    && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-pulse   && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags --without-fftw3   && $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		&& $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		--without-sqlite3 \
		--without-pgsql \
		&& $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags \
		--without-alsa \
		--without-pulse \
		&& $(MAKE) clean && $(MAKE) -j6
	./configure --with-developer-flags \
		--without-alsa \
		--without-pulse \
		--without-fftw3 \
		&& $(MAKE) clean && $(MAKE) -j6

