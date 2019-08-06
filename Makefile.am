SUBDIRS = data po src

TESTS = \
	tests/developer-flags \
	tests/without-ffmpeg \
	tests/without-mariadb \
	tests/without-mysql \
	tests/without-pgsql \
	tests/without-sql \
	tests/without-sqlite3 \
	tests/without-v4l2 \
	tests/without-webp

EXTRA_DIST = CREDITS $(TESTS)

dist_html_DATA = \
	doc/mask1.png \
	doc/motion_build.html \
	doc/motion_config.html \
	doc/motion_guide.html \
	doc/motion_stylesheet.css \
	doc/normal.jpg \
	doc/outputmotion1.jpg \
	doc/outputnormal1.jpg

dist_man_MANS = man/motion.1
