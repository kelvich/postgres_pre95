#
# Another kinda weird Makefile.inc cause we need two
#  scanner/parsers in the backend and most yaccs and lexs
#  don't have the prefix option.
#
#	sed files are HACK CITY! - redo...
#
BD= ${.CURDIR}/bootstrap

.PATH: ${BD}

BOOTYACCS= bootstrap.h bootparse.c

SRCS+= socket.c bootstrap.c bootparse.c bootscanner.c

${BOOTYACCS}: bootparse.y
	${YACC} ${YFLAGS} ${BD}/bootparse.y; \
	sed -f ${BD}/yacc.sed < y.tab.c > bootparse.c; \
	mv y.tab.h bootstrap.h; \
	rm -f y.tab.c

bootscanner.c: bootscanner.lex
	lex ${BD}/bootscanner.lex; \
	sed -f ${BD}/lex.sed < lex.yy.c > bootscanner.c; \
	rm -f lex.yy.c

#
# The following insures that y.tab.h gets made as bootstrap.c
# includes it
#
bootstrap.o: ${BOOTYACCS}

.depend: bootparse.c bootscanner.c

CLEANFILES+= bootscanner.c ${BOOTYACCS} y.tab.h y.output

HEADERS+= bkint.h
