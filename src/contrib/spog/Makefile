#
# spog (simple postgres query interface)
#
# $Id$
#
# $Log: Makefile,v
#
# Revision 2.3  1992/08/13  11:47:33  schoenw
# scripts using spog added
#
# Revision 2.2  1992/05/27  14:50:54  schoenw
# HAVE_XMALLOC define for some readline versions added
#
# Revision 2.1  1992/05/22  12:53:33  schoenw
# this is the public release 1.0
#
# Revision 1.4  1992/05/22  12:46:00  schoenw
# builtin-commands, help functions and completion added
#
# Revision 1.3  1992/03/09  15:39:21  schoenw
# bug in install target
#
# Revision 1.2  1992/03/09  15:37:46  schoenw
# clean target added
#
# Revision 1.1  1992/01/24  16:45:20  schoenw
# Initial revision
#

#
# I decided not to install the shell programs, seeing as how Juergen
# doesn't recommend casual use of them ...  -- pma 2/19/93
#
.include <postgres.global.mk>

NOMAN=

LDFLAGS= -g

CFLAGS= -g  #-DDEBUG

#
# some readline libraries have xmalloc and xrealloc, some don't.
# comment out this line if your readline library has xmalloc and xrealloc.
#
CFLAGS+= -DNEED_XMALLOC

#
# some versions of UNIX have strdup, some don't.
# comment out this line if your standard library has strdup.
#
CFLAGS+= -DNEED_STRDUP

#
# set POSTGRESDIR to the top directory of postgres (one above src/).
#	spog will be installed in BINDIR.
#	HEADERDIR and LIBDIR should point to the install libpq.
#
#POSTGRESDIR=${.CURDIR}/../../..
POSTGRESDIR= /private/devel/postgres
BINDIR= ${POSTGRESDIR}/bin
HEADERDIR= ${POSTGRESDIR}/include
LIBDIR= ${POSTGRESDIR}/lib
CFLAGS+= -I${HEADERDIR}
LDADD+= -L${LIBDIR}

#
# these lines should point to the GNU readline library.
# comment them out if the GNU readline library is in a standard place.
#
CFLAGS+= -I/usr/sww/include
LDADD+= -L/usr/sww/lib

LDADD+= -lpq -lreadline -ltermcap

.MAIN: ${PROG}

PROG= spog

SRCS= spog.c query.c command.c

.include <postgres.prog.mk>
