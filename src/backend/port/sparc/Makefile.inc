#
# sparc specific stuff
#

SRCS+= dynamic_syms.c port.c

# use dynloader.c in dld if USE_DLD defined
.ifndef USE_DLD
SRCS+= dynloader.c
.endif
