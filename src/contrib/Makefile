#
# $Header$
#

.include <postgres.global.mk>

#
# C programs
#
SUBDIR=  pgperl spog tkmonitor
.if (${PORTNAME} == "sparc")
SUBDIR+= xposthelp
.endif

#
# Shell scripts
#
SUBDIR+= pgbrowse

#
# Subsystems
#
SUBDIR += cplusplus

.if defined(OLD_MKDIR)
beforeinstall:
	@-if test ! -d ${DESTDIR}${BINDIR}; \
		then mkdir ${DESTDIR}${BINDIR}; fi
.endif

.include <postgres.subdir.mk>
