.include <postgres.global.mk>

PROG= pg_ufp

SRCS= ufp.c dfmgr.c dynloader.c

.PATH:	${.CURDIR}/../../backend/utils/fmgr \
	${.CURDIR}/../../backend/port/${PORTNAME}

CFLAGS+= -I${.CURDIR}/../../backend/port/${PORTNAME} \
	 -I${.CURDIR}/../../backend/obj \
	 -I${.CURDIR}/../../backend
CFLAGS+= -DUFP

.if ${PORTNAME} == "ultrix4"
CFLAGS+= -G 0
LDFLAGS+= -G 0
LDFLAGS+= -N
.endif

.include <postgres.prog.mk>
