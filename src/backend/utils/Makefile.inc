#
# $Header$
#
.include "${.CURDIR}/utils/adt/Makefile.inc"
.include "${.CURDIR}/utils/cache/Makefile.inc"
.include "${.CURDIR}/utils/error/Makefile.inc"
.include "${.CURDIR}/utils/fmgr/Makefile.inc"
.include "${.CURDIR}/utils/hash/Makefile.inc"
.include "${.CURDIR}/utils/init/Makefile.inc"
.include "${.CURDIR}/utils/mmgr/Makefile.inc"
.include "${.CURDIR}/utils/sort/Makefile.inc"
.include "${.CURDIR}/utils/time/Makefile.inc"

UTILD=${.CURDIR}/utils
CATD=${.CURDIR}/catalog
.PATH: ${UTILD} ${CATD}

FMGRTABFILES= fmgrtab.c fmgr.h
GENFMGRTAB= ${UTILD}/Gen_fmgrtab.sh

${FMGRTABFILES}: ${GENFMGRTAB} ${CATD}/pg_proc.h
	sh ${SHOPTS} ${GENFMGRTAB} ${CATD}/pg_proc.h

${UTILD}/fmgr.h: ${FMGRTABFILES}
	rm -f ${.TARGET}; \
	ln -s ${.CURDIR}/obj/fmgr.h ${.TARGET}

HEADERS+= builtins.h catcache.h dynamic_loader.h exc.h exception.h \
	 excid.h fcache.h fmgrtab.h geo-decls.h hsearch.h \
	 inval.h large_object.h log.h lselect.h lsyscache.h \
	 mcxt.h memutils.h module.h nabstime.h oidcompos.h palloc.h \
	 params.h pg_malloc.h psort.h rel.h relcache.h syms.h

${PROG}: ${UTILD}/fmgr.h

SRCS+= fmgrtab.c

HEADERS+= fmgr.h

CLEANFILES+= ${FMGRTABFILES}
