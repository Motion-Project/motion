# New ports collection makefile for:	motion
# Date created:				22 Dec 2005
# Whom:					Angel Carpintero <ack@telefonica.net>
#
# $FreeBSD: ports/multimedia/motion/Makefile,v 1.3 2006/11/30 11:27:19 miwi Exp $
#

PORTNAME=	motion-20070207
PORTVERSION=	051001 		
CATEGORIES=	multimedia
MASTER_SITES=	http://www.lavrsen.dk/sources/motion-daily/
MASTER_SITE_SUBDIR=	motion-daily

PATCH_SITES=	http://www.lavrsen.dk/twiki/pub/Motion/FreeBSDPatch/
PATCHFILES=	motion-daily-freebsd.diff.gz
PATCH_DIST_STRIP=	-p1

MAINTAINER=	ack@telefonica.net
COMMENT=	A motion detection application

LIB_DEPENDS=	jpeg.9:${PORTSDIR}/graphics/jpeg \
		avcodec.1:${PORTSDIR}/multimedia/ffmpeg

ONLY_FOR_ARCHS=	i386 amd64

CONFIGURE_TARGET=	--build=${MACHINE_ARCH}-portbld-freebsd${OSREL}

MAN1=		motion.1

USE_AUTOTOOLS=	autoconf:259
GNU_CONFIGURE=	yes
USE_GMAKE=	yes
MANCOMPRESSED=	no
USE_GCC=	3.4+



MAN1=		motion.1
PORTDOCS=	CHANGELOG CREDITS README.FreeBSD README \
		motion_guide.html
EXAMPLES=	motion-dist.conf motion.init-FreeBSD.sh thread1.conf \
		thread2.conf thread3.conf thread4.conf

OPTIONS=	BKTR "Support for TV Capture cards" on \
		LINUXTHREADS "Use LinuxThreads" off

MAKE_ENV+=	CC="${CC}"

.include <bsd.port.pre.mk>

.if !defined(WITH_BKTR)
CFLAGS+=	-DWITHOUT_V4L
.endif

.if defined(WITH_LINUXTHREADS) || ${OSVERSION} < 504000
CONFIGURE_ARGS=	--with-linuxthreads

.if ${OSVERSION} > 500000
LIB_DEPENDS+=	lthread.[35]:${PORTSDIR}/devel/linuxthreads
.else
LIB_DEPENDS+=	lthread.[24]:${PORTSDIR}/devel/linuxthreads
.endif

.endif

post-install:
.if !exists(${PREFIX}/etc/motion.conf)
	${INSTALL_DATA} ${PREFIX}/etc/motion.conf.sample ${PREFIX}/etc/motion.conf
.endif
	${MKDIR} ${EXAMPLESDIR}
.for f in ${EXAMPLES}
	${INSTALL_DATA} ${WRKSRC}/${f} ${EXAMPLESDIR}
.endfor

.if !defined(NOPORTDOCS)
	${MKDIR} ${DOCSDIR}
.for f in ${PORTDOCS}
	${INSTALL_DATA} ${WRKSRC}/${f} ${DOCSDIR}
.endfor
.endif

.if defined(WITH_BKTR)
	@${ECHO_MSG} "==============================================================================="
	@${ECHO_MSG} "Please make sure you have bktr(4) kernel support."
	@${ECHO_MSG} "==============================================================================="
.endif

.include <bsd.port.post.mk>
