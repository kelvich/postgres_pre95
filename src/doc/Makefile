.include <postgres.global.mk>

.MAIN: all

PG_DOCDIR=${POSTDOCDIR}

SRCS= postgres-setup.me release4.2.me
DOCOBJS= ${SRCS:S/.me/.cat/g} ${SRCS:S/.me/.ps/g}

SUBDIRS= implementation papers user_manual

all: ${DOCOBJS}

install: all
	@-if [ ! -d ${PG_DOCDIR} ]; then \
		mkdir ${PG_DOCDIR}; \
	fi
	for i in ${DOCOBJS}; do \
		${INSTALL} -m 444 $$i ${PG_DOCDIR}/$$i; \
	done
	for i in ${SUBDIRS}; do \
		(cd ${.CURDIR}/$$i; bmake all install); \
	done

.include <postgres.prog.mk>
