.include <postgres.global.mk>

PROG= ppwd

MAN1= ppwd.1P

SRCS= ppwd.c

# need strerror on sparc
.if ${PORTNAME} == "sparc"
.PATH: ${.CURDIR}/../pmkdir
SRCS+= strerror.c
.endif

.include <postgres.prog.mk>
