
/* ----------------------------------------------------------------
 *   FILE
 *	pg_prs2stub.h
 *
 *   DESCRIPTION
 *	"Relation-level" rule stubs are stored in this relation.
 *
 *   NOTES
 *	the genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgPrs2stubIncluded
#define PgPrs2stubIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_prs2stub definition.  cpp turns this into
 *	typedef struct FormData_pg_prs2stub
 * ----------------
 */ 
CATALOG(pg_prs2stub) {
    oid 	prs2relid;
    stub	prs2stub;	/* VARIABLE LENGTH FIELD */
} FormData_pg_prs2stub;

/* ----------------
 *	Form_pg_prs2stub corresponds to a pointer to a tuple with
 *	the format of pg_prs2stub relation.
 * ----------------
 */
typedef FormData_pg_prs2stub	*Form_pg_prs2stub;

/* ----------------
 *	compiler constants for pg_prs2stub
 * ----------------
 */
#define Name_pg_prs2stub		"pg_prs2stub"
#define Natts_pg_prs2stub		2
#define Anum_pg_prs2stub_prs2relid	1
#define Anum_pg_prs2stub_prs2stub	2

    
#endif PgPrs2stubIncluded
