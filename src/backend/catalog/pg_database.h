/* ----------------------------------------------------------------
 *   FILE
 *	pg_database.h
 *
 *   DESCRIPTION
 *	definition of the system "database" relation (pg_database)
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
#ifndef PgDatabaseIncluded
#define PgDatabaseIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_database definition.  cpp turns this into
 *	typedef struct FormData_pg_database
 * ----------------
 */ 
CATALOG(pg_database) BOOTSTRAP {
    char16 	datname;
    oid 	datdba;
    text 	datpath;	/* VARIABLE LENGTH FIELD */
} FormData_pg_database;

/* ----------------
 *	Form_pg_database corresponds to a pointer to a tuple with
 *	the format of pg_database relation.
 * ----------------
 */
typedef FormData_pg_database	*Form_pg_database;

/* ----------------
 *	compiler constants for pg_database
 * ----------------
 */
#define Name_pg_database		"pg_database"
#define Natts_pg_database		3
#define Anum_pg_database_datname	1
#define Anum_pg_database_datdba		2
#define Anum_pg_database_datpath	3

/* ----------------
 *	old definition of struct database
 * ----------------
 */
#ifndef struct_database_Defined
#define struct_database_Defined 1
    
struct	database {
	char	datname[16];
	OID	datdba;
	struct	varlena	datpath;
}; /* VARIABLE LENGTH STRUCTURE */

#endif struct_database_Defined


#endif PgDatabaseIncluded
