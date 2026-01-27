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


ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src po

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
libstatedir = @localstatedir@/lib/@PACKAGE@
libwebdir = @localstatedir@/lib/@PACKAGE@/webcontrol
webuiinstalldir = @localstatedir@/lib/@PACKAGE@/webui
scriptsdir = $(srcdir)/scripts

dist_libstate_DATA = \
	data/motion-dist.conf \
	data/camera1-dist.conf \
	data/camera2-dist.conf \
	data/camera3-dist.conf \
	data/sound1-dist.conf

dist_libweb_DATA = \
	data/webcontrol/samplepage.html

dist_man_MANS = man/motion.1

dist_doc_DATA = \
	doc/motion_guide.html \
	doc/motion_stylesheet.css \
	doc/motion_build.html \
	doc/motion_config.html \
	doc/motion_examples.html \
	doc/motion.gif \
	doc/motion.png

###################################################################
## Clean tilde crumbs and add prefix in config file.
###################################################################
all-local:
	@rm -f po/*.po\~
	@sed -e 's|$${prefix}|$(prefix)|' data/motion-dist.conf > data/motion-dist.conf.tmp && mv -f data/motion-dist.conf.tmp data/motion-dist.conf

###################################################################
## Install React webui files (handles dynamic asset filenames)
###################################################################
install-data-local:
	@echo "Installing React webui..."
	$(MKDIR_P) $(DESTDIR)$(webuiinstalldir)/assets
	$(INSTALL_DATA) data/webui/index.html $(DESTDIR)$(webuiinstalldir)/
	$(INSTALL_DATA) data/webui/vite.svg $(DESTDIR)$(webuiinstalldir)/ 2>/dev/null || true
	@for f in data/webui/assets/*; do \
		if [ -f "$$f" ]; then \
			$(INSTALL_DATA) "$$f" $(DESTDIR)$(webuiinstalldir)/assets/; \
		fi; \
	done

uninstall-local:
	rm -rf $(DESTDIR)$(webuiinstalldir)

###################################################################
## Systemd service installation
###################################################################
SYSTEMD_UNIT_DIR = $(shell pkg-config --variable=systemdsystemunitdir systemd 2>/dev/null || echo "/etc/systemd/system")

install-service: data/motion-dist.service
	$(MKDIR_P) $(DESTDIR)$(SYSTEMD_UNIT_DIR)
	@sed -e 's|$${exec_prefix}|$(exec_prefix)|g' \
	     -e 's|$${prefix}|$(prefix)|g' \
	     data/motion-dist.service > $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service.tmp
	$(INSTALL_DATA) $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service.tmp $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service
	@rm -f $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service.tmp
	@echo ""
	@echo "Service installed to $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service"
	@if [ -z "$(DESTDIR)" ] && command -v systemctl >/dev/null 2>&1; then \
		echo "Reloading systemd and enabling service..."; \
		systemctl daemon-reload; \
		systemctl enable motion; \
		echo "Service enabled. Start with: sudo systemctl start motion"; \
	else \
		echo ""; \
		echo "To enable and start:"; \
		echo "  sudo systemctl daemon-reload"; \
		echo "  sudo systemctl enable motion"; \
		echo "  sudo systemctl start motion"; \
	fi
	@echo ""

uninstall-service:
	rm -f $(DESTDIR)$(SYSTEMD_UNIT_DIR)/motion.service
	@echo "Service removed. Run 'sudo systemctl daemon-reload' to complete uninstall"

setup-service: install-service
	@echo ""
	@echo "Running Motion service setup..."
	@echo ""
	@if [ ! -x $(scriptsdir)/setup-motion-service.sh ]; then \
		echo "Error: Setup script not found or not executable"; \
		echo "Expected: $(scriptsdir)/setup-motion-service.sh"; \
		exit 1; \
	fi
	@if [ `id -u` -ne 0 ]; then \
		echo "Error: This target must be run with sudo"; \
		echo "Usage: sudo make setup-service"; \
		exit 1; \
	fi
	@echo "Usage: $(scriptsdir)/setup-motion-service.sh --admin-pass PASSWORD --viewer-pass PASSWORD"
	@echo ""
	@echo "Run the setup script manually with your desired passwords."

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.hpp.in config.hpp.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f po/en@boldquot.header po/en@quot.header po/insert-header.sin
	@rm -f po/Makevars.template po/quot.sed po/remove-potcdate.sin
	@rm -f po/Rules-quot po/stamp-po po/*.gmo po/motion.pot po/boldquot.sed
	@rm -f Makefile.in src/Makefile.in po/Makefile.in.in man/Makefile.in
	@rm -f data/motion-dist.service data/motion-dist.conf
	@rm -f data/camera1-dist.conf data/camera2-dist.conf
	@rm -f data/camera3-dist.conf data/sound1-dist.conf
	@rm -f data/webcontrol/samplepage.conf

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

