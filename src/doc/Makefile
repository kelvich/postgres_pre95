.include <postgres.global.mk>

.MAIN: all

SRCS= postgres-setup.me
OBJS= ${SRCS:S/.me/.cat/g} ${SRCS:S/.me/.ps/g}

all: ${OBJS}

.include <postgres.prog.mk>
