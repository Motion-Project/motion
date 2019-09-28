ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po man

sysconfdir = @sysconfdir@/$(PACKAGE)
sysconf_DATA = \
	data/motion-dist.conf \
	data/camera1-dist.conf \
	data/camera2-dist.conf \
	data/camera3-dist.conf \
	data/camera4-dist.conf

man_MANS = man/motion.1

docdir = $(datadir)/doc/@PACKAGE@
doc_DATA = \
	doc/mask1.png \
	doc/normal.jpg \
	doc/outputmotion1.jpg  \
	doc/outputnormal1.jpg \
	doc/motion_guide.html \
	doc/motion_stylesheet.css \
	doc/COPYING \
	doc/CREDITS \
	doc/motion_build.html \
	doc/motion_config.html

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.h.in config.h.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f po/en@boldquot.header po/en@quot.header po/insert-header.sin
	@rm -f po/Makevars.template po/quot.sed po/remove-potcdate.sin
	@rm -f po/Rules-quot po/stamp-po po/*.gmo po/motion.pot po/boldquot.sed
	@rm -f Makefile.in src/Makefile.in po/Makefile.in.in man/Makefile.in
	@rm -f data/motion.service data/motion-dist.conf
	@rm -f data/camera1-dist.conf data/camera2-dist.conf
	@rm -f data/camera3-dist.conf data/camera4-dist.conf

###################################################################
## Clean out the tilde crumb files left by gettext autotool processing
###################################################################
all-local:
	@rm -f po/*.po\~

check:
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
	./configure --with-developer-flags \
		--without-mysql \
		--without-mariadb \
		--without-sqlite3 \
		--without-pgsql \
		&& $(MAKE) clean && $(MAKE)
