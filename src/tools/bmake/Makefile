#
# $Header$
#

.include <postgres.global.mk>

#
# ################# BEGIN OS-DEPENDENT SECTION #################
#

.if ( ${PORTNAME} == "aix" )
CFLAGS+= -DUSE_AIA -D_BSD
SRCS+= setenv.c
LDADD+= -lbsd
.endif

.if ( ${PORTNAME} == "alpha" )
CFLAGS+= -D_BSD
LDADD+= -lbsd
.endif

.if ( ${PORTNAME} == "hpux" )
CFLAGS+= -Dconst= -DUSE_RANLIB
SRCS+= setenv.c
.endif

.if ( ${PORTNAME} == "linux" )
CFLAGS+= -D_BSD_SOURCE -I/usr/include/bsd
LDADD+= -lbsd
.endif

.if ( ${PORTNAME} == "sparc" )
CFLAGS+= -Dvoid="char *" -Dconst= -DUSE_RANLIB -DNEED_STRINGS
SRCS+= strerror.c setenv.c
.endif

.if ( ${PORTNAME} == "sparc_solaris" )
CFLAGS+= -Dd_fileno=d_ino -I/usr/ucbinclude
SRCS+= setenv.c
LDADD+= -R/usr/ucblib -L/usr/ucblib -lucb
.  if ( ${CC} != "cc" )
CFLAGS+= -BSD -notraditional 
LDADD+= -BSD
.  endif
.endif

.if ( ${PORTNAME} == "ultrix4" )
CFLAGS+= -Dvoid="char *" -Dconst= -DUSE_RANLIB
SRCS+= strdup.c
.endif

#
# ################# END OS-DEPENDENT SECTION #################
#

PROG=	bmake

PROTODIR?=	${DESTDIR}${LIBDIR}/mk

CFLAGS+=-I${.CURDIR} 
CFLAGS+= -D_PATH_DEFSYSMK='"${PROTODIR}/sys.mk"' \
	-D_PATH_DEFSYSPATH='"${PROTODIR}"'

SRCS+=	arch.c buf.c compat.c cond.c dir.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c

.PATH:	${.CURDIR}/lst.lib

beforeinstall: installproto

installproto::
.if defined(OLD_MKDIR)
	-if test ! -d ${PROTODIR}; \
		then mkdir ${PROTODIR}; fi || true
.endif
	for i in ${.CURDIR}/mk-proto/*.mk ; \
	do \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$i ${PROTODIR}/`basename $$i`; \
	done

#
# For obscure reasons bootmake fails if forked from GNU make unless
# this is defined.  (It somehow inherits an extra target "make" from
# GNU make.)  -- PMA 08/07/93
#
make::

.include <postgres.prog.mk>
