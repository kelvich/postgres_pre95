/* ----------------------------------------------------------------
 *   FILE
 *	pg_version.h
 *
 *   DESCRIPTION
 *	definition of the system "version" relation (pg_version)
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
#ifndef PgVersionIncluded
#define PgVersionIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_version definition.  cpp turns this into
 *	typedef struct FormData_pg_version
 * ----------------
 */ 
CATALOG(pg_version) {
    oid 	verrelid;
    oid 	verbaseid;
    dt 		vertime;
} FormData_pg_version;

/* ----------------
 *	Form_pg_version corresponds to a pointer to a tuple with
 *	the format of pg_version relation.
 * ----------------
 */
typedef FormData_pg_version	*Form_pg_version;

/* ----------------
 *	compiler constants for pg_version
 * ----------------
 */
#define Name_pg_version			"pg_version"
#define Natts_pg_version		3
#define Anum_pg_version_verrelid	1
#define Anum_pg_version_verbaseid	2
#define Anum_pg_version_vertime		3

/* ----------------
 *	old definition of VersionTupleForm
 * ----------------
 */
#ifndef VersionTupleForm_Defined
#define VersionTupleForm_Defined 1

typedef struct VersionTupleFormD {
	ObjectId	verrelid;
	ObjectId	verbaseid;
	ABSTIME		vertime;
} VersionTupleFormD;

typedef VersionTupleFormD	*VersionTupleForm;

#endif VersionTupleForm_Defined

/* ----------------
 *	old definition of struct version
 * ----------------
 */
#ifndef struct_version_Defined
#define struct_version_Defined 1

struct	version {
	OID	verrelid;
	OID	verbaseid;
	ABSTIME	vertime;
};
#endif struct_version_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */    
#define	VersionRelationIdAttributeNumber \
    Anum_pg_version_verrelid
#define	VersionBaseRelationIdAttributeNumber \
    Anum_pg_version_verbaseid

#endif PgVersionIncluded
