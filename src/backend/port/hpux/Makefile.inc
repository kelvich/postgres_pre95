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

.if (${HPUX_MAJOR} == "08")
#
# cbrt(3m) and rint(3m) are missing from the 8.07 math library.
#
CFLAGS+= -DNEED_RINT -DNEED_CBRT

.elif (${HPUX_MAJOR} == "09")
#
# rint(3m) is broken in the 9.01 math library.
#
CFLAGS+= -DNEED_RINT
#
# 9.00 on the 800s seems to be missing cbrt(3m)
#
.if (${HPUX_MINOR} == "00")
CFLAGS+= -DNEED_CBRT
.endif

#
# The #pragma trick required on 8.07 no longer works -- the #pragma
# is thoroughly broken.  However, the +u flag has been extended to
# handle alignment requirement arguments (defaulting to 2) for things 
# other than struct references, so the #pragma is no longer needed.
#

#
# The YACC grammar is too big..
# (HP-UX 9.01 added basic block limits for +O2.)
#
.  if !defined(CDEBUG)
CFLAGS+= +Obb600
.  endif
.endif

HEADERS+= fixade.h machine.h port-protos.h

SRCS+= dynloader.c port.c
