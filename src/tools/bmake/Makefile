.include <postgres.global.mk>

PROG=	bmake

PROTODIR?=	${DESTDIR}${LIBDIR}/mk

CFLAGS+=-I${.CURDIR} 
CFLAGS+= -D_PATH_DEFSYSMK='"${PROTODIR}/sys.mk"'  \
	-D_PATH_DEFSYSPATH='"${PROTODIR}"'


#
# o/s specific grot:
#

# actually, if broken ultrix compiler
.if (${MACHINE} == "mips")
CFLAGS+= -Dvoid="char *"
.endif

# actually, if OSF
.if (${MACHINE} == "alpha")
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
.PATH:	${.CURDIR}/lst.lib

beforeinstall: installproto

installproto::
	for i in ${.CURDIR}/mk-proto/*.mk ; \
	do \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i ${PROTODIR}/`basename $$i`; \
	done

.include <postgres.prog.mk>
