#
# sparc specific stuff
#

SRCS+= dynamic_syms.c port.c
HEADERS+= machine.h

# use dynloader.c in dld if USE_DLD defined
.ifndef USE_DLD
SRCS+= dynloader.c
.endif
