CC= cc
CFLAGS = -G 0 -g -I..

LIBDL = ../libdl.a

SUITES= main call_mf abc 

OBJS= foo.o bar.o baz.o \
	mathfunc.o \
	a.o b.o c.o libabc.a jumboobj.o

ALLOBJS= $(OBJS) $(SUITES) jumbo 

all:	$(ALLOBJS)

$(SUITES):	$(LIBDL)
	$(CC) $(CFLAGS) $@.c $(LIBDL) -o $@

jumbo:	jumbo.c $(LIBDL)
	$(CC) $(CFLAGS) jumbo.c $(LIBDL) -lm_G0 -o $@

$(LIBDL):
	cd ..; make

libabc.a: a.o b.o c.o
	rm -f libabc.a
	ar q libabc.a a.o b.o c.o
	ranlib libabc.a

clean:
	rm -f $(ALLOBJS)

