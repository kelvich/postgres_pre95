#
# Pull in Makefile.global's from this and any parent dirs.
# If a Makefile.global also exists in the obj/ directory,
# include that afterwards (so it can override any previous
# settings).
#
.if !defined(NOINCLUDE)
.if exists(Makefile.private)
.    include "Makefile.private"
.endif
.if exists(${.CURDIR}/../Makefile.global)
.    if exists(${.CURDIR}/../../Makefile.global)
.	if exists(${.CURDIR}/../../../Makefile.global)
.	    if exists(${.CURDIR}/../../../../Makefile.global)
.		include "${.CURDIR}/../../../../Makefile.global"
.		if exists(${.CURDIR}/../../../../obj/Makefile.global)
.		    include "${.CURDIR}/../../../../obj/Makefile.global"
.		endif
.	    endif
.	    include "${.CURDIR}/../../../Makefile.global"
.	    if exists(${.CURDIR}/../../../obj/Makefile.global)
.		include "${.CURDIR}/../../../obj/Makefile.global"
.	    endif
.	endif
.	include "${.CURDIR}/../../Makefile.global"
.	if exists(${.CURDIR}/../../obj/Makefile.global)
.	    include "${.CURDIR}/../../obj/Makefile.global"
.	endif
.    endif
.    include "${.CURDIR}/../Makefile.global"
.    if exists(${.CURDIR}/../obj/Makefile.global)
.	include "${.CURDIR}/../obj/Makefile.global"
.    endif
.endif

.if exists(${.CURDIR}/Makefile.global)
.    include "${.CURDIR}/Makefile.global"
.endif

.if exists(${.CURDIR}/obj/Makefile.global)
.    include "${.CURDIR}/obj/Makefile.global"
.endif
.endif
