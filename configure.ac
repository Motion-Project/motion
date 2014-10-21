# Process this file with autoconf to produce a configure script

AC_INIT(motion, esyscmd(['./version.sh']))
AC_GNU_SOURCE
AC_CONFIG_SRCDIR([motion.c])
AC_CONFIG_HEADERS(config.h)
AC_PROG_CC


THREAD_CFLAGS=""
THREAD_CHECK="/usr/include/pthread.h"

Darwin=""
FreeBSD=""

LINUXTHREADS="no"
AC_ARG_WITH(linuxthreads,
[  --with-linuxthreads     Use linuxthreads in BSD instead of native pthreads
				]
				,
LINUXTHREADS="$withval"
)

PWCBSD="no"
AC_ARG_WITH(pwcbsd,
[  --with-pwcbsd           Use pwcbsd based webcams ( only BSD )
]
,
PWCBSD="$withval"
)


AC_MSG_CHECKING(for Darwin)
Darwin=`uname -a | grep "Darwin"`

if test "${Darwin}" = ""; then
	AC_MSG_RESULT(no)
	AC_MSG_CHECKING(for *BSD)

	FreeBSD=`uname -a | grep "BSD"`
	if test "${FreeBSD}" = ""; then
		AC_MSG_RESULT(no)
		VIDEO="video.o video2.o video_common.o"
	else
		AC_MSG_RESULT(yes)
		if test "${LINUXTHREADS}" = "no"; then
			AC_MSG_CHECKING(Linuxthreads)
			AC_MSG_RESULT(skipping)
		else
			THREAD_CHECK="/usr/local/include/pthread/linuxthreads/pthread.h"
			THREAD_LIB_CHECK="/usr/local/lib/liblthread.so"
		fi

		if test "${PWCBSD}" != "no"; then
			VIDEO="video.o video2.o video_common.o"
			TEMP_CFLAGS="${CFLAGS} -I/usr/local/include -DPWCBSD"
		else
			VIDEO="video_freebsd.o"
			TEMP_CFLAGS="${CFLAGS} -I/usr/local/include"
		fi

		TEMP_LDFLAGS="${LDFLAGS} -L/usr/local/lib"
		TEMP_LIBS="-L/usr/local/lib"
		AC_MSG_RESULT(Build with PWCBSD support $PWCBSD)

	fi
else
	TEMP_CFLAGS="${CFLAGS} -I/sw/include"
	TEMP_LDFLAGS="${LDFLAGS} -L/sw/lib"
	TEMP_LIBS="-L/sw/lib"
	VIDEO="video_freebsd.o"
	FINK_LIB="-L/sw/lib"
	Darwin="yes"
	V4L="no"
	AC_MSG_RESULT($Darwin)
fi


AC_SUBST(VIDEO)

# Checks for programs.
AC_PROG_CC

TEMP_LIBS="-lm ${TEMP_LIBS}"
TEMP_CFLAGS="${TEMP_CFLAGS} ${CFLAGS}"
TEMP_LDFLAGS="${TEMP_LDFLAGS} ${LDFLAGS}"

if test "${FreeBSD}" != "" && test "${PWCBSD}" = "no"; then

	AC_MSG_CHECKING(bktr headers in /usr/include/dev/bktr)

	if test -f /usr/include/dev/bktr/ioctl_meteor.h && test -f /usr/include/dev/bktr/ioctl_bt848.h; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
		TEMP_CFLAGS="${TEMP_CFLAGS} -DOLD_BKTR"
	fi
#
# Check to Exclude BKTR
#
BKTR="yes"
AC_ARG_WITH(bktr,
[  --without-bktr          Exclude to use bktr subsystem , that usually useful
                          for devices as network cameras ( ONLY used in *BSD).
                          ]
                          ,
BKTR="$withval"
)

	if test "${BKTR}" = "no"; then
		TEMP_CFLAGS="${TEMP_CFLAGS} -DWITHOUT_V4L"
	fi

else

#
# Check to Exclude V4L
#
V4L="yes"
AC_ARG_WITH(v4l,
[  --without-v4l           Exclude using v4l (video4linux) subsystem.
                          Makes Motion so it only supports network cameras.
                          ],
V4L="$withval"
)

fi


if test "${V4L}" = "no"; then
	TEMP_CFLAGS="${TEMP_CFLAGS} -DWITHOUT_V4L"
fi


if test "${FreeBSD}" != "" && test "${LINUXTHREADS}" != "no" ; then

AC_MSG_CHECKING(for linuxthreads)

#
# Check for thread header
#
	if test -f "${THREAD_CHECK}"; then
		HEADERS_THREAD_CFLAGS="-I/usr/local/include/pthread/linuxthreads"
		THREADS="yes"
	else
		THREADS="no"
	fi

#
# Check for thread lib
#
	if test -f "${THREAD_LIB_CHECK}" ; then
		THREADS="yes"
		LIB_THREAD="-llthread -llgcc_r"
	else
		THREADS="no"
	fi

# Checks for Library linuxthreads for FreeBSD
#
# linuxthreads on freeBSD, ports collection
# /usr/local/include/pthreads/linuxthreads/pthread.h
# #include <linuxthreads/pthread.h>
# /usr/local/lib/libpthread.so
#

	if test "${THREADS}" = "yes"; then
		TEMP_CFLAGS="${HEADERS_THREAD_CFLAGS} $TEMP_CFLAGS -DWITH_LINUXTREADS"
        TEMP_LIBS="$TEMP_LIBS ${LIB_THREAD}"
        THREAD_CFLAGS="-D_THREAD_SAFE"
        PTHREAD_SUPPORT="yes"
        AC_MSG_RESULT($THREADS)
    else
        PTHREAD_SUPPORT="no"
        AC_MSG_RESULT($THREADS)
        echo
        echo "You do not have linuxthread installed"
        echo
    fi

elif test -f "${THREAD_CHECK}"; then


AC_MSG_CHECKING(threads)
AC_TRY_LINK([#include <pthread.h>],
[pthread_t th; pthread_join(th, 0);
 pthread_attr_init(0); pthread_cleanup_push(0, 0);
 pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
 [PTHREAD_LIB=yes])

if test x$PTHREAD_LIB != xyes; then

	if test "${FreeBSD}" != ""; then
		TEMP_LIBS="$TEMP_LIBS -pthread"
		TEMP_CFLAGS="${TEMP_CFLAGS} -D_REENTRANT -D_THREAD_SAFE"
	else
		TEMP_LIBS="$TEMP_LIBS -lpthread"
		TEMP_CFLAGS="${TEMP_CFLAGS} -D_REENTRANT"
	fi
	PTHREAD_SUPPORT="yes"
fi
	AC_MSG_RESULT($PTHREAD_SUPPORT)

else
	echo
	echo "You do not have threads support"
	echo
fi


#
# Check for sdl library
#
SDL_SUPPORT="no"
AC_ARG_WITH(sdl,
[  --without-sdl           Compile without sdl support to get stream in SDL window.
],
[],
[])
AC_MSG_CHECKING(for sdl)
if test "x$withval" = "xno"; then
	AC_MSG_RESULT(skipped)
else
	if test "${FreeBSD}" != ""; then
		CONFIG_SDL='sdl11-config'
	else
		CONFIG_SDL='sdl-config'
	fi
	if test -z "`($CONFIG_SDL --version) 2>/dev/null`" ;then
		AC_MSG_RESULT(no)
		if test "$withval" = "yes"; then
			echo ""
			echo "****************************************************"
			echo "* sdl-config could not be found. Please install it *"
			echo "* and remove the --with-sdl configure argument.    *"
			echo "* libSDL can be found at http://www.libsdl.org     *"
			echo "****************************************************"
			echo ""
		fi
	else
		AC_MSG_RESULT(yes)
		SDL_SUPPORT="yes"
		TEMP_LIBS="$TEMP_LIBS `${CONFIG_SDL} --libs`"
		TEMP_CFLAGS="${TEMP_CFLAGS} `${CONFIG_SDL} --cflags`"
		AC_DEFINE([HAVE_SDL],1,[Define to 1 if you have SDL support])
		SDL_OBJ="sdl.o"
		AC_SUBST(SDL_OBJ)
	fi
fi

#
# Check for the libjpeg-turbo library
#
JPEG_TURBO="no"
JPEG_TURBO_OK="not_found"

AC_ARG_WITH(jpeg-turbo,
[  --with-jpeg-turbo[=DIR]   Specify the prefix for the install path for
                          jpeg-turbo for optimized jpeg handling (optional).
                          ],
JPEG_TURBO="$withval"
)

if test "${JPEG_TURBO}" = "no"; then
    AC_MSG_CHECKING(for libjpeg-turbo)
    AC_MSG_RESULT(skipping)
else
    AC_MSG_CHECKING(for libjpeg-turbo in -> [${JPEG_TURBO}] <-)
    if test -f ${JPEG_TURBO}/lib/libjpeg.a ; then
        AC_MSG_RESULT(found)
        JPEG_TURBO_OK="found"
    else
        AC_MSG_RESULT(not found)
    fi
fi


if test "${JPEG_TURBO_OK}" = "found"; then
    saved_CFLAGS="$CFLAGS"
    saved_LIBS="$LIBS"
    saved_LDFLAGS="$LDFLAGS"
    LDFLAGS="-L${JPEG_TURBO}/lib"
    CFLAGS="$CFLAGS -I${JPEG_TURBO}/include"
    LIBS="$LIBS -L${JPEG_TURBO}/lib -ljpeg"
    AC_CHECK_LIB(jpeg, jpeg_start_compress,
        [ TEMP_LIBS="$LIBS"
          TEMP_CFLAGS="${CFLAGS}"
          TEMP_LDFLAGS="$TEMP_LDFLAGS $LDFLAGS"
          JPEG_SUPPORT="yes"],,)
    LIBS="$saved_LIBS"
    CFLAGS="$saved_CFLAGS"
    LDFLAGS="$saved_LDFLAGS"
    JPEG_SUPPORT_TURBO="yes"
fi


#
# Check for the special mmx accelerated jpeg library
#
JPEG_MMX="no"
JPEG_MMX_OK="not_found"
AC_ARG_WITH(jpeg-mmx,
[  --with-jpeg-mmx[=DIR]     Specify the prefix for the install path for
                          jpeg-mmx for optimized jpeg handling (optional).
                          If this is not specified motion will try to find
                          the library /usr/lib/libjpeg-mmx.a /usr/local/lib/libjpeg-mmx.a.
                          ],
JPEG_MMX="$withval"
)

#
# --without-jpeg-mmx or with-jpeg-mmx=no
#

if test "${JPEG_MMX}" = "no" || test x$JPEG_SUPPORT != xyes; then
        AC_MSG_CHECKING(for libjpeg-mmx)
        AC_MSG_RESULT(skipping)
elif test "${JPEG_MMX}" = "yes"; then
	# AUTODETECT STATIC LIB
	AC_MSG_CHECKING(for libjpeg-mmx autodetecting)

	if test -f /usr/lib/libjpeg-mmx.a ; then
		AC_MSG_RESULT(found)
		JPEG_MMX_OK="found"
		JPEG_MMX="/usr/lib"
	elif test -f /usr/local/lib/libjpeg-mmx.a ; then
		AC_MSG_RESULT(found)
		JPEG_MMX_OK="found"
		JPEG_MMX="/usr/local/lib"
	else
		AC_MSG_RESULT(not found)
	fi
else
	AC_MSG_CHECKING(for libjpeg-mmx in -> [${JPEG_MMX}] <-)
	if test -f ${JPEG_MMX}/libjpeg-mmx.a ; then
		AC_MSG_RESULT(found)
		JPEG_MMX_OK="found"
	else
		AC_MSG_RESULT(not found)
	fi
fi

if test "${JPEG_MMX_OK}" = "found"; then
	saved_CFLAGS="$CFLAGS"
	saved_LIBS="$LIBS"
	CFLAGS="$CFLAGS -I${JPEG_MMX}"
	LIBS="$LIBS -L${JPEG_MMX}"
	AC_CHECK_LIB(jpeg-mmx, jpeg_start_compress,
		[ TEMP_LIBS="$TEMP_LIBS -ljpeg-mmx"
		TEMP_CFLAGS="${TEMP_CFLAGS} -I${JPEG_MMX}"
		JPEG_SUPPORT="yes"],,)
	LIBS="$saved_LIBS"
	CFLAGS="$saved_CFLAGS"
	JPEG_SUPPORT_MMX="yes"
fi

#
# Look for _a_ jpeg lib that will work.
#
if test x$JPEG_SUPPORT != xyes ; then
  # Checks for libraries
  LDFLAGS=$TEMP_LDFLAGS

  AC_CHECK_LIB(jpeg, jpeg_set_defaults, [
		TEMP_LIBS="$TEMP_LIBS -ljpeg"
		JPEG_SUPPORT="yes"
	], [
		echo
		echo "You do not have libjpeg installed"
		echo
	]
  )
fi


#
# Check for libavcodec and libavformat from ffmpeg
#

FFMPEG_DIR="yes"
FFMPEG_OK="no_found"
FFMPEG_OBJ=""
AC_ARG_WITH(ffmpeg,
[  --with-ffmpeg[=DIR]       Specify the prefix for the install path for
                          libavcodec/libavformat (part of ffmpeg) be able to
                          encode mpeg movies realtime.
                          If this is not specified motion will try to find
                          the libraries in /usr and /usr/local.
                          ],
FFMPEG_DIR="$withval"
)

#
# ffmpeg headers custom location
#
FFMPEG_HEADERS_DIR="yes"
AC_ARG_WITH(ffmpeg_headers,
[  --with-ffmpeg-headers[=DIR] Specify the prefix for ffmpeg headers.
                               ],
FFMPEG_HEADERS_DIR="$withval"
)

#
# ffmpeg custom extra libraries
#
FFMPEG_EXTRALIBS=" -lavformat -lavcodec -lavutil -lm -lz -lswscale "
AC_ARG_WITH(ffmpeg-libs,
[  --with-ffmpeg-libs[=libs]  Specify the extra libs for ffmpeg
                               ],
FFMPEG_EXTRALIBS="$withval"
)

#
# --without-ffmpeg or with-ffmpeg=no
#
if test "${FFMPEG_DIR}" = "no"; then
	AC_MSG_CHECKING(for ffmpeg)
	AC_MSG_RESULT(skipping)
#
# with-ffmpeg=<dir> or nothing
#
else if test "${FFMPEG_DIR}" = "yes"; then
	# AUTODETECT STATIC/SHARED LIB
	AC_MSG_CHECKING(for ffmpeg autodetecting libraries)

	if test -f /usr/lib64/libavcodec.a -o -f /usr/lib64/libavcodec.so && test -f /usr/lib64/libavformat.a -o -f /usr/lib64/libavformat.so ; then
		AC_MSG_RESULT(found in /usr/lib64)
		FFMPEG_OK="found"
		FFMPEG_LIB="/usr/lib64"
		FFMPEG_DIR="/usr"
	elif test -f /usr/lib/libavcodec.a -o -f /usr/lib/libavcodec.so && test -f /usr/lib/libavformat.a -o -f /usr/lib/libavformat.so ; then
		AC_MSG_RESULT(found in /usr/lib)
		FFMPEG_OK="found"
		FFMPEG_LIB="/usr/lib"
		FFMPEG_DIR="/usr"
	elif test -f /usr/local/lib/libavcodec.a -o -f /usr/local/lib/libavcodec.so && test -f /usr/local/lib/libavformat.a -o -f /usr/local/lib/libavformat.so ; then
		AC_MSG_RESULT(found in /usr/local/lib)
		FFMPEG_OK="found"
		FFMPEG_LIB="/usr/local/lib"
		FFMPEG_DIR="/usr/local"
    elif test -f /usr/lib/x86_64-linux-gnu/libavcodec.a -o -f /usr/lib/x86_64-linux-gnu/libavcodec.so && test /usr/lib/x86_64-linux-gnu/libavformat.a -o -f /usr/lib/x86_64-linux-gnu/libavformat.so ; then
        AC_MSG_RESULT(found in /usr/lib/x86_64-linux-gnu)
        FFMPEG_OK="found"
        FFMPEG_LIB="/usr/lib/x86_64-linux-gnu"
        FFMPEG_DIR="/usr"
	elif test -f /usr/lib/i386-linux-gnu/libavcodec.a -o -f /usr/lib/i386-linux-gnu/libavcodec.so && test /usr/lib/i386-linux-gnu/libavformat.a -o -f /usr/lib/i386-linux-gnu/libavformat.so ; then
        AC_MSG_RESULT(found in /usr/lib/i386-linux-gnu)
        FFMPEG_OK="found"
        FFMPEG_LIB="/usr/lib/i386-linux-gnu"
        FFMPEG_DIR="/usr"
	else
		AC_MSG_RESULT(not found)
		echo ""
		echo "**********************************************"
		echo "* libavcodec.a or libavcodec.so or           *"
		echo "* libavformat.a or libavformat.so not found: *"
		echo "*    ALL FFMPEG FEATURES DISABLED            *"
		echo "*                                            *"
		echo "* Please read the Motion Guide for help:     *"
		echo "* http://motion.sourceforge.net              *"
		echo "**********************************************"
		echo ""
	fi
else
	AC_MSG_CHECKING(for ffmpeg libraries in -> [${FFMPEG_DIR}] <-)
	if test -f ${FFMPEG_DIR}/lib/libavcodec.a -o -f ${FFMPEG_DIR}/lib/libavcodec.so && test -f ${FFMPEG_DIR}/lib/libavformat.a -o -f ${FFMPEG_DIR}/lib/libavformat.so ; then
		AC_MSG_RESULT(found)
		FFMPEG_OK="found"
		FFMPEG_LIB="${FFMPEG_DIR}/lib"
	elif test -f ${FFMPEG_DIR}/libavcodec.a -o -f ${FFMPEG_DIR}/libavcodec.so && test -f ${FFMPEG_DIR}/libavformat.a -o -f ${FFMPEG_DIR}/libavformat.so ; then
		AC_MSG_RESULT(found)
		FFMPEG_LIB="${FFMPEG_DIR}"
		FFMPEG_OK="found"
	else
		AC_MSG_RESULT(not found)
		if test "${FFMPEG_OK}" != "found"; then
			echo ""
			echo "**********************************************"
			echo "* libavcodec.a or libavcodec.so or           *"
			echo "* libavformat.a or libavformat.so not found: *"
			echo "*    ALL FFMPEG FEATURES DISABLED            *"
			echo "*                                            *"
			echo "* Please read the Motion Guide for help:     *"
			echo "* http://motion.sourceforge.net              *"
			echo "**********************************************"
			echo ""
		fi
	fi
fi


#
# Now check for ffmpeg headers ( avformat.h ) if ffmpeg libs were found
#

AVFORMAT_DIR=""

if test "${FFMPEG_OK}" = "found"; then
    if test "${FFMPEG_HEADERS_DIR}" = "yes"; then
        AC_MSG_CHECKING(for ffmpeg headers in ${FFMPEG_DIR})
        AVFORMAT_DIR="${FFMPEG_DIR}"
    else
        AC_MSG_CHECKING(for ffmpeg headers in ${FFMPEG_HEADERS_DIR})
        FFMPEG_DIR="${FFMPEG_HEADERS_DIR}"
        AVFORMAT_DIR="${FFMPEG_DIR}"
    fi

    if test -f ${FFMPEG_DIR}/include/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/include/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR}/include"
        AVFORMAT_DIR="${FFMPEG_DIR}/include/avformat.h"
    elif test -f ${FFMPEG_DIR}/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR}"
        AVFORMAT_DIR="${FFMPEG_DIR}/avformat.h"
    elif test -f ${FFMPEG_DIR}/include/ffmpeg/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/include/ffmpeg/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR}/include/ffmpeg"
        AVFORMAT_DIR="${FFMPEG_DIR}/include/ffmpeg/avformat.h"
    elif test -f ${FFMPEG_DIR}/include/libavformat/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/include/libavformat/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR}/include -DFFMPEG_NEW_INCLUDES"
        AVFORMAT="-I${FFMPEG_DIR}/include/libavformat"
        AVFORMAT_DIR="${FFMPEG_DIR}/include/libavformat/avformat.h"
    elif test -f ${FFMPEG_DIR}/include/ffmpeg/libavformat/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/include/ffmpeg/libavformat/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR}/include/ffmpeg -DFFMPEG_NEW_INCLUDES"
        AVFORMAT="-I${FFMPEG_DIR}/include/ffmpeg/libavformat"
        AVFORMAT_DIR="${FFMPEG_DIR}/include/ffmpeg/libavformat/avformat.h"
    elif test -f ${FFMPEG_DIR}/libavformat/avformat.h; then
        AC_MSG_RESULT(found ${FFMPEG_DIR}/libavformat/avformat.h)
        FFMPEG_CFLAGS="-I${FFMPEG_DIR} -DFFMPEG_NEW_INCLUDES"
        AVFORMAT="-I{FFMPEG_DIR}/libavformat"
        AVFORMAT_DIR="${FFMPEG_DIR}/libavformat/avformat.h"
    else
        AC_MSG_RESULT(not found)
        FFMPEG_OK="no_found"
        AVFORMAT_DIR="avformat.h"
        echo "**********************************************"
        echo "*       avformat.h not found:                *"
        echo "*    ALL FFMPEG FEATURES DISABLED            *"
        echo "*                                            *"
        echo "* Please read the Motion Guide for help:     *"
        echo "* http://motion.sourceforge.net              *"
        echo "**********************************************"
        echo ""
    fi

#
# If ffmpeg libs and headers have been found
#

    if  test "${FFMPEG_OK}" = "found"; then
        TEMP_LIBS="$TEMP_LIBS -L${FFMPEG_LIB} ${FFMPEG_EXTRALIBS}"
        TEMP_LDFLAGS="${TEMP_LDFLAGS} -L${FFMPEG_LIB}"
        TEMP_CFLAGS="${TEMP_CFLAGS} -DHAVE_FFMPEG ${FFMPEG_CFLAGS}"

        FFMPEG_OBJ="ffmpeg.o"
        AC_SUBST(FFMPEG_OBJ)

        AC_MSG_CHECKING([file_protocol is defined in ffmpeg ?])
        saved_CFLAGS=$CFLAGS
        saved_LIBS=$LIBS

        CFLAGS="${FFMPEG_CFLAGS} ${AVFORMAT}"
        LIBS="$TEMP_LIBS"

        AC_COMPILE_IFELSE([AC_LANG_SOURCE([
            [
             #include <${AVFORMAT_DIR}>
             URLProtocol test_file_protocol;
             int main(void){
                 test_file_protocol.url_read  = file_protocol.url_read;
                 return 0;
             }
            ]])],
            [AC_MSG_RESULT(yes)],
            [
             AC_MSG_RESULT(no)
             TEMP_CFLAGS="${TEMP_CFLAGS} -DHAVE_FFMPEG_NEW"
            ]
        )
        CFLAGS=$saved_CFLAGS
        LIBS=$saved_LIBS
    fi
fi
fi

# Revised RTSP module so that it can be included
# whether or not FFMPEG is found.

    RTPS_OBJ="netcam_rtsp.o"
    AC_SUBST(RTPS_OBJ)

#
# Check SQLITE3
#

SQLITE3_SUPPORT="no"
AC_ARG_WITH(sqlite3,
    [  --without-sqlite3       Disable sqlite3 support in motion.
    ],
    SQLITE3="$withval"
    # if not given argument, assume standard
)

if test "${SQLITE3}" = "no"; then
    AC_MSG_CHECKING(for sqlite3)
    AC_MSG_RESULT(skipping)
else
    saved_CFLAGS=$CFLAGS
    saved_LIBS=$LIBS

    AC_CHECK_LIB(sqlite3, sqlite3_open,
    [
    TEMP_LIBS="$TEMP_LIBS -lsqlite3"
    SQLITE3_SUPPORT="yes"
    AC_DEFINE([HAVE_SQLITE3],1,[Define to 1 if you have SQLITE3 support])
    ]
    )

    CFLAGS=$saved_CFLAGS
    LIBS=$saved_LIBS
fi


#
# Check Mysql
#

MYSQL="yes"
MYSQL_SUPPORT="no"
MYSQL_HEADERS="yes"
MYSQL_LIBS="yes"

AC_MSG_CHECKING(for mysql support)

AC_ARG_WITH(mysql,
[  --without-mysql         Disable mysql support in motion.
                          ],
MYSQL="$withval"
# if not given argument, assume standard
)

AC_ARG_WITH(mysql-lib,
[  --with-mysql-lib[=DIR]        Normally, configure will scan all possible default
                          installation paths for mysql libs. When it fails, use
                          this command to tell configure where mysql libs
                          installation root directory is.
                          ],
MYSQL_LIBS="$withval"
# if not given argument, assume standard
)


AC_ARG_WITH(mysql-include,
[  --with-mysql-include[=DIR]    Normally, configure will scan all possible default
                          installation paths for mysql include. When it fails, use
                          this command to tell configure where mysql include
                          installation root directory is.
                          ],
MYSQL_HEADERS="$withval"
# if not given argument, assume standard
)


if test "${MYSQL}" = "no"; then
	AC_MSG_RESULT(skipped)
else
	AC_MSG_RESULT(testing)
	# ******* Search mysql headers *******

	if test "${MYSQL_HEADERS}" = "yes"; then
		AC_MSG_CHECKING(autodect mysql headers)
		# Autodetect
		for w in /usr/include /usr/local/include /usr/mysql /usr/local/mysql /usr/local/mysql/include /opt /opt/mysql; do
			# check for plain setups
			if test -f $w/mysql.h; then
				MYSQL_INCDIR=$w
				break
			fi
			# check for "/usr/include/<packagename>" type setups
			if test -f $w/mysql/mysql.h; then
				MYSQL_INCDIR=$w/mysql
				break
			fi
			# check for "/usr/<packagename>/include" type setups
			if test -f $w/mysql/include/mysql.h; then
				MYSQL_INCDIR=$w/mysql/include
				break
			fi
		done
	elif test "${MYSQL_HEADERS}" = "no"; then
		AC_MSG_CHECKING(for mysql headers)
		AC_MSG_RESULT(skipped)
	else
		AC_MSG_CHECKING(for mysql headers in $MYSQL_HEADERS)
		# Manual detection for <withval>
		if test -f $MYSQL_HEADERS/mysql.h; then
			MYSQL_INCDIR=$MYSQL_HEADERS
	        fi
	fi

	if test -z "$MYSQL_INCDIR" ; then
		MYSQL_HEADERS="no"
		AC_MSG_RESULT(not found)
		echo "Invalid MySQL directory - unable to find mysql.h."
	else
		AC_MSG_RESULT($MYSQL_INCDIR yes)
		MYSQL_HEADERS="yes"
	fi


	if test "${MYSQL_HEADERS}" = "yes"; then

	# ******* Search mysql libs *********
	if test "${MYSQL_LIBS}" = "yes"; then
		AC_MSG_CHECKING(autodect mysql libs)
		# Autodetect
		for w in /usr/lib64 /usr/lib /usr/local/lib /usr/mysql /usr/local/mysql /usr/local/mysql/lib /opt /opt/mysql /usr/lib/x86_64-linux-gnu; do
			# check for plain setups
			if test -f $w/libmysqlclient.a -o -f $w/libmysqlclient.so; then
				MYSQL_LIBDIR=$w
				break
			fi
			# check for "/usr/lib/<packagename>" type setups
			if test -f $w/mysql/libmysqlclient.a -o -f $w/mysql/libmysqlclient.so; then
				MYSQL_LIBDIR=$w/mysql
				break
			fi
			# check for "/usr/<packagename>/lib" type setups
			if test -f $w/mysql/lib/libmysqlclient.a -o -f $w/mysql/lib/libmysqlclient.so; then
				MYSQL_LIBDIR=$w/mysql/lib
				break
			fi
		done
	elif test "${MYSQL_LIBS}" = "no"; then
		AC_MSG_CHECKING(for mysql libs)
	        AC_MSG_RESULT(skipped)
	else
		AC_MSG_CHECKING(for mysql libs in $MYSQL_LIBS)
		# Manual detection for <withval>
		if test -f $MYSQL_LIBS/libmysqlclient.a -o -f $MYSQL_LIBS/libmysqlclient.so; then
			MYSQL_LIBDIR=$MYSQL_LIBS
		fi
	fi


	if test -z "$MYSQL_LIBDIR" ; then
		AC_MSG_RESULT(not found)
		echo "Invalid MySQL directory - unable to find libmysqlclient.a or libmysqlclient.so."
	else
		AC_MSG_RESULT($MYSQL_LIBDIR)
		#LDFLAGS="-L$MYSQL_LIBDIR"
		saved_CFLAGS=$CFLAGS
		saved_LIBS=$LIBS
		CFLAGS="-I$MYSQL_INCDIR"
		LIBS="-L$MYSQL_LIBDIR"
		AC_CHECK_LIB(mysqlclient,mysql_init,[
					TEMP_LIBS="$TEMP_LIBS -L$MYSQL_LIBDIR -lmysqlclient -lz"
					TEMP_CFLAGS="$TEMP_CFLAGS -I$MYSQL_INCDIR"
					MYSQL_SUPPORT="yes"
					AC_DEFINE([HAVE_MYSQL],1,[Define to 1 if you have MySQL support])
					],
					AC_MSG_ERROR(MySQL support can't build without MySQL libraries))
		CFLAGS=$saved_CFLAGS
		LIBS=$saved_LIBS
	fi

	# end mysql-include , mysql-libs
	fi

# end Mysql detection
fi


#
# Check PostgreSQL
#
PGSQL="yes"
PGSQL_SUPPORT="no"
PGSQL_HEADERS="yes"
PGSQL_LIBS="yes"

AC_DEFUN([PGSQL_INC_CHK],[if test -r $i$1/libpq-fe.h; then PGSQL_DIR=$i; PGSQL_INCDIR=$i$1])

AC_ARG_WITH(pgsql,
[  --without-pgsql         Disable PostgreSQL support in motion.
                          ],
PGSQL="$withval"
# if not given argument, assume standard
)

AC_ARG_WITH(pgsql-lib,
[  --with-pgsql-lib[=DIR]        Normally, configure will scan all possible default
                          installation paths for pgsql libs. When it fails, use
                          this command to tell configure where pgsql libs
                          installation root directory is.
                          ],
PGSQL_LIBS="$withval"
# if not given argument, assume standard
)

AC_ARG_WITH(pgsql-include,
[  --with-pgsql-include[=DIR]    Normally, configure will scan all possible default
                          installation paths for pgsql include. When it fails, use
                          this command to tell configure where pgsql include
                          installation root directory is.
                          ],
PGSQL_HEADERS="$withval"
# if not given argument, assume standard
)

AC_MSG_CHECKING(for PostgreSQL)

if test "${PGSQL}" = "no"; then
	AC_MSG_RESULT(skipped)
else
	AC_MSG_RESULT(testing)

	# ******* Search pgsql headers *******
	if test "${PGSQL_HEADERS}" = "yes"; then

		AC_MSG_CHECKING(autodect pgsql headers)
		# Autodetect
		for i in /usr /usr/local /usr/local/pgsql $PHP_PGSQL; do
			PGSQL_INC_CHK(/include)
			el[]PGSQL_INC_CHK(/include/pgsql)
			el[]PGSQL_INC_CHK(/include/postgresql)
			fi
		done

	elif test "${PGSQL_HEADERS}" = "no"; then
		AC_MSG_CHECKING(for pgsql headers)
		AC_MSG_RESULT(skipped)
	else
		AC_MSG_CHECKING(for pgsql headers in $PGSQL_HEADERS)
		# Manual detection for <withval>
		if test -f $PGSQL_HEADERS/libpq-fe.h; then
			PGSQL_INCDIR=$PGSQL_HEADERS
	        fi
	fi

	if test -z "$PGSQL_INCDIR" ; then
		PGSQL_HEADERS="no"
		AC_MSG_RESULT(not found)
		echo "Invalid PostgreSQL directory - unable to find libpq-fe.h."
	else
		AC_MSG_RESULT(yes [$PGSQL_INCDIR])
		PGSQL_HEADERS="yes"
	fi


	if test "${PGSQL_HEADERS}" = "yes"; then

		# ******* Search pgsql libs *********
		if test "${PGSQL_LIBS}" = "yes"; then
			AC_MSG_CHECKING(autodect pgsql libs)
			# Autodetect
			PGSQL_INCLUDE="-I$PGSQL_INCDIR"
			PGSQL_LIBDIR=$PGSQL_DIR/lib

			if test -f /usr/lib64/libpq.so ; then
				PGSQL_LIBDIR=/usr/lib64
			elif test -f $PGSQL_DIR/lib/pgsql/libpq.so ; then
				PGSQL_LIBDIR=$PGSQL_DIR/lib/pgsql
			elif test -f $PGSQL_DIR/lib/postgresql/libpq.so ; then
				PGSQL_LIBDIR=$PGSQL_DIR/lib/postgresql
			elif test -f $PGSQL_DIR/lib/libpq.so ; then
				PGSQL_LIBDIR=$PGSQL_DIR/lib
			else
				PGSQL_LIBDIR=""
			fi

			AC_MSG_RESULT($PGSQL_LIBDIR)

		elif test "${PGSQL_LIBS}" = "no"; then
			AC_MSG_CHECKING(for pgsql libs)
		        AC_MSG_RESULT(skipped)
		else
			AC_MSG_CHECKING(for pgsql libs in $PGSQL_LIBS)
			# Manual detection for <withval>
			if test -f $PGSQL_LIBS/libpq.a -o -f $PGSQL_LIBS/libpq.so; then
				PGSQL_LIBDIR=$PGSQL_LIBS
			fi
		fi


		if test -z "$PGSQL_LIBDIR" ; then
			AC_MSG_RESULT(not found)
			echo "Invalid PostgreSQL directory [$PGSQL_LIBDIR] - unable to find libpq.a or libpq.so."
		else
			#LDFLAGS="$TEMP_LDFLAGS -L$PGSQL_LIBDIR"
			saved_CFLAGS=$CFLAGS
			saved_LIBS=$LIBS
			CFLAGS="-I$PGSQL_INCDIR"
			LIBS="-L$PGSQL_LIBDIR"
			AC_CHECK_LIB(pq, PQconnectStart, [
						PGSQL_SUPPORT="yes"
						TEMP_LIBS="$TEMP_LIBS -L$PGSQL_LIBDIR -lpq"
						TEMP_CFLAGS="$TEMP_CFLAGS -I$PGSQL_INCDIR"
						AC_DEFINE([HAVE_PGSQL],1,[Define to 1 if you have PostgreSQL support])
					],
			AC_MSG_ERROR(PostgreSQL support can't build without PostgreSQL libraries))
			LDFLAGS=""
			CFLAGS=$saved_CFLAGS
			LIBS=$saved_LIBS
		fi

	fi # end pgsql-include , pgsql-libs

# end PostgreSQL detection
fi


#Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stdio.h unistd.h stdint.h fcntl.h time.h signal.h sys/ioctl.h sys/mman.h linux/videodev.h linux/videodev2.h sys/param.h sys/types.h)

AC_CHECK_FUNCS(get_current_dir_name)

# Check if v4l2 is available
SUPPORTED_V4L2=false
SUPPORTED_V4L2_old=false

if test "${V4L}" = "no"; then
	AC_MSG_CHECKING(for V42L support)
	AC_MSG_RESULT(skipping)
else
	AC_CHECK_TYPE([struct v4l2_buffer],
		[SUPPORTED_V4L2=true],
		[SUPPORTED_V4L2=false],
		[#include <sys/time.h>
			#include <linux/videodev.h>])

	AC_MSG_CHECKING(for V42L support)
	if test x$SUPPORTED_V4L2 = xtrue; then
		AC_MSG_RESULT(yes)
		TEMP_CFLAGS="${TEMP_CFLAGS} -DMOTION_V4L2"
	else
		AC_MSG_RESULT(no)
	fi

	# linux/videodev.h doesn't include videodev2.h
	if test x$SUPPORTED_V4L2 = xfalse; then
		AC_MSG_CHECKING(for V42L *old* support)
		AC_MSG_RESULT(testing)
		AC_CHECK_HEADERS(linux/videodev2.h,[SUPPORTED_V4L2_old=true],[], [#include <asm/types.h>])
	fi


	if test x$SUPPORTED_V4L2_old = xtrue; then
		TEMP_CFLAGS="${TEMP_CFLAGS} -DMOTION_V4L2 -DMOTION_V4L2_OLD"
		SUPPORTED_V4L2=true
	fi

fi


# Check sizes of integer types
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long int)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(int *)
AC_CHECK_SIZEOF(void *)

if test "$ac_cv_sizeof_short" = "4"; then
	TEMP_CFLAGS="${TEMP_CFLAGS} -DTYPE_32BIT=\"short\""
else
	if test "$ac_cv_sizeof_int" = "4"; then
		TEMP_CFLAGS="${TEMP_CFLAGS} -DTYPE_32BIT=\"int\""
	else
		if test "$ac_cv_sizeof_long_int" = "4"; then
			TEMP_CFLAGS="${TEMP_CFLAGS} -DTYPE_32BIT=\"long int\""
		fi
	fi
fi


OPTIMIZECPU="yes"

AC_ARG_WITH(optimizecpu,
[  --without-optimizecpu   Exclude autodetecting platform and cpu type.
                          This will disable the compilation of gcc
                          optimizing code by platform and cpu.
			  ],
OPTIMIZECPU="$withval"
)

DEVELOPER_FLAGS="no"

AC_ARG_WITH(developer-flags,
[  --with-developer-flags  Causes practically all of the possible gcc
                          warning flags to be set.  This may produce
                          a large amount of warnings.],
DEVELOPER_FLAGS="$withval"
)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

if test "${FreeBSD}" != ""; then
	OPTIMIZECPU=""
fi

if test "${OPTIMIZECPU}" = "yes"; then

# Try to autodetect cpu type
CPU_NAME="unknown"
CPU_TYPE="unknown"
if test -e "/proc/cpuinfo" ; then
	intel[[30]]="-march=i386"
	intel[[32]]="-march=i386"
	intel[[34]]="-march=i386"
	intel[[40]]="-march=i486"
	intel[[41]]="-march=i486"
	intel[[42]]="-march=i486"
	intel[[43]]="-march=i486"
	intel[[44]]="-march=i486"
	intel[[45]]="-march=i486"
	intel[[47]]="-march=i486"
	intel[[48]]="-march=i486"
	intel[[51]]="-march=pentium"
	intel[[52]]="-march=pentium"
	intel[[54]]="-march=pentium-mmx"
	intel[[56]]="-march=pentium-mmx"
	intel[[61]]="-march=pentiumpro"
	intel[[63]]="-march=pentium2"
	intel[[65]]="-march=pentium2"
	intel[[66]]="-march=pentium2"
	intel[[67]]="-march=pentium3"
	intel[[68]]="-march=pentium3"
	intel[[610]]="-march=pentium3"
	intel[[611]]="-march=pentium3"
	intel[[150]]="-march=pentium4"
	intel[[151]]="-march=pentium4"
	intel[[152]]="-march=pentium4"
	intel[[154]]="-march=pentium4"
	intel[[614]]="-march=prescott"
	intel[[628]]="-march=core2"
	amd[[50]]="-march=i586"
	amd[[51]]="-march=i586"
	amd[[52]]="-march=i586"
	amd[[53]]="-march=i586"
	amd[[56]]="-march=k6"
	amd[[57]]="-march=k6"
	amd[[58]]="-march=k6-2"
	amd[[510]]="-march=k6-2"
	amd[[59]]="-march=k6-3"
	amd[[513]]="-march=k6-3"
	amd[[61]]="-march=athlon"
	amd[[62]]="-march=athlon"
	amd[[63]]="-march=athlon"
	amd[[64]]="-march=athlon"
	amd[[66]]="-march=athlon"
	amd[[67]]="-march=athlon"
	amd[[68]]="-march=athlon"
	amd[[610]]="-march=athlon"
	amd[[158]]="-march=athlon-xp"
	amd[[154]]="-march=k8"
	amd[[155]]="-march=athlon64"
	amd[[1543]]="-march=athlon64"
	amd[[1544]]="-march=athlon64"
	amd[[1565]]="-march=opteron"
	amd[[1572]]="-march=k8"
	via[[67]]="-march=c3"
	via[[68]]="-march=c3"
	via[[69]]="-march=i686"
	via[[610]]="-march=i686"

	CPU_TYPE="known"
	CPU_FAMILY=`cat /proc/cpuinfo | grep "cpu family" | head -n1`
	CPU_MODEL=`cat /proc/cpuinfo | grep model[[^\ ]] | head -n1`
	CPU_NAME=`cat /proc/cpuinfo | grep "model name" | head -n1`
	CPU_FLAGS=`cat /proc/cpuinfo | grep "flags" | head -n1`
	CPU_VENDOR=`cat /proc/cpuinfo | grep "vendor_id" | head -n1`
	CPU_FAMILY=${CPU_FAMILY#*: }
	CPU_MODEL=${CPU_MODEL#*: }
	CPU_NAME=${CPU_NAME#*: }
	CPU_FLAGS=${CPU_FLAGS#*: }
	CPU_VENDOR=${CPU_VENDOR#*: }
	if test "x${CPU_VENDOR}" = "xGenuineIntel" ; then
		CPU_OPTIONS=${intel[[$CPU_FAMILY$CPU_MODEL]]}
	fi
	if test "x${CPU_VENDOR}" = "xAuthenticAMD" ; then
		CPU_OPTIONS=${amd[[$CPU_FAMILY$CPU_MODEL]]}
	fi
	if test "x${CPU_VENDOR}" = "xCentaurHauls"; then
		CPU_OPTIONS=${via[[$CPU_FAMILY$CPU_MODEL]]}
	fi
	if test "x${CPU_OPTIONS}" = "x" ; then
		CPU_TYPE="unknown"
	fi
	CPU_EXT=""
	for i in $CPU_FLAGS ; do
		case $i in
		fpu)
			CPU_FPU="-mfpmath=387"
			;;
		mmx)
			CPU_EXT="$CPU_EXT -mmmx"
			;;
		sse)
			CPU_FPU="-mfpmath=sse -msse"
			;;
		sse2)
			CPU_FPU="-mfpmath=sse -msse2"
			;;
		sse3)
			CPU_FPU="-msse3"
			;;
		ssse3)
			CPU_FPU="-mfpmath=sse -msse2 -mssse3"
			;;
		3dnow)
			CPU_EXT="$CPU_EXT -m3dnow"
			;;
		esac
	done
	CPU_OPTIONS="$CPU_OPTIONS $CPU_FPU $CPU_EXT"
fi
if test "x${CPU_TYPE}" = "xunknown"; then
	CPU_TYPE=`( uname -p ) 2>&1`
	case $CPU_TYPE in
	i386)
		CPU_OPTIONS="-march=i386"
		;;
	i486)
		CPU_OPTIONS="-march=i486"
		;;
	Pentium2)
		CPU_OPTIONS="-march=pentium2"
		;;
	Pentiumpro)
		CPU_OPTIONS="-march=pentiumpro"
		;;
	Pentium*)
		CPU_OPTIONS="-march=pentium"
		;;
	k6)
		CPU_OPTIONS="-march=k6"
		;;
	k6-2)
		CPU_OPTIONS="-march=k6-2"
		;;
	k6-3)
		CPU_OPTIONS="-march=k6-3"
		;;
	"VIA C3 Ezra")
		CPU_OPTIONS="-march=c3"
		CPU_TYPE="known"
               ;;
	*)
		CPU_OPTIONS=""
		CPU_TYPE="unknown"
		;;
	esac
	if test "x${CPU_TYPE}" = "xunknown"; then
		CPU_TYPE=`( uname -m ) 2>&1`
		case $CPU_TYPE in
		i386)
			CPU_OPTIONS="-march=i386"
			;;
		i486)
			CPU_OPTIONS="-march=i486"
			;;
		i586)
			CPU_OPTIONS="-march=i586"
			;;
		i686)
			CPU_OPTIONS="-march=i686"
			;;
		Pentium2)
			CPU_OPTIONS="-march=pentium2"
			;;
		Pentiumpro)
			CPU_OPTIONS="-march=pentiumpro"
			;;
		k6)
			CPU_OPTIONS="-march=k6"
			;;
		k6-2)
			CPU_OPTIONS="-march=k6-2"
			;;
		k6-3)
			CPU_OPTIONS="-march=k6-3"
			;;
		*)
			CPU_OPTIONS="-march=native -mtune=native"
			;;
		esac
	fi
fi
echo "Detected CPU: $CPU_NAME"
# Now we check if the compiler supports the detected cpu
COMPILER=$CC
for I in "$TMPDIR" "$TEMPDIR" "/tmp" ; do
	test "$I" && break
done
TMPC="$I/cpu_test-$RANDOM-$$.c"
TMPO="$I/cpu_test-$RANDOM-$$.o"
cat > $TMPC << EOF
int main(void) { return 0; }
EOF
( $COMPILER $CPU_OPTIONS -o $TMPO $TMPC ) 2>&1
TMP="$?"
rm -f $TMPO
rm -f $TMPC


if test "x${TMP}" = "x1" ; then
	CPU_OPTIONS=""
	echo "No CPU optimizations will be added"
else
	echo "CPU optimization: $CPU_OPTIONS"
fi

else
	CPU_OPTIONS=""
fi


AC_MSG_CHECKING(for bswap instruction)
AC_LINK_IFELSE([
	AC_LANG_PROGRAM([ ],
		[unsigned int __x=0;
		register unsigned int __v;
		__asm("bswap %0" : "=r" (__v) : "0" (__x));])
	],
	[
		TEMP_CFLAGS="${TEMP_CFLAGS} -DHAVE_BSWAP"
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
	])


if test "${DEVELOPER_FLAGS}" = "yes"; then
	TEMP_CFLAGS="${TEMP_CFLAGS} -W -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Winline -Wredundant-decls -Wno-long-long -ggdb -g3"
fi

CFLAGS="${TEMP_CFLAGS} $UNAME_DEFS  $CPU_OPTIONS"

LIBS="${TEMP_LIBS}"
LDFLAGS="${TEMP_LDFLAGS}"


#
# Add the right exec path for rc scripts
#
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

AC_CONFIG_FILES([
thread1.conf
thread2.conf
thread3.conf
thread4.conf
motion-dist.conf
motion.init-FreeBSD.sh
motion.init-Debian
motion.init-Fedora
motion.spec
Makefile
])
AC_OUTPUT

echo ""
echo "   **************************"
echo "      Configure status       "
echo "      ${PACKAGE_NAME} ${PACKAGE_VERSION}"
echo "   **************************"
echo


if test "${Darwin}" != ""; then
    echo "OS             :     Darwin"
elif test "${FreeBSD}" != ""; then
    echo "OS             :     *BSD"
else
    echo "OS             :     Linux"
fi

if test "${PTHREAD_SUPPORT}" = "yes"; then
    echo "pthread support:     Yes"
else
    echo "pthread support:     No"
    echo "**********************************************"
    echo "** Fatal Error YOU MUST HAVE pthread Support *"
    echo "**********************************************"
fi


if test "${JPEG_SUPPORT_TURBO}" = "yes"; then
    echo "jpeg turbo support:  Yes"
elif test "${JPEG_SUPPORT_MMX}" = "yes"; then
    echo "jpeg-mmx support:    Yes"
elif test "${JPEG_SUPPORT}" = "yes"; then
    echo "jpeg support:        Yes"
else
    echo "jpeg support:        No"
    echo "**********************************************"
    echo "** Fatal Error YOU MUST HAVE jpeg Support  ***"
    echo "**********************************************"
fi

if test "${FreeBSD}" != ""; then
	if test "${BKTR}" = "yes"; then
        echo "BKTR included:       Yes"
	else
        echo "BKTR included:       No"
	fi

	if test "${PWCBSD}" = "yes"; then
        echo "PWCBSD include:      Yes"
	else
        echo "PWCBSD include:      No"
	fi

else
	if test "${V4L}" = "yes"; then
        echo "V4L support:         Yes"
	else
        echo "V4L support:         No"
	fi

	if test x$SUPPORTED_V4L2 = xtrue; then
        echo "V4L2 support:        Yes"
	else
        echo "V4L2 support:        No"
	fi
fi

if test "${SDL_SUPPORT}" = "yes"; then
	echo "SDL support:         Yes"
else
	echo "SDL support:         No"
fi

if test "${FFMPEG_OK}" = "found"; then
    echo "FFmpeg support:      Yes"
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
