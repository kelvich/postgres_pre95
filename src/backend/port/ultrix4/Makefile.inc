#
# Ultrix4.x specific stuff
#
# $Header$
#

LDADD+= -ldl -lln

#
# The YACC grammar is too big..
#
.if !defined(CDEBUG)
CFLAGS+= -Olimit 1300
.endif

HEADERS+= machine.h 

SRCS+= dynloader.c port.c
