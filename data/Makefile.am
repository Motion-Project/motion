dist_sysconf_DATA = camera1-dist.conf camera2-dist.conf camera3-dist.conf \
	camera4-dist.conf motion-dist.conf motion.service

CLEANFILES = $(dist_sysconf_DATA)

in_files = camera1-dist.conf.in camera2-dist.conf.in camera3-dist.conf.in \
	camera4-dist.conf.in motion-dist.conf.in motion.service.in
do_subst = sed \
	-e 's,[@]PACKAGE_NAME[@],$(PACKAGE_NAME),g' \
	-e 's,[@]PACKAGE_VERSION[@],$(PACKAGE_VERSION),g' \
	-e 's,[@]bindir[@],$(bindir),g' \
	-e 's,[@]prefix[@],$(prefix),g' \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g'

$(dist_sysconf_DATA): $(in_files)
	$(do_subst) < $(srcdir)/$@.in > $@
