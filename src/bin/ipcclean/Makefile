#
# $Header$
#

.include <postgres.global.mk>

.PATH: ${.CURDIR}/${PORTNAME}

.if (${PORTNAME} == "ultrix4" || ${PORTNAME} == "hpux")
IPCCLEANPATH=/bin:/usr/bin:/usr/ucb
.elif (${PORTNAME} == "sparc" || ${PORTNAME} == "alpha" || ${PORTNAME} == "aix")
IPCCLEANPATH=/bin:/sbin:/usr/ucb
.endif

SEDSCRIPT= -e "s^_fUnKy_IPCCLEANPATH_sTuFf_^${IPCCLEANPATH}^g"

SHPROG= ipcclean

.include <postgres.shell.mk>
