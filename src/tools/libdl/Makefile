#
#  Makefile for Ultrix 4.x Dynamic Loader Library Version 1.0
#
#  Copyright (c) 1993 Andrew K. Yu, University of California at Berkeley
#  All rights reserved.
#
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for educational, research, and non-profit purposes and
#  without fee is hereby granted, provided that the above copyright
#  notice appear in all copies and that both that copyright notice and
#  this permission notice appear in supporting documentation. Permission
#  to incorporate this software into commercial products can be obtained
#  from the author. The University of California and the author make
#  no representations about the suitability of this software for any 
#  purpose. It is provided "as is" without express or implied warranty. 
#

#
#  this package should also compile under gcc with no problem.
#
CC = cc
CFLAGS = -G 0 -g

LIBS = dlRef.o dlReloc.o dlInterf.o dlArch.o 

all:	libdl.a 

libdl.a: ${LIBS}
	rm -f libdl.a
	ar q libdl.a ${LIBS}
	ranlib libdl.a

clean:
	rm -f libdl.a $(LIBS)

install:
	install -m 555 -o bin -g bin libdl.a /usr/local/lib/libdl.a



