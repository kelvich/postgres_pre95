/* ----------------------------------------------------------------
 *   FILE
 *	pg_defaults.h
 *
 *   DESCRIPTION
 *	definition of the system "defaults" relation (pg_defaults)
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
#ifndef PgDefaultsIncluded
#define PgDefaultsIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_defaults definition.  cpp turns this into
 *	typedef struct FormData_pg_defaults
 * ----------------
 */ 
CATALOG(pg_defaults) BOOTSTRAP {
    char16 	defname;
    char16 	defvalue;
} FormData_pg_defaults;

/* ----------------
 *	Form_pg_defaults corresponds to a pointer to a tuple with
 *	the format of pg_defaults relation.
 * ----------------
 */
typedef FormData_pg_defaults	*Form_pg_defaults;

/* ----------------
 *	compiler constants for pg_defaults
 * ----------------
 */
#define Name_pg_defaults		"pg_defaults"
#define Natts_pg_defaults		2
#define Anum_pg_defaults_defname	1
#define Anum_pg_defaults_defvalue	2

/* ----------------
 *	old definition of struct defaults
 * ----------------
 */
#ifndef struct_defaults_Defined
#define struct_defaults_Defined 1

struct	defaults {
	char	defname[16];
	char	defvalue[16];
};

#endif struct_defaults_Defined


#endif PgDefaultsIncluded
