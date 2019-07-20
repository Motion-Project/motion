AC_INIT(motion, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/motion.c])

AC_USE_SYSTEM_EXTENSIONS

################################################################################
# Configurations for external software support
################################################################################
AC_ARG_WITH([bktr],
	[AS_HELP_STRING([--without-bktr],
		[Compile without Brooktree (bktr) device support])],
	[], [with_bktr=check])

AC_ARG_WITH([ffmpeg],
	[AS_HELP_STRING([--without-ffmpeg],
		[Compile without FFmpeg support])],
	[], [with_ffmpeg=check])

AC_ARG_WITH([mmal],
	[AS_HELP_STRING([--without-mmal],
		[Compile without Multi-Media Abstraction Layer (MMAL) support])],
	[], [with_mmal=check])

AC_ARG_WITH([sqlite3],
	[AS_HELP_STRING([--without-sqlite3],
		[Compile without SQLite version 3 support])],
	[], [with_sqlite3=check])

AC_ARG_WITH([v4l2],
	[AS_HELP_STRING([--without-v4l2],
		[Compile without Video4Linux2 (V4L2) support])],
	[], [with_v4l2=check])

AC_ARG_WITH([webp],
	[AS_HELP_STRING([--without-webp],
		[Compile without WebP image support])],
	[], [with_webp=check])

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

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AS_IF([test "x$with_ffmpeg" != xno], [
	AC_LIB_HAVE_LINKFLAGS([avdevice], [avcodec avformat avutil swscale])
	LIBS="$LIBAVDEVICE $LIBS"

	AC_SEARCH_LIBS([avcodec_find_decoder], [avcodec], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])])
	AC_SEARCH_LIBS([avdevice_register_all], [avdevice], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])])
	AC_SEARCH_LIBS([avformat_alloc_context], [avformat], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])])
	AC_SEARCH_LIBS([av_image_fill_arrays], [avutil], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])])
	AC_SEARCH_LIBS([sws_scale], [swscale], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])])
])

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

AS_IF([test "x$with_sqlite3" != xno], [
	AC_LIB_HAVE_LINKFLAGS([sqlite3])
	LIBS="$LIBSQLITE3 $LIBS"

	AC_SEARCH_LIBS([sqlite3_open], [sqlite3], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_sqlite3" = xcheck], [with_sqlite3=no])])
])

AS_IF([test "x$with_webp" != xno], [
	AC_LIB_HAVE_LINKFLAGS([webp])
	LIBS="$LIBWEBP $LIBS"

	AC_SEARCH_LIBS([WebPEncode], [webp], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_webp" = xcheck], [with_webp=no])])
	AC_SEARCH_LIBS([WebPMuxAssemble], [webpmux], [LIBS=$ac_func_search_save_LIBS],
		[AS_IF([test "x$with_webp" = xcheck], [with_webp=no])])
])

################################################################################
# Checks for header files
################################################################################
AC_CHECK_HEADERS([jerror.h jpeglib.h], [], [jpeg_headers=no])

AC_CHECK_HEADERS([microhttpd.h pthread_np.h])

AS_IF([test "x$with_bktr" != xno], [
	AC_CHECK_HEADERS([dnl
		dev/bktr/ioctl_meteor.h dnl
		dev/bktr/ioctl_bt848.h dnl
	], [], [
		AC_CHECK_HEADER([dev/ic/bt8xx.h], [], [
			bktr_headers=no
			AS_IF([test "x$with_bktr" = xcheck], [with_bktr=no])
		])
	])
])

AS_IF([test "x$with_ffmpeg" != xno], [
	AC_CHECK_HEADERS([dnl
		libavcodec/avcodec.h dnl
		libavdevice/avdevice.h dnl
		libavformat/avformat.h dnl
		libavutil/avutil.h dnl
		libavutil/imgutils.h dnl
		libavutil/mathematics.h dnl
		libswscale/swscale.h dnl
	], [], [
		ffmpeg_headers=no
		AS_IF([test "x$with_ffmpeg" = xcheck], [with_ffmpeg=no])
	])
])

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

AS_IF([test "x$with_sqlite3" != xno], [
	AC_CHECK_HEADER([sqlite3.h], [],
		[AS_IF([test "x$with_sqlite3" = xcheck], [with_sqlite3=no])])
])

AS_IF([test "x$with_v4l2" != xno], [
	AC_CHECK_HEADER([linux/videodev2.h], [],
		[AS_IF([test "x$with_v4l2" = xcheck], [with_v4l2=no])])
])

AS_IF([test "x$with_webp" != xno], [
	AC_CHECK_HEADERS([dnl
		webp/encode.h dnl
		webp/mux.h dnl
	], [], [
		webp_headers=no
		AS_IF([test "x$with_webp" = xcheck], [with_webp=no])
	])
])

################################################################################
# Checks for library functions
################################################################################
AC_FUNC_STRERROR_R

AC_CHECK_FUNCS_ONCE([memmem])
AC_CHECK_FUNCS([pthread_getname_np pthread_setname_np])

################################################################################
# Reconcile configurations and checks
################################################################################
AS_IF([test "x$ac_cv_func_strerror_r" != xyes],
	[AC_MSG_ERROR([strerror_r function not found])])

AS_IF([test "x$ac_cv_func_memmem" != xyes],
	[AC_MSG_ERROR([memmem function not found])])

AS_IF([test "x$ac_cv_search_jpeg_std_error" = xno],
	[AC_MSG_ERROR([libjpeg library not found])])
AS_IF([test "x$jpeg_headers" = xno],
	[AC_MSG_ERROR([libjpeg header file not found])])

AS_IF([test "x$ac_cv_search_MHD_start_daemon" = xno],
	[AC_MSG_ERROR([microhttpd library not found])])
AS_IF([test "x$ac_cv_header_microhttpd_h" != xyes],
	[AC_MSG_ERROR([microhttpd header file not found])])

AS_IF([test "x$ax_pthread_ok" = xno],
	[AC_MSG_ERROR([pthreads not found])])

AS_IF([test "x$with_bktr" != xno], [
	AS_IF([test "x$bktr_headers" = xno],
		[AC_MSG_ERROR([Brooktree (bktr) header files not found])])

	AC_DEFINE([HAVE_BKTR], 1,
		[Define to build with Brooktree (bktr) device support])

	with_bktr=yes
])

AS_IF([test "x$with_ffmpeg" != xno], [
	AS_IF([test "x$ac_cv_search_avcodec_find_decoder" = xno],
		[AC_MSG_ERROR([avcodec library not found])])
	AS_IF([test "x$ac_cv_search_avdevice_register_all" = xno],
		[AC_MSG_ERROR([avdevice library not found])])
	AS_IF([test "x$ac_cv_search_avformat_alloc_context" = xno],
		[AC_MSG_ERROR([avformat library not found])])
	AS_IF([test "x$ac_cv_search_av_image_fill_arrays" = xno],
		[AC_MSG_ERROR([avutil library not found])])
	AS_IF([test "x$ac_cv_search_sws_scale" = xno],
		[AC_MSG_ERROR([swscale library not found])])
	AS_IF([test "x$ffmpeg_headers" = xno],
		[AC_MSG_ERROR([FFmpeg header files not found])])

	AS_IF([test "x$ac_cv_search_avcodec_find_decoder" != "xnone required"],
		[LIBS="$ac_cv_search_avcodec_find_decoder $LIBS"])
	AS_IF([test "x$ac_cv_search_avdevice_register_all" != "xnone required"],
		[LIBS="$ac_cv_search_avdevice_register_all $LIBS"])
	AS_IF([test "x$ac_cv_search_avformat_alloc_context" != "xnone required"],
		[LIBS="$ac_cv_search_avformat_alloc_context $LIBS"])
	AS_IF([test "x$ac_cv_search_av_image_fill_arrays" != "xnone required"],
		[LIBS="$ac_cv_search_av_image_fill_arrays $LIBS"])
	AS_IF([test "x$ac_cv_search_sws_scale" != "xnone required"],
		[LIBS="$ac_cv_search_sws_scale $LIBS"])

	AC_DEFINE([HAVE_FFMPEG], 1,
		[Define to build with FFmpeg support])

	with_ffmpeg=yes
])

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

AS_IF([test "x$with_sqlite3" != xno], [
	AS_IF([test "x$ac_cv_header_sqlite3_h" != xyes],
		[AC_MSG_ERROR([SQLite version 3 header file not found])])

	AS_IF([test "x$ac_cv_search_sqlite3_open" = xno],
		[AC_MSG_ERROR([SQLite version 3 library not found])])

	AS_IF([test "x$ac_cv_search_sqlite3_open" != "xnone required"],
		[LIBS="$ac_cv_search_sqlite3_open $LIBS"])

	AC_DEFINE([HAVE_SQLITE3], 1,
		[Define to build with SQLite version 3 support])

	with_sqlite3=yes
])

AS_IF([test "x$with_v4l2" != xno], [
	AS_IF([test "x$ac_cv_header_linux_videodev2_h" != xyes],
		[AC_MSG_ERROR([Video4Linux2 (V4L2) header file not found])])

	AC_DEFINE([HAVE_V4L2], 1,
		[Define to build with Video4Linux2 (V4L2) support])

	with_v4l2=yes
])
AM_CONDITIONAL([V4L2], [test "x$with_v4l2" != xno])

AS_IF([test "x$with_webp" != xno], [
	AS_IF([test "x$ac_cv_search_WebPEncode" = xno],
		[AC_MSG_ERROR([webp library not found])])
	AS_IF([test "x$ac_cv_search_WebPMuxAssemble" = xno],
		[AC_MSG_ERROR([webpmux library not found])])
	AS_IF([test "x$webp_headers" = xno],
		[AC_MSG_ERROR([WebP header files not found])])

	AS_IF([test "x$ac_cv_search_WebPEncode" != "xnone required"],
		[LIBS="$ac_cv_search_WebPEncode $LIBS"])
	AS_IF([test "x$ac_cv_search_WebPMuxAssemble" != "xnone required"],
		[LIBS="$ac_cv_search_WebPMuxAssemble $LIBS"])

	AC_DEFINE([HAVE_WEBP], 1,
		[Define to build with WebP image support])

	with_webp=yes
])

################################################################################
# Report configuration results
################################################################################
AS_ECHO(["
CONFIGURATION RESULTS:
----------------------
  Brooktree (bktr) device support: $with_bktr
  FFmpeg support: $with_ffmpeg
  Multi-Media Abstraction Layer (MMAL) support: $with_mmal
  SQLite version 3 support: $with_sqlite3
  Video4Linux2 (V4L2) support: $with_v4l2
  WebP image support: $with_webp
"])

AC_CONFIG_FILES([
	Makefile
	data/Makefile
	src/Makefile
])
AC_OUTPUT
