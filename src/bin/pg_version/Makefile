.include <postgres.global.mk>

PROG= pg_version
SRCS= pg_version.c magic.c
.PATH: ${.CURDIR}/../../backend/utils/init

CFLAGS+= -I${.CURDIR}/../../backend/port/${PORTNAME}
	
.include <postgres.prog.mk>
