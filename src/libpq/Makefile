.include <postgres.global.mk>

LIB= pq

.PATH:	${.CURDIR}/../backend/libpq \
	${.CURDIR}/../backend/lib

CFLAGS+= -DFRONTEND \
	-I${.CURDIR}/../backend/obj \
	-I${.CURDIR}/../backend

.ifdef KRBVERS
CFLAGS+= ${KRBFLAGS}
.endif

SRCS=	fe-dumpdata.c fe-pqexec.c fe-pqstubs.c fe-pqufs.c \
	auth.c portal.c portalbuf.c pqcomm.c pqpacket.c simplelists.c

.include <postgres.lib.mk>
