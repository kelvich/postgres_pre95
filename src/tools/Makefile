#
# $Header$
#

.include <postgres.global.mk>

SUBDIR= bmake bmkdir mkdep
.if (${PORTNAME} == "hpux")
SUBDIR+= bsdinst
.endif
.if (${PORTNAME} == "aix")
SUBDIR+= mkldexport
.endif

.include <postgres.subdir.mk>
