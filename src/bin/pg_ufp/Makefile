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

.if (${PORTNAME} == "ultrix4")
CFLAGS+= -G 0
LDFLAGS+= -N -G 0
LDADD+= -ldl
.elif (${PORTNAME} == "hpux")
CFLAGS+= -W l,-E
LDFLAGS+= -W l,-E
LDADD+= -ldld
.else
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>
