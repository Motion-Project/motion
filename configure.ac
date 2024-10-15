AC_INIT(motionplus, esyscmd(['./scripts/version.sh']))
AM_INIT_AUTOMAKE([subdir-objects foreign])
CXXFLAGS="$CXXFLAGS -std=c++17"
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_HEADERS([config.hpp])
AC_CONFIG_SRCDIR([src/motionplus.cpp])
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
###  Check pkgconf  - Required.  Needed to get lib paths/info
##############################################################################
AC_CHECK_PROG([PKGCONFIG],[pkgconf],[yes],[no])
AS_IF([test "${PKGCONFIG}" = "no" ],[
    AC_MSG_ERROR([Required package 'pkgconf' not found, please check motionplus_guide.html and install necessary dependencies.])
  ]
)

##############################################################################
###  Check pthread
##############################################################################
AC_CHECK_HEADERS(pthread.h,,AC_MSG_ERROR([pthread is required.]))
CPPFLAGS="${CPPFLAGS} -D_THREAD_SAFE "
LIBS="$LIBS -pthread "

##############################################################################
###  Check JPG - Required.  Needed for image processing
##############################################################################
AS_IF([pkgconf libjpeg ], [
    TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libjpeg`
    TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libjpeg`
  ],[
    TEMP_LIBS="$TEMP_LIBS -ljpeg"
  ]
)
HOLD_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $TEMP_CPPFLAGS"
AC_CHECK_HEADERS(setjmp.h jerror.h jpeglib.h,[JPGS="yes"],[JPGS="no"])
AC_MSG_CHECKING(jpg libraries)
AC_MSG_RESULT($JPGS)
AS_IF([test "${JPGS}" != "yes" ], [
    AC_MSG_ERROR([Required jpeg headers not found, please check motionplus_guide.html and install necessary dependencies])
  ]
)
CPPFLAGS="$HOLD_CPPFLAGS"

##############################################################################
###  Check libmicrohttpd - Required.  Needed for stream/webcontrol
##############################################################################
AS_IF([pkgconf libmicrohttpd ], [
    TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libmicrohttpd`
    TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libmicrohttpd`
  ],[
    AC_MSG_ERROR([Required package libmicrohttpd-dev not found, please check motionplus_guide.html and install necessary dependencies])
  ]
)
HOLD_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $TEMP_CPPFLAGS"
AC_CHECK_HEADERS(microhttpd.h,[MHTTP="yes"],[MHTTP="no"])
AC_MSG_CHECKING(libmicrohttpd libraries)
AC_MSG_RESULT($MHTTP)
AS_IF([test "${MHTTP}" != "yes" ], [
    AC_MSG_ERROR([Required package libmicrohttpd-dev not found, please check motionplus_guide.html and install necessary dependencies])
  ]
)
CPPFLAGS="$HOLD_CPPFLAGS"

##############################################################################
###  Check zlib - Required.  Needed for stream/webcontrol
##############################################################################
AS_IF([pkgconf zlib ], [
    TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags zlib`
    TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs zlib`
  ],[
    AC_MSG_ERROR([Required package zlib not found, please check motionplus_guide.html and install necessary dependencies])
  ]
)
HOLD_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $TEMP_CPPFLAGS"
AC_CHECK_HEADERS(zlib.h,[ZLIB="yes"],[ZLIB="no"])
AC_MSG_CHECKING(zlib libraries)
AC_MSG_RESULT($ZLIB)
AS_IF([test "${ZLIB}" != "yes" ], [
    AC_MSG_ERROR([Required package zlib not found, please check motionplus_guide.html and install necessary dependencies])
  ]
)
CPPFLAGS="$HOLD_CPPFLAGS"

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
###  Check XSI strerror_r.
##############################################################################
AC_MSG_CHECKING([for XSI strerror_r])
HOLD_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -Werror"
AC_LINK_IFELSE(
  [AC_LANG_SOURCE([[
    #include <string.h>
    #include <errno.h>
    int main(int argc, char** argv) {
      char buf[1024];
      int ret = strerror_r(ENOMEM, buf, sizeof(buf));
      return ret;
    }]])
  ],[
    AC_DEFINE([XSI_STRERROR_R], [1], [Define if you have XSI strerror_r function.])
    XSI_STRERROR="yes"
    AC_MSG_RESULT([yes])
  ],[
    XSI_STRERROR="no"
    AC_MSG_RESULT([no])
  ]
)
CPPFLAGS="$HOLD_CPPFLAGS"

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
WEBP_VER=""
AS_IF([test "${WEBP}" = "yes" ], [
    AC_MSG_CHECKING(for webp)
    AS_IF([pkgconf libwebpmux ], [
        AC_MSG_RESULT(yes)
        AC_DEFINE([HAVE_WEBP], [1], [Define to 1 if WEBP is around])
        WEBP_VER="("`pkgconf --modversion libwebp`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libwebpmux libwebp`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libwebpmux libwebp`
      ],[
        AC_MSG_RESULT(no)
      ]
    )
  ]
)

##############################################################################
###  libcamera - Optional.
##############################################################################
AC_ARG_WITH([libcam],
  AS_HELP_STRING([--with-libcam[=DIR]],[Compile with libcamera support]),
  [LIBCAM="$withval"],
  [LIBCAM="yes"]
)
LIBCAM_VER=""
AS_IF([test "${LIBCAM}" = "no"], [
    AC_MSG_CHECKING(for LIBCAM)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(LIBCAM pkgconf path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${LIBCAM}" != "yes"], [
        PKG_CONFIG_PATH=${LIBCAM}/lib/pkgconfig:$PKG_CONFIG_PATH
        LIBCAM="yes"
      ],[
        PKG_CONFIG_PATH=.:/usr/lib/pkgconfig
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for LIBCAM)
    AS_IF([pkgconf libcamera], [
        LIBCAM_VER="("`pkgconf --modversion libcamera`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libcamera`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libcamera`
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_LIBCAM], [1], [Define to 1 if LIBCAM is around])
      ],[
        AC_MSG_RESULT([no])
        LIBCAM="no"
      ]
    )
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for ffmpeg
##############################################################################
FFMPEG_VER="--"
AC_ARG_WITH([ffmpeg],
  AS_HELP_STRING([--with-ffmpeg[=DIR]],[Build with FFMPEG support]),
  [FFMPEG=$withval],
  [FFMPEG="yes"]
)
FFMPEG_VER=""
AS_IF([test "${FFMPEG}" = "no"], [
    AC_MSG_CHECKING(for FFmpeg)
    AC_MSG_ERROR([Package ffmpeg is required.])
  ],[
    AC_MSG_CHECKING(FFmpeg pkgconf path)
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
    AS_IF([pkgconf $FFMPEG_DEPS], [
        FFMPEG_VER="("`pkgconf --modversion libavformat`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags $FFMPEG_DEPS`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs $FFMPEG_DEPS`
        AC_MSG_RESULT(yes)
      ],[
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Required ffmpeg packages 'libavutil-dev libavformat-dev libavcodec-dev libswscale-dev libavdevice-dev' were not found.  Please check motionplus_guide.html and install necessary dependencies])
      ]
    )
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)
##############################################################################
###  OpenCV support
##############################################################################
OPENCV_VER="--"
AC_ARG_WITH([opencv],
  AS_HELP_STRING([--with-opencv[=DIR]],[Build with OpenCV support]),
  [OPENCV=$withval],
  [OPENCV="yes"]
)
OPENCV_VER=""
AS_IF([test "${OPENCV}" = "no"], [
    AC_MSG_CHECKING(for OpenCV support)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(OpenCV pkgconf path)
    AS_IF([test "${OPENCV}" != "yes"], [
        PKG_CONFIG_PATH=${OPENCV}/lib/pkgconfig:$PKG_CONFIG_PATH
        OPENCV="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for OpenCV)
    AS_IF([pkgconf opencv], [
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags opencv`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs opencv`
        OPENCV_VER="("`pkgconf --modversion opencv`")"
        AC_DEFINE([HAVE_OPENCV], [1], [Define to 1 if OpenCV is around])
      ],[
        AS_IF([pkgconf opencv4], [
            TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags opencv4`
            TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs opencv4`
            OPENCV_VER="("`pkgconf --modversion opencv4`")"
            AC_DEFINE([HAVE_OPENCV], [1], [Define to 1 if OpenCV is around])
          ],[
            OPENCV="no"
          ]
        )
      ]
    )

    AC_MSG_RESULT([$OPENCV])
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
MARIADB_VER=""
AS_IF([test "${MARIADB}" = "no"], [
    AC_MSG_CHECKING(for MariaDB)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(MariaDB pkgconf path)
    AS_IF([test "${MARIADB}" != "yes"], [
        PKG_CONFIG_PATH=${MARIADB}/lib/pkgconfig:$PKG_CONFIG_PATH
        MARIADB="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for MariaDB)
    AS_IF([pkgconf libmariadb], [
        MARIADB_VER="("`pkgconf --modversion libmariadb`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libmariadb`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libmariadb`
        AC_DEFINE([HAVE_MARIADB], [1], [Define to 1 if you have MariaDB support])
      ],[
        MARIADB="no"
      ]
    )
    # Check for the old pkg name for MariaDB
    AS_IF([test "${MARIADB}" = "no"], [
      AS_IF([pkgconf mariadb], [
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags mariadb`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs mariadb`
        MARIADB="yes"
        AC_DEFINE([HAVE_MARIADB], [1], [Define to 1 if you have MariaDB support])
      ])
    ])

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
  [MYSQLDB="$withval"],
  [MYSQLDB="yes"]
)
MYSQLDB_VER=""
AS_IF([test "${MYSQLDB}" = "no"], [
    AC_MSG_CHECKING(for MySQL)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(MySQL pkgconf path)
    AS_IF([test "${MYSQLDB}" != "yes"], [
        PKG_CONFIG_PATH=${MYSQLDB}/lib/pkgconfig:$PKG_CONFIG_PATH
        MYSQLDB="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for MySQL)
    AS_IF([pkgconf mysqlclient], [
        MYSQLDB_VER="("`pkgconf --modversion mysqlclient`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags mysqlclient`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs mysqlclient`
        AC_DEFINE([HAVE_MARIADB], [1], [Define to 1 if you have MySQLDB/MariaDB support])
        MYSQLDB="yes"
      ],[
        MYSQLDB="no"
      ]
    )
    AC_MSG_RESULT([$MYSQLDB])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for PostgreSQL - Optional
##############################################################################
AC_ARG_WITH(pgsql,
  AS_HELP_STRING([--with-pgsql[=DIR]],[Build with PostgreSQL support]),
  [PGSQLDB="$withval"],
  [PGSQLDB="yes"]
)
PGSQLDB_VER=""
AS_IF([test "${PGSQLDB}" = "no"], [
    AC_MSG_CHECKING(for PostgreSQL)
    AC_MSG_RESULT(skipped)
  ],[
    TEMP_PATH=$PKG_CONFIG_PATH
    AC_MSG_CHECKING(PostgreSQL pkgconf path)
    AS_IF([test "${PGSQLDB}" != "yes"], [
        PKG_CONFIG_PATH=${PGSQLDB}/lib/pkgconfig:$PKG_CONFIG_PATH
        PGSQLDB="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH)

    AC_MSG_CHECKING(for PostgreSQL)
    AS_IF([pkgconf libpq], [
        PGSQLDB_VER="("`pkgconf --modversion libpq`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libpq`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libpq`
        AC_DEFINE([HAVE_PGSQLDB], [1], [Define to 1 if you have PostgreSQL support])
      ],[
        PGSQLDB="no"
      ]
    )
    AC_MSG_RESULT([$PGSQLDB])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for SQLITE3DB
##############################################################################
AC_ARG_WITH([sqlite3],
  AS_HELP_STRING([--with-sqlite3[=DIR]],[Build with sqlite3 support]),
  [SQLITE3DB=$withval],
  [SQLITE3DB="yes"]
)
SQLITE3DB_VER=""
AS_IF([test "${SQLITE3DB}" = "no"], [
    AC_MSG_CHECKING(for SQLite3)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(SQLite3 pkgconf path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${SQLITE3DB}" != "yes"], [
        PKG_CONFIG_PATH=${SQLITE3DB}/lib/pkgconfig:$PKG_CONFIG_PATH
        SQLITE3DB="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH done)

    AC_MSG_CHECKING(for SQLite3)
    AS_IF([pkgconf sqlite3], [
        SQLITE3DB_VER="("`pkgconf --modversion sqlite3`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags sqlite3`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs sqlite3`
        AC_DEFINE([HAVE_SQLITE3DB], [1], [Define to 1 if you have SQLITE3 support])
        SQLITE3DB="yes"
      ],[
        SQLITE3DB="no"
      ]
    )

    AC_MSG_RESULT([$SQLITE3DB])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for Pulse
##############################################################################
AC_ARG_WITH([pulse],
  AS_HELP_STRING([--with-pulse[=DIR]],[Build with PulseAudio support]),
  [PULSE=$withval],
  [PULSE="yes"]
)
PULSE_VER=""
AS_IF([test "${PULSE}" = "no"], [
    AC_MSG_CHECKING(for PulseAudio)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(PULSE pkgconf path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${PULSE}" != "yes"], [
        PKG_CONFIG_PATH=${PULSE}/lib/pkgconfig:$PKG_CONFIG_PATH
        PULSE="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH done)

    AC_MSG_CHECKING(for PulseAudio)
    AS_IF([pkgconf libpulse], [
        PULSE_VER="("`pkgconf --modversion libpulse`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags libpulse`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs libpulse`" -lpulse-simple "
        AC_DEFINE([HAVE_PULSE], [1], [Define to 1 if you have PulseAudio support])
        PULSE="yes"
      ],[
        PULSE="no"
      ]
    )

    AC_MSG_RESULT([$PULSE])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for ALSA
##############################################################################
AC_ARG_WITH([alsa],
  AS_HELP_STRING([--with-alsa[=DIR]],[Build with ALSA support]),
  [ALSA=$withval],
  [ALSA="yes"]
)
ALSA_VER=""
AS_IF([test "${ALSA}" = "no"], [
    AC_MSG_CHECKING(for ALSA)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(ALSA pkgconf path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${ALSA}" != "yes"], [
        PKG_CONFIG_PATH=${ALSA}/lib/pkgconfig:$PKG_CONFIG_PATH
        ALSA="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH done)

    AC_MSG_CHECKING(for ALSA)
    AS_IF([pkgconf alsa], [
        ALSA_VER="("`pkgconf --modversion alsa`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags alsa`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs alsa`
        AC_DEFINE([HAVE_ALSA], [1], [Define to 1 if you have ALSA support])
        ALSA="yes"
      ],[
        ALSA="no"
      ]
    )

    AC_MSG_RESULT([$ALSA])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
  ]
)

##############################################################################
###  Check for FFTW3
##############################################################################
AC_ARG_WITH([fftw3],
  AS_HELP_STRING([--with-fftw3[=DIR]],[Build with FFTW3 support]),
  [FFTW3=$withval],
  [FFTW3="yes"]
)
FFTW3_VER=""
AS_IF([test "${FFTW3}" = "no"], [
    AC_MSG_CHECKING(for FFTW3)
    AC_MSG_RESULT(skipped)
  ],[
    AC_MSG_CHECKING(FFTW3 pkgconf path)
    TEMP_PATH=$PKG_CONFIG_PATH
    AS_IF([test "${FFTW3}" != "yes"], [
        PKG_CONFIG_PATH=${FFTW3}/lib/pkgconfig:$PKG_CONFIG_PATH
        FFTW3="yes"
      ]
    )
    export PKG_CONFIG_PATH
    AC_MSG_RESULT($PKG_CONFIG_PATH done)

    AC_MSG_CHECKING(for FFTW3)
    AS_IF([pkgconf fftw3], [
        FFTW3_VER="("`pkgconf --modversion fftw3`")"
        TEMP_CPPFLAGS="$TEMP_CPPFLAGS "`pkgconf --cflags fftw3`
        TEMP_LIBS="$TEMP_LIBS "`pkgconf --libs fftw3`
        AC_DEFINE([HAVE_FFTW3], [1], [Define to 1 if you have FFTW3 support])
        FFTW3="yes"
      ],[
        FFTW3="no"
      ]
    )

    AC_MSG_RESULT([$FFTW3])
    PKG_CONFIG_PATH=$TEMP_PATH
    export PKG_CONFIG_PATH
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

AC_ARG_WITH([clang-flags],
    AS_HELP_STRING([--with-clang-flags],
    [clang warning flags]),
    [CLANG_FLAGS=$withval],
    [CLANG_FLAGS=no])

AS_IF([test "${DEVELOPER_FLAGS}" = "yes"], [
    TEMP_CPPFLAGS="$TEMP_CPPFLAGS -W -pedantic -Werror -Wall -Wextra -Wconversion -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Wredundant-decls -Wno-sign-conversion -Wno-sign-compare -ggdb -g3"
  ],[
    AS_IF([test "${CLANG_FLAGS}" = "yes"], [
      TEMP_CPPFLAGS="$TEMP_CPPFLAGS -W -pedantic -Werror -Wall -Wextra -Wconversion -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Wredundant-decls -Wno-c11-extensions -Wno-sign-conversion -Wno-sign-compare -ggdb -g3 "
    ],[
      TEMP_CPPFLAGS="$TEMP_CPPFLAGS -W -O3 -Wall -Wextra -Wconversion -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Wredundant-decls -Wno-sign-conversion -Wno-sign-compare -ggdb -g3 "
    ])
  ])

CPPFLAGS="$CPPFLAGS $TEMP_CPPFLAGS"
LIBS="$LIBS $TEMP_LIBS"
LDFLAGS="$TEMP_LDFLAGS"

AC_CONFIG_FILES([
	Makefile
	src/Makefile
  po/Makefile.in
  data/camera1.conf
  data/camera2.conf
  data/camera3.conf
  data/sound1.conf
  data/motionplus.conf
  data/motionplus.service
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
echo "CPPFLAGS: $CPPFLAGS"
echo
echo "LIBS: $LIBS"
echo
echo "LDFLAGS: $LDFLAGS"
echo
echo
echo "OS                    : $host_os"
echo "pthread_np            : $PTHREAD_NP"
echo "pthread_setname_np    : $PTHREAD_SETNAME_NP"
echo "pthread_getname_np    : $PTHREAD_GETNAME_NP"
echo "XSI error             : $XSI_STRERROR"
echo "V4L2                  : $V4L2"
echo "webp                  : $WEBP$WEBP_VER"
echo "libcamera             : $LIBCAM$LIBCAM_VER"
echo "FFmpeg                : $FFMPEG$FFMPEG_VER"
echo "OpenCV                : $OPENCV$OPENCV_VER"
echo "SQLite3               : $SQLITE3DB$SQLITE3DB_VER"
echo "MYSQL                 : $MYSQLDB$MYSQLDB_VER"
echo "PostgreSQL            : $PGSQLDB$PGSQLDB_VER"
echo "MariaDB               : $MARIADB$MARIADB_VER"
echo "ALSA                  : $ALSA$ALSA_VER"
echo "PulseAudio            : $PULSE$PULSE_VER"
echo "FFTW                  : $FFTW3$FFTW3_VER"
echo
echo "Install prefix        : $prefix"
echo
