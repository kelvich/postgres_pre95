.include <postgres.global.mk>

PROG= pmv

SRCS= pmv.c

# to pick up strerror(1) on sparc
.if ${PORTNAME} == "sparc"
.PATH: ${.CURDIR}/../pmkdir
SRCS+= strerror.c
.endif

.include <postgres.prog.mk>
