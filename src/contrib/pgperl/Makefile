# Makefile for pgperl.
# $Header: /genome/src/postgres/src/contrib/pgperl/RCS/Makefile,v 1.2 92/02/07
 16:37:46 hartzell Exp $
# Thu Aug 23 15:00:05 1990  Igor Metz <metz@iam.unibe.ch>
# touched up to work with my configuration under postgres3.1 and perl4.x

# PERLDIR is the directory, where your uperl.o and all perl header file live
PERLDIR = ..

# GLOBINCS defines include paths, where your Postgres header files live
GLOBINCS = -I../../pg -I/usr/postgres/src/lib/H

# LIBS defines, where your libpq.a lives
LIBS = -L/usr/postgres/obj.sparc -lpq -lm

DISTFILES = README Makefile pg-mus usersub.c pg-libpq.mus testlibpq.pl

CC=cc
CFLAGS = -g -I$(PERLDIR) $(GLOBINCS)
#CC=gcc
#CFLAGS = -traditional -fpcc-struct-return -g -I$(PERLDIR) $(GLOBINCS)

all: pgperl

pgperl: $(PERLDIR)/uperl.o usersub.o pg-libpq.o
	$(CC) -g $(PERLDIR)/uperl.o usersub.o pg-libpq.o $(LIBS) -o pgperl 

pg-libpq.c: pg-libpq.mus
	pg-mus pg-libpq.mus > pg-libpq.c

usersub.o: usersub.c
pg-libpq.o: pg-libpq.c

clean:
	rm usersub.o pg-libpq.o pg-libpq.c

dist:
	shar $(DISTFILES) > dist.shar
