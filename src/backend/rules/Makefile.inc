#
# $Header$
#
.include "${.CURDIR}/rules/prs2/Makefile.inc"
.include "${.CURDIR}/rules/stubs/Makefile.inc"

.PATH: ${.CURDIR}/rules

HEADERS+= params.h prs2.h prs2locks.h prs2stub.h rac.h rlock.h
