AC_INIT(motion, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/motion.c])

AC_USE_SYSTEM_EXTENSIONS

################################################################################
# Configurations for external software support
################################################################################
AC_ARG_WITH([mmal],
	[AS_HELP_STRING([--without-mmal],
		[Compile without Multi-Media Abstraction Layer (MMAL) support])],
	[], [with_mmal=check])

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

AS_IF([test "x$with_mmal" != xno], [
	AC_LIB_HAVE_LINKFLAGS([mmal_core], [mmal_util])
	LIBS="$LIBMMAL_CORE $LIBS"

	AC_SEARCH_LIBS([mmal_component_create], [mmal_core], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])])
	AC_SEARCH_LIBS([mmal_port_parameter_set_boolean], [mmal_util], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])])

	AC_LIB_HAVE_LINKFLAGS([vcos], [vchostif])
	LIBS="$LIBVCOS $LIBS"

	AC_SEARCH_LIBS([vcos_assert], [vcos], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])])
	AC_SEARCH_LIBS([vc_gencmd], [vchostif], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])])
])

################################################################################
# Checks for header files
################################################################################
AC_CHECK_HEADERS([jerror.h jpeglib.h], [], [jpeg_headers=no])

AC_CHECK_HEADER([microhttpd.h])

AS_IF([test "x$with_mmal" != xno], [
	AC_CHECK_HEADERS([dnl
		interface/mmal/mmal.h dnl
		interface/mmal/mmal_buffer.h dnl
		interface/mmal/mmal_port.h dnl
		interface/mmal/util/mmal_connection.h dnl
		interface/mmal/util/mmal_default_components.h dnl
		interface/mmal/util/mmal_util.h dnl
		interface/mmal/util/mmal_util_params.h dnl
	], [], [
		mmal_headers=no
		AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])
	])

	AC_CHECK_HEADERS([dnl
		interface/vcos/vcos.h dnl
		interface/vmcs_host/vc_vchi_gencmd.h dnl
	], [], [
		videocore_headers=no
		AS_IF([test "x$with_mmal" = xcheck], [with_mmal=no])
	])
])

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

AS_IF([test "x$with_mmal" != xno], [
	AS_IF([test "x$ac_cv_search_mmal_component_create" = xno],
		[AC_MSG_ERROR([mmal_core library not found])])
	AS_IF([test "x$ac_cv_search_mmal_port_parameter_set_boolean" = xno],
		[AC_MSG_ERROR([mmal_util library not found])])
	AS_IF([test "x$ac_cv_search_vcos_assert" = xno],
		[AC_MSG_ERROR([vcos library not found])])
	AS_IF([test "x$ac_cv_search_vc_gencmd" = xno],
		[AC_MSG_ERROR([vchostif library not found])])
	AS_IF([test "x$mmal_headers" = xno],
		[AC_MSG_ERROR([MMAL header files not found])])
	AS_IF([test "x$videocore_headers" = xno],
		[AC_MSG_ERROR([VideoCore header file not found])])

	AS_IF([test "x$ac_cv_search_mmal_component_create" != "xnone required"],
		[LIBS="$ac_cv_search_mmal_component_create $LIBS"])
	AS_IF([test "x$ac_cv_search_mmal_port_parameter_set_boolean" != "xnone required"],
		[LIBS="$ac_cv_search_mmal_port_parameter_set_boolean $LIBS"])
	AS_IF([test "x$ac_cv_search_vcos_assert" != "xnone required"],
		[LIBS="$ac_cv_search_vcos_assert $LIBS"])
	AS_IF([test "x$ac_cv_search_vc_gencmd" != "xnone required"],
		[LIBS="$ac_cv_search_vc_gencmd $LIBS"])

	AC_DEFINE([HAVE_MMAL], 1,
		[Define to build with Multi-Media Abstraction Layer (MMAL) support])

	with_mmal=yes
])
AM_CONDITIONAL([MMAL], [test "x$with_mmal" != xno])

################################################################################
# Report configuration results
################################################################################
AS_ECHO(["
CONFIGURATION RESULTS:
----------------------
  Multi-Media Abstraction Layer (MMAL) support: $with_mmal
"])

AC_CONFIG_FILES([
	Makefile
	data/Makefile
	src/Makefile
])
AC_OUTPUT
