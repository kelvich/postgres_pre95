#
# $Header$
#

.include <postgres.global.mk>

#
# C programs
#
SUBDIR=  pgperl spog relshow
.if (${PORTNAME} == "sparc")
SUBDIR+= xposthelp
.endif

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
