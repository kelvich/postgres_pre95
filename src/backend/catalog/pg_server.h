/* ----------------------------------------------------------------
 *   FILE
 *	pg_server.h
 *
 *   DESCRIPTION
 *	definition of the system "server" relation (pg_server)
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
#ifndef PgServerIncluded
#define PgServerIncluded 1	/* include this only once */

/* ----------------
 *	catmacros.h defines the CATALOG(), BOOTSTRAP and
 *	DATA() sugar words so this file can be read by both
 *	genbki.sh and the C compiler.
 * ----------------
 */
#include "catalog/catmacros.h"

/* ----------------
 *	pg_server definition.  cpp turns this into
 *	typedef struct FormData_pg_server
 * ----------------
 */ 
CATALOG(pg_server) BOOTSTRAP {
    char16 	sername;
    int2 	serpid;
    int2 	serport;
} FormData_pg_server;

/* ----------------
 *	Form_pg_server corresponds to a pointer to a tuple with
 *	the format of pg_server relation.
 * ----------------
 */
typedef FormData_pg_server	*Form_pg_server;

/* ----------------
 *	compiler constants for pg_server
 * ----------------
 */
#define Name_pg_server			"pg_server"
#define Natts_pg_server			3
#define Anum_pg_server_sername		1
#define Anum_pg_server_serpid		2
#define Anum_pg_server_serport		3

/* ----------------
 *	old definition of struct server
 * ----------------
 */
#ifndef struct_server_Defined
#define struct_server_Defined 1

struct	server {	
	char	sername[16];
	int16	serpid;		/* XXX uint16 better? */
	int16	serport;	/* XXX uint16 better? */
};
#endif struct_server_Defined

#endif PgServerIncluded
