.include <postgres.global.mk>

LIB= pq
.PATH: ${.CURDIR}/../backend/libpq
CFLAGS+= -I${.CURDIR}/../backend
SRCS=	fe-dumpdata.c fe-pqexec.c fe-pqstubs.c fe-pqufs.c \
	portal.c portalbuf.c pqcomm.c pqpacket.c

.include <postgres.lib.mk>
