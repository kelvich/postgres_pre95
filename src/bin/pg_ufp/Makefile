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
# this is basically copied from the port/<foo>/Makefile.inc
# and, unfortunately, must be sync'd by hand.
#
.if (${PORTNAME} == "ultrix4")
LDFLAGS+= -N
LDADD+= -ldl
.elif (${PORTNAME} == "hpux")
CFLAGS+= -W l,-E
LDFLAGS+= -W l,-E
LDADD+= -ldld
.elif (${PORTNAME} == "sparc")
LDADD+= -ldl
.elif (${PORTNAME} == "alpha")
.else
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>
