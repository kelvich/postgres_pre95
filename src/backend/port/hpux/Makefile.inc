#
# HP-UX specific stuff
#
# $Header$
#

#
# HP-UX needs:
# -DSYSV_SIGNAL since it has SVID2 signal(2) instead of BSD signal(2)
# -Wl,-E - export symbols for linking with the shared libraries dynamic loader
# -Wp,-H400000 - expand cpp #define table size so Nodes files don't break it
#
CFLAGS+= -DSYSV_SIGNAL -W l,-E -W p,-H400000
LDFLAGS+= -W l,-E
LDADD+= -ll -ldld

SRCS+= dynloader.c port.c
HEADERS+= machine.h port-protos.h
