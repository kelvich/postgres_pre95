#
# $Header$
#
.include <postgres.global.mk>

LIB= pq
.PATH: ${.CURDIR}/../backend/libpq
CFLAGS+= -I${.CURDIR}/../backend -DFRONTEND
SRCS=	fe-dumpdata.c fe-pqexec.c fe-pqstubs.c fe-pqufs.c \
	auth.c portal.c portalbuf.c pqcomm.c pqpacket.c

.include <postgres.lib.mk>
