#
# Pull in Makefile.global's from this and any parent dirs.
# If a Makefile.global also exists in the obj/ directory,
# include that afterwards (so it can override any previous
# settings).  We use "./..." to disable make from doing a path
# search to locate files as this gets *very* confusing on
# a large system like this.
#
.if !defined(NOINCLUDE)
.if exists(./Makefile.local)
.    include "./Makefile.local"
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
