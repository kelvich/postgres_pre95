#
# SPARC/SunOS 4 specific stuff
#
# $Header$
#

LDADD+= -lln -ldl

SRCS+= port.c dynloader.c
HEADERS+= machine.h

#
# To use Purify (SunOS only), define PURIFY to be the path (and
# options) with which to invoke the Purify loader.  Only the executable
# needs to be loaded with Purify.
#
#
.if defined(PURIFY)
${PROG}: ${OBJS} ${LIBC} ${DPADD}
	${PURIFY} ${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.endif
