#
# Ultrix4.x specific stuff
#

SRCS+= dynamic_syms.c on_exit.c port.c sendannounce.c
HEADERS+= machine.h

# use dynloader.c in dld if USE_DLD defined
.ifndef USE_DLD
SRCS+= dynloader.c
.endif
