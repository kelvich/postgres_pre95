#
# Pull in Makefile.global's from this and any parent dirs.
# If a Makefile.global also exists in the obj/ directory,
# include that first.
#
.if !defined(NOINCLUDE)
.if exists(Makefile.private)
.	include "Makefile.private"
.endif
.if exists(${.CURDIR}/../Makefile.global)
.if exists(${.CURDIR}/../../Makefile.global)
.if exists(${.CURDIR}/../../../Makefile.global)
.if exists(${.CURDIR}/../../../../Makefile.global)
.if exists(${.CURDIR}/../../../../obj/Makefile.global)
.include "${.CURDIR}/../../../../obj/Makefile.global"
.endif
.include "${.CURDIR}/../../../../Makefile.global"
.endif
.if exists(${.CURDIR}/../../../obj/Makefile.global)
.include "${.CURDIR}/../../../obj/Makefile.global"
.endif
.include "${.CURDIR}/../../../Makefile.global"
.endif
.if exists(${.CURDIR}/../../obj/Makefile.global)
.include "${.CURDIR}/../../obj/Makefile.global"
.endif
.include "${.CURDIR}/../../Makefile.global"
.endif
.if exists(${.CURDIR}/../obj/Makefile.global)
.include "${.CURDIR}/../obj/Makefile.global"
.endif
.include "${.CURDIR}/../Makefile.global"
.endif
.if exists(${.CURDIR}/obj/Makefile.global)
.include "${.CURDIR}/obj/Makefile.global"
.endif
.if exists(${.CURDIR}/Makefile.global)
.include "${.CURDIR}/Makefile.global"
.endif
.endif

