#
# $Header$
#
PARSED= ${.CURDIR}/parser

.PATH: ${PARSED}

PARSEYACCS= gram.c parse.h

${PARSEYACCS}: gram.y
	yacc -d ${.ALLSRC}; \
	mv y.tab.c gram.c; \
	mv y.tab.h parse.h;

SRCS+= catalog_utils.c dbcommands.c error.c io.c parse_query.c scanner.c \
	 ylib.c gram.c scan.c

CLEANFILES+= scan.c ${PARSEYACCS}

${PROG}: ${PARSEYACCS}

.depend: scan.c ${PARSEYACCS}

.if ${PORTNAME} == "bsd44"
YFLAGS+= -l
.endif

HEADERS+= atoms.h catalog_utils.h exceptions.h io.h parse_query.h \
	 parsetree.h
