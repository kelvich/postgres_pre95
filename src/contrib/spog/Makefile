#
# spog (simple postgres query interface)
#
# $Id$
#
# $Log$
# Revision 1.1  1993/02/18 23:27:32  aoki
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

# some readline libraries have xmalloc and xrealloc, some don't.
# define NEED_XMALLOC if your readline library lacks xmalloc and xrealloc.
# some versions of UNIX have strdup, some don't.
# define NEED_STRDUP if your standard library lacks strdup.
CC=cc
CFLAGS= -O -DNEED_XMALLOC -DNEED_STRDUP #-DDEBUG

# set POSTGRESTOP to the top directory of postgres (one above src/).
POSTGRESTOP=${.CURDIR}/../../..

PGINC=$(POSTGRESTOP)/src/backend
INCLUDES=-I$(PGINC)

# let RLINC point to the include files of gnu readline.
# comment it out if they're in a standard place (e.g., /usr/include).
RLINC=/usr/local/devel
.if defined(RLINC)
INCLUDES+= -I$(RLINC)
.endif

PGLIB=$(POSTGRESTOP)/lib
LIBS=-L$(PGLIB)

# let RLINC point to the gnu readline library.
# comment it out if it's in a standard place (e.g., /usr/local/lib).
RLLIB=/usr/local/devel/readline
.if defined(RLLIB)
LIBS+= -L$(RLLIB)
.endif

spog: spog.o query.o command.o Makefile
	$(CC) -o spog $(CFLAGS) spog.o query.o command.o \
              $(LIBS) -lreadline -ltermcap -lpq

spog.o: spog.c spog.h
	$(CC) -c $(CFLAGS) $(INCLUDES) spog.c

command.o: command.c spog.h
	$(CC) -c $(CFLAGS) $(INCLUDES) command.c

query.o: query.c spog.h
	$(CC) -c $(CFLAGS) $(INCLUDES) query.c

install: spog spog.1
	cp spog $(POSTGRESTOP)/bin
	cp spog.1 $(POSTGRESTOP)/ref/man/man1
	cp pg_edit.sh $(POSTGRESTOP)/bin/pg_edit
	cp pg_edit.1 $(POSTGRESTOP)/ref/man/man1
	cp pg_info.sh $(POSTGRESTOP)/bin/pg_info
	cp pg_info.1 $(POSTGRESTOP)/ref/man/man1

clean:
	rm -f spog *.o
