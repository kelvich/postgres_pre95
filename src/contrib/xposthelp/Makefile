#
# $Header$
#

.include <postgres.global.mk>

SRCS=	posthelp.c PostgresRoutines.c 

#
#  	Your favorite compiler
#
CC=	cc

#
#  	Compiler flags
#
CFLAGS=	-g

#	Postgres library
CFLAGS+= -I${.CURDIR}/../../backend
LDADD+=	-L${LIBDIR} -lpq

PROG=posthelp

afterinstall:
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		xposthelp ${DESTDIR}${BINDIR}/xposthelp

.include <postgres.prog.mk>

#
#	XView library calls and header files
#
OPENWINHOME=/usr/sww/X11
CFLAGS+= -I$(OPENWINHOME)/include 
LDADD+=	-L$(OPENWINHOME)/lib -lxview -lolgx -lX11

xposthelp: xposthelp.o PostgresRoutines.o
	$(CC) ${LDFLAGS} -o ${.TARGET} xposthelp.o PostgresRoutines.o ${LDADD}

all: xposthelp

CLEANFILES+= xposthelp xposthelp.o
