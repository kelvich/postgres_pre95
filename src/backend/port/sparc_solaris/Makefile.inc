#
# SPARC/Solaris 2.x specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME -DNEED_ISINF -DNEED_RUSAGE

LDADD+= -ll -ldl

SRCS+= port.c dynloader.c tas.s

HEADERS+= machine.h
