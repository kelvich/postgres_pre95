/* ----------------------------------------------------------------
 *   FILE
 *	pg_am.h
 *
 *   DESCRIPTION
 *	definition of the system "am" relation (pg_am)
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
#ifndef PgAmIncluded
#define PgAmIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_am definition.  cpp turns this into
 *	typedef struct FormData_pg_am
 * ----------------
 */ 
CATALOG(pg_am) {
    char16 	amname;
    oid 	amowner;
    char	amkind;
    int2 	amstrategies;
    int2 	amsupport;
    regproc 	amgettuple;
    regproc	aminsert;
    regproc 	amdelete;
    regproc 	amgetattr;
    regproc 	amsetlock;
    regproc 	amsettid;
    regproc	amfreetuple;
    regproc 	ambeginscan;
    regproc 	amrescan;
    regproc 	amendscan;
    regproc 	ammarkpos;
    regproc 	amrestrpos;
    regproc 	amopen;
    regproc 	amclose;
    regproc 	ambuild;
    regproc 	amcreate; 
    regproc 	amdestroy;
} FormData_pg_am;

/* ----------------
 *	Form_pg_am corresponds to a pointer to a tuple with
 *	the format of pg_am relation.
 * ----------------
 */
typedef FormData_pg_am	*Form_pg_am;

/* ----------------
 *	compiler constants for pg_am
 * ----------------
 */
#define Name_pg_am			"pg_am"
#define Natts_pg_am			22
#define Anum_pg_am_amname		1
#define Anum_pg_am_amowner		2
#define Anum_pg_am_amkind		3
#define Anum_pg_am_amstrategies		4
#define Anum_pg_am_amsupport		5
#define Anum_pg_am_amgettuple		6
#define Anum_pg_am_aminsert		7
#define Anum_pg_am_amdelete		8
#define Anum_pg_am_amgetattr		9
#define Anum_pg_am_amsetlock		10
#define Anum_pg_am_amsettid		11
#define Anum_pg_am_amfreetuple		12
#define Anum_pg_am_ambeginscan		13
#define Anum_pg_am_amrescan		14
#define Anum_pg_am_amendscan		15
#define Anum_pg_am_ammarkpos		16
#define Anum_pg_am_amrestrpos		17
#define Anum_pg_am_amopen		18
#define Anum_pg_am_amclose		19
#define Anum_pg_am_ambuild		20
#define Anum_pg_am_amcreate		21
#define Anum_pg_am_amdestroy		22

/* ----------------
 *	initial contents of pg_am
 * ----------------
 */

DATA(insert OID = 400 (  oldbtree 6 "o" 5 0 btreegettuple btreeinsert btreedelete - - - - btreebeginscan btreerescan btreeendscan btreemarkpos btreerestrpos - - btreebuild - - ));
DATA(insert OID = 401 (  fbtree 6 "o" 5 0 fbtreegettuple fbtreeinsert fbtreedelete - - - - - - - - - - - fbtreebuild - - ));
DATA(insert OID = 402 (  rtree 6 "o" 5 3 rtgettuple rtinsert rtdelete - - - - rtbeginscan rtrescan rtendscan rtmarkpos rtrestrpos - - rtbuild - - ));
DATA(insert OID = 403 (  btree 6 "o" 5 1 btgettuple btinsert btdelete - - - - btbeginscan btrescan btendscan btmarkpos btrestrpos - - btbuild - - ));

/* ----------------
 *	old definition of AccessMethodTupleForm
 * ----------------
 */
#ifndef AccessMethodTupleForm_Defined
#define AccessMethodTupleForm_Defined 1

typedef struct AccessMethodTupleFormD {
	NameData	amname;
	ObjectId	amowner;
	char		amkind;		/* XXX */
/*	typedef uint16	StrategyNumber; */
	uint16		amstrategies;
	uint16		amsupport;
	RegProcedure	amgettuple;
	RegProcedure	aminsert;
	RegProcedure	amdelete;
	RegProcedure	amgetattr;
	RegProcedure	amsetlock;
	RegProcedure	amsettid;
	RegProcedure	amfreetuple;
	RegProcedure	ambeginscan;
	RegProcedure	amrescan;
	RegProcedure	amendscan;
	RegProcedure	ammarkpos;
	RegProcedure	amrestrpos;
	RegProcedure	amopen;
	RegProcedure	amclose;
	RegProcedure	ambuild;
	RegProcedure	amcreate;
	RegProcedure	amdestroy;
} AccessMethodTupleFormD;

typedef AccessMethodTupleFormD		*AccessMethodTupleForm;

#endif AccessMethodTupleForm_Defined

/* ----------------
 *	old definition of struct am
 * ----------------
 */
#ifndef struct_am_Defined
#define struct_am_Defined 1

struct	am {
	char	amname[16];
	OID	amowner;
	char	amkind;
	uint16  amstrategies;
	uint16  amsupport;
	REGPROC	amgettuple;
	REGPROC	aminsert;
	REGPROC	amdelete;
	REGPROC	amgetattr;
	REGPROC	amsetlock;
	REGPROC	amsettid;
	REGPROC	amfreetuple;
	REGPROC	ambeginscan;
	REGPROC	amrescan;
	REGPROC	amendscan;
	REGPROC	ammarkpos;
	REGPROC	amrestrpos;
	REGPROC	amopen;
	REGPROC	amclose;
	REGPROC	ambuild;
	REGPROC	amcreate;
	REGPROC	amdestroy;
};

#endif struct_am_Defined

/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define AccessMethodNameAttributeNumber	\
    Anum_pg_am_amname
    
#endif PgAmIncluded
