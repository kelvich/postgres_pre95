PARSED= ${.CURDIR}/parser

.PATH: ${PARSED}

PARSEYACCS= gram.c parse.h

${PARSEYACCS}: gram.y
	yacc -d ${PARSED}/gram.y; \
	mv y.tab.c gram.c; \
	mv y.tab.h parse.h;

${PARSED}/parse.h: ${PARSEYACCS}
	rm -f ${.TARGET}; \
	ln -s ${.CURDIR}/obj/parse.h ${.TARGET}

SRCS+= catalog_utils.c dbcommands.c error.c io.c parse_query.c scanner.c \
	 ylib.c gram.c scan.c

CLEANFILES+= scan.c ${PARSEYACCS}

${PROG}: ${PARSED}/parse.h

.if ${PORTNAME} == "bsd44"
YFLAGS+= -l
.endif

HEADERS+= atoms.h catalog_utils.h exceptions.h io.h parse_query.h \
	 parsetree.h
