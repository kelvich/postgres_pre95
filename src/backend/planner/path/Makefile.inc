.PATH: ${.CURDIR}/planner/path

SRCS+= allpaths.c clausesel.c costsize.c hashutils.c indxpath.c \
	joinpath.c joinrels.c joinutils.c mergeutils.c orindxpath.c \
	predmig.c prune.c xfunc.c
