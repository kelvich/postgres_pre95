#
# SPARC/SunOS 4 specific stuff
#
# $Header$
#

LDADD+= -lln -ldl

SRCS+= port.c dynloader.c
HEADERS+= machine.h
