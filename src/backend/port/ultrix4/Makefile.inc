#
# Ultrix4.x specific stuff
#
# $Header$
#

LDADD+= -ldl -lln
.if !defined(PROFILE)
LDADD+= -N
.endif

HEADERS+= machine.h 

SRCS+= dynloader.c port.c
