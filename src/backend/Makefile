.include <postgres.global.mk>
#
# Use "bmake" (BSD 4.4 make) to run this Makefile
#
PROG= postgres

#
# never strip backend
#
STRIP=

#
# This part pulls in the subdirectory partial makefiles.
#
.include "${.CURDIR}/access/Makefile.inc"
.include "${.CURDIR}/bootstrap/Makefile.inc"
.include "${.CURDIR}/catalog/Makefile.inc"
.include "${.CURDIR}/commands/Makefile.inc"
.include "${.CURDIR}/executor/Makefile.inc"
.include "${.CURDIR}/lib/Makefile.inc"
.include "${.CURDIR}/libpq/Makefile.inc"
.include "${.CURDIR}/main/Makefile.inc"
.include "${.CURDIR}/nodes/Makefile.inc"
.include "${.CURDIR}/parser/Makefile.inc"
.include "${.CURDIR}/planner/Makefile.inc"
.include "${.CURDIR}/port/Makefile.inc"
.include "${.CURDIR}/rewrite/Makefile.inc"
.include "${.CURDIR}/rules/Makefile.inc"
.include "${.CURDIR}/storage/Makefile.inc"
.include "${.CURDIR}/tcop/Makefile.inc"
.include "${.CURDIR}/utils/Makefile.inc"

#
# If you change the value of the folowing you need to "bmake clean; bmake"
# Only other solution is to track the dependecies, but that seems
# unreasonable.  But if you feel inspired, I suppose you could grep -w
# the sources for POSTGRESDIR, DATADIR, etc and make a dependency
# below somewhere.  Wouldn't larry wall do that?
#
CFLAGS+=  -DPOSTGRESDIR='"${POSTGRESDIR}"' \
	-DDATADIR='"${DATADIR}"' \
	-DBINDIR='"${BINDIR}"' \
	-I${.CURDIR}/. -I${.CURDIR}/obj \
	-I${.CURDIR}/port/${PORTNAME}

LDADD=	-lm -lln

#
# XXX - grot: rename tmp to something sensible
#
CFLAGS+= -I${.CURDIR}/tmp

#
# XXX - who makes this?
#
CLEANFILES+= y.output

# TODO: include tmp/*.h and port/?/*.h files in $HEADERS


afterinstall:
	install ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		postmaster ${DESTDIR}${BINDIR}/postmaster
	for bki in ${BKIFILES}; do \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$bki ${DESTDIR}${DATADIR}/files/$$bki; \
	done

###
# `depend' experiment: since the backend has a bazillion source
#	files, doing a mkdep on the whole thing is unweildly. This
#	method has a higher fixed cost, but we only remake the
#	dependecies we need to.
#
.SUFFIXES: .d
.c.d:
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.IMPSRC}; \
	mv .depend ${.PREFIX}.d

DSRCS+= ${SRCS:S/.c/.d/g}

depend: .depend
.depend: ${DSRCS}
	cat ${DSRCS} > .depend

#
# Special rule to generate cpp'd version of a .c file.  This is especially
# useful given all the hellish macro processing going on.  The cpp'd
# version has a .C suffix.  To create foo.C from foo.c, just type
#
#	bmake foo.C
#
.SUFFIXES: .C
.c.C:
	${CC} -E ${CFLAGS} ${.IMPSRC} | cat -s > ${.TARGET}; \
	indent ${.TARGET}

.include <postgres.prog.mk>

#
# (have to set this after the .include so target "all" gets defined)
# Bit of a hack.  Could redo .mk files to handle multiple
# program targets based on a common or overlapping set of
# sources.
#
.PATH: ${.CURDIR}/postmaster
postmaster: postmaster.o ${OBJS}
	${CC} ${CFLAGS} -o postmaster postmaster.o ${OBJS:S/^main.o$//} ${LDADD}
all: postmaster
