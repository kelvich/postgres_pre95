#
# AIX specific stuff
#
# $Header$
#

CFLAGS+= -DCLASS_CONFLICT -DHAVE_STRFTIME

LDFLAGS+= -bE:${PROG}.exp

LDADD+= -ll -lld

HEADERS+= dlfcn.h machine.h port-protos.h

SRCS+= dlfcn.c dynloader.c

${PROG}.exp: ${PROG}.noexp
	mv -f ${.ALLSRC} ${PROG}
	mkldexport ${PROG} ${BINDIR} > ${.TARGET}
	mv -f ${PROG} ${.ALLSRC}

${PROG}.noexp: ${OBJS}
	touch -f ${PROG}.exp
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

EXPORTS= ${PROG}.exp

CLEANFILES+= ${PROG}.noexp ${PROG}.exp
