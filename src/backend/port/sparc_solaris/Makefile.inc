#
# SPARC/Solaris 2.x specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME -DNEED_ISINF -DNEED_RUSAGE -DNO_EMPTY_STMTS

LDADD+= -ll -ldl

SRCS+= port.c dynloader.c tas.s

HEADERS+= machine.h
