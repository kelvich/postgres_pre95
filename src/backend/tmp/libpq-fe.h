/* ----------------------------------------------------------------
 *   FILE
 *	libpq-fe.h
 *
 *   DESCRIPTION
 *	This file contains definitions for structures and
 *	externs for functions used by frontend applications.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef LibpqFeIncluded 	/* include this file only once. */
#define LibpqFeIncluded 	1

/* ----------------
 *	include stuff common to fe and be
 * ----------------
 */
#include "tmp/libpq.h"

typedef void *TUPLE;
/* ----------------
 *	declarations for frontend libpq support routines
 * ----------------
 */
#undef palloc
#undef pfree

extern char *GetAttributeByName ARGS ((TUPLE tuple, char *attname, char *isnull ));
extern char *GetAttributeByNum ARGS ((TUPLE tuple, int attnum, char *isnull ));
extern int dump_data ARGS((char *portal_name, int rule_p));
extern void read_remark ARGS((char id[]));
extern char *PQdb ARGS(());
extern void PQsetdb ARGS((char *dbname));
extern void PQreset ARGS(());
extern void PQfinish ARGS(());
extern int PQFlushI ARGS((int i_count));
extern char *PQfn ARGS((int fnid, int *result_buf, int result_len, int *actual_result_len, int result_type, PQArgBlock *args, int nargs));
extern char *PQexec ARGS((char *query));
extern Pointer palloc ARGS((Size size));
extern void pfree ARGS((Pointer pointer));
extern void elog ARGS((...));
extern void AssertionFailed ARGS((const String assertionName, const String fileName, const int lineNumber));

#endif LibpqFeIncluded

