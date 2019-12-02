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

###################################################################
## Update contents of sample files with the correct directory
###################################################################
install-data-local:
	@sed -e 's|$${prefix}|$(prefix)|' ${DESTDIR}${sysconfdir}/motion-dist.conf > ${DESTDIR}${sysconfdir}/motion-dist.conf.tmp && mv -f ${DESTDIR}${sysconfdir}/motion-dist.conf.tmp ${DESTDIR}${sysconfdir}/motion-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' ${DESTDIR}${sysconfdir}/camera1-dist.conf > ${DESTDIR}${sysconfdir}/camera1-dist.conf.tmp && mv -f ${DESTDIR}${sysconfdir}/camera1-dist.conf.tmp ${DESTDIR}${sysconfdir}/camera1-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' ${DESTDIR}${sysconfdir}/camera2-dist.conf > ${DESTDIR}${sysconfdir}/camera2-dist.conf.tmp && mv -f ${DESTDIR}${sysconfdir}/camera2-dist.conf.tmp ${DESTDIR}${sysconfdir}/camera2-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' ${DESTDIR}${sysconfdir}/camera3-dist.conf > ${DESTDIR}${sysconfdir}/camera3-dist.conf.tmp && mv -f ${DESTDIR}${sysconfdir}/camera3-dist.conf.tmp ${DESTDIR}${sysconfdir}/camera3-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' ${DESTDIR}${sysconfdir}/camera4-dist.conf > ${DESTDIR}${sysconfdir}/camera4-dist.conf.tmp && mv -f ${DESTDIR}${sysconfdir}/camera4-dist.conf.tmp ${DESTDIR}${sysconfdir}/camera4-dist.conf

	@sed -e 's|$${exec_prefix}|$(exec_prefix)|' data/motion.service > data/motion.service.tmp && mv -f data/motion.service.tmp data/motion.service

	@sed -e 's|$${prefix}|$(prefix)|' data/motion-dist.conf > data/motion-dist.conf.tmp && mv -f data/motion-dist.conf.tmp data/motion-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' data/camera1-dist.conf > data/camera1-dist.conf.tmp && mv -f data/camera1-dist.conf.tmp data/camera1-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' data/camera2-dist.conf > data/camera2-dist.conf.tmp && mv -f data/camera2-dist.conf.tmp data/camera2-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' data/camera3-dist.conf > data/camera3-dist.conf.tmp && mv -f data/camera3-dist.conf.tmp data/camera3-dist.conf
	@sed -e 's|$${prefix}|$(prefix)|' data/camera4-dist.conf > data/camera4-dist.conf.tmp && mv -f data/camera4-dist.conf.tmp data/camera4-dist.conf

check:
	./configure --with-developer-flags                   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mysql   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-mariadb && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-sqlite3 && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-pgsql   && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-v4l2    && $(MAKE) clean && $(MAKE)
	./configure --with-developer-flags --without-webp    && $(MAKE) clean && $(MAKE)
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
