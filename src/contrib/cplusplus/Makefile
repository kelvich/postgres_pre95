#
# $Header$
#
# Makefile for Justin Smith's C++/POSTGRES interface package.
#

.include <postgres.global.mk>

NOMAN=
LDFLAGS= -g
CFLAGS= -g
CFLAGS+= -I${HEADERDIR}
LDADD+= -L${LIBDIR}

#
# seems to work with both C++ compilers...
#
#CC= CC
CC= g++

#
# additional stuff required for C++
#

# location of C++ library, if required
#LDADD+= -L/usr/sww/lib

# location of C++ include files, if required
#CFLAGS+= -I/usr/sww/lib/g++-include

# jsmith added these flags for CC (cfront).
.if ${CC} == "CC"
CFLAGS+= -DNO_ALLOCA -DNO_DMINUS -DMATH_H_EXTERNS
.endif

# CenterLine CC wouldn't work without this.  don't know why.
.if ${CC} == "CC"
CFLAGS+= -D__STDLIB_H
.endif

SRCS= pgtest.c

HEADERS= pg.h

LDADD+= -lpq

.MAIN: ${PROG}

PROG= pgtest
TESTOBJ = pgtest.out

runtest: ${TESTOBJ}

${TESTOBJ}:
	./${PROG} 2>&1 | tee ${TESTOBJ}

CLEANFILES+= ${TESTOBJ}

.include <postgres.prog.mk>
