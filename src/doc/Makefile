.include <postgres.global.mk>

.MAIN: all

.SUFFIXES: .cat .ps .me

DOCSRCS= manual.me postgres-setup.me release4.1.me

NROFF=nroff

GROFF=groff -spt

DOCOBJS= ${DOCSRCS:S/.me/.cat/g} ${DOCSRCS:S/.me/.ps/g}

.me.cat:
	${GROFF} -me -Tascii ${.IMPSRC} > ${.TARGET}

.me.ps:
	${GROFF} -me -Tps ${.IMPSRC} > ${.TARGET}

all: ${DOCOBJS}

.include <postgres.prog.mk>
