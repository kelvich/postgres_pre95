#
# Ultrix4.x specific stuff
#
# $Header$
#

LDADD+= -ldl -lln

#
# pixie(1) can't handle OMAGIC executables.
# (Note that this breaks dynamic loading.)
#
.if !defined(PROFILE)
LDADD+= -N
.endif

#
# The YACC grammar is too big..
#
.if !defined(CDEBUG)
CFLAGS+= -Olimit 1300
.endif

HEADERS+= machine.h 

SRCS+= dynloader.c port.c
