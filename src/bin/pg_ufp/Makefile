#
# $Header$
#

.include <postgres.global.mk>

PROG= pg_ufp

SRCS= ufp.c dfmgr.c dynloader.c

.PATH:	${.CURDIR}/../../backend/port/${PORTNAME} \
	${.CURDIR}/../../backend/utils/fmgr

CFLAGS+= -I${.CURDIR}/../../backend/port/${PORTNAME} \
	 -I${.CURDIR}/../../backend/${MAKEOBJDIR} \
	 -I${.CURDIR}/../../backend
CFLAGS+= -DUFP
#CFLAGS+= -DUFPDEBUG -DUFPDEBUG_PROTO

#
# This is basically copied from the port/<foo>/Makefile.inc
# (minus the lex/yacc libraries).
# Unfortunately, it must be sync'd by hand.
#
.if (${PORTNAME} == "ultrix4" || ${PORTNAME} == "sparc")
LDADD+= -ldl
.elif (${PORTNAME} == "alpha")
# nothing
.elif (${PORTNAME} == "hpux")
CFLAGS+= -W l,-E
LDFLAGS+= -W l,-E
LDADD+= -ldld
.elif (${PORTNAME} == "aix")
LDADD+= -lld
HEADERS+= dlfcn.h
SRCS+= dlfcn.c

LDFLAGS+= -bE:${PROG}.exp

${PROG}.exp: ${PROG}.noexp
	mv -f ${.ALLSRC} ${PROG}
	mkldexport ${PROG} ${BINDIR} > ${.TARGET}
	mv -f ${PROG} ${.ALLSRC}

${PROG}.noexp: ${OBJS}
	touch -f ${PROG}.exp
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

CLEANFILES+= ${PROG}.noexp ${PROG}.exp
.else
# put the executable in OMAGIC format
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>

.if defined(MAKE_EXPORTS)
${PROG}: ${PROG}.exp

installexp: ${PROG}.exp
.  if defined(OLD_MKDIR)
	@-if test ! -d ${DESTDIR}${LIBDIR}; \
	then mkdir ${DESTDIR}${LIBDIR}; fi
.  endif
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	${PROG}.exp ${DESTDIR}${LIBDIR}/${PROG}.exp

all: installexp
.endif
