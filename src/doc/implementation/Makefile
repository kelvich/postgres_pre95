.include <postgres.global.mk>

.MAIN: all

.SUFFIXES: .cat .ps .me .ms

DOCSRCS= am.me array.me smgr.me fmgr.me libpq.me fastpath.me parse-tree.me query-tree.me

NROFF=nroff

GROFF=groff -set

DOCOBJS= ${DOCSRCS:S/.me/.ps/g}

.me.cat:
	${GROFF} -me -Tascii ${.IMPSRC} > ${.TARGET}

.me.ps:
	${GROFF} -me -Tps ${.IMPSRC} > ${.TARGET}

all: ${DOCOBJS}

install: all
	@-if [ ! -d ${POSTDOCDIR} ]; then \
		mkdir ${POSTDOCDIR}; \
	fi
	@-if [ ! -d ${POSTDOCDIR}/implementation ]; then \
		mkdir ${POSTDOCDIR}/papers; \
	fi
	for i in ${DOCOBJS}; do \
		${INSTALL} -m 444 $$i ${POSTDOCDIR}/implementation/$$i; \
	done; \
	${INSTALL} -c -m 444 README ${POSTDOCDIR}/implementation/README

.include <postgres.prog.mk>
