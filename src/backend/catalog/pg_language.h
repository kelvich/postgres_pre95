/* ----------------------------------------------------------------
 *   FILE
 *	pg_language.h
 *
 *   DESCRIPTION
 *	definition of the system "language" relation (pg_language)
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
#ifndef PgLanguageIncluded
#define PgLanguageIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_language definition.  cpp turns this into
 *	typedef struct FormData_pg_language
 * ----------------
 */ 
CATALOG(pg_language) {
    char16 	lanname;
    text 	lancompiler;	/* VARIABLE LENGTH FIELD */
} FormData_pg_language;

/* ----------------
 *	Form_pg_language corresponds to a pointer to a tuple with
 *	the format of pg_language relation.
 * ----------------
 */
typedef FormData_pg_language	*Form_pg_language;

/* ----------------
 *	compiler constants for pg_language
 * ----------------
 */
#define Name_pg_language		"pg_language"
#define Natts_pg_language		2
#define Anum_pg_language_lanname	1
#define Anum_pg_language_lancompiler	2

/* ----------------
 *	initial contents of pg_language
 * ----------------
 */

DATA(insert OID = 11 ( internal "n/a" ));
#define INTERNALlanguageId 11
DATA(insert OID = 12 ( lisp "/usr/ucb/liszt" ));
DATA(insert OID = 13 ( "C" "/bin/cc" ));
#define ClanguageId 13
DATA(insert OID = 14 ( "postquel" "postgres"));
#define POSTQUELlanguageId 14

/* ----------------
 *	old definition of struct language
 * ----------------
 */
#ifndef struct_language_Defined
#define struct_language_Defined 1

struct	language {
	char	lanname[16];
	struct	varlena	lancompiler;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_language_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define	LanguageNameAttributeNumber \
    Anum_pg_language_lanname
    
#endif PgLanguageIncluded







