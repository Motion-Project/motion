AC_INIT(motion, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/motion.c])

AC_USE_SYSTEM_EXTENSIONS

################################################################################
# Checks for programs
################################################################################
AC_PROG_CC

AC_CONFIG_FILES([
	Makefile
	data/Makefile
	src/Makefile
])
AC_OUTPUT
