#
# $Header$
#

.include <postgres.global.mk>

depend lint tags:

SHPROG=mkdep

.if (${PORTNAME} == "hpux")
SEDSCRIPT= -e 's/^cc /gcc -traditional /'
.endif

.if (${PORTNAME} == "sparc_solaris")
SEDSCRIPT= -e 's/^cc /solcc /'
.endif

.if (${PORTNAME} == "aix")
SEDSCRIPT= -e 's/^cc /cc -c /' -e 's/^.AIX//'
.endif

.include <postgres.shell.mk>
