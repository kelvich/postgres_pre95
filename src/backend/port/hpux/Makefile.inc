#
# HP-UX specific stuff
#
# $Header$
#

#
# "uname -r" prints release.major.minor, e.g.: A.09.01
#
HPUX_VERS!=uname -r
HPUX_MAJOR=${HPUX_VERS:R:E}

#
# HP-UX needs:
# -W l,-E	export symbols for linking with the shared libraries 
#		dynamic loader
# -W p,-H400000	expand cpp #define table size so the Nodes files don't 
#		break it
#
CFLAGS+= -W l,-E -W p,-H400000
LDFLAGS+= -W l,-E
LDADD+= -ll -ldld

.if (${HPUX_MAJOR} == "08")
CFLAGS+= -DNEED_RINT
.  if !defined(ENFORCE_ALIGNMENT)
CFLAGS+= -DHP_S500_ALIGN
.  endif
.elif (${HPUX_MAJOR} == "09")
#
# The #pragma trick required on 8.07 no longer works -- the #pragma
# is thoroughly broken.  However, the +u flag has been extended to
# handle alignment requirement arguments (defaulting to 2) for things 
# other than struct references, so the #pragma is no longer needed.
#
.  if !defined(ENFORCE_ALIGNMENT)
#CFLAGS+= -DHP_S500_ALIGN
.  endif
#
# The YACC grammar is too big..
# (HP-UX 9.01 added basic block limits for +O2.)
#
.  if !defined(CDEBUG)
CFLAGS+= +Obb600
.  endif
.endif

SRCS+= dynloader.c port.c
HEADERS+= fixade.h machine.h port-protos.h
