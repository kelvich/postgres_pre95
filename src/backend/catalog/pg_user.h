/* ----------------------------------------------------------------
 *   FILE
 *	pg_user.h
 *
 *   DESCRIPTION
 *	definition of the system "user" relation (pg_user)
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
#ifndef PgUserIncluded
#define PgUserIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_user definition.  cpp turns this into
 *	typedef struct FormData_pg_user
 * ----------------
 */ 
#define USER_NAMESIZE 16

CATALOG(pg_user) BOOTSTRAP {
    char16 	usename;
    int2 	usesysid;
    bool 	usecreatedb;
    bool 	usetrace;
    bool 	usesuper;
    bool 	usecatupd;  
} FormData_pg_user;

/* ----------------
 *	Form_pg_user corresponds to a pointer to a tuple with
 *	the format of pg_user relation.
 * ----------------
 */
typedef FormData_pg_user	*Form_pg_user;

/* ----------------
 *	compiler constants for pg_user
 * ----------------
 */
#define Name_pg_user			"pg_user"
#define Natts_pg_user			6
#define Anum_pg_user_usename		1
#define Anum_pg_user_usesysid		2
#define Anum_pg_user_usecreatedb	3
#define Anum_pg_user_usetrace		4
#define Anum_pg_user_usesuper		5
#define Anum_pg_user_usecatupd		6

/* ----------------
 *	initial contents of pg_user
 * ----------------
 */
DATA(insert OID =    PGUID ( postgres PGUID t t t t ));

BKI_BEGIN
#ifdef ALLOW_PG_GROUP
BKI_END

DATA(insert OID =  799 ( mike 799 t t t t ));
DATA(insert OID = 1511 ( sp 1511 t t t t ));
DATA(insert OID = 2359 ( cimarron 2359 t t t t ));
DATA(insert OID = 2469 ( hong 2469 t t t t ));
DATA(insert OID = 1806 ( mao 1806 t t t t ));
DATA(insert OID = 2697 ( margo 2697 t t t t ));
DATA(insert OID = 1517 ( sullivan 1517 t t t t ));
DATA(insert OID = 3491 ( kemnitz 3491 t t t t ));
DATA(insert OID = 3898 ( choi 3898 t t t t ));
DATA(insert OID = 3918 ( glass 3918 t t t t ));
DATA(insert OID = 4613 ( mer 4613 t t t t ));
DATA(insert OID = 4616 ( dpassage 4616 t t t t ));
DATA(insert OID = 5059 ( caetta 5059 t t t t ));
DATA(insert OID = 5209 ( joey 5209 t t t t ));
DATA(insert OID = 5568 ( clarsen 5568 t t t t ));
DATA(insert OID = 5443 ( jolly 5443 t t t t ));
DATA(insert OID = 6559 ( sunita 6559 t t t t ));
DATA(insert OID = 3029 ( paxson 3029 t t t t));

BKI_BEGIN
#endif ALLOW_PG_GROUP
BKI_END

/* ----------------
 *	old definition of struct user
 * ----------------
 */
#ifndef struct_user_Defined
#define struct_user_Defined 1

struct	user {
	char	usename[16];
	int16	usesysid;	/* XXX uint16 better? */
	Boolean	usecreatedb;
	Boolean	usetrace;
	Boolean	usesuper;
	Boolean	usecatupd;
};

#endif struct_user_Defined

    
#endif PgUserIncluded
