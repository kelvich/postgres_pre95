CATD= ${.CURDIR}/catalog
.PATH: ${CATD}

SRCS+= cat.c catalog.c catname.c heap.c index.c indexing.c \
	pg_aggregate.c pg_lobj.c pg_naming.c pg_operator.c \
	pg_proc.c pg_type.c

HEADERS+= catmacros.h catname.h indexing.h pg_aggregate.h \
	 pg_am.h pg_amop.h pg_amproc.h pg_attribute.h pg_database.h \
	 pg_defaults.h pg_demon.h pg_group.h pg_index.h pg_inheritproc.h \
	 pg_inherits.h pg_ipl.h pg_language.h pg_listener.h \
	 pg_lobj.h pg_log.h pg_magic.h pg_naming.h pg_opclass.h \
	 pg_operator.h pg_parg.h pg_platter.h pg_plmap.h pg_proc.h \
	 pg_protos.h pg_prs2plans.h pg_prs2rule.h pg_prs2stub.h \
	 pg_relation.h pg_rewrite.h pg_server.h pg_statistic.h \
	 pg_time.h pg_type.h pg_user.h pg_variable.h pg_version.h \
	 syscache.h sysunmap.h

#
# The following is to create the .bki files.
# TODO: sort headers, (figure some automatic way of of determining
#  the bki sources?)
#
#
# XXX - more grot.  includes names and uid's in the header file.  FIX THIS
#  (not sure if i got this right - which do i need - or should i 
#   burn the whole damned thing)
#
CFLAGS+= -DALLOW_PG_GROUP
BKIOPTS= -DALLOW_PG_GROUP

GENBKI= ${CATD}/genbki.sh
BKIFILES= global1.bki local1_template1.bki 

GLOBALBKI_SRCS= pg_database.h pg_demon.h pg_magic.h pg_defaults.h \
	pg_variable.h pg_server.h pg_user.h pg_log.h pg_time.h

LOCALBKI_SRCS= pg_proc.h pg_type.h pg_attribute.h pg_relation.h \
	pg_inherits.h pg_index.h pg_version.h pg_statistic.h pg_operator.h \
	pg_opclass.h pg_am.h pg_amop.h pg_amproc.h pg_language.h pg_parg.h \
	pg_aggregate.h pg_ipl.h pg_inheritproc.h pg_platter.h pg_plmap.h \
	pg_prs2rule.h pg_prs2plans.h pg_prs2stub.h pg_rewrite.h \
	pg_listener.h pg_naming.h pg_lobj.h indexing.h

global1.bki: ${GENBKI} ${GLOBALBKI_SRCS}
	sh ${GENBKI} ${BKIOPTS} ${.ALLSRC:S/${GENBKI}//:} > ${.TARGET}

local1_template1.bki: ${GENBKI} ${LOCALBKI_SRCS}
	sh ${GENBKI} ${BKIOPTS} ${.ALLSRC:S/${GENBKI}//:} > ${.TARGET}
	

${PROG}: ${BKIFILES}

CLEANFILES+= ${BKIFILES}
