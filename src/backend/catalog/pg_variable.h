/* ----------------------------------------------------------------
 *   FILE
 *	pg_variable.h
 *
 *   DESCRIPTION
 *	the system variable relation "pg_variable" is not
 *	presently a "heap" relation.  It is automatically created
 *	by the transam/ code and the CATALOG() information.
 *	below is bogus.  It is likely to all go away as soon
 *	as I can obsolete all code depending on "struct variable".
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
#include "postgres.h"

/* ----------------
 *	pg_variable definition.  cpp turns this into
 *	typedef struct FormData_pg_variable
 *
 *	WARNING: this is bogus. this is only here so that the
 *		 .bki script is generated correctly -cim 6/18/90
 * ----------------
 */ 
CATALOG(pg_variable) BOOTSTRAP {
    char16 	varname;
    bytea 	varvalue;
} FormData_pg_variable;

/* ----------------
 *	Form_pg_variable corresponds to a pointer to a tuple with
 *	the format of pg_variable relation.
 * ----------------
 */
typedef FormData_pg_variable	*Form_pg_variable;

/* ----------------
 *	compiler constants for pg_variable
 *
 *	WARNING: this is bogus. see above comments -cim 6/18/90
 * ----------------
 */
#define Name_pg_variable		"pg_variable"
#define Natts_pg_variable		2
#define Anum_pg_variable_varname	1
#define Anum_pg_variable_varvalue	2

/* ----------------
 *	old definition of struct variables
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
