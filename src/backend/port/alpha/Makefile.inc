#
# Alpha OSF/1 specific stuff
#
# $Header$
#

CFLAGS+= -DHAVE_STRFTIME

LDADD+= -lln

HEADERS+= machine.h 

SRCS+= dynloader.c port.c
