# $Header$
#
# RWW's Makefile modified for use with bmake.
#
# RWW's comments:
# Makefile for pgperl (Postgres 4.0.1 and Perl 4.19).
# 
# My guess is that this will work for any perl => 4.19 and < 5.x
#
# This makefile assumes that you are in the subdirectory src/contrib/pgperl of
# your Postgres distribution.  You must select the port you are building
# against and the location of your perl distribution and your compiler.
#

# PERLDIR is the perl distribution directory
PERLDIR=/usr/local/devel/perl

# Select your compiler and compilation flags
#CC=gcc
#CCFLAGS = -traditional -fpcc-struct-return -g 
CCFLAGS= -O

MUS= $(PERLDIR)/usub/mus
LIBS= -L../../libpq/obj -lpq -lm
PGINCS= -I../../backend
CFLAGS= $(CCFLAGS) -I$(PERLDIR) $(PGINCS)

all: pgperl

pgperl: $(PERLDIR)/uperl.o usersub.o pg-libpq.o
	$(CC) $(CCFLAGS) ${.ALLSRC} $(LIBS) $(PERLLIBS) -o ${.TARGET}
	@echo
	@echo Remember to start the postmaster before running the test script.
	@echo

pg-libpq.c: pg-libpq.mus
	$(MUS) pg-libpq.mus > pg-libpq.c

testlibpq.out: pgperl testlibpq.pl
	./pgperl testlibpq.pl > ${.TARGET}

runtest: testlibpq.out

clean:
	rm -f usersub.o pg-libpq.o pg-libpq.c pgperl testlibpq.out
