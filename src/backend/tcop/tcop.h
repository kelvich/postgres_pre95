/* ----------------------------------------------------------------
 *   FILE
 *	tcop.h
 *	
 *   DESCRIPTION
 *	header information for the traffic cop module
 *
 *   NOTES
 *	We should reorginize the .h files better.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef TcopIncluded
#define TcopIncluded

/* ----------------------------------------------------------------
 *     #includes
 * ----------------------------------------------------------------
 */
#include <signal.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <strings.h>
#include <ctype.h>

#include "tmp/postgres.h"

#include "access/attnum.h"
#include "access/ftup.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/sdir.h"
#include "access/tupdesc.h"
#include "access/xcxt.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "commands/command.h"
#include "commands/defrem.h"
#include "executor/execdefs.h"
#include "nodes/pg_lisp.h"
#include "parser/parse.h"
#include "rules/params.h"
#include "rules/prs2.h"
#include "storage/buf.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "utils/exc.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/rel.h"

#include "catalog/pg_type.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_ipl.h"

#include "tcop/tcopdebug.h"
#include "tcop/tcopprot.h"
#include "executor/execdebug.h"

#include "tcop/creatinh.h"

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  TcopIncluded
