/* ----------------------------------------------------------------
 *   FILE
 *	pg_inheritproc.h
 *
 *   DESCRIPTION
 *	definition of the system "inheritproc" relation (pg_inheritproc)
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
#ifndef PgInheritprocIncluded
#define PgInheritprocIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_inheritproc definition.  cpp turns this into
 *	typedef struct FormData_pg_inheritproc
 * ----------------
 */ 
CATALOG(pg_inheritproc) {
     char16 	inhproname;
     oid 	inhargrel;
     oid 	inhdefrel;
     oid 	inhproc;
} FormData_pg_inheritproc;

/* ----------------
 *	Form_pg_inheritproc corresponds to a pointer to a tuple with
 *	the format of pg_inheritproc relation.
 * ----------------
 */
typedef FormData_pg_inheritproc	*Form_pg_inheritproc;

/* ----------------
 *	compiler constants for pg_inheritproc
 * ----------------
 */
#define Name_pg_inheritproc		"pg_inheritproc"
#define Natts_pg_inheritproc		4
#define Anum_pg_inheritproc_inhproname	1
#define Anum_pg_inheritproc_inhargrel	2
#define Anum_pg_inheritproc_inhdefrel	3
#define Anum_pg_inheritproc_inhproc	4

/* ----------------
 *	old definition of struct inheritproc
 * ----------------
 */
#ifndef struct_inheritproc_Defined
#define struct_inheritproc_Defined 1

struct	inheritproc {
	char	inhprocname[16];
	OID	inhargrel;
	OID	inhdefrel;
	OID	inhproc;
};
#endif struct_inheritproc_Defined

#endif PgInheritprocIncluded
