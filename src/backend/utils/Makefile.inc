.include "${.CURDIR}/utils/adt/Makefile.inc"
.include "${.CURDIR}/utils/cache/Makefile.inc"
.include "${.CURDIR}/utils/error/Makefile.inc"
.include "${.CURDIR}/utils/fmgr/Makefile.inc"
.include "${.CURDIR}/utils/hash/Makefile.inc"
.include "${.CURDIR}/utils/init/Makefile.inc"
.include "${.CURDIR}/utils/mmgr/Makefile.inc"
.include "${.CURDIR}/utils/sort/Makefile.inc"
.include "${.CURDIR}/utils/time/Makefile.inc"

UTILD= ${.CURDIR}/utils
CATD= ${.CURDIR}/catalog

.PATH: ${UTILD}

GENFMGRTAB= ${UTILD}/Gen_fmgrtab.sh
GENFMGRTABFILES= fmgr.h fmgrtab.c

#
# BKIOPTS is set in ../catalog/Makefile.inc and sets the -D flags for
# the DATA(...); statements.  Hence, ../catalog/Makefile.inc had better
# get slurped in prior to this Makefile.inc, or BKIOPTS should be set
# in a higher directory level.
#
${GENFMGRTABFILES}: ${GENFMGRTAB} ${CATD}/pg_proc.h
	sh ${SHOPTS} ${GENFMGRTAB} ${BKIOPTS} ${CATD}/pg_proc.h; \
	rm -f ${UTILD}/fmgr.h; \
	ln -s ${.CURDIR}/obj/fmgr.h ${UTILD}

${UTILD}/fmgr.h: ${GENFMGRTABFILES}

.depend: ${GENFMGRTABFILES}

${PROG}: ${UTILD}/fmgr.h

CLEANFILES+= ${GENFMGRTABFILES}

SRCS+= fmgrtab.c

HEADERS+= builtins.h catcache.h dynamic_loader.h \
	 exc.h exception.h excid.h fcache.h fmgr.h geo-decls.h hsearch.h \
	 inval.h large_object.h log.h lselect.h lsyscache.h \
	 mcxt.h memutils.h module.h nabstime.h oidcompos.h palloc.h \
	 params.h pg_malloc.h psort.h rel.h relcache.h syms.h
