/* ----------------------------------------------------------------
 *   FILE
 *	pg_demon.h
 *
 *   DESCRIPTION
 *	definition of the system "demon" relation (pg_demon)
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
#ifndef PgDemonIncluded
#define PgDemonIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_demon definition.  cpp turns this into
 *	typedef struct FormData_pg_demon
 * ----------------
 */ 
CATALOG(pg_demon) BOOTSTRAP {
    oid 	demserid;
    char16 	demname;
    oid 	demowner;
    regproc 	demcode;
} FormData_pg_demon;

/* ----------------
 *	Form_pg_demon corresponds to a pointer to a tuple with
 *	the format of pg_demon relation.
 * ----------------
 */
typedef FormData_pg_demon	*Form_pg_demon;

/* ----------------
 *	compiler constants for pg_demon
 * ----------------
 */
#define Name_pg_demon			"pg_demon"
#define Natts_pg_demon			4
#define Anum_pg_demon_demserid		1
#define Anum_pg_demon_demname		2
#define Anum_pg_demon_demowner		3
#define Anum_pg_demon_demcode		4

/* ----------------
 *	old definition of struct demon
 * ----------------
 */
#ifndef struct_demon_Defined
#define struct_demon_Defined 1

struct	demon {
	OID	demserid;
	char	demname[16];
	OID	demowner;
	REGPROC	demcode;
};

#endif struct_demon_Defined


#endif PgDemonIncluded
