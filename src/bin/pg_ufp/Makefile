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
.if (${PORTNAME} == "ultrix4")
LDADD+= -ldl
.elif (${PORTNAME} == "hpux")
CFLAGS+= -W l,-E
LDFLAGS+= -W l,-E
LDADD+= -ldld
.elif (${PORTNAME} == "sparc")
LDADD+= -ldl
.elif (${PORTNAME} == "alpha")
.else
# put the executable in OMAGIC format
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>
