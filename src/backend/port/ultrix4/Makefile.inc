#
# Ultrix4.x specific stuff
#
# $Header$
#

CFLAGS+= -DNEED_ISINF

LDADD+= -ldl -lln

#
# The YACC grammar is too big..
#
.if !defined(CDEBUG)
CFLAGS+= -Olimit 2000
.endif

HEADERS+= machine.h 

SRCS+= dynloader.c port.c
