<<<<<<< HEAD
=======
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


>>>>>>> 5.0
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
<<<<<<< HEAD
dist_pkgsysconf_DATA = \
=======
libstatedir = @localstatedir@/lib/@PACKAGE@

dist_libstate_DATA = \
>>>>>>> 5.0
	data/motion-dist.conf \
	data/camera1-dist.conf \
	data/camera2-dist.conf \
	data/camera3-dist.conf \
<<<<<<< HEAD
	data/camera4-dist.conf
=======
	data/sound1-dist.conf
>>>>>>> 5.0

dist_man_MANS = man/motion.1

dist_doc_DATA = \
<<<<<<< HEAD
	doc/mask1.png \
	doc/normal.jpg \
	doc/outputmotion1.jpg  \
	doc/outputnormal1.jpg \
	doc/motion_guide.html \
	doc/motion_stylesheet.css \
	doc/CREDITS \
=======
	doc/motion_guide.html \
	doc/motion_stylesheet.css \
>>>>>>> 5.0
	doc/motion_build.html \
	doc/motion_config.html

###################################################################
<<<<<<< HEAD
=======
## Clean tilde crumbs and add prefix in config file.
###################################################################
all-local:
	@rm -f po/*.po\~
	@sed -e 's|$${prefix}|$(prefix)|' data/motion-dist.conf > data/motion-dist.conf.tmp && mv -f data/motion-dist.conf.tmp data/motion-dist.conf

###################################################################
>>>>>>> 5.0
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
<<<<<<< HEAD
	@rm -f config.h.in config.h.in~ aclocal.m4 config.sub ABOUT-NLS missing
=======
	@rm -f config.hpp.in config.hpp.in~ aclocal.m4 config.sub ABOUT-NLS missing
>>>>>>> 5.0
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f po/en@boldquot.header po/en@quot.header po/insert-header.sin
	@rm -f po/Makevars.template po/quot.sed po/remove-potcdate.sin
	@rm -f po/Rules-quot po/stamp-po po/*.gmo po/motion.pot po/boldquot.sed
	@rm -f Makefile.in src/Makefile.in po/Makefile.in.in man/Makefile.in
<<<<<<< HEAD
	@rm -f data/motion.service data/motion-dist.conf
	@rm -f data/camera1-dist.conf data/camera2-dist.conf
	@rm -f data/camera3-dist.conf data/camera4-dist.conf

###################################################################
## Clean out the tilde crumb files left by gettext autotool processing
###################################################################
all-local:
	@rm -f po/*.po\~

maintainer-check:
	./configure --with-prototype-flags                   && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-ffmpeg  && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-mysql   && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-sqlite3 && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-pgsql   && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-v4l2    && $(MAKE) clean && $(MAKE)
	./configure --with-prototype-flags --without-webp    && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		&& $(MAKE) clean && $(MAKE)
=======
	@rm -f data/motion-dist.service data/motion-dist.conf
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
>>>>>>> 5.0
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		--without-sqlite3 \
		--without-pgsql \
<<<<<<< HEAD
		&& $(MAKE) clean && $(MAKE)
=======
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

>>>>>>> 5.0
