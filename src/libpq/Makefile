.include <postgres.global.mk>

LIB= pq

.PATH:	${.CURDIR}/../backend/libpq \
	${.CURDIR}/../backend/lib

CFLAGS+= -I${.CURDIR}/../backend -DFRONTEND

.ifdef KRBVERS
CFLAGS+= ${KRBFLAGS}
.endif

SRCS=	fe-dumpdata.c fe-pqexec.c fe-pqstubs.c fe-pqufs.c \
	auth.c portal.c portalbuf.c pqcomm.c pqpacket.c simplelists.c

.include <postgres.lib.mk>
