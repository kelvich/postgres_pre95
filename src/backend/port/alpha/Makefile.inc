#
# Alpha OSF/1 specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME -DDISABLE_XOPEN_NLS -DNEED_ISINF

LDADD+= -lln

#
# The YACC grammar is too big..
#
.if !defined(CDEBUG)
CFLAGS+= -Olimit 2000
.endif

HEADERS+= machine.h 

SRCS+= dynloader.c port.c tas.s
