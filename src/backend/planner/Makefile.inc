.include "${.CURDIR}/planner/path/Makefile.inc"
.include "${.CURDIR}/planner/plan/Makefile.inc"
.include "${.CURDIR}/planner/prep/Makefile.inc"
.include "${.CURDIR}/planner/sys/Makefile.inc"
.include "${.CURDIR}/planner/util/Makefile.inc"

.PATH: ${.CURDIR}/planner

HEADERS+= allpaths.h cfi.h clause.h clauseinfo.h clauses.h clausesel.h \
	costsize.h createplan.h handleunion.h hashutils.h indexnode.h \
	indxpath.h initsplan.h internal.h joininfo.h joinpath.h joinrels.h \
	joinutils.h keys.h mergeutils.h ordering.h orindxpath.h paramutils.h \
	 pathnode.h plancat.h planmain.h planner.h prepqual.h preptlist.h \
	 prepunion.h prune.h recurplanner.h relnode.h semanopt.h setrefs.h \
	 sortresult.h tlist.h var.h xfunc.h
