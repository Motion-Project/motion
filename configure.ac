AC_INIT(motion, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/motion.c])

AC_USE_SYSTEM_EXTENSIONS

################################################################################
# Checks for programs
################################################################################
AC_PROG_CC

################################################################################
# Checks for libraries
################################################################################
AC_LIB_HAVE_LINKFLAGS([jpeg])
LIBS="$LIBJPEG $LIBS"
AC_SEARCH_LIBS([jpeg_std_error], [jpeg])

AC_LIB_HAVE_LINKFLAGS([microhttpd])
LIBS="$LIBMICROHTTPD $LIBS"
AC_SEARCH_LIBS([MHD_start_daemon], [microhttpd])

################################################################################
# Checks for header files
################################################################################
AC_CHECK_HEADERS([jerror.h jpeglib.h], [], [jpeg_headers=no])

AC_CHECK_HEADER([microhttpd.h])

################################################################################
# Reconcile configurations and checks
################################################################################
AS_IF([test "x$ac_cv_search_jpeg_std_error" = xno],
	[AC_MSG_ERROR([libjpeg library not found])])
AS_IF([test "x$jpeg_headers" = xno],
	[AC_MSG_ERROR([libjpeg header file not found])])

AS_IF([test "x$ac_cv_search_MHD_start_daemon" = xno],
	[AC_MSG_ERROR([microhttpd library not found])])
AS_IF([test "x$ac_cv_header_microhttpd_h" != xyes],
	[AC_MSG_ERROR([microhttpd header file not found])])

AC_CONFIG_FILES([
	Makefile
	data/Makefile
	src/Makefile
])
AC_OUTPUT
