/* ----------------------------------------------------------------
 *   FILE
 *	pg_amop.h
 *
 *   DESCRIPTION
 *	definition of the system "amop" relation (pg_amop)
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
#ifndef PgAmopIncluded
#define PgAmopIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------
 *	pg_amop definition.  cpp turns this into
 *	typedef struct FormData_pg_amop
 * ----------------
 */ 
CATALOG(pg_amop) {
    oid 	amopid;
    oid 	amopclaid;
    oid 	amopopr;
    int2 	amopstrategy;
    regproc 	amopselect;
    regproc 	amopnpages;  
} FormData_pg_amop;

/* ----------------
 *	Form_pg_amop corresponds to a pointer to a tuple with
 *	the format of pg_amop relation.
 * ----------------
 */
typedef FormData_pg_amop	*Form_pg_amop;

/* ----------------
 *	compiler constants for pg_amop
 * ----------------
 */
#define Name_pg_amop			"pg_amop"
#define Natts_pg_amop			6
#define Anum_pg_amop_amopid   		1
#define Anum_pg_amop_amopclaid 		2
#define Anum_pg_amop_amopopr		3
#define Anum_pg_amop_amopstrategy	4
#define Anum_pg_amop_amopselect		5
#define Anum_pg_amop_amopnpages		6

/* ----------------
 *	initial contents of pg_amop
 * ----------------
 */

DATA(insert OID = 0 (  400 421  95 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 421 522 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 421  94 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 421 524 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 421 520 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 424 534 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 424 540 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 424 532 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 424 542 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 424 536 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 425 535 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 425 541 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 425 533 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 425 543 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 425 537 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 426  97 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 426 523 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 426  96 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 426 525 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 426 521 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 427 609 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 427 611 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 427 607 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 427 612 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 427 610 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 428 622 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 428 624 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 428 620 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 428 625 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 428 623 5 btreesel btreenpage ));
DATA(insert OID = 0 (  400 422 504 1 btreesel btreenpage ));
DATA(insert OID = 0 (  400 422 505 2 btreesel btreenpage ));
DATA(insert OID = 0 (  400 422 503 3 btreesel btreenpage ));
DATA(insert OID = 0 (  400 422 501 4 btreesel btreenpage ));
DATA(insert OID = 0 (  400 422 502 5 btreesel btreenpage ));
DATA(insert OID = 0 (  401 421  95 1 int4eq int4eq ));
DATA(insert OID = 0 (  401 421  94 3 int4eq int4eq ));
DATA(insert OID = 0 (  402 422 493 1 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 494 2 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 500 3 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 495 4 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 496 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 499 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 498 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 497 5 rtsel rtnpage ));

/* ----------------
 *	old definition of AccessMethodOperatorTupleForm
 * ----------------
 */
#ifndef AccessMethodOperatorTupleForm_Defined
#define AccessMethodOperatorTupleForm_Defined 1

typedef struct AccessMethodOperatorTupleFormD {
	ObjectId	amopamid;
	ObjectId	amopclaid;
	ObjectId	amopoprid;
	StrategyNumber	amopstrategy;
	RegProcedure	amopselect;
	RegProcedure	amopnpages;
} AccessMethodOperatorTupleFormD;

typedef AccessMethodOperatorTupleFormD	*AccessMethodOperatorTupleForm;

#endif AccessMethodOperatorTupleForm_Defined

/* ----------------
 *	old definition of struct amop
 * ----------------
 */
#ifndef struct_amop_Defined
#define struct_amop_Defined 1

struct	amop {
	OID	amopamid;
	OID	amopclaid;
	OID	amopoprid;
	uint16	amopstrategy;
	REGPROC	amopselect;
	REGPROC	amopnpages;
};

#endif struct_amop_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define AccessMethodOperatorAccessMethodIdAttributeNumber \
    Anum_pg_amop_amopid
#define AccessMethodOperatorOperatorClassIdAttributeNumber \
    Anum_pg_amop_amopclaid
#define AccessMethodOperatorOperatorIdAttributeNumber \
    Anum_pg_amop_amopopr
#define AccessMethodOperatorStrategyAttributeNumber \
    Anum_pg_amop_amopstrategy

#endif PgAmopIncluded
