#	@(#)bsd.subdir.mk	5.10 (Berkeley) 8/7/91

.MAIN: all

STRIP?=	-s

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

OBJDEST?=       /private/obj/
OBJSTRIPPREFIX?=        /usr/

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR}; do \
		(if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			echo "===> $${entry}.${MACHINE}"; \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			echo "===> $$entry"; \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:realinstall=install}); \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all

.if !target(all)
all: _SUBDIRUSE
.endif

.if !target(clean)
clean: _SUBDIRUSE
.endif

.if !target(cleandir)
cleandir: _SUBDIRUSE
.endif

.if !target(depend)
depend: _SUBDIRUSE
.endif

.if !target(manpages)
manpages: _SUBDIRUSE
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif
.if !target(maninstall)
maninstall: _SUBDIRUSE
.endif

.if !target(lint)
lint: _SUBDIRUSE
.endif

#
# Build objs for subdirs too (so users can put local
# Makefiles in them)
# 
.if !target(obj)
.if defined(NOOBJ)
obj: _SUBDIRUSE
.else
obj: _SUBDIRUSE
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
objdir: _SUBDIRUSE
.else
objdir: _SUBDIRUSE
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
localobj: _SUBDIRUSE
.else
localobj: _SUBDIRUSE
	@-cd ${.CURDIR}; \
	rm -f obj >/dev/null 2>&1; \
	mkdir obj 2>/dev/null; \
	true
.endif
.endif

.if !target(tags)
tags: _SUBDIRUSE
.endif
