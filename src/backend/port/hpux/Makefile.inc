#
# HP-UX specific stuff
#
# $Header$
#

#
# HP-UX needs:
# -Wl,-E - export symbols for linking with the shared libraries dynamic loader
# -Wp,-H400000 - expand cpp #define table size so Nodes files don't break it
#
CFLAGS+= -W l,-E -W p,-H400000
LDFLAGS+= -W l,-E
LDADD+= -ll -ldld

SRCS+= dynloader.c port.c
HEADERS+= machine.h port-protos.h
