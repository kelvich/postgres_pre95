/* ----------------------------------------------------------------
 *   FILE
 *	pg_variable.h
 *
 *   DESCRIPTION
 *	the system variable relation "pg_variable" is not a "heap" relation.
 *	it is automatically created by the transam/ code and the
 *	information here is all bogus and is just here to make the
 *	relcache code happy.
 *
 *   NOTES
 *	The structures and macros used by the transam/ code
 *	to access pg_variable should someday go here -cim 6/18/90
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgVariableIncluded
#define PgVariableIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

CATALOG(pg_variable) BOOTSTRAP {
    oid  	varfoo;
} FormData_pg_variable;

typedef FormData_pg_variable	*Form_pg_variable;

#define Name_pg_variable	"pg_variable"
#define Natts_pg_variable	1
#define Anum_pg_variable_varfoo	1

/* ----------------
 *	old crap
 * ----------------
 */
#ifndef struct_variables_Defined
#define struct_variables_Defined 1

struct	variables {
	char	varname[16];
	struct	varlena	varvalue;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_variables_Defined

#endif PgVariableIncluded
