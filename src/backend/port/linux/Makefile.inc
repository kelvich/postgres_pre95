#
# Linux specific stuff.  Right now, we disable the dynamic loader.
#
# (C) 1994, Kai Petzke, wpp@marie.physik.tu-berlin.de
#


SRCS+= dynloader.c # port.c
HEADERS+= machine.h

CFLAGS+= -DNEED_CBRT

LDADD+= -ldld
