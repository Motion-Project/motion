AC_INIT(motion, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([subdir-objects foreign])
AM_MAINTAINER_MODE([enable])
AC_PROG_CC
AC_PROG_CXX
AC_GNU_SOURCE
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/motion.c])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19])

##############################################################################
###  Check additional system headers
##############################################################################
AC_CHECK_HEADERS(stdio.h stdlib.h sys/time.h sys/wait.h \
    sys/ioctl.h sys/mman.h sys/param.h sys/socket.h stdarg.h \
    fcntl.h time.h signal.h limits.h errno.h assert.h netdb.h \
    ctype.h regex.h math.h locale.h dirent.h ctype.h \
    arpa/inet.h netinet/in.h termios.h,,[
    AC_MSG_ERROR([Required system headers do not exist.])
  ]
)

##############################################################################
###  Check pkg-config  - Required.  Needed to get lib paths/info
##############################################################################
AC_CHECK_PROG([PKGCONFIG],[pkg-config],[yes],[no])
AS_IF([test "${PKGCONFIG}" = "no" ],[
    AC_MSG_ERROR([Required package 'pkg-config' not found, please check motion_guide.html and install necessary dependencies.])
  ]
)

##############################################################################
###  Check pthread
##############################################################################
AC_CHECK_HEADERS(pthread.h,,AC_MSG_ERROR([pthread is required.]))
TEMP_CFLAGS="-D_THREAD_SAFE"
LIBS="$LIBS -pthread "

##############################################################################
###  Check JPG - Required.  Needed for image processing
##############################################################################
AC_CHECK_HEADERS(setjmp.h jerror.h jpeglib.h,[JPGS="yes"],[JPGS="no"])
AC_MSG_CHECKING(jpg libraries)
AC_MSG_RESULT($JPGS)
AS_IF([test "${JPGS}" = "yes" ], [
    AS_IF([pkg-config libjpeg ], [
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags libjpeg`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs libjpeg`
      ],[
        TEMP_LIBS="$TEMP_LIBS -ljpeg"
      ]
    )
  ],[
    AC_MSG_ERROR([Required package libjpeg-dev not found, please check motion_guide.html and install necessary dependencies])
  ]
)
##############################################################################
###  Check libmicrohttpd - Required.  Needed for stream/webcontrol
##############################################################################
AC_CHECK_HEADERS(microhttpd.h,[MHTTP="yes"],[MHTTP="no"])
AC_MSG_CHECKING(libmicrohttpd libraries)
AC_MSG_RESULT($MHTTP)
AS_IF([test "${MHTTP}" = "yes" ], [
    TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags libmicrohttpd`
    TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs libmicrohttpd`
  ],[
    AC_MSG_ERROR([Required package libmicrohttpd-dev not found, please check motion_guide.html and install necessary dependencies])
  ]
)

##############################################################################
###  Check setting/getting thread names
##############################################################################
AC_CHECK_HEADERS(pthread_np.h,[PTHREAD_NP="yes"],[PTHREAD_NP="no"])

AC_MSG_CHECKING([for pthread_setname_np])
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <pthread.h>], [pthread_setname_np(pthread_self(), "name")])
  ],[
    AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], [Define if you have pthread_setname_np function.])
    PTHREAD_SETNAME_NP="yes"
    AC_MSG_RESULT([yes])
  ],[
    PTHREAD_SETNAME_NP="no"
    AC_MSG_RESULT([no])
  ]
)

AC_MSG_CHECKING([for pthread_getname_np])
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <pthread.h>], [pthread_getname_np(pthread_self(), NULL, 0)])
  ],[
    AC_DEFINE([HAVE_PTHREAD_GETNAME_NP], [1], [Define if you have pthread_getname_np function.])
    PTHREAD_GETNAME_NP="yes"
    AC_MSG_RESULT([yes])
  ],[
    PTHREAD_GETNAME_NP="no"
    AC_MSG_RESULT([no])
  ]
)

##############################################################################
###  Check XSI strerror_r.  Check for Linux/*BSD/Apple/MUSL variations
##############################################################################
AC_MSG_CHECKING([for XSI strerror_r])
HOLD_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Werror"
AC_LINK_IFELSE(
  [AC_LANG_SOURCE[
    #include <string.h>
    #include <errno.h>
    int main(int argc, char** argv) {
      char buf[1024];
      int ret = strerror_r(ENOMEM, buf, sizeof(buf));
      return ret;
    }]
  ],[
    AC_DEFINE([XSI_STRERROR_R], [1], [Define if you have XSI strerror_r function.])
    XSI_STRERROR="yes"
    AC_MSG_RESULT([yes])
  ],[
    XSI_STRERROR="no"
    AC_MSG_RESULT([no])
  ]
)
CFLAGS="$HOLD_CFLAGS"

###############################################################################
###  BKTR Video System - Optional
###############################################################################
AC_ARG_WITH(bktr,
  AS_HELP_STRING([--without-bktr],[Disable bktr devices]),
  [BKTR="$withval"],
  [BKTR="yes"]
)
AS_IF([test "${BKTR}" = "yes" ], [
    AC_CHECK_HEADERS(dev/bktr/ioctl_meteor.h dev/bktr/ioctl_bt848.h,[BKTR="yes"],[BKTR="no"])
    AS_IF([test "${BKTR}" = "no" ], [
        AC_CHECK_HEADERS(dev/ic/bt8xx.h,[BKTR="yes"],[BKTR="no"])
      ]
    )
    AS_IF([test "${BKTR}" = "yes" ], [
        AC_DEFINE([HAVE_BKTR], [1], [Define to 1 if BKTR is around])
      ]
    )
  ]
)

###############################################################################
###  V4L2 Video System - Optional
###############################################################################
AC_ARG_WITH(v4l2,
  AS_HELP_STRING([--without-v4l2],[Disable V4L2 devices]),
  [V4L2="$withval"],
  [V4L2="yes"]
)
AS_IF([test "${V4L2}" = "yes" ], [
  AC_CHECK_HEADERS(linux/videodev2.h sys/videoio.h,[V4L2="yes";break],[V4L2="no"])
    AS_IF([test "${V4L2}" = "yes" ], [
        AC_DEFINE([HAVE_V4L2], [1], [Define to 1 if V4L2 is around])
      ]
    )
  ]
)

##############################################################################
###  Webp Image Format - Optional.
##############################################################################
AC_ARG_WITH([webp],
  AS_HELP_STRING([--with-webp],[Compile with Webp image support]),
  [WEBP="$withval"],
  [WEBP="yes]"
)

AS_IF([test "${WEBP}" = "yes" ], [
    AC_MSG_CHECKING(for webp)
    AS_IF([pkg-config libwebpmux ], [
        AC_MSG_RESULT(yes)
        AC_DEFINE([HAVE_WEBP], [1], [Define to 1 if WEBP is around])
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags libwebpmux`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs libwebpmux`
      ],[
        AC_MSG_RESULT(no)
      ]
    )
  ]
)

##############################################################################
###  raspberry pi mmal - Optional.
##############################################################################
AC_ARG_WITH([mmal],
  AS_HELP_STRING([--with-mmal[=DIR]],[Compile with RasperryPi mmal camera support]),
  [MMAL="$withval"],
  [MMAL="yes"]
)

AS_IF([test "${MMAL}" = "no"], [
    AC_MSG_CHECKING(for MMAL)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(MMAL pkg-config path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${MMAL}" != "yes"], [
        PKG_CONFIG_PATH=${MMAL}/lib/pkgconfig:$PKG_CONFIG_PATH
        MMAL="yes"
      ],[
        PKG_CONFIG_PATH=.:/opt/vc/lib/pkgconfig
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for MMAL)
    AS_IF([pkg-config mmal], [
        TEMP_CFLAGS="$TEMP_CFLAGS -Irasppicam "`pkg-config --cflags mmal`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs mmal`
        #pkg config parm does not provide -lvchostif that buildroot requires
        TEMP_LIBS="$TEMP_LIBS -lvchostif "
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_MMAL], [1], [Define to 1 if MMAL is around])
      ],[
        AC_MSG_RESULT([no])
        MMAL="no"
      ]
    )
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)
AM_CONDITIONAL([INC_MMAL_SRC],[test x${MMAL} = xyes])

##############################################################################
###  Check for ffmpeg - Optional.
##############################################################################
FFMPEG_VER="--"
AC_ARG_WITH([ffmpeg],
  AS_HELP_STRING([--with-ffmpeg[=DIR]],[Build with FFMPEG support]),
  [FFMPEG=$withval],
  [FFMPEG="yes"]
)

AS_IF([test "${FFMPEG}" = "no"], [
    AC_MSG_CHECKING(for FFmpeg)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(FFmpeg pkg-config path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${FFMPEG}" != "yes"], [
        PKG_CONFIG_PATH=${FFMPEG}/lib/pkgconfig:$PKG_CONFIG_PATH
        FFMPEG="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    FFMPEG_DEPS="libavutil libavformat libavcodec libswscale libavdevice"
    AC_MSG_CHECKING(for FFmpeg)
    AS_IF([pkg-config $FFMPEG_DEPS], [
        FFMPEG_VER=`pkg-config --modversion libavformat`
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags $FFMPEG_DEPS`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs $FFMPEG_DEPS`
        AC_DEFINE([HAVE_FFMPEG], [1], [Define to 1 if FFMPEG is around])
        AC_MSG_RESULT(yes)
      ],[
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Required ffmpeg packages 'libavutil-dev libavformat-dev libavcodec-dev libswscale-dev libavdevice-dev' were not found.  Please check motion_guide.html and install necessary dependencies or use the '--without-ffmpeg' configuration option.])
      ]
    )
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for MariaDB - Optional
##############################################################################
AC_ARG_WITH(mariadb,
  AS_HELP_STRING([--with-mariadb[=DIR]],[Build with MariaDB support]),
  [MARIADB="$withval"],
  [MARIADB="yes"]
)

AS_IF([test "${MARIADB}" = "no"], [
    AC_MSG_CHECKING(for MariaDB)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(MariaDB pkg-config path)
    AS_IF([test "${MARIADB}" != "yes"], [
        PKG_CONFIG_PATH=${MARIADB}/lib/pkgconfig:$PKG_CONFIG_PATH
        MARIADB="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for MariaDB)
    AS_IF([pkg-config mariadb], [
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags mariadb`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs mariadb`
        AC_DEFINE([HAVE_MARIADB], [1], [Define to 1 if you have MariaDB support])
      ],[
        MARIADB="no"
      ]
    )
    AC_MSG_RESULT([$MARIADB])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for MySQL - Optional
##############################################################################
AC_ARG_WITH(mysql,
  AS_HELP_STRING([--with-mysql[=DIR]],[Build with MySQL support]),
  [MYSQL="$withval"],
  [MYSQL="yes"]
)

AS_IF([test "${MYSQL}" = "no"], [
    AC_MSG_CHECKING(for MySQL)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(MySQL pkg-config path)
    AS_IF([test "${MYSQL}" != "yes"], [
        PKG_CONFIG_PATH=${MYSQL}/lib/pkgconfig:$PKG_CONFIG_PATH
        MYSQL="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for MySQL)
    AS_IF([pkg-config mysqlclient], [
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags mysqlclient`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs mysqlclient`
        AC_DEFINE([HAVE_MYSQL], [1], [Define to 1 if you have MySQL support])
      ],[
        MYSQL="no"
      ]
    )
    AC_MSG_RESULT([$MYSQL])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for PostgreSQL - Optional
##############################################################################
AC_ARG_WITH(pgsql,
  AS_HELP_STRING([--with-pgsql[=DIR]],[Build with PostgreSQL support]),
  [PGSQL="$withval"],
  [PGSQL="yes"]
)

AS_IF([test "${PGSQL}" = "no"], [
    AC_MSG_CHECKING(for PostgreSQL)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(PostgreSQL pkg-config path)
    AS_IF([test "${PGSQL}" != "yes"], [
        PKG_CONFIG_PATH=${PGSQL}/lib/pkgconfig:$PKG_CONFIG_PATH
        PGSQL="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for PostgreSQL)
    AS_IF([pkg-config libpq], [
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags libpq`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs libpq`
        AC_DEFINE([HAVE_PGSQL], [1], [Define to 1 if you have PostgreSQL support])
      ],[
        PGSQL="no"
      ]
    )
    AC_MSG_RESULT([$PGSQL])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for SQLITE3 - Optional
##############################################################################
AC_ARG_WITH(sqlite3,
  AS_HELP_STRING([--without-sqlite3],[Disable sqlite3 support.]),
  [SQLITE3="$withval"],
  [SQLITE3="yes"]
)
AS_IF([test "${SQLITE3}" = "no"], [
    AC_MSG_CHECKING(for sqlite3)
    AC_MSG_RESULT(skipping)
  ],[
    AC_CHECK_HEADERS(sqlite3.h, [
        TEMP_CFLAGS="$TEMP_CFLAGS "`pkg-config --cflags sqlite3`
        TEMP_LIBS="$TEMP_LIBS "`pkg-config --libs sqlite3`
        AC_DEFINE([HAVE_SQLITE3], [1], [Define to 1 if you have SQLITE3 support])
      ],[
        SQLITE3="no"
      ]
    )
  ]
)

##############################################################################
###  Optimize compiler
##############################################################################
AC_ARG_WITH([optimizecpu],
  AS_HELP_STRING([--without-optimizecpu],
    [Exclude autodetecting platform and cpu type. This will disable the compilation of gcc optimizing code by platform and cpu.]),
  [OPTIMIZECPU=$withval],
  [OPTIMIZECPU=no]
)

AS_IF([test "${OPTIMIZECPU}" = "yes"], [
    AS_IF([test -e "/proc/device-tree/model"], [
        # explicit test for RPI3 as /proc/cpuinfo reports armv7 even though it is armv8
        RPI3=`grep "Raspberry Pi 3 Model" /proc/device-tree/model`
        AS_IF([test "x${RPI3}" != "x"], [
            TEMP_CFLAGS="$TEMP_CFLAGS -mcpu=cortex-a53 -mfpu=neon-fp-armv8"
          ]
        )
      ]
    )
  ]
)

##############################################################################
###  Developer Flags
##############################################################################
AC_ARG_WITH([developer-flags],
    AS_HELP_STRING([--with-developer-flags],
    [Causes practically all of the possible gcc warning flags to be set.  This may produce a large amount of warnings.]),
    [DEVELOPER_FLAGS=$withval],
    [DEVELOPER_FLAGS=no])

AS_IF([test "${DEVELOPER_FLAGS}" = "yes"], [
    TEMP_CFLAGS="$TEMP_CFLAGS -W -Werror -Wall -Wextra -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline -Wredundant-decls -Wno-long-long -ggdb -g3"
  ]
)
##############################################################################
###  Prototype Flags
###  mysql and mariadb have upstream prototype errors so remove that check
##############################################################################
AC_ARG_WITH([prototype-flags],
    AS_HELP_STRING([--with-prototype-flags],
    [Causes practically all of the possible gcc warning flags to be set except missing prototypes.]),
    [PROTOTYPE_FLAGS=$withval],
    [PROTOTYPE_FLAGS=no])

AS_IF([test "${PROTOTYPE_FLAGS}" = "yes"], [
    TEMP_CFLAGS="$TEMP_CFLAGS -W -Werror -Wall -Wextra -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Waggregate-return -Wnested-externs -Winline -Wredundant-decls -Wno-long-long -ggdb -g3"
  ]
)


AC_SUBST(AM_CFLAGS, "$TEMP_CFLAGS")
LIBS="$LIBS $TEMP_LIBS"
AC_SUBST(AM_LDFLAGS, "$TEMP_LDFLAGS")

AC_CONFIG_FILES([
	Makefile
	src/Makefile
  po/Makefile.in
  man/Makefile
  data/camera1-dist.conf
  data/camera2-dist.conf
  data/camera3-dist.conf
  data/camera4-dist.conf
  data/motion-dist.conf
  data/motion.service
])

AC_ARG_VAR([KILL],[the 'kill' program to use to send signals to motion])
AC_PATH_PROG([KILL],[kill],['/bin/kill'])
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
echo "CFLAGS: $TEMP_CFLAGS $CFLAGS"
echo
echo "LIBS: $LIBS"
echo
echo "LDFLAGS: $TEMP_LDFLAGS $LDFLAGS"
echo
echo
echo "OS                  : $host_os"
echo "pthread_np          : $PTHREAD_NP"
echo "pthread_setname_np  : $PTHREAD_SETNAME_NP"
echo "pthread_getname_np  : $PTHREAD_GETNAME_NP"
echo "XSI error           : $XSI_STRERROR"
echo "webp support        : $WEBP"
echo "V4L2 support        : $V4L2"
echo "BKTR support        : $BKTR"
echo "MMAL support        : $MMAL"
echo "FFmpeg support      : $FFMPEG"
echo "libavformat version : $FFMPEG_VER"
echo "SQLite3 support     : $SQLITE3"
echo "MYSQL support       : $MYSQL"
echo "PostgreSQL support  : $PGSQL"
echo "MariaDB support     : $MARIADB"
echo
echo  "Install prefix:       $prefix"
echo
