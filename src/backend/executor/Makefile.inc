#
# $Header$
#
.PATH: ${.CURDIR}/executor

SRCS+= aggregate.c ex_ami.c ex_debug.c ex_flatten.c ex_junk.c ex_main.c \
	ex_procnode.c ex_qual.c ex_scan.c ex_shmem.c ex_tuples.c ex_utils.c \
	ex_xdebug.c functions.c n_append.c n_hash.c n_hashjoin.c \
	n_indexscan.c n_material.c n_mergejoin.c n_nestloop.c n_result.c \
	n_scantemps.c n_seqscan.c n_sort.c n_unique.c pre_nestdots.c

HEADERS+= aggregate.h execdebug.h execdefs.h execdesc.h execmisc.h \
	execshmem.h executor.h externs.h flatten.h hashjoin.h \
	paralleldebug.h recursion.h recursion_a.h \
	tuptable.h x_append.h x_debug.h x_endnode.h x_eutils.h \
	x_execam.h x_execfmgr.h x_execinit.h x_execmain.h x_execmmgr.h \
	x_execstats.h x_exist.h x_hash.h x_hashjoin.h x_indexscan.h \
	x_initnode.h x_junk.h x_material.h x_mergejoin.h x_nestloop.h \
	x_parallel.h x_procnode.h x_qual.h x_result.h x_scan.h \
	x_seqscan.h x_shmem.h x_sort.h x_tuples.h x_unique.h x_xdebug.h
