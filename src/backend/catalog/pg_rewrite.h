/* ----------------------------------------------------------------
 *   FILE
 *	pg_rewrite.h
 *
 *   DESCRIPTION
 *	definition of the system "rewrite-rule" relation (pg_rewrite)
 *	along with the relation's initial contents.
 *
 *   NOTES
 *	the genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgRewriteIncluded
#define PgRewriteIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_rewrite definition.  cpp turns this into
 *	typedef struct FormData_pg_rewrite
 * ----------------
 */ 
CATALOG(pg_rewrite) {
    char16 	rulename;
    char 	ev_type;
    oid 	ev_class;
    int2 	ev_attr;
    float8	fril_lb;
    float8	fril_ub;
    bool 	is_instead;
    text	ev_qual;	/* VARLENA */
    text 	action;		/* VARLENA */
} FormData_pg_rewrite;

/* ----------------
 *	Form_pg_rewrite corresponds to a pointer to a tuple with
 *	the format of pg_rewrite relation.
 * ----------------
 */
typedef FormData_pg_rewrite *Form_pg_rewrite;

/* ----------------
 *	compiler constants for pg_rewrite
 * ----------------
 */
#define Name_pg_rewrite			"pg_rewrite"
#define Natts_pg_rewrite		9
#define Anum_pg_rewrite_rulename	1
#define Anum_pg_rewrite_ev_type 	2
#define Anum_pg_rewrite_ev_class	3
#define Anum_pg_rewrite_ev_attr  	4
#define Anum_pg_rewrite_fril_lb   	5
#define Anum_pg_rewrite_fril_ub   	6
#define Anum_pg_rewrite_is_instead      7
#define Anum_pg_rewrite_ev_qual		8
#define Anum_pg_rewrite_action	        9

#endif PgRewriteIncluded
