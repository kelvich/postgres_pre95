# No man pages for now
NOMAN= true

.SUFFIXES: .out .o .c .y .l .s .8 .7 .6 .5 .4 .3 .2 .1 .0

.8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -man ${.IMPSRC} > ${.TARGET}

CFLAGS+=${COPTS}

STRIP?=	-s

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

LIBC?=		/usr/lib/libc.a
LIBCOMPAT?=	/usr/lib/libcompat.a
LIBCURSES?=	/usr/lib/libcurses.a
LIBDBM?=	/usr/lib/libdbm.a
LIBDES?=	/usr/lib/libdes.a
LIBL?=		/usr/lib/libl.a
LIBKDB?=	/usr/lib/libkdb.a
LIBKRB?=	/usr/lib/libkrb.a
LIBKVM?=	/usr/lib/libkvm.a
LIBM?=		/usr/lib/libm.a
LIBMP?=		/usr/lib/libmp.a
LIBPC?=		/usr/lib/libpc.a
LIBPLOT?=	/usr/lib/libplot.a
LIBRESOLV?=	/usr/lib/libresolv.a
LIBRPC?=	/usr/lib/sunrpc.a
LIBTERM?=	/usr/lib/libterm.a
LIBUTIL?=	/usr/lib/libutil.a

OBJDEST?=       /private/obj/
OBJSTRIPPREFIX?=        /usr/

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c
.endif

.if defined(PROG)
.if defined(SRCS)

OBJS+=  ${SRCS:R:S/$/.o/g}

${PROG}: ${OBJS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

.else defined(SRCS)

SRCS= ${PROG}.c

${PROG}: ${SRCS} ${LIBC} ${DPADD}
	${CC} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/${SRCS} ${LDADD}

MKDEP=	-p

.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${PROG}.0
.endif
.endif
.if !defined(NOMAN)
MANALL=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8}
.else
MANALL=
.endif
manpages: ${MANALL}

_PROGSUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(echo "===> $$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/}); \
	done
.endif

.if !target(all)
.MAIN: all
all: ${PROG} ${MANALL} _PROGSUBDIR
.endif

.if !target(clean)
clean: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog core.${PROG} ${PROG} ${OBJS} ${CLEANFILES}
.endif

.if !target(cleandir)
cleandir: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog core core.${PROG} ${PROG} ${OBJS} ${CLEANFILES} \
	.depend ${MANALL} *.d
.endif

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: .depend _PROGSUBDIR
.depend: ${SRCS}
.if defined(PROG)
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _PROGSUBDIR
.if defined(PROG)
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}/${PROG}
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif

install: afterinstall maninstall
afterinstall: realinstall
realinstall: beforeinstall
.if defined(ALLINSTALL)
beforeinstall: all
.endif
.endif

.if !target(lint)
lint: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=${OBJDEST}`echo $$here | sed 's,${OBJSTRIPPREFIX},,'`; \
	echo "$$here -> $$dest"; ln -s $$dest obj; \
	if test ! -d $$dest; then \
		bmkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(objdir)
.if defined(NOOBJ)
objdir: _PROGSUBDIR
.else
objdir: _PROGSUBDIR
	@cd ${.CURDIR}; \
	dest=`ls -ld obj | awk '{print $$NF}'`; \
	if test ! -d $$dest; then \
		bmkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(localobj)
.if defined(NOOBJ)
localobj:
.else
localobj:
	@-cd ${.CURDIR}; \
	rm -f obj >/dev/null 2>&1; \
	mkdir obj 2>/dev/null; \
	true
.endif
.endif

.if !target(TAGS)
TAGS: ${HEADERS} ${SRCS} _PROGSUBDIR
.if defined(PROG)
.if (${.CURDIR} == "..")
	-rm -f TAGS; \
	etags -t ${.ALLSRC:S%access/common/../../%%g}; \
	sed -e '/^.$$/,/,/s%^%obj/%' \
	    -e 's%^obj/\(.\)$$%\1%' \
	    -e "s%obj/\.\./%%" < TAGS > ../TAGS
.else
	-cd ${.CURDIR}; \
	rm -f TAGS; \
	etags -t ${.ALLSRC:S%${.CURDIR}/%%g}
.endif
.endif
.endif

.if !target(tags)
tags: _linktags _maketags

_maketags: ${HEADERS} ${SRCS} _PROGSUBDIR
.if defined(PROG)
#  WAS:
#
#	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC} | \
#	    sed "s;${.CURDIR}/;;" > tags
#
# The following is just AWFUL.  The problem is that ${.ALLSRC} is
# gigantic due to the cross product of the number of sources times
# the length of ${.CURDIR} if we are using symbolic linked obj directories
# (that is, if we are not using a local tree), so the whole mess is just way too big for an
# argument vector.  So we move to ${.CURDIR} and strip it off
# the sources, then anything that is relative to the (old) current 
# directory (doesn't have a slash in it) gets a duplicate with
# an obj/ prepended to
# it (which breaks if we aren't using "obj" directories), and the
# mess is fed to ctags.  Then we massage the tags output to have the
# absolute pathname in the file reference so we can refer to this
# tags file from any source subdirectory below.  Wheww...  There
# must be a better way...
#
# If we are using local obj dirs (${.CURDIR} == "..") it becomes
# marginally easier.
#
.if ${.CURDIR} == ".."
	-rm -f tags; \
	ctags -t ${.ALLSRC:S%access/common/../../%%g}; \
	HERE=`pwd`/; \
	sed "s,	,	$$HERE," < tags > ../tags; \
	#rm tags
.else
	-cd ${.CURDIR}; \
	    rm -f tags; \
	    ctags -t `echo  ${.ALLSRC:S,${.CURDIR}/,,g} | \
		tr ' ' '\012' | awk '! /\// {print "obj/"$$1; } {print}'` 2> /dev/null ; \
	    mv tags tags.tmp; \
	    TOPDIR=`pwd`/; \
	    sed "s,	,	$$TOPDIR," < tags.tmp > tags; \
	    rm tags.tmp
.endif
.endif
_linktags::
.if defined(PROG)
#
# and this is a hack too - should explicitly mark what directories
# are to have tags links
#
	-cd ${.CURDIR};\
		TAGFILE=`pwd`/tags; \
		find * -name tags -exec rm -f {} \; ; \
		find * ! -name RCS -type d -exec ln -s $$TAGFILE {} \; ;
.endif
.endif

.if !defined(NOMAN)
.include <postgres.man.mk>
.else
maninstall:
.endif
