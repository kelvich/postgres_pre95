#
# $Header$
#
.include <postgres.global.mk>

CFLAGS= -g

.if (${PORTNAME} == "ultrix4")
.PATH: ${.CURDIR}/../../tools/bmake
SRCS+= strdup.c
.endif

LDADD+= -L${LIBDIR} -lpq

CFLAGS+= -I${HEADERDIR}

SRCS+= relshow.c
PROG= relshow

.MAIN: ${PROG}

.include <postgres.prog.mk>
