.PATH: ${.CURDIR}/parser

SRCS+= catalog_utils.c dbcommands.c error.c io.c parse_query.c scanner.c \
	 ylib.c gram.c scan.c

CLEANFILES+= gram.c scan.c y.tab.h

.if ${PORTNAME} == "bsd44"
YFLAGS+= -l
.endif

HEADERS+= atoms.h catalog_utils.h exceptions.h io.h parse.h parse_query.h \
	 parsetree.h
