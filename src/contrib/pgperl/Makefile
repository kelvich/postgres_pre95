#
# $Header$
#
.include <postgres.global.mk>

#
# PERLDIR is the perl distribution directory.
#	you must have already built all of perl (including mus) in order
#	to start building pgperl.
#
PERLDIR=/usr/local/devel/perl
MUS= ${PERLDIR}/usub/mus

CFLAGS= -g

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

PROG= pgperl

.MAIN: ${PROG}

${PROG}: ${PERLDIR}/uperl.o

testlibpq.out: ${PROG} testlibpq.pl
	./${PROG} testlibpq.pl 2>&1 | tee ${.TARGET}

runtest: testlibpq.out

.include <postgres.prog.mk>
