.include <postgres.global.mk>

.MAIN: all

.SUFFIXES: .cat .ps .me .ms

DOCSRCS= am.me smgr.me fmgr.me libpq.me fastpath.me parse-tree.me query-tree.me

NROFF=nroff

GROFF=groff -set

#DOCOBJS= ${DOCSRCS:S/.me/.cat/g} ${DOCSRCS:S/.me/.ps/g}
DOCOBJS= ${DOCSRCS:S/.me/.ps/g} README

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
	done

.include <postgres.prog.mk>
