#
# $Header$
#

.include <postgres.global.mk>

PROG= pg_ufp

SRCS= ufp.c dfmgr.c dynloader.c

.if defined(USE_DLD)
.PATH:	${.CURDIR}/../../backend/port/dld \
	${.CURDIR}/../../backend/utils/fmgr
CFLAGS+= -DUSE_DLD
LDADD+= -ldld
.else
.PATH:	${.CURDIR}/../../backend/port/${PORTNAME} \
	${.CURDIR}/../../backend/utils/fmgr
.endif


CFLAGS+= -I${.CURDIR}/../../backend/port/${PORTNAME} \
	 -I${.CURDIR}/../../backend/obj \
	 -I${.CURDIR}/../../backend
CFLAGS+= -DUFP
#CFLAGS+= -DUFPDEBUG -DUFPDEBUG_PROTO

.if (${PORTNAME} == "ultrix4")
CFLAGS+= -G 0
LDFLAGS+= -N -G 0
.elif (${PORTNAME} == "hpux")
CFLAGS+= -W l,-E
LDADD+= -ldld
.else
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>
