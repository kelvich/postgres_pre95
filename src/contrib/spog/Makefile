#
# spog (simple postgres query interface)
#
# $Id$
#
# $Log$
# Revision 1.2  1993/02/20 04:40:41  aoki
# removed installation of scripts
#
# Revision 1.1  1993/02/18  23:27:32  aoki
# Initial revision
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
# himself doesn't recommend casual use of them ...  -- pma 2/19/93
#
.include <postgres.global.mk>

NOMAN=

# some readline libraries have xmalloc and xrealloc, some don't.
# define NEED_XMALLOC if your readline library lacks xmalloc and xrealloc.
# some versions of UNIX have strdup, some don't.
# define NEED_STRDUP if your standard library lacks strdup.
CFLAGS= -O -DNEED_XMALLOC -DNEED_STRDUP #-DDEBUG

# set POSTGRESTOP to the top directory of postgres (one above src/).
POSTGRESTOP=${.CURDIR}/../../..
BINDIR= ${POSTGRESTOP}/bin
CFLAGS+= -I$(POSTGRESTOP)/src/backend
LDADD+= -L$(POSTGRESTOP)/src/libpq/obj

# this points to the include files of gnu readline.
# comment this out if they're in a standard place (e.g., /usr/include).
CFLAGS+= -I/usr/local/devel

# this points to the gnu readline library.
# comment this out if it's in a standard place (e.g., /usr/local/lib).
LDADD+= -L/usr/local/devel/readline

.MAIN: ${PROG}

PROG= spog

SRCS= spog.c query.c command.c

LDADD+= -lpq -lreadline -ltermcap

.include <postgres.prog.mk>
