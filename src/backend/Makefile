#
# $Header$
#
# The following turns on intermediate linking of big objects to speed
# the link cycle during development.  It's on by default.  You can
# disable it by commenting out the line in your local copy, or by
# putting:
#	.undef BIGOBJS
# in the file obj/Makefile.global in your main tree.
#
BIGOBJS= true

.include <postgres.global.mk>

PROG= postgres

#
# never strip backend
#
STRIP=

#
# Build the file, "./ID", used by the "gid" (grep-for-identifier) tool
#
IDFILE= ID
${IDFILE}:
	cd ${.CURDIR}; ./makeID ${PORTNAME}

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
SRCS:=
.endif

# the following to to get HEADERS from tmp/ (for ctags)

.include "${.CURDIR}/tmp/Makefile.inc"

.if defined (BIGOBJS)
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

.endif

#
# If you change the value of the following you need to "bmake clean;
# bmake".  The only other solution is to track the dependencies, but
# that seems unreasonable.  If you feel inspired, I suppose you could
# "grep -w" the sources for POSTGRESDIR, DATADIR, etc. and make a
# dependency below somewhere.
#
CFLAGS+=  -DPOSTGRESDIR='"${POSTGRESDIR}"' \
	-DDATADIR='"${DATADIR}"' \
	-DBINDIR='"${BINDIR}"' \
	-I${.CURDIR}/. -I${.CURDIR}/${MAKEOBJDIR} \
	-I${.CURDIR}/tmp \
	-I${.CURDIR}/port/${PORTNAME}

#
# All systems require the math library.
# Loader flags for system-dependent libraries are appended in
#	src/backend/port/<port>/Makefile.inc
#
LDADD+=	-lm

#
# == BEGIN MACHINE-SPECIFIC SECTION ==
#

.if defined(OLD_MKDIR)
beforeinstall:
	@-if test ! -d ${DESTDIR}${BINDIR}; \
		then mkdir ${DESTDIR}${BINDIR}; fi
.endif

#
# Install the bki files to the data directory.  We also copy a version
# of them that has "PGUID" intact, so one can change the value of the
# postgres userid before running initdb in the case of customizing the
# binary release (i.e., fixing up PGUID w/o recompiling the system).
# Those files are copied out as foo.source.  The program newbki(1) can
# be run later to reset the postgres login id (but it must be run before
# initdb is run, or after clearing the data directory with
# cleardbdir(1)).
#
afterinstall: pg_id
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		postmaster ${DESTDIR}${BINDIR}/postmaster
.if defined(OLD_MKDIR)
	@-if test ! -d ${DESTDIR}${DATADIR}; \
		then mkdir ${DESTDIR}${DATADIR}; fi
	@-if test ! -d ${DESTDIR}${DATADIR}/files; \
		then mkdir ${DESTDIR}${DATADIR}/files; fi
.endif
	-PG_UID=`./pg_id ${POSTGRESLOGIN}`; \
	case $$PG_UID in "NOUSER") \
		echo "Warning: no account named ${POSTGRESLOGIN}, using yours";\
		PG_UID=`./pg_id`;; \
	esac ;\
	for bki in ${BKIFILES}; do \
		sed "s/PGUID/$$PG_UID/" < $$bki > $$bki.sed ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$bki.sed ${DESTDIR}${DATADIR}/files/$$bki; \
		rm -f $$bki.sed; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		    $$bki ${DESTDIR}${DATADIR}/files/$$bki.source; \
	done

#
# so we can get the UID of the postgres owner (w/o moving pg_id to
# src/tools)
#
.PATH: ${.CURDIR}/../bin/pg_id
pg_id:	pg_id.c
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC} 

CLEANFILES+= pg_id


#
# depend target: since the backend has a bazillion source files, doing
# a mkdep on the whole thing is unwieldly.  This method has a higher
# fixed cost, but we only remake the dependencies we need to and don't
# lose if the mkdep fails on a file.
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
# Special rule to generate cpp'd version of a .c file.  This is
# especially useful given all the hellish macro processing going on.
# The cpp'd version has a .C suffix.  To create foo.C from foo.c, just
# type
#	bmake foo.C
#
.SUFFIXES: .C
.c.C:
	${CC} -E ${CFLAGS} ${.IMPSRC} | cat -s | cb > ${.TARGET}

cppall: ${SRCS:S/.c/.C/g}

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
# To use Purify (SunOS only), define PURIFY to be the path (and
# options) with which to invoke the Purify loader.  Only the executable
# needs to be loaded with Purify.
#
# (This has to be after <postgres.prog.mk>, which unconditionally
#  redefines the PROG target :-P)
#
.if defined(PURIFY)
${PROG}: ${OBJS} ${LIBC} ${DPADD}
	${PURIFY} ${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}
.endif

#
# We have to set this after the .include so target "all" gets defined.
# This is a bit of a hack.  Could redo the .mk files to handle multiple
# program targets based on a common or overlapping set of sources.
#
.PATH: ${.CURDIR}/postmaster
postmaster: postmaster.o ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} postmaster.o ${OBJS:S/^main.o$//} ${LDADD}
all: postmaster

CLEANFILES+= postmaster postmaster.o
