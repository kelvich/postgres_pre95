#
# Alpha OSF/1 specific stuff
#
# $Header$
#

CFLAGS+= -DUSE_POSIX_TIME -DDISABLE_XOPEN_NLS

LDADD+= -lln

HEADERS+= machine.h 

SRCS+= dynloader.c port.c tas.s
