/* ----------------------------------------------------------------
 *   FILE
 *	pg_magic.h
 *
 *   DESCRIPTION
 *	definition of the system "magic" relation (pg_magic)
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
#ifndef PgMagicIncluded
#define PgMagicIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_magic definition.  cpp turns this into
 *	typedef struct FormData_pg_magic
 * ----------------
 */ 
CATALOG(pg_magic) BOOTSTRAP {
    char16 	magname;
    char16 	magvalue;
} FormData_pg_magic;

/* ----------------
 *	Form_pg_magic corresponds to a pointer to a tuple with
 *	the format of pg_magic relation.
 * ----------------
 */
typedef FormData_pg_magic	*Form_pg_magic;

/* ----------------
 *	compiler constants for pg_magic
 * ----------------
 */
#define Name_pg_magic			"pg_magic"
#define Natts_pg_magic			2
#define Anum_pg_magic_magname		1
#define Anum_pg_magic_magvalue		2

/* ----------------
 *	old definition of struct magic
 * ----------------
 */
#ifndef struct_magic_Defined
#define struct_magic_Defined 1

struct	magic {
	char	magname[16];
	char	magvalue[16];
};
#endif struct_magic_Defined


#endif PgMagicIncluded
