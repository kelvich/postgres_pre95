#
# The following turns on intermediate linking of big objects
# to speed the link cycle during development.  It's on by default.
# You can disable it by commenting out the line in your local copy,
# or by putting:
#	.undef BIGOBJS
# in the file obj/Makefile.global in your main tree.
#
BIGOBJS= true

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
.undef SRCS
.include "${.CURDIR}/access/Makefile.inc"
.if defined (BIGOBJS)
OBJS_ACCESS:= ${SRCS:S/.c/.o/g}
SRCS_ACCESS:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/bootstrap/Makefile.inc"
.if defined (BIGOBJS)
OBJS_BOOTSTRAP:= ${SRCS:S/.c/.o/g}
SRCS_BOOTSTRAP:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/commands/Makefile.inc"
.if defined (BIGOBJS)
OBJS_COMMANDS:= ${SRCS:S/.c/.o/g}
SRCS_COMMANDS:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/executor/Makefile.inc"
.if defined (BIGOBJS)
OBJS_EXECUTOR:= ${SRCS:S/.c/.o/g}
SRCS_EXECUTOR:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/main/Makefile.inc"
.if defined (BIGOBJS)
OBJS_MAIN:= ${SRCS:S/.c/.o/g}
SRCS_MAIN:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/catalog/Makefile.inc"
.include "${.CURDIR}/lib/Makefile.inc"
.include "${.CURDIR}/libpq/Makefile.inc"
.if defined (BIGOBJS)
OBJS_MISC:= ${SRCS:S/.c/.o/g}
SRCS_MISC:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/nodes/Makefile.inc"
.if defined (BIGOBJS)
OBJS_NODES:= ${SRCS:S/.c/.o/g}
SRCS_NODES:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/parser/Makefile.inc"
.if defined (BIGOBJS)
OBJS_PARSER:= ${SRCS:S/.c/.o/g}
SRCS_PARSER:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/planner/Makefile.inc"
.if defined (BIGOBJS)
OBJS_PLANNER:= ${SRCS:S/.c/.o/g}
SRCS_PLANNER:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/port/Makefile.inc"
.if defined (BIGOBJS)
OBJS_PORT:= ${SRCS:S/.c/.o/g}
SRCS_PORT:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/rewrite/Makefile.inc"
.if defined (BIGOBJS)
OBJS_REWRITE:= ${SRCS:S/.c/.o/g}
SRCS_REWRITE:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/rules/Makefile.inc"
.if defined (BIGOBJS)
OBJS_RULES:= ${SRCS:S/.c/.o/g}
SRCS_RULES:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/storage/Makefile.inc"
.if defined (BIGOBJS)
OBJS_STORAGE:= ${SRCS:S/.c/.o/g}
SRCS_STORAGE:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/tcop/Makefile.inc"
.if defined (BIGOBJS)
OBJS_TCOP:= ${SRCS:S/.c/.o/g}
SRCS_TCOP:= ${SRCS}
SRCS:=
.endif
.include "${.CURDIR}/utils/Makefile.inc"
.if defined (BIGOBJS)
OBJS_UTILS:= ${SRCS:S/.c/.o/g}
SRCS_UTILS:= ${SRCS}
SRCS:=	${SRCS_ACCESS} ${SRCS_BOOTSTRAP} ${SRCS_COMMANDS} ${SRCS_EXECUTOR} \
	${SRCS_MAIN} ${SRCS_MISC} ${SRCS_NODES} ${SRCS_PARSER} \
	${SRCS_PLANNER} ${SRCS_PORT} ${SRCS_REWRITE} ${SRCS_RULES} \
	${SRCS_STORAGE} ${SRCS_TCOP} ${SRCS_UTILS}

PARTIAL_LINK: .USE 
	${LD} -r -o ${.TARGET} ${.ALLSRC}

ACCESS.o:	${OBJS_ACCESS} PARTIAL_LINK
CLEANFILES+=	${OBJS_ACCESS}
BOOTSTRAP.o:	${OBJS_BOOTSTRAP} PARTIAL_LINK
CLEANFILES+=	${OBJS_BOOTSTRAP}
COMMANDS.o:	${OBJS_COMMANDS} PARTIAL_LINK
CLEANFILES+=	${OBJS_COMMANDS}
EXECUTOR.o:	${OBJS_EXECUTOR} PARTIAL_LINK
CLEANFILES+=	${OBJS_EXECUTOR}
MISC.o:		${OBJS_MISC} ${OBJS_MAIN:S/^main.o$//} PARTIAL_LINK
CLEANFILES+=	${OBJS_MISC} main.o
NODES.o:	${OBJS_NODES} PARTIAL_LINK
CLEANFILES+=	${OBJS_NODES}
PARSER.o:	${OBJS_PARSER} PARTIAL_LINK
CLEANFILES+=	${OBJS_PARSER}
PLANNER.o:	${OBJS_PLANNER} PARTIAL_LINK
CLEANFILES+=	${OBJS_PLANNER}
PORT.o:		${OBJS_PORT} PARTIAL_LINK
CLEANFILES+=	${OBJS_PORT}
REWRITE.o:	${OBJS_REWRITE} PARTIAL_LINK
CLEANFILES+=	${OBJS_REWRITE}
RULES.o:	${OBJS_RULES} PARTIAL_LINK
CLEANFILES+=	${OBJS_RULES}
STORAGE.o:	${OBJS_STORAGE} PARTIAL_LINK
CLEANFILES+=	${OBJS_STORAGE}
TCOP.o:		${OBJS_TCOP} PARTIAL_LINK
CLEANFILES+=	${OBJS_TCOP}
UTILS.o:	${OBJS_UTILS} PARTIAL_LINK
CLEANFILES+=	${OBJS_UTILS}
SUBOBJS= ACCESS.o BOOTSTRAP.o COMMANDS.o EXECUTOR.o main.o MISC.o NODES.o \
	 PARSER.o PLANNER.o PORT.o REWRITE.o RULES.o STORAGE.o TCOP.o UTILS.o

.if defined(USE_DLD)
LDADD+= -ldld 
.endif

.endif

#
# If you change the value of the following you need to "bmake clean; bmake"
# Only other solution is to track the dependecies, but that seems
# unreasonable.  But if you feel inspired, I suppose you could grep -w
# the sources for POSTGRESDIR, DATADIR, etc and make a dependency
# below somewhere (wouldn't larry wall do that ;)?
#
CFLAGS+=  -DPOSTGRESDIR='"${POSTGRESDIR}"' \
	-DDATADIR='"${DATADIR}"' \
	-DBINDIR='"${BINDIR}"' \
	-I${.CURDIR}/. -I${.CURDIR}/obj \
	-I${.CURDIR}/port/${PORTNAME}

LDADD+=	-N -lm -lln

#
# XXX - grot: rename tmp to something sensible
#
CFLAGS+= -I${.CURDIR}/tmp

.if ${PORTNAME} == "ultrix4"
CFLAGS+= -DNOFIXADE
.endif

#
# XXX - who makes this?
#
CLEANFILES+= y.output

# TODO: include tmp/*.h and port/?/*.h files in $HEADERS


#
# Install the bki files to the data directory.  We also
# copy a version of them that has "PGUID" intact, so one
# can change the value of the postgres userid before
# running initdb in the case of customizing the binary
# release (i.e. fixing up PGUID w/o recompiling the system).
# Those files are copied out as foo.source.  The program
# newbki(1) can be run later to reset the postgres login id (but it must
# be run before initdb is run, or after clearing the data directory
# with cleardbdir(1)).
#
afterinstall: pg_id
	install ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		postmaster ${DESTDIR}${BINDIR}/postmaster
.if ${PORTNAME} == "sparc"
#
# 	actually, if mkdir doesn't have the -p flag.  we'll paramaterize
# 	all this stuff in the next release so please bear with us.
#
	if test ! -d ${DESTDIR}${DATADIR}; \
		then mkdir ${DESTDIR}${DATADIR}; fi
	if test ! -d ${DESTDIR}${DATADIR}/files; \
		then mkdir ${DESTDIR}${DATADIR}/files; fi
.endif
	PG_UID=`./pg_id ${POSTGRESLOGIN}`; \
	case $$PG_UID in "NOUSER") \
		echo "Warning: no account named ${POSTGRESLOGIN}, using yours";\
		PG_UID=`./pg_id`;; \
	esac ;\
	for bki in ${BKIFILES}; do \
		sed "s/PGUID/$$PG_UID/" < $$bki > $$bki.sed ; \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$bki.sed ${DESTDIR}${DATADIR}/files/$$bki; \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$bki ${DESTDIR}${DATADIR}/files/$$bki.source; \
	done

#
# so we can get the UID of the postgres owner (w/o moving pg_id to tools/)
#
.PATH: ${.CURDIR}/../bin/pg_id
pg_id:	pg_id.c
	${CC} -o pg_id ${.ALLSRC} 

CLEANFILES+= pg_ig

###
# depend target: since the backend has a bazillion source
#	files, doing a mkdep on the whole thing is unweildly. This
#	method has a higher fixed cost, but we only remake the
#	dependecies we need to and don't lose if the mkdep fails on
#	a file.
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
# Support partial loading of big .o's
#
.if defined(BIGOBJS)
CLEANFILES+= ${OBJS}
OBJS= ${SUBOBJS}
${PROG}: ${SUBOBJS}
.endif

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

CLEANFILES+= postmaster postmaster.o
