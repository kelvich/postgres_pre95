#
# HP-UX specific stuff
#
# $Header$
#

#
# HP-UX needs:
# -W l,-E	export symbols for linking with the shared libraries 
#		dynamic loader
# -W p,-H400000	expand cpp #define table size so the Nodes files don't 
#		break it
#
CFLAGS+= -W l,-E -W p,-H400000
CFLAGS+= -DUSE_POSIX_TIME
LDFLAGS+= -W l,-E
LDADD+= -ll -ldld

#
# cbrt(3m) and rint(3m) are missing from 8.07.
# cbrt(3m) and rint(3m) are broken in 9.01.
# cbrt(3m) seems to be missing on 9.00 even though it is documented.
#
CFLAGS+= -DNEED_RINT -DNEED_CBRT

#
# The #pragma trick required on 8.07 no longer works -- the #pragma
# is thoroughly broken.  However, the +u flag has been extended to
# handle alignment requirement arguments (defaulting to 2) for things 
# other than struct references, so the #pragma is no longer needed.
#

#
# (1) The YACC grammar is too big..
# (HP-UX 9.0x, x<2, added basic block limits for +O2; 9.0x, x>=2, changed
# the syntax to something else.)
#
# (2) The 9.00 optimizer chokes on some of our source.
#
.if (${HPUX_MAJOR} == "09")
.  if !defined(CDEBUG)
.    if (${HPUX_MINOR} == "00" || ${HPUX_MINOR} == "01")
CFLAGS+= +Obb600
CFLAGS+= -DWEAK_C_OPTIMIZER
.    else
CFLAGS+= +Onolimit
.    endif
.  endif
.endif

HEADERS+= fixade.h machine.h port-protos.h

SRCS+= dynloader.c port.c tas.s
