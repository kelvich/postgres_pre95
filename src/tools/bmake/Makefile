.include <postgres.global.mk>

PROG=	bmake

PROTODIR?=	${DESTDIR}${LIBDIR}/mk

CFLAGS+=-I${.CURDIR} 
CFLAGS+= -D_PATH_DEFSYSMK='"${PROTODIR}/sys.mk"'  \
	-D_PATH_DEFSYSPATH='"${PROTODIR}"'


#
# o/s specific grot:
#

# if void broken in compiler
.if ( ${PORTNAME} == "ultrix4" || ${PORTNAME} == "sparc" )
CFLAGS+= -Dvoid="char *"
.endif

# DEC OSF on alpha
.if (${PORTNAME} == "alpha")
CFLAGS+= -D_BSD
.endif


SRCS=	arch.c buf.c compat.c cond.c dir.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
SRCS+=	strdup.c

.if ( ${PORTNAME} == "sparc" )
SRCS+= strerror.c setenv.c
.endif

.PATH:	${.CURDIR}/lst.lib

beforeinstall: installproto

installproto::
.if (${PORTNAME} == "sparc")
	if test ! -d ${PROTODIR}; then mkdir ${PROTODIR}; fi
.endif
	for i in ${.CURDIR}/mk-proto/*.mk ; \
	do \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i ${PROTODIR}/`basename $$i`; \
	done

.include <postgres.prog.mk>
