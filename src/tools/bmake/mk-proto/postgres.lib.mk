#	@(#)bsd.lib.mk	5.35 (Berkeley) 7/29/92

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.if exists(Makefile.local)
.include "Makefile.local"
.endif

LIBDIR?=	/usr/lib
LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	bin
LIBOWN?=	bin
LIBMODE?=	444

STRIP?=	-s

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

OBJDEST?=	/private/obj/
OBJSTRIPPREFIX?=	/usr/

.MAIN: all

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
.SUFFIXES:
.SUFFIXES: .out .o .po .s .c .f .y .l .8 .7 .6 .5 .4 .3 .2 .1 .0 .m4

.8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -man ${.IMPSRC} > ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} 
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.c.po:
	${CC} -p ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
.if (${MACHINE} == "mips" || ${MACHINE} == "sparc")
	@${LD} -X -r ${.TARGET}
.else
	@${LD} -r ${.TARGET}
.endif
	@mv a.out ${.TARGET}

#.if (${MACHINE} == "mips")
#NOPROFILE=1
#.endif

.s.o:
	${CPP} -E ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.s.po:
	${CPP} -E -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

MANALL=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8}
manpages: ${MANALL}

.if !defined(NOPROFILE)
_LIBS=lib${LIB}.a lib${LIB}_p.a
.else
_LIBS=lib${LIB}.a
.endif

all: ${_LIBS} # llib-l${LIB}.ln
.if !defined(NOMAN)
all: ${MANALL}
.endif

OBJS+=	${SRCS:R:S/$/.o/g}

lib${LIB}.a:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} cq lib${LIB}.a `lorder ${OBJS} | tsort` ${LDADD}
	ranlib lib${LIB}.a

POBJS+=	${OBJS:.o=.po}
lib${LIB}_p.a:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} cq lib${LIB}_p.a `lorder ${POBJS} | tsort` ${LDADD}
	ranlib lib${LIB}_p.a

llib-l${LIB}.ln: ${SRCS}
	${LINT} -C${LIB} ${CFLAGS} ${.ALLSRC:M*.c}

.if !target(clean)
clean:
	rm -f a.out Errs errs mklog ${CLEANFILES} ${OBJS} ${POBJS} \
	    profiled/*.o lib${LIB}.a lib${LIB}_p.a llib-l${LIB}.ln
.endif

.if !target(cleandir)
cleandir:
	rm -f a.out Errs errs mklog ${CLEANFILES} ${OBJS} ${POBJS} \
	    profiled/*.o lib${LIB}.a lib${LIB}_p.a llib-l${LIB}.ln
	rm -f ${MANALL} ${.CURDIR}/tags .depend
.endif

.if !target(depend)
depend: .depend
.depend: ${SRCS}
	mkdep ${CFLAGS:M-[ID]*} ${AINC} ${.ALLSRC}
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o *:/\1.o \1.po:/' < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall: beforeinstall
	${INSTALL} -c -o ${LIBOWN} -g ${LIBGRP} -m 664 lib${LIB}.a \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.a; \
	cd ${DESTDIR}${LIBDIR}; \
	ranlib lib${LIB}.a; \
	chmod ${LIBMODE} lib${LIB}.a
.if !defined(NOPROFILE)
	${INSTALL} -c -o ${LIBOWN} -g ${LIBGRP} -m 664 \
	    lib${LIB}_p.a ${DESTDIR}${LIBDIR}/lib${LIB}_p.a; \
	cd ${DESTDIR}${LIBDIR}; \
	ranlib lib${LIB}_p.a; \
	chmod ${LIBMODE} lib${LIB}_p.a
.endif
#	${INSTALL} -c -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
#	    llib-l${LIB}.ln ${DESTDIR}${LINTLIBDIR}
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

install: afterinstall
afterinstall: realinstall
.if !defined(NOMAN)
afterinstall: maninstall
.endif
#
# If ALLINSTALL is defined, then "make install" will build targets if necessary
#
.if defined(ALLINSTALL)
beforeinstall: all
.endif
.endif

.if !target(lint)
lint:
.endif

.if !target(tags)
tags: ${SRCS}
	ctags -t ${.ALLSRC:M*.c}; \
	mv tags tags.tmp; \
	sed "s;${.CURDIR}/;;" < tags.tmp > ${.CURDIR}/tags; \
#	rm tags.tmp
#	ctags -f is a 4.4ism
#	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:M*.c} | \
#	    sed "s;\${.CURDIR}/;;" > tags
.endif

.include <postgres.man.mk>
.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:
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
objdir:
.else
objdir:
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

