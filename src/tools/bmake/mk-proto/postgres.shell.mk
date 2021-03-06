#
# postgres.shell.mk - originally based on
#	@(#)bsd.prog.mk 5.35 (Berkeley) 8/26/92
#
# $Header$
#

# No man pages for now
NOMAN= true

.SUFFIXES: .sh .8 .7 .6 .5 .4 .3 .2 .1 .0

.8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -man ${.IMPSRC} > ${.TARGET}

SHFLAGS+=${SHOPTS}

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

INSTALL?=	install

MAKEOBJDIR?=	obj

OBJDEST?=       /private/obj/
OBJSTRIPPREFIX?=        /usr/

.if defined(SHPROG)

${SHPROG}: ${SHPROG}.sh
.if defined(SEDSCRIPT)
	sed ${SEDSCRIPT} < ${.ALLSRC} > ${SHPROG}
.else
	cp ${.ALLSRC} ${SHPROG}
.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(NOMAN)
MAN1=	${SHPROG}.0
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
all: ${SHPROG} ${MANALL} _PROGSUBDIR
.endif

.if !target(clean)
clean: _PROGSUBDIR
	rm -f ${SHPROG}
.endif

.if !target(cleandir)
cleandir: _PROGSUBDIR
	rm -f ${SHPROG}
.endif

.if !target(depend)
depend: .depend _PROGSUBDIR
.depend:
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _PROGSUBDIR
.if defined(SHPROG)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${SHPROG} ${DESTDIR}${BINDIR}/${SHPROG}
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
lint: _PROGSUBDIR
.endif

#
# "obj" places links pointing into the object tree (rooted at OBJDEST)
# inside of the source tree.
#
# The object directories themselves may not exist yet and must be created
# by doing "objdir".
#
# The convention is that the name of the object tree for /foo/bar/...
# is ${OBJDEST}/foo/bar/...  though /usr/foo/bar/... is special-cased
# to become ${OBJDEST}/foo/bar/... as well.
#
.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf ${MAKEOBJDIR}; \
	here=`pwd`; dest=${OBJDEST}`echo $$here | sed 's,${OBJSTRIPPREFIX},,'`; \
	echo "$$here -> $$dest"; ln -s $$dest ${MAKEOBJDIR}; \
	if test ! -d $$dest; then \
		bmkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

#
# "objdir" actually creates the object tree directories.
#
# It assumes that the links generated by the "obj" target already exist.
#
.if !target(objdir)
.if defined(NOOBJ)
objdir: _PROGSUBDIR
.else
objdir: _PROGSUBDIR
	@cd ${.CURDIR}; \
	dest=`ls -ld ${MAKEOBJDIR} | awk '{print $$NF}'`; \
	if test ! -d $$dest; then \
		bmkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

#
# "localobj" creates actual "obj" directories inside of the source tree.
#
# This is used in source trees that have been copied from a master tree.
#
.if !target(localobj)
.if defined(NOOBJ)
localobj:
.else
localobj:
	@-cd ${.CURDIR}; \
	[ ! -d obj ] && mkdir obj 2>/dev/null; \
	true
.endif
.endif

#
# We do not have to duplicate Marc's disgusting tagsfile hack here
# (c.f. postgres.prog.mk) since non-program directoriess obviously
# don't have tags.  However, this directory may have subdirectories
# that do have tags.
#
.if !target(tags)
tags: _PROGSUBDIR
.endif

.if !defined(NOMAN)
.include <postgres.man.mk>
.else
maninstall:
.endif
