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

#include "postgres.h"

#include "aset.h"
#include "attnum.h"
#include "buf.h"
#include "catname.h"
#include "command.h"
#include "defrem.h"
#include "exc.h"
#include "execdefs.h"
#include "ftup.h"
#include "globals.h"
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "name.h"
#include "palloc.h"
#include "params.h"
#include "parse.h"
#include "pg_lisp.h"
#include "pinit.h"
#include "pmod.h"
#include "portal.h"
#include "prs2.h"
#include "rel.h"
#include "sdir.h"
#include "syscache.h"
#include "tim.h"
#include "tupdesc.h"
#include "xcxt.h"
#include "fmgr.h"

#include "catalog/pg_type.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_ipl.h"

#include "tcopdebug.h"
#include "execdebug.h"

/* ----------------
 *	externs
 * ----------------
 */
extern int _reset_stats_after_query_;
extern int _show_stats_after_query_;

extern void die();
extern void handle_warn();

#include "tcop/creatinh.h"

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  TcopIncluded
