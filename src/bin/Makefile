#
# $Header$
#

.include <postgres.global.mk>

#
# C programs
#
SUBDIR=  monitor pagedoc pg_id pg_ufp pg_version shmemdoc
#
# Shell scripts
#
SUBDIR+= cleardbdir createdb createuser destroydb destroyuser \
	initdb newbki vacuum ipcclean
#
# Subsystems
#
SUBDIR += fsutils s2kutils

#
# the postmaster had so many files in common with the backend (like 80%)
# so i just made it a second target in the backend Makefile.
#

.if defined(OLD_MKDIR)
beforeinstall:
	@-if test ! -d ${DESTDIR}${BINDIR}; \
		then mkdir ${DESTDIR}${BINDIR}; fi
.endif

.include <postgres.subdir.mk>
