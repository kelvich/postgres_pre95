.PATH: ${.CURDIR}/lib

SRCS+= atoms.c bit.c copyfuncs.c equalfuncs.c fcache.c fstack.c \
	hasht.c lispdep.c lisplist.c lispread.c lispsort.c \
	lsyscache.c nodeFuncs.c nodes.c oset.c outfuncs.c \
	ppreserve.c qsort.c readfuncs.c simplelists.c stringinfo.c \
	utilities.c xid.c

HEADERS+= catalog.h equalfuncs.h index.h lispread.h outfuncs.h \
	copyfuncs.h heap.h lisplist.h lispsort.h readfuncs.h

