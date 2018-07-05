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
###  Host search paths for MYSQL/PGSQL
###############################################################################
#dpkg-architecture may not be available so manually list
SEARCH_INC="/usr/include/x86_64-linux-gnu"
SEARCH_INC=$SEARCH_INC" /usr/include/i386-linux-gnu"
SEARCH_INC=$SEARCH_INC" /usr"
SEARCH_INC=$SEARCH_INC" /usr/include"
SEARCH_INC=$SEARCH_INC" /usr/local"
SEARCH_INC=$SEARCH_INC" /usr/local/include"
SEARCH_INC=$SEARCH_INC" /opt"

SEARCH_LIB="/usr/lib/x86_64-linux-gnu"
SEARCH_LIB=$SEARCH_LIB" /usr/lib/i386-linux-gnu"
SEARCH_LIB=$SEARCH_LIB" /usr"
SEARCH_LIB=$SEARCH_LIB" /usr/lib64"
SEARCH_LIB=$SEARCH_LIB" /usr/lib"
SEARCH_LIB=$SEARCH_LIB" /usr/local"
SEARCH_LIB=$SEARCH_LIB" /usr/local/lib"
SEARCH_LIB=$SEARCH_LIB" /opt"

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

##############################################################################
###  Check for pthread_setname_np (nonstandard GNU extension)
##############################################################################
  AC_MSG_CHECKING([for pthread_setname_np])
  HOLD_LIBS="$LIBS"
  LIBS="$TEMP_LIBS"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <pthread.h>], [pthread_setname_np(pthread_self(), "name")])],
    [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP], [1], [Define if you have pthread_setname_np function.])
    AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])] )

##############################################################################
###  Check for pthread_getname_np (nonstandard GNU extension)
##############################################################################
  AC_MSG_CHECKING([for pthread_getname_np])
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <pthread.h>], [pthread_getname_np(pthread_self(), NULL, 0)])],
    [AC_DEFINE([HAVE_PTHREAD_GETNAME_NP], [1], [Define if you have pthread_getname_np function.])
    AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])] )

  LIBS="$HOLD_LIBS"
fi

##############################################################################
###  Check for XSI strerror_r
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
    }
  ]],
  [AC_DEFINE([XSI_STRERROR_R], [1], [Define if you have XSI strerror_r function.])
  AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])] )
CFLAGS="$HOLD_CFLAGS"

##############################################################################
###  Check for full gettext support (lib and required programs)
##############################################################################
LANGCDS=""
AC_CHECK_HEADERS(libintl.h,[INTL="yes"],[INTL="no"])
if (test "x${INTL}"  = "xyes"); then
    HOLD_CFLAGS=$CFLAGS
    HOLD_LIBS=$LIBS
    # Check without a lib
    CFLAGS=""
    LIBS=""
    AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <libintl.h>], [bindtextdomain ("motion", "/")])],
    [INTL="yes"],[INTL="no"])

    # If it has flipped to no after our test, try it with a lib
    if (test "x${INTL}"  = "xno"); then
        LIBS=" -lintl"
        AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <libintl.h>], [bindtextdomain ("motion", "/")])],
        [INTL="yes"],[INTL="no"])
        # If it now a yes, then we needed the lib, add it to temp_libs
        # HAVE_INTL still may turn off based upon below so the lib May not be needed.
        if (test "x${INTL}"  = "xyes"); then
            TEMP_LIBS="$TEMP_LIBS -lintl"
        fi
    fi
    CFLAGS=$HOLD_CFLAGS
    LIBS=$HOLD_LIBS
fi
MSGFMT=`msgfmt --version  2>/dev/null`
GETTXT=`gettext --version 2>/dev/null`
# Get the langs from what po files exist
for f in ./po/*.po; do
    TMP=`basename $f`
    TMP=`echo $TMP | sed -e 's/\.po$//'`
    if test "x$TMP" != "x*.po"; then
        LANGCDS=$LANGCDS" "$TMP
    fi
done
if (test "x${INTL}"  = "xyes" &&
    test "x${MSGFMT}"  != "x" &&
    test "x${GETTXT}"  != "x" &&
    test "x${LANGCDS}" != "x" ); then
    AC_DEFINE([HAVE_INTL], 1, [Define to 1 if libintl is around])
else
    INTL="no"
fi
AC_MSG_CHECKING(Native language support)
AC_MSG_RESULT($INTL)
AC_SUBST(INTL)
AC_SUBST(LANGCDS)

##############################################################################
###  Check for JPG
##############################################################################
AC_CHECK_HEADERS(setjmp.h jerror.h jpeglib.h,[JPGS="yes"],[JPGS="no"])
AC_MSG_CHECKING(jpg libraries)
AC_MSG_RESULT($JPGS)
if test x$JPGS = xyes ; then
  TEMP_LIBS="$TEMP_LIBS -ljpeg"
else
  AC_MSG_ERROR([Required package libjpeg-dev not found, please check motion_guide.html and install necessary dependencies])
fi

##############################################################################
###  Check for libmicrohttpd
##############################################################################
AC_CHECK_HEADERS(microhttpd.h,[MHTTP="yes"],[MHTTP="no"])
AC_MSG_CHECKING(libmicrohttpd libraries)
AC_MSG_RESULT($MHTTP)
if test x$MHTTP = xyes ; then
  TEMP_LIBS="$TEMP_LIBS -lmicrohttpd "
else
  AC_MSG_ERROR([Required package libmicrohttpd-dev not found, please check motion_guide.html and install necessary dependencies])
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
    WEBP="yes")

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

AC_ARG_WITH([mmal-lib],
  AS_HELP_STRING([--with-mmal-lib[=DIR]],
    [Use this command to tell configure where mmal libs directory is.]),
  MMAL_LIBS_DIR="$withval",
  MMAL_LIBS_DIR="/opt/vc/lib"
)

AC_ARG_WITH([mmal-include],
  AS_HELP_STRING([--with-mmal-include[=DIR]],
    [Use this command to tell configure where mmal include installation root directory is.]),
  MMAL_HEADERS="$withval",
  MMAL_HEADERS="/opt/vc/include"
)

if test "${WITHOUT_MMAL}" = "no"; then
  HAVE_MMAL=""
  if test "${DISTRO}" = "FreeBSD" ; then
    LIBRASPBERRYPIDEVPATH="/usr/local/include/interface/mmal"
  else
    LIBRASPBERRYPIDEVPATH="/opt/vc/include/interface/mmal"
  fi

  if test -d ${LIBRASPBERRYPIDEVPATH}; then
       HAVE_MMAL="yes"
  elif test -d ${MMAL_HEADERS}/interface/mmal; then
       HAVE_MMAL="yes"
  fi

  AS_IF([test "${HAVE_MMAL}" = "yes" ], [
    AC_SUBST(MMAL_CFLAGS)
    AC_SUBST(MMAL_OBJ)
    AC_SUBST(MMAL_LIBS)
    MMAL_OBJ="mmalcam.o raspicam/RaspiCamControl.o raspicam/RaspiCLI.o"
    MMAL_CFLAGS="-std=gnu99 -DHAVE_MMAL -Irasppicam -I${MMAL_HEADERS}"
    AS_IF([test "${DISTRO}" = "FreeBSD" ], [
      MMAL_CFLAGS="${MMAL_CFLAGS} -I/usr/local/include -I/usr/local/include/interface/vcos -I/usr/local/include/interface/vcos/pthreads/ -I/usr/local/include/interface/vmcs_host/linux"
      ])
      MMAL_LIBS="-L${MMAL_LIBS_DIR} -lmmal_core -lmmal_util -Wl,--push-state,--no-as-needed -lmmal_vc_client -Wl,--pop-state -lvcos -lvchostif -lvchiq_arm"
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
               FFMPEG_VER=`pkg-config --modversion libavformat`
               AC_MSG_CHECKING(for libavformat)
               AC_MSG_RESULT(yes $FFMPEG_VER)
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
MYSQL="yes"
MYSQL_SUPPORT="no"
MYSQL_HEADERS="yes"
MYSQL_LIBS="yes"
MYSQL_INCDIR=""
MYSQL_LIBDIR=""

AC_ARG_WITH(mysql,
    AS_HELP_STRING([--without-mysql],[Disable mysql support]),
    [MYSQL="$withval"])
AC_ARG_WITH(mysql-lib,
    AS_HELP_STRING([--with-mysql-lib[=DIR]],[Specify the library path for mysql]),
    [MYSQL_LIBS="$withval"])
AC_ARG_WITH(mysql-include,
    AS_HELP_STRING([--with-mysql-include[=DIR]],[Specify the include path for mysql]),
    [MYSQL_HEADERS="$withval"])


if test "${MYSQL}" = "no"; then
    AC_MSG_CHECKING(for mysql support)
    AC_MSG_RESULT(skipped)
else
    AC_MSG_CHECKING(for db package config)
    if pkg-config mariadb; then
        AC_MSG_RESULT(mariadb found)
        MYSQL_INCDIR=`pkg-config --cflags mariadb`
        MYSQL_LIBDIR=`pkg-config --libs mariadb`
    elif pkg-config mysqlclient; then
        AC_MSG_RESULT(mysqlclient found)
        MYSQL_INCDIR=`pkg-config --cflags mysqlclient`
        MYSQL_LIBDIR=`pkg-config --libs mysqlclient`
    else
        AC_MSG_RESULT(not found)
        if test "${MYSQL_HEADERS}" = "yes"; then
            AC_MSG_CHECKING(for mysql headers)
            for w in $SEARCH_INC; do
                if test -f $w/mysql.h; then
                    MYSQL_INCDIR=$w
                    break
                fi
                if test -f $w/mysql/mysql.h; then
                    MYSQL_INCDIR=$w/mysql
                    break
                fi
                if test -f $w/mysql/include/mysql.h; then
                    MYSQL_INCDIR=$w/mysql/include
                    break
                fi
            done
        elif test "${MYSQL_HEADERS}" = "no"; then
            AC_MSG_CHECKING(for mysql headers)
        else
            AC_MSG_CHECKING(for mysql headers in $MYSQL_HEADERS)
            if test -f $MYSQL_HEADERS/mysql.h; then
                MYSQL_INCDIR=$MYSQL_HEADERS
            fi
        fi

        if test -z "$MYSQL_INCDIR" ; then
            MYSQL_HEADERS="no"
            AC_MSG_RESULT(not found)
        else
            AC_MSG_RESULT($MYSQL_INCDIR yes)
            MYSQL_HEADERS="yes"
        fi

    	# ******* Search mysql libs *********
        if test "${MYSQL_LIBS}" = "yes"; then
            AC_MSG_CHECKING(for mysql libs)
            for w in $SEARCH_LIB; do
                if test -f $w/libmysqlclient.a -o -f $w/libmysqlclient.so; then
                    MYSQL_LIBDIR=$w
                    break
                fi
                if test -f $w/mysql/libmysqlclient.a -o -f $w/mysql/libmysqlclient.so; then
                    MYSQL_LIBDIR=$w/mysql
                    break
                fi
                if test -f $w/mysql/lib/libmysqlclient.a -o -f $w/mysql/lib/libmysqlclient.so; then
                    MYSQL_LIBDIR=$w/mysql/lib
                    break
                fi
            done
            AC_MSG_RESULT($MYSQL_LIBDIR)
        elif test "${MYSQL_LIBS}" = "no"; then
            AC_MSG_CHECKING(for mysql libs)
            AC_MSG_RESULT(skipped)
        else
            AC_MSG_CHECKING(for mysql libs in $MYSQL_LIBS)
            if test -f $MYSQL_LIBS/libmysqlclient.a -o -f $MYSQL_LIBS/libmysqlclient.so; then
                MYSQL_LIBDIR=$MYSQL_LIBS
            fi
            AC_MSG_RESULT($MYSQL_LIBS)
        fi
        if test "x$MYSQL_INCDIR" != "x"; then MYSQL_INCDIR="-I$MYSQL_INCDIR"; fi
        if test "x$MYSQL_LIBDIR" != "x"; then
            MYSQL_LIBDIR=" -L$MYSQL_LIBDIR -lmysqlclient -lz";
        else
            MYSQL_LIBDIR=" -lmysqlclient -lz"
        fi
    fi
    # ******* Validate MYSQL *********
    HOLD_CFLAGS=$CFLAGS
    HOLD_LIBS=$LIBS
    CFLAGS=$MYSQL_INCDIR
    LIBS=$MYSQL_LIBDIR
    AC_CHECK_LIB(mysqlclient,mysql_init,[
        TEMP_CFLAGS="$TEMP_CFLAGS $MYSQL_INCDIR"
        TEMP_LIBS="$TEMP_LIBS $MYSQL_LIBDIR"
        MYSQL_SUPPORT="yes"
        AC_DEFINE([HAVE_MYSQL],1,[Define to 1 if you have MYSQL support])])
    CFLAGS=$HOLD_CFLAGS
    LIBS=$HOLD_LIBS
fi


##############################################################################
###  Check PostgreSQL
##############################################################################
PGSQL="yes"
PGSQL_SUPPORT="no"
PGSQL_HEADERS="yes"
PGSQL_LIBS="yes"
PGSQL_INCDIR=""
PGSQL_LIBDIR=""

AC_ARG_WITH(pgsql,
    AS_HELP_STRING([--without-pgsql],[Disable pgsql support]),
    [PGSQL="$withval"])
AC_ARG_WITH(pgsql-lib,
    AS_HELP_STRING([--with-pgsql-lib[=DIR]],[Specify the library path for pgsql]),
    [PGSQL_LIBS="$withval"])
AC_ARG_WITH(pgsql-include,
    AS_HELP_STRING([--with-pgsql-include[=DIR]],[Specify the include path for pgsql]),
    [PGSQL_HEADERS="$withval"])

if test "${PGSQL}" = "no"; then
    AC_MSG_CHECKING(for pgsql support)
    AC_MSG_RESULT(skipped)
else
    AC_MSG_CHECKING(for pgsql package config)
    if pkg-config libpq; then
        AC_MSG_RESULT(found)
        PGSQL_INCDIR=`pkg-config --cflags libpq`
        PGSQL_LIBDIR=`pkg-config --libs libpq`
    else
        AC_MSG_RESULT(not found)
        if test "${PGSQL_HEADERS}" = "yes"; then
            AC_MSG_CHECKING(for pgsql headers)
            for w in $SEARCH_INC; do
                if test -f $w/libpq-fe.h; then
                    PGSQL_INCDIR=$w
                    break
                fi
                if test -f $w/postgresql/libpq-fe.h; then
                    PGSQL_INCDIR=$w/postgresql
                    break
                fi
                if test -f $w/postgresql/include/libpq-fe.h; then
                    PGSQL_INCDIR=$w/postgresql/include
                    break
                fi
            done
        elif test "${PGSQL_HEADERS}" = "no"; then
            AC_MSG_CHECKING(for pgsql headers)
            AC_MSG_RESULT(skipped)
        else
            AC_MSG_CHECKING(for pgsql headers in $PGSQL_HEADERS)
            if test -f $PGSQL_HEADERS/libpq-fe.h; then
                PGSQL_INCDIR=$PGSQL_HEADERS
            fi
        fi

        if test -z "$PGSQL_INCDIR" ; then
            PGSQL_HEADERS="no"
            AC_MSG_RESULT(not found)
        else
            AC_MSG_RESULT($PGSQL_INCDIR yes)
            PGSQL_HEADERS="yes"
        fi

    	# ******* Search pgsql libs *********
        if test "${PGSQL_LIBS}" = "yes"; then
            AC_MSG_CHECKING(for pgsql libs)
            for w in $SEARCH_LIB; do
                if test -f $w/libpq.a -o -f $w/libpq.so; then
                    PGSQL_LIBDIR=$w
                    break
                fi
                if test -f $w/postgresql/libpq.a -o -f $w/postgresql/libpq.so; then
                    PGSQL_LIBDIR=$w/postgresql
                    break
                fi
                if test -f $w/postgresql/lib/libpq.a -o -f $w/postgresql/lib/libpq.so; then
                    PGSQL_LIBDIR=$w/postgresql/lib
                    break
                fi
            done
            AC_MSG_RESULT($PGSQL_LIBDIR)
        elif test "${PGSQL_LIBS}" = "no"; then
            AC_MSG_CHECKING(for pgsql libs)
            AC_MSG_RESULT(skipped)
        else
            AC_MSG_CHECKING(for pgsql libs in $PGSQL_LIBS)
            if test -f $PGSQL_LIBS/libpq.a -o -f $PGSQL_LIBS/libpq.so; then
                PGSQL_LIBDIR=$PGSQL_LIBS
            fi
            AC_MSG_RESULT($PGSQL_LIBDIR)
        fi
        if test "x$PGSQL_INCDIR" != "x"; then PGSQL_INCDIR="-I$PGSQL_INCDIR"; fi
        if test "x$PGSQL_LIBDIR" != "x"; then
            PGSQL_LIBDIR=" -L$PGSQL_LIBS -lpq";
        else
            PGSQL_LIBDIR=" -lpq"
        fi
    fi
    # ******* Validate PGSQL *********
    HOLD_CFLAGS=$CFLAGS
    HOLD_LIBS=$LIBS
    CFLAGS=$PGSQL_INCDIR
    LIBS=$PGSQL_LIBDIR
    AC_CHECK_LIB(pq, PQconnectStart, [
        TEMP_CFLAGS="$TEMP_CFLAGS $PGSQL_INCDIR"
        TEMP_LIBS="$TEMP_LIBS $PGSQL_LIBDIR"
        PGSQL_SUPPORT="yes"
        AC_DEFINE([HAVE_PGSQL],1,[Define to 1 if you have PGSQL support])])
    CFLAGS=$HOLD_CFLAGS
    LIBS=$HOLD_LIBS
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
if test "${OPTIMIZECPU}" = "yes"; then
    if test -e "/proc/device-tree/model"; then
        # explicit test for RPI3 as /proc/cpuinfo reports armv7 even though it is armv8
        RPI3=`grep "Raspberry Pi 3 Model" /proc/device-tree/model`
        if test "x${RPI3}" != "x"; then
            CPU_OPTIONS="-mcpu=cortex-a53 -mfpu=neon-fp-armv8"
        fi
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
    TEMP_CFLAGS="${TEMP_CFLAGS} -W -Werror -Wall -Wextra -Wformat -Wshadow -Wpointer-arith -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline -Wredundant-decls -Wno-long-long -ggdb -g3"
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

if test "${MYSQL_SUPPORT}" = "yes"; then
    echo "MYSQL support:       Yes"
else
    echo "MYSQL support:       No"
fi

if test "${PGSQL_SUPPORT}" = "yes"; then
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
