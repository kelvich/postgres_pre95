#
# SPARC/SunOS 4 specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME

LDADD+= -lln -ldl

#
# SunOS 4 strtol is broken -- doesn't report overflow using errno.
#
SRCS+= strtol.c

SRCS+= port.c dynloader.c
HEADERS+= machine.h
