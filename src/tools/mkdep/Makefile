#
# $Header$
#

.include <postgres.global.mk>

depend lint tags:

SHPROG=mkdep

.if (${PORTNAME} == "hpux")
SEDSCRIPT= -e 's/^cc /gcc -traditional /'
.endif

.include <postgres.shell.mk>
