.PATH: ${.CURDIR}/tcop

SRCS+= aclchk.c creatinh.c dest.c fastpath.c parsev.c pfrag.c postgres.c \
	pquery.c slaves.c utility.c

HEADERS+= creatinh.h dest.h parsev.h pfrag.h pquery.h \
	 slaves.h tcop.h tcopdebug.h tcopprot.h utility.h
