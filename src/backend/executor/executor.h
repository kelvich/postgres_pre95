/* ----------------------------------------------------------------
 *      FILE
 *     	executor.h
 *     
 *      DESCRIPTION
 *     	support for the POSTGRES executor module
 *
 *	$Header$"
 * ----------------------------------------------------------------
 */

#ifndef ExecutorIncluded
#define ExecutorIncluded

/* ----------------------------------------------------------------
 *     #includes
 * ----------------------------------------------------------------
 */
#include <stdio.h>
#include <strings.h>

#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"

/* ----------------
 * executor debugging definitions are kept in a separate file
 * so people can customize what debugging they want to see and not
 * have this information clobbered every time a new version of
 * executor.h is checked in -cim 10/26/89
 * ----------------
 */
#include "executor/execdebug.h"

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/istrat.h"
#include "access/itup.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "executor/execdefs.h"
#include "executor/recursion.h"
#include "executor/recursion_a.h"
#include "executor/tuptable.h"
#include "parser/parse.h"
#include "storage/buf.h"
#include "tmp/miscadmin.h"
#include "tmp/simplelists.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/pg_aggregate.h"

#include "access/printtup.h"
#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.h"
#include "parser/parsetree.h"

#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "tcop/dest.h"
#include "storage/smgr.h"

/* ----------------
 * .h files made from running pgdecs over generated .c files in obj/lib/C
 * ----------------
 */
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.a.h"

#include "executor/execshmem.h"
#include "executor/execmisc.h"
/* 
 *	public executor functions
 *
 *	XXX reorganize me.
 */
#include "executor/externs.h"

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecutorIncluded
