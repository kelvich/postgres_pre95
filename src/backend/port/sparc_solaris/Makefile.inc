#
# SPARC/Solaris 2.x specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME -DNEED_ISINF

LDADD+= -lsocket -lnsl -ll -ldl

SRCS+= port.c dynloader.c
HEADERS+= machine.h
