.include <postgres.global.mk>

PROG= pg_ufp

SRCS= ufp.c dfmgr.c dynloader.c

.PATH:	${.CURDIR}/../../backend/utils/fmgr \
	${.CURDIR}/../../backend/port/${PORTNAME}

CFLAGS+= -I${.CURDIR}/../../backend/port/${PORTNAME} \
	 -I${.CURDIR}/../../backend/obj \
	 -I${.CURDIR}/../../backend
CFLAGS+= -DUFP
CFLAGS+= -G 0

LDFLAGS+= -G 0
LDFLAGS+= -N

.include <postgres.prog.mk>
