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
#include "access/istrat.h"

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

/*
 *  rtree box_ops
 */

DATA(insert OID = 0 (  402 422 493 1 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 494 2 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 500 3 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 495 4 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 496 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 499 6 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 498 7 rtsel rtnpage ));
DATA(insert OID = 0 (  402 422 497 8 rtsel rtnpage ));

/*
 *  rtree bigbox_ops
 */

DATA(insert OID = 0 (  402 433 493 1 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 494 2 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 500 3 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 495 4 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 496 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 499 6 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 498 7 rtsel rtnpage ));
DATA(insert OID = 0 (  402 433 497 8 rtsel rtnpage ));

/*
 *  rtree poly_ops (supports polygons)
 */

DATA(insert OID = 0 (  402 434 485 1 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 486 2 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 487 3 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 488 4 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 489 5 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 490 6 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 491 7 rtsel rtnpage ));
DATA(insert OID = 0 (  402 434 492 8 rtsel rtnpage ));

/*
 *  nbtree int2_ops
 */

DATA(insert OID = 0 (  403 421  95 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 421 522 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 421  94 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 421 524 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 421 520 5 btreesel btreenpage ));

/*
 *  nbtree float8_ops
 */

DATA(insert OID = 0 (  403 423 672 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 423 673 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 423 670 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 423 675 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 423 674 5 btreesel btreenpage ));

/*
 *  nbtree int24_ops
 */

DATA(insert OID = 0 (  403 424 534 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 424 540 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 424 532 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 424 542 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 424 536 5 btreesel btreenpage ));

/*
 *  nbtree int42_ops
 */

DATA(insert OID = 0 (  403 425 535 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 425 541 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 425 533 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 425 543 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 425 537 5 btreesel btreenpage ));

/*
 *  nbtree int4_ops
 */

DATA(insert OID = 0 (  403 426  97 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 426 523 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 426  96 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 426 525 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 426 521 5 btreesel btreenpage ));

/*
 *  nbtree oid_ops
 */

DATA(insert OID = 0 (  403 427 609 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 427 611 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 427 607 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 427 612 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 427 610 5 btreesel btreenpage ));

/*
 *  nbtree float4_ops
 */

DATA(insert OID = 0 (  403 428 622 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 428 624 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 428 620 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 428 625 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 428 623 5 btreesel btreenpage ));

/*
 *  nbtree char_ops
 */

DATA(insert OID = 0 (  403 429 631 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 429 632 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 429 92 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 429 634 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 429 633 5 btreesel btreenpage ));

/*
 *  nbtree char16_ops
 */

DATA(insert OID = 0 (  403 430 660 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 430 661 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 430 93 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 430 663 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 430 662 5 btreesel btreenpage ));

/*
 *  nbtree text_ops
 */

DATA(insert OID = 0 (  403 431 664 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 431 665 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 431 98 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 431 667 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 431 666 5 btreesel btreenpage ));

/*
 *  nbtree abstime_ops
 */

DATA(insert OID = 0 (  403 432 562 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 432 564 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 432 560 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 432 565 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 432 563 5 btreesel btreenpage ));

/*
 *  nbtree oidint4_ops
 */

DATA(insert OID = 0 (  403 435 930 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 435 931 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 435 932 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 435 933 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 435 934 5 btreesel btreenpage ));

/*
 *  nbtree oidint2_ops
 */

DATA(insert OID = 0 (  403 437 830 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 437 831 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 437 832 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 437 833 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 437 834 5 btreesel btreenpage ));

/*
 *  nbtree oidchar16_ops
 */

DATA(insert OID = 0 (  403 436 676 1 btreesel btreenpage ));
DATA(insert OID = 0 (  403 436 677 2 btreesel btreenpage ));
DATA(insert OID = 0 (  403 436 678 3 btreesel btreenpage ));
DATA(insert OID = 0 (  403 436 679 4 btreesel btreenpage ));
DATA(insert OID = 0 (  403 436 680 5 btreesel btreenpage ));


BKI_BEGIN
#ifdef NOBTREE
BKI_END
/*
 *  nobtree int2_ops
 */

DATA(insert OID = 0 (  404 421  95 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 421 522 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 421  94 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 421 524 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 421 520 5 btreesel btreenpage ));

/*
 *  nobtree float8_ops
 */

DATA(insert OID = 0 (  404 423 672 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 423 673 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 423 670 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 423 675 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 423 674 5 btreesel btreenpage ));

/*
 *  nobtree int24_ops
 */

DATA(insert OID = 0 (  404 424 534 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 424 540 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 424 532 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 424 542 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 424 536 5 btreesel btreenpage ));

/*
 *  nobtree int42_ops
 */

DATA(insert OID = 0 (  404 425 535 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 425 541 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 425 533 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 425 543 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 425 537 5 btreesel btreenpage ));

/*
 *  nobtree int4_ops
 */

DATA(insert OID = 0 (  404 426  97 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 426 523 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 426  96 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 426 525 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 426 521 5 btreesel btreenpage ));

/*
 *  nobtree oid_ops
 */

DATA(insert OID = 0 (  404 427 609 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 427 611 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 427 607 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 427 612 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 427 610 5 btreesel btreenpage ));

/*
 *  nobtree float4_ops
 */

DATA(insert OID = 0 (  404 428 622 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 428 624 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 428 620 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 428 625 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 428 623 5 btreesel btreenpage ));

/*
 *  nobtree char_ops
 */

DATA(insert OID = 0 (  404 429 631 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 429 632 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 429 92 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 429 634 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 429 633 5 btreesel btreenpage ));

/*
 *  nobtree char16_ops
 */

DATA(insert OID = 0 (  404 430 660 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 430 661 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 430 93 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 430 663 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 430 662 5 btreesel btreenpage ));

/*
 *  nobtree text_ops
 */

DATA(insert OID = 0 (  404 431 664 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 431 665 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 431 98 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 431 667 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 431 666 5 btreesel btreenpage ));

/*
 *  nobtree abstime_ops
 */

DATA(insert OID = 0 (  404 432 562 1 btreesel btreenpage ));
DATA(insert OID = 0 (  404 432 564 2 btreesel btreenpage ));
DATA(insert OID = 0 (  404 432 560 3 btreesel btreenpage ));
DATA(insert OID = 0 (  404 432 565 4 btreesel btreenpage ));
DATA(insert OID = 0 (  404 432 563 5 btreesel btreenpage ));

BKI_BEGIN
#endif /* NOBTREE */
BKI_END

/*
 *  hash table int2_ops
 */
DATA(insert OID = 0 (  405 421  94 1 btreesel btreenpage ));
/*
 *  hash table float8_ops
 */
DATA(insert OID = 0 (  405 423 670 1 btreesel btreenpage ));
/*
 *  hash table int4_ops
 */
DATA(insert OID = 0 (  405 426  96 1 hashsel hashnpage ));
/*
 *  hash table oid_ops
 */
DATA(insert OID = 0 (  405 427 607 1 hashsel hashnpage ));
/*
 *  hash table float4_ops
 */
DATA(insert OID = 0 (  405 428 620 1 hashsel hashnpage ));
/*
 *  hash table char_ops
 */
DATA(insert OID = 0 (  405 429 92 1 hashsel hashnpage ));
/*
 *  hash table char16_ops
 */
DATA(insert OID = 0 (  405 430 93 1 hashsel hashnpage ));
/*
 *  hash table text_ops
 */
DATA(insert OID = 0 (  405 431 98 1 hashsel hashnpage ));


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
