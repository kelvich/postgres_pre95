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
DATA(insert OID = 0 ( postgres PGUID t t t t ));

BKI_BEGIN
#ifdef ALLOW_PG_GROUP
BKI_END

DATA(insert OID = 0 ( mike 799 t t t t ));
DATA(insert OID = 0 ( mao 1806 t t t t ));
DATA(insert OID = 0 ( hellers 1089 t t t t ));
DATA(insert OID = 0 ( joey 5209 t t t t ));
DATA(insert OID = 0 ( jolly 5443 t t t t ));
DATA(insert OID = 0 ( sunita 6559 t t t t ));
DATA(insert OID = 0 ( paxson 3029 t t t t ));
DATA(insert OID = 0 ( marc 2435 t t t t ));
DATA(insert OID = 0 ( jiangwu 6124 t t t t ));
DATA(insert OID = 0 ( aoki 2360 t t t t ));
DATA(insert OID = 0 ( avi 31080 t t t t ));
DATA(insert OID = 0 ( kristin 1123 t t t t ));
DATA(insert OID = 0 ( andrew 5229 t t t t ));
DATA(insert OID = 0 ( nobuko 5493 t t t t ));
DATA(insert OID = 0 ( hartzell 6676 t t t t ));
DATA(insert OID = 0 ( devine 6724 t t t t ));
DATA(insert OID = 0 ( boris 6396 t t t t ));
DATA(insert OID = 0 ( sklower 354 t t t t ));
DATA(insert OID = 0 ( marcel 31113 t t t t ));
DATA(insert OID = 0 ( ginger 3692 t t t t ));

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
