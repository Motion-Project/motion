# Process this file with autoconf to produce a configure script

AC_INIT(motion, esyscmd(['./version.sh']))
AC_GNU_SOURCE
AC_CONFIG_SRCDIR([motion.c])
AC_CONFIG_HEADERS(config.h)
AC_PROG_CC
AC_HEADER_STDC
AC_C_CONST


###############################################################################
###  Host system
###############################################################################
AC_MSG_CHECKING(for Darwin/BSD)
DISTRO=""
DISTRO=`uname -a | grep -i "Darwin"`
if test "x${DISTRO}" = "x"; then
  DISTRO=`uname -s | grep -i "FreeBSD"`
fi
if test "x${DISTRO}" = "x"; then
  DISTRO=`uname -s | grep -i "NetBSD"`
fi
if test "x${DISTRO}" = "x"; then
  DISTRO=`uname -s | grep -i "OpenBSD"`
fi
if test "x${DISTRO}" = "x"; then
  DISTRO="Linux"
  AC_MSG_RESULT(no)
else
  AC_MSG_RESULT($DISTRO)
fi
AC_SUBST(DISTRO)

###############################################################################
###  Host specific paths
###############################################################################
TEMP_LIBS=""
TEMP_CFLAGS=""
TEMP_CPPFLAGS=""
TEMP_LDFLAGS=""
if test "${DISTRO}" = "Darwin"; then
  TEMP_CFLAGS="${CFLAGS} -I/sw/include"
  TEMP_CPPFLAGS="${CPPFLAGS} -I/sw/include"
  TEMP_LDFLAGS="${LDFLAGS} -L/sw/lib"
  TEMP_LIBS="-L/sw/lib"
else
  if test "${DISTRO}" != "Linux"; then
    TEMP_CFLAGS="${CFLAGS} -I/usr/local/include"
    TEMP_CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
    TEMP_LDFLAGS="${LDFLAGS} -L/usr/local/lib"
    TEMP_LIBS="-L/usr/local/lib"
  fi
fi
TEMP_LIBS="-lm ${TEMP_LIBS}"
TEMP_CFLAGS="${TEMP_CFLAGS} ${CFLAGS}"
TEMP_CPPFLAGS="${TEMP_CPPFLAGS} ${CPPFLAGS}"
TEMP_LDFLAGS="${TEMP_LDFLAGS} ${LDFLAGS}"

AC_SUBST(CFLAGS, "${TEMP_CFLAGS}")
AC_SUBST(CPPFLAGS, "${TEMP_CPPFLAGS}")
AC_SUBST(LDFLAGS, "${TEMP_LDFLAGS}")

###############################################################################
###  Video System
###############################################################################
BKTR="yes"
AC_ARG_WITH(bktr,
  AS_HELP_STRING([--without-bktr],
    [Exclude to use bktr subsystem for BSD]),
  BKTR="$withval")

V4L2="yes"
AC_ARG_WITH(v4l2,
  AS_HELP_STRING([--without-v4l2],[Disable V4L2 devices]),
  V4L2="$withval")

if test "x${BKTR}" = "xyes"; then
  if test "${DISTRO}" = "FreeBSD"; then
    AC_CHECK_HEADERS(dev/bktr/ioctl_meteor.h dev/bktr/ioctl_bt848.h,[BKTR="yes"],[BKTR="no"])
  elif test "${DISTRO}" = "OpenBSD" || test "${DISTRO}" = "NetBSD"; then
    AC_CHECK_HEADERS(dev/ic/bt8xx.h,[BKTR="yes"],[BKTR="no"])
  else
    BKTR="no"
  fi
fi
if test "${V4L2}" = "yes"; then
  AC_CHECK_HEADERS(linux/videodev2.h,[V4L2="yes"],[V4L2="no"])
fi

if test "x${V4L2}" = "xyes"; then
  AC_DEFINE([HAVE_V4L2], 1, [Define to 1 if V4L2 is around])
fi
if test "x${BKTR}" = "xyes"; then
  AC_DEFINE([HAVE_BKTR], 1, [Define to 1 if BKTR is around])
fi

##############################################################################
###  Check for threading
##############################################################################
THREADS="yes"
if test "${DISTRO}" = "FreeBSD"; then
  AC_CHECK_HEADERS(pthread_np.h,[THREADS="yes"],[THREADS="no"])
  AC_MSG_CHECKING(for threads)
  AC_MSG_RESULT($THREADS)
fi
if test x$THREADS = xyes; then
  TEMP_LIBS="$TEMP_LIBS -pthread"
  TEMP_CFLAGS="${TEMP_CFLAGS} -D_THREAD_SAFE"
fi

##############################################################################
###  Check for pthread_setname_np (nonstandard GNU extension)
##############################################################################
AC_MSG_CHECKING([for pthread_setname_np])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <pthread.h>], [pthread_setname_np(pthread_self(), "name")])],
    [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], [Define if you have pthread_setname_np function.])
    AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])] )

##############################################################################
###  Check for JPG
##############################################################################
AC_CHECK_HEADERS(setjmp.h jerror.h jpeglib.h,[JPGS="yes"],[JPGS="no"])
AC_MSG_CHECKING(jpg libraries)
AC_MSG_RESULT($JPGS)
if test x$JPGS = xyes ; then
  TEMP_LIBS="$TEMP_LIBS -ljpeg"
fi

##############################################################################
###  pkg-config
##############################################################################
AC_CHECK_PROG([PKGCONFIG],[pkg-config],[yes],[no])
AM_CONDITIONAL([FOUND_PKGCONFIG], [test "x$PKGCONFIG" = xyes])
AM_COND_IF([FOUND_PKGCONFIG],,[AC_MSG_ERROR([Required package 'pkg-config' not found, please check motion_guide.html and install necessary dependencies.])])

##############################################################################
###  Webp Image Format
##############################################################################
AC_ARG_WITH([webp],
    AS_HELP_STRING([--with-webp],
      [Compile with Webp image support]),
    WEBP="$withval",
    WEBP="no")

HAVE_WEBP=""
if test "${WEBP}" = "yes"; then
  AC_MSG_CHECKING(for libwebp)
  WEBP_DEPS="libwebp"
  if pkg-config $WEBP_DEPS; then
    AC_MSG_RESULT(found)
    AC_DEFINE([HAVE_WEBP], 1, [Define to 1 if WEBP is around])
    HAVE_WEBP="yes"
    TEMP_LIBS="$TEMP_LIBS -lwebp -lwebpmux"
  else
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR([Required package 'libwebp-dev' not found. Please check motion_guide.html and install necessary dependencies or use the '--without-webp' configuration option.])
  fi
fi

##############################################################################
###  raspberry pi mmal
##############################################################################
WITHOUT_MMAL="no"
AC_ARG_WITH([mmal],
  AS_HELP_STRING([--without-mmal],
    [Compile without RaspberyPi mmal camera support]),
  WITHOUT_MMAL="yes",
  WITHOUT_MMAL="no")

if test "${WITHOUT_MMAL}" = "no"; then
  HAVE_MMAL=""
  if test "${DISTRO}" = "FreeBSD" ; then
    LIBRASPBERRYPIDEVPATH="/usr/local/include/interface/mmal"
  else
    LIBRASPBERRYPIDEVPATH="/opt/vc/include/interface/mmal"
  fi

  if test -d ${LIBRASPBERRYPIDEVPATH}; then
       HAVE_MMAL="yes"
  fi

  AS_IF([test "${HAVE_MMAL}" = "yes" ], [
    AC_SUBST(MMAL_CFLAGS)
    AC_SUBST(MMAL_OBJ)
    AC_SUBST(MMAL_LIBS)
    MMAL_OBJ="mmalcam.o raspicam/RaspiCamControl.o raspicam/RaspiCLI.o"
    MMAL_CFLAGS="-std=gnu99 -DHAVE_MMAL -Irasppicam -I/opt/vc/include"
    AS_IF([test "${DISTRO}" = "FreeBSD" ], [
      MMAL_CFLAGS="${MMAL_CFLAGS} -I/usr/local/include -I/usr/local/include/interface/vcos -I/usr/local/include/interface/vcos/pthreads/ -I/usr/local/include/interface/vmcs_host/linux"
      ])
      MMAL_LIBS="-L/opt/vc/lib -lmmal_core -lmmal_util -lmmal_vc_client -lvcos -lvchostif -lvchiq_arm"
      AC_DEFINE([HAVE_MMAL], 1, [Define to 1 if we want MMAL])
  ])
fi

##############################################################################
###  ffmpeg
##############################################################################
AC_ARG_WITH([ffmpeg],
    AS_HELP_STRING([--with-ffmpeg[=DIR]],
      [Build with FFMPEG support]),
    [with_ffmpeg=$withval],
    [with_ffmpeg=yes])

AS_IF([test "x$with_ffmpeg" != "xno"], [
       AS_IF([test "x$with_ffmpeg" != "xyes"], [
          PKG_CONFIG_PATH=${with_ffmpeg}/lib/pkgconfig:$PKG_CONFIG_PATH
          export PKG_CONFIG_PATH
    ])

       FFMPEG_DEPS="libavutil libavformat libavcodec libswscale libavdevice"
       if pkg-config $FFMPEG_DEPS; then
               FFMPEG_CFLAGS=`pkg-config --cflags $FFMPEG_DEPS`
               FFMPEG_LIBS=`pkg-config --libs $FFMPEG_DEPS`
               HAVE_FFMPEG="yes"
       else
              AC_MSG_ERROR([Required ffmpeg packages 'libavutil-dev libavformat-dev libavcodec-dev libswscale-dev libavdevice-dev' were not found.  Please check motion_guide.html and install necessary dependencies or use the '--without-ffmpeg' configuration option.])
       fi
       AC_SUBST(FFMPEG_LIBS)
       AC_SUBST(FFMPEG_CFLAGS)
])

AS_IF([test "${HAVE_FFMPEG}" = "yes" ], [
       AC_DEFINE([HAVE_FFMPEG], 1, [Define to 1 if FFMPEG is around])
])

##############################################################################
###  Check SQLITE3
##############################################################################
SQLITE_OBJ=""
SQLITE3_SUPPORT="no"
AC_ARG_WITH(sqlite3,
    AS_HELP_STRING([--without-sqlite3],
      [Disable sqlite3 support in motion.]),
    [SQLITE3="$withval"])

if test "${SQLITE3}" = "no"; then
    AC_MSG_CHECKING(for sqlite3)
    AC_MSG_RESULT(skipping)
else
    # first we check to see if the sqlite3 amalgamation (sqlite3.c), is in with our source
    # this is the preferred way to use sqlite
    if test -f sqlite3.c; then
        SQLITE3_SUPPORT="yes"
        SQLITE_OBJ="sqlite3.o"
        TEMP_LIBS="$TEMP_LIBS -ldl"
        AC_DEFINE([HAVE_SQLITE3],1,[Define to 1 if you have SQLITE3])
        AC_DEFINE([HAVE_SQLITE3_EMBEDDED],1,[Define to 1 if you have SQLITE3 embedded support])
    else
        # if sqlite3.c is not found then we look for the shared library
        AC_CHECK_HEADERS(sqlite3.h,
            [
            TEMP_LIBS="$TEMP_LIBS -lsqlite3"
            SQLITE3_SUPPORT="yes"
            AC_DEFINE([HAVE_SQLITE3],1,[Define to 1 if you have SQLITE3 shared library support])
            ]
        )
    fi
fi
AC_SUBST(SQLITE_OBJ)

##############################################################################
###  Check mysql
##############################################################################
AC_ARG_WITH([mysql],
    AS_HELP_STRING([--without-mysql],
    [Build without mysql support]),
    [MYSQL=$withval],
    [MYSQL=yes])

if test "x${MYSQL}" = "xno"; then
    AC_MSG_CHECKING(for mysql support)
    AC_MSG_RESULT(skipped)
else
    AC_CHECK_HEADERS(mysql/mysql.h,[MYSQL="yes"],[MYSQL="no"])
    AC_MSG_CHECKING(for mysql support)
    AC_MSG_RESULT($MYSQL)
    if test "x${MYSQL}" = "xyes"; then
        TEMP_LIBS="$TEMP_LIBS -lmysqlclient -lz"
        AC_DEFINE([HAVE_MYSQL],1,[Define to 1 if you have MySQL support])
    fi
fi

##############################################################################
###  Check PostgreSQL
##############################################################################
AC_ARG_WITH([pgsql],
        AS_HELP_STRING([--without-pgsql], [Build without pgsql support]),
        [PGSQL=$withval],
        [PGSQL=yes])

if test "x${PGSQL}" = "xno"; then
    AC_MSG_CHECKING(for PostgreSQL support)
    AC_MSG_RESULT(skipped)
else
    AC_CHECK_HEADERS(postgresql/libpq-fe.h,[PGSQL="yes"],[PGSQL="no"])
    AC_MSG_CHECKING(for PostgreSQL support)
    AC_MSG_RESULT($PGSQL)
    if test "x${PGSQL}" = "xyes"; then
        TEMP_LIBS="$TEMP_LIBS -lpq"
        AC_DEFINE([HAVE_PGSQL],1,[Define to 1 if you have PostgreSQL support])
    fi
fi

##############################################################################
###  Optimize compiler
##############################################################################
AC_ARG_WITH([optimizecpu],
        AS_HELP_STRING([--without-optimizecpu],
            [Exclude autodetecting platform and cpu type. This will disable the compilation of gcc optimizing code by platform and cpu.]),
        [OPTIMIZECPU=$withval],
        [OPTIMIZECPU=no])

CPU_OPTIONS=""
if test -e "/proc/device-tree/model"; then
    # explicit test for RPI3 as /proc/cpuinfo reports armv7 even though it is armv8
    RPI3=`grep "Raspberry Pi 3 Model" /proc/device-tree/model`
    if test "x${RPI3}" != "x"; then
    CPU_OPTIONS="-mcpu=cortex-a53 -mfpu=neon-fp-armv8"
    fi
fi


##############################################################################
###  Developer Flags
##############################################################################
AC_ARG_WITH([developer-flags],
    AS_HELP_STRING([--with-developer-flags],
    [Causes practically all of the possible gcc warning flags to be set.  This may produce a large amount of warnings.]),
    [DEVELOPER_FLAGS=$withval],
    [DEVELOPER_FLAGS=no])

if test "${DEVELOPER_FLAGS}" = "yes"; then
    TEMP_CFLAGS="${TEMP_CFLAGS} -W -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline -Wredundant-decls -Wno-long-long -ggdb -g3"
fi

CFLAGS="${TEMP_CFLAGS} $CPU_OPTIONS"
LIBS="${TEMP_LIBS}"
LDFLAGS="${TEMP_LDFLAGS}"

##############################################################################
###  exec paths
##############################################################################
if test $prefix = "NONE";then
    BIN_PATH="$ac_default_prefix"
    if test $exec_prefix = "NONE"; then
        BIN_PATH="$BIN_PATH/bin"
    else
        BIN_PATH="$BIN_PATH/$bindir"
    fi
else
    if test $exec_prefix = "NONE";then
        BIN_PATH="$prefix/bin"
    else
        BIN_PATH="$prefix/$bindir"
    fi
fi
AC_SUBST(BIN_PATH)

AC_CHECK_HEADERS(stdio.h unistd.h stdint.h fcntl.h time.h signal.h sys/ioctl.h sys/mman.h sys/param.h sys/types.h)

AC_CONFIG_FILES([
camera1-dist.conf
camera2-dist.conf
camera3-dist.conf
camera4-dist.conf
motion-dist.conf
motion.init-FreeBSD.sh
motion.init-Debian
motion.service
motion.spec
Makefile
])
AC_OUTPUT

##############################################################################
###  Report results to user
##############################################################################
echo ""
echo "   **************************"
echo "      Configure status       "
echo "      ${PACKAGE_NAME} ${PACKAGE_VERSION}"
echo "   **************************"
echo


if test "${DISTRO}" = "Darwin"; then
    echo "OS             :     Darwin"
elif test "${DISTRO}" != "Linux"; then
    echo "OS             :     *BSD"
else
    echo "OS             :     Linux"
fi

if test "${THREADS}" = "yes"; then
    echo "pthread support:     Yes"
else
    echo "pthread support:     No"
    echo "**********************************************"
    echo "** Fatal Error YOU MUST HAVE pthread Support *"
    echo "**********************************************"
fi

if test "${JPGS}" = "yes"; then
    echo "jpeg support:        Yes"
else
    echo "jpeg support:        No"
    echo "**********************************************"
    echo "** Fatal Error YOU MUST HAVE jpeg Support  ***"
    echo "**********************************************"
fi

if test "${HAVE_WEBP}" = "yes"; then
   echo "webp support:        Yes"
else
   echo "webp support:        No"
fi

if test "$V4L2" = "yes"; then
    echo "V4L2 support:        Yes"
else
    echo "V4L2 support:        No"
fi

if test "${BKTR}" = "yes"; then
    echo "BKTR support:        Yes"
else
    echo "BKTR support:        No"
fi

if test "${HAVE_MMAL}" = "yes"; then
    echo "MMAL support:        Yes"
    echo " ... MMAL_CFLAGS: $MMAL_CFLAGS"
    echo " ... MMAL_OBJ: $MMAL_OBJ"
    echo " ... MMAL_LIBS: $MMAL_LIBS"
elif test "${WITHOUT_MMAL}" = "yes"; then
    echo "MMAL support:        disabled"
else
    echo "MMAL support:        No"
    echo " ... libraspberrypi-dev package not installed"
fi

if test "${HAVE_FFMPEG}" = "yes"; then
    echo "FFmpeg support:      Yes"
    echo " ... FFMPEG_CFLAGS: $FFMPEG_CFLAGS"
    echo " ... FFMPEG_LIBS: $FFMPEG_LIBS"
else
    echo "FFmpeg support:      No"
fi

if test "${SQLITE3_SUPPORT}" = "yes"; then
    echo "SQLite3 support:     Yes"
else
    echo "SQLite3 support:     No"
fi

if test "${MYSQL}" = "yes"; then
    echo "MYSQL support:       Yes"
else
    echo "MYSQL support:       No"
fi

if test "${PGSQL}" = "yes"; then
    echo "PostgreSQL support:  Yes"
else
    echo "PostgreSQL support:  No"
fi
echo
echo "CFLAGS: $CFLAGS"
echo "LIBS: $LIBS"
echo "LDFLAGS: $LDFLAGS"
echo
echo  "Install prefix:       $prefix"
echo
