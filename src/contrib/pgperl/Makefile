#
# $Header$
#
.include <postgres.global.mk>

CFLAGS= -g

#
# PERLDIR is the perl distribution directory.
#	you must have already built all of perl (including mus) in order
#	to start building pgperl.
#
PERLDIR=/usr/local/devel/perl
MUS= ${PERLDIR}/usub/mus

# 
# Don't add uperl.o to OBJS or it will be removed when you do 
# "bmake clean"... :-(
#
LDADD+= ${PERLDIR}/uperl.o
LDADD+= -L${LIBDIR} -lpq -lm

CFLAGS+= -I${HEADERDIR} -I${PERLDIR}

.SUFFIXES: .mus

.mus.c:
	${MUS} $< > ${.TARGET}

SRCS= pg-libpq.mus usersub.c
TESTOBJ= testlibpq.out
PROG= pgperl

.MAIN: ${PROG}

${PROG}: ${PERLDIR}/uperl.o

${TESTOBJ}: ${PROG} testlibpq.pl
	./${PROG} testlibpq.pl 2>&1 | tee ${.TARGET}

runtest: ${TESTOBJ}

CLEANFILES+= ${TESTOBJ} pg-libpq.c

.include <postgres.prog.mk>
