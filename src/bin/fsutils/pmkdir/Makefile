.include <postgres.global.mk>

PROG= pmkdir

SRCS= pmkdir.c

.if ${PORTNAME} == "sparc"
SRCS+= strerror.c
.endif

.include <postgres.prog.mk>
