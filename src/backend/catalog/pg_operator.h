/* ----------------------------------------------------------------
 *   FILE
 *	pg_operator.h
 *
 *   DESCRIPTION
 *	definition of the system "operator" relation (pg_operator)
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
#ifndef PgOperatorIncluded
#define PgOperatorIncluded 1	/* include this only once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "postgres.h"

/* ----------------
 *	pg_operator definition.  cpp turns this into
 *	typedef struct FormData_pg_operator
 * ----------------
 */
CATALOG(pg_operator) {
    char16 	oprname;
    oid 	oprowner;
    int2 	oprprec;
    char 	oprkind;
    bool 	oprisleft;
    bool	oprcanhash;
    oid 	oprleft;
    oid 	oprright;
    oid		oprresult;
    oid 	oprcom;
    oid 	oprnegate;
    oid 	oprlsortop;
    oid 	oprrsortop;
    regproc 	oprcode;
    regproc 	oprrest;
    regproc 	oprjoin;
} FormData_pg_operator;

/* ----------------
 *	Form_pg_operator corresponds to a pointer to a tuple with
 *	the format of pg_operator relation.
 * ----------------
 */
typedef FormData_pg_operator	*Form_pg_operator;

/* ----------------
 *	compiler constants for pg_operator
 * ----------------
 */
#define Name_pg_operator		"pg_operator"
#define Natts_pg_operator		16
#define Anum_pg_operator_oprname	1
#define Anum_pg_operator_oprowner	2
#define Anum_pg_operator_oprprec	3
#define Anum_pg_operator_oprkind	4
#define Anum_pg_operator_oprisleft	5
#define Anum_pg_operator_oprcanhash	6
#define Anum_pg_operator_oprleft	7
#define Anum_pg_operator_oprright	8
#define Anum_pg_operator_oprresult	9
#define Anum_pg_operator_oprcom		10
#define Anum_pg_operator_oprnegate	11
#define Anum_pg_operator_oprlsortop	12
#define Anum_pg_operator_oprrsortop	13
#define Anum_pg_operator_oprcode	14
#define Anum_pg_operator_oprrest	15
#define Anum_pg_operator_oprjoin	16

/* ----------------
 *	initial contents of pg_operator
 * ----------------
 */

DATA(insert OID = 91 (  "="        6 0 b t t  16  16  16  91   0  0  0 booleq eqsel eqjoinsel ));
DATA(insert OID = 92 (  "="        6 0 b t t  18  18  16  92 630  0  0 chareq eqsel eqjoinsel ));
DATA(insert OID = 93 (  "="        6 0 b t t  19  19  16  93   0  0  0 char16eq eqsel eqjoinsel ));
DATA(insert OID = 94 (  "="        6 0 b t t  21  21  16  94 519 95 95 int2eq eqsel eqjoinsel ));
DATA(insert OID = 95 (  "<"        6 0 b t f  21  21  16 520 524 95 95 int2lt intltsel intltjoinsel ));
DATA(insert OID = 96 (  "="        6 0 b t t  23  23  16  96 518 97 97 int4eq eqsel eqjoinsel ));
DATA(insert OID = 97 (  "<"        6 0 b t f  23  23  16 521 525 97 97 int4lt intltsel intltjoinsel ));
DATA(insert OID = 98 (  "="        6 0 b t t  25  25  16  98 531  0  0 texteq eqsel eqjoinsel ));
DATA(insert OID = 493 (  "<<"      6 0 b t f 603 603  16   0   0   0   0 box_left intltsel intltjoinsel ));
DATA(insert OID = 494 (  "&<"      6 0 b t f 603 603  16   0   0   0   0 box_overleft intltsel intltjoinsel ));
DATA(insert OID = 495 (  "&>"      6 0 b t f 603 603  16   0   0   0   0 box_overright intltsel intltjoinsel ));
DATA(insert OID = 496 (  ">>"      6 0 b t f 603 603  16   0   0   0   0 box_right intltsel intltjoinsel ));
DATA(insert OID = 497 (  "@"       6 0 b t f 603 603  16   0   0   0   0 box_contained intltsel intltjoinsel ));
DATA(insert OID = 498 (  "~"       6 0 b t f 603 603  16   0   0   0   0 box_contain intltsel intltjoinsel ));
DATA(insert OID = 499 (  "~="      6 0 b t f 603 603  16   0   0   0   0 box_same intltsel intltjoinsel ));
DATA(insert OID = 500 (  "&&"      6 0 b t f 603 603  16   0   0   0   0 box_overlap intltsel intltjoinsel ));
DATA(insert OID = 501 (  ">="      6 0 b t f 603 603  16   0   0   0   0 box_ge areasel areajoinsel ));
DATA(insert OID = 502 (  ">"       6 0 b t f 603 603  16   0   0   0   0 box_gt areasel areajoinsel ));
DATA(insert OID = 503 (  "="       6 0 b t t 603 603  16   0   0   0   0 box_eq areasel areajoinsel ));
DATA(insert OID = 504 (  "<"       6 0 b t f 603 603  16   0   0   0   0 box_lt areasel areajoinsel ));
DATA(insert OID = 505 (  "<="      6 0 b t f 603 603  16   0   0   0   0 box_le areasel areajoinsel ));
DATA(insert OID = 506 (  "!^"      6 0 b t f 600 600  16   0   0   0   0 point_above intltsel intltjoinsel ));
DATA(insert OID = 507 (  "!<"      6 0 b t f 600 600  16   0   0   0   0 point_left intltsel intltjoinsel ));
DATA(insert OID = 508 (  "!>"      6 0 b t f 600 600  16   0   0   0   0 point_right intltsel intltjoinsel ));
DATA(insert OID = 509 (  "!|"      6 0 b t f 600 600  16   0   0   0   0 point_below intltsel intltjoinsel ));
DATA(insert OID = 510 (  "=|="     6 0 b t f 600 600  16   0   0   0   0 point_eq intltsel intltjoinsel ));
DATA(insert OID = 511 (  "--->"    6 0 b t f 600 603  16   0   0   0   0 on_pb intltsel intltjoinsel ));
DATA(insert OID = 512 (  "---`"    6 0 b t f 600 602  16   0   0   0   0 on_ppath intltsel intltjoinsel ));
DATA(insert OID = 513 (  "@@"      6 0 l t f   0 603 600   0   0   0   0 box_center intltsel intltjoinsel ));
DATA(insert OID = 514 (  "*"       6 0 b t f  23  23  23 514   0   0   0 int4mul intltsel intltjoinsel ));
DATA(insert OID = 515 (  "!"       6 0 r t f  23   0  23   0   0   0   0 int4fac intltsel intltjoinsel ));
DATA(insert OID = 516 (  "!!"      6 0 l t f   0  23  23   0   0   0   0 int4fac intltsel intltjoinsel ));
DATA(insert OID = 517 (  "<--->"   6 0 b t f 600 600  23   0   0   0   0 pointdist intltsel intltjoinsel ));
DATA(insert OID = 518 (  "!="      6 0 b t f  23  23  16 518  96  97  97 int4ne neqsel neqjoinsel ));
DATA(insert OID = 519 (  "!="      6 0 b t f  21  21  16 519  94  95  95 int2ne neqsel neqjoinsel ));
DATA(insert OID = 520 (  ">"       6 0 b t f  21  21  16  95   0  95  95 int2gt intltsel intltjoinsel ));
DATA(insert OID = 521 (  ">"       6 0 b t f  23  23  16  97   0  97  97 int4gt intltsel intltjoinsel ));
DATA(insert OID = 522 (  "<="      6 0 b t f  21  21  16 524 520  95  95 int2le intltsel intltjoinsel ));
DATA(insert OID = 523 (  "<="      6 0 b t f  23  23  16 525 521  97  97 int4le intltsel intltjoinsel ));
DATA(insert OID = 524 (  ">="      6 0 b t f  21  21  16 522  95  95  95 int2ge intltsel intltjoinsel ));
DATA(insert OID = 525 (  ">="      6 0 b t f  23  23  16 523  97  97  97 int4ge intltsel intltjoinsel ));
DATA(insert OID = 526 (  "*"       6 0 b t f  21  21  21 526   0  95  95 int2mul intltsel intltjoinsel ));
DATA(insert OID = 527 (  "/"       6 0 b t f  21  21  21   0   0  95  95 int2div intltsel intltjoinsel ));
DATA(insert OID = 528 (  "/"       6 0 b t f  23  23  23   0   0  97  97 int4div intltsel intltjoinsel ));
DATA(insert OID = 529 (  "%"       0 b 0 t f  21  21  21   6   0  95  95 int2mod intltsel intltjoinsel ));
DATA(insert OID = 530 (  "%"       0 b 0 t f  23  23  23   6   0  97  97 int4mod intltsel intltjoinsel ));
DATA(insert OID = 531 (  "!="      6 0 b t f  25  25  16 531  98   0   0 textne neqsel neqjoinsel ));
DATA(insert OID = 532 (  "="       6 0 b t t  21  23  16 533 538  95  97 int24eq eqsel eqjoinsel ));
DATA(insert OID = 533 (  "="       6 0 b t t  23  21  16 532 539  97  95 int42eq eqsel eqjoinsel ));
DATA(insert OID = 534 (  "<"       6 0 b t f  21  23  16 537 542  95  97 int24lt intltsel intltjoinsel ));
DATA(insert OID = 535 (  "<"       6 0 b t f  23  21  16 536 543  97  95 int42lt intltsel intltjoinsel ));
DATA(insert OID = 536 (  ">"       6 0 b t f  21  23  16 535 540  95  97 int24gt intgtsel intgtjoinsel ));
DATA(insert OID = 537 (  ">"       6 0 b t f  23  21  16 534 541  97  95 int42lt intgtsel intgtjoinsel ));
DATA(insert OID = 538 (  "!="      6 0 b t f  21  23  16 539 532  95  97 int24ne neqsel neqjoinsel ));
DATA(insert OID = 539 (  "!="      6 0 b t f  23  21  16 538 533  97  95 int42ne neqsel neqjoinsel ));
DATA(insert OID = 540 (  "<="      6 0 b t f  21  23  16 543 536  95  97 int24le intltsel intltjoinsel ));
DATA(insert OID = 541 (  "<="      6 0 b t f  23  21  16 542 537  97  95 int42le intltsel intltjoinsel ));
DATA(insert OID = 542 (  ">="      6 0 b t f  21  23  16 541 534  95  97 int24ge intgtsel intgtjoinsel ));
DATA(insert OID = 543 (  ">="      6 0 b t f  23  21  16 540 535  97  95 int42ge intgtsel intgtjoinsel ));
DATA(insert OID = 544 (  "*"       6 0 b t f  21  23  23 545   0  95  97 int24mul intltsel intltjoinsel ));
DATA(insert OID = 545 (  "*"       6 0 b t f  23  21  23 544   0  97  95 int42mul intltsel intltjoinsel ));
DATA(insert OID = 546 (  "/"       6 0 b t f  21  23  23   0   0  95  97 int24div intltsel intltjoinsel ));
DATA(insert OID = 547 (  "/"       6 0 b t f  23  21  23   0   0  97  95 int42div intltsel intltjoinsel ));
DATA(insert OID = 548 (  "%"       0 0 b t f  21  23  23   6   0  95  97 int24mod intltsel intltjoinsel ));
DATA(insert OID = 549 (  "%"       0 0 b t f  23  21  23   6   0  97  95 int42mod intltsel intltjoinsel ));
DATA(insert OID = 550 (  "+"       6 0 b t f  21  21  21 550   0   0   0 int2pl intltsel intltjoinsel ));
DATA(insert OID = 551 (  "+"       6 0 b t f  23  23  23 551   0   0   0 int4pl intltsel intltjoinsel ));
DATA(insert OID = 552 (  "+"       6 0 b t f  21  23  23 553   0   0   0 int24pl intltsel intltjoinsel ));
DATA(insert OID = 553 (  "+"       6 0 b t f  23  21  23 552   0   0   0 int42pl intltsel intltjoinsel ));
DATA(insert OID = 554 (  "-"       6 0 b t f  21  21  21   0   0   0   0 int2mi intltsel intltjoinsel ));
DATA(insert OID = 555 (  "-"       6 0 b t f  23  23  23   0   0   0   0 int4mi intltsel intltjoinsel ));
DATA(insert OID = 556 (  "-"       6 0 b t f  21  23  23   0   0   0   0 int24mi intltsel intltjoinsel ));
DATA(insert OID = 557 (  "-"       6 0 b t f  23  21  23   0   0   0   0 int42mi intltsel intltjoinsel ));
DATA(insert OID = 558 (  "="       6 0 b t t  26  26  16 558 559   0   0 oideq intltsel intltjoinsel ));
DATA(insert OID = 559 (  "!="      6 0 b t f  26  26  16 559 558   0   0 oidneq neqsel neqjoinsel ));
DATA(insert OID = 560 (  "="       6 0 b t t 702 702  16 560 561 562 562 abstimeeq - - ));
DATA(insert OID = 561 (  "!="      6 0 b t f 702 702  16 561 560 562 562 abstimene - - ));
DATA(insert OID = 562 (  "<"       6 0 b t f 702 702  16 563 565 562 562 abstimelt - - ));
DATA(insert OID = 563 (  ">"       6 0 b t f 702 702  16 562 564 562 562 abstimegt - - ));
DATA(insert OID = 564 (  "<="      6 0 b t f 702 702  16 565 563 562 562 abstimele - - ));
DATA(insert OID = 565 (  ">="      6 0 b t f 702 702  16 564 562 562 562 abstimege - - ));
DATA(insert OID = 566 (  "="       6 0 b t t 703 703  16 566 567 568 568 reltimeeq - - ));
DATA(insert OID = 567 (  "!="      6 0 b t f 703 703  16 567 566 568 568 reltimene - - ));
DATA(insert OID = 568 (  "<"       6 0 b t f 703 703  16 569 571 568 568 reltimelt - - ));
DATA(insert OID = 569 (  ">"       6 0 b t f 703 703  16 568 570 568 568 reltimegt - - ));
DATA(insert OID = 570 (  "<="      6 0 b t f 703 703  16 571 569 568 568 reltimele - - ));
DATA(insert OID = 571 (  ">="      6 0 b t f 703 703  16 570 568 568 568 reltimege - - ));
DATA(insert OID = 572 (  "="       6 0 b t t 704 704  16 572   0   0   0 intervaleq - - ));
DATA(insert OID = 573 (  "<<"      6 0 b t f 704 704  16   0   0   0   0 intervalct - - ));
DATA(insert OID = 574 (  "&&"      6 0 b t f 704 704  16   0   0   0   0 intervalov - - ));
DATA(insert OID = 575 (  "#="      6 0 b t f 704 703  16   0 576   0 568 intervalleneq - - ));
DATA(insert OID = 576 (  "#!="     6 0 b t f 704 703  16   0 575   0 568 intervallenne - - ));
DATA(insert OID = 577 (  "#<"      6 0 b t f 704 703  16   0 580   0 568 intervallenlt - - ));
DATA(insert OID = 578 (  "#>"      6 0 b t f 704 703  16   0 579   0 568 intervallengt - - ));
DATA(insert OID = 579 (  "#<="     6 0 b t f 704 703  16   0 578   0 568 intervallenle - - ));
DATA(insert OID = 580 (  "#>="     6 0 b t f 704 703  16   0 577   0 568 intervallenge - - ));
DATA(insert OID = 581 (  "+"       6 0 b t f 702 703 702 581   0 562 562 timepl - - ));
DATA(insert OID = 582 (  "-"       6 0 b t f 702 703 702   0   0 562 568 timemi - - ));
DATA(insert OID = 583 (  "<?>"     6 0 b t f 702 704  16   0   0 562   0 ininterval - - ));
DATA(insert OID = 584 (  "-"       6 0 l t f   0 700 700   0   0   0   0 float4um - - ));
DATA(insert OID = 585 (  "-"       6 0 l t f   0 701 701   0   0   0   0 float8um - - ));
DATA(insert OID = 586 (  "+"       6 0 b t f 700 700 700 586   0   0   0 float4pl - - ));
DATA(insert OID = 587 (  "-"       6 0 b t f 700 700 700   0   0   0   0 float4mi - - ));
DATA(insert OID = 588 (  "/"       6 0 b t f 700 700 700   0   0   0   0 float4div - - ));
DATA(insert OID = 589 (  "*"       6 0 b t f 700 700 700 589   0   0   0 float4mul - - ));
DATA(insert OID = 590 (  "@"       6 0 l t f   0 700 700   0   0   0   0 float4abs - - ));
DATA(insert OID = 591 (  "+"       6 0 b t f 701 701 701 591   0   0   0 float8pl - - ));
DATA(insert OID = 592 (  "-"       6 0 b t f 701 701 701   0   0   0   0 float8mi - - ));
DATA(insert OID = 593 (  "/"       6 0 b t f 701 701 701   0   0   0   0 float8div - - ));
DATA(insert OID = 594 (  "*"       6 0 b t f 701 701 701 594   0   0   0 float8mul - - ));
DATA(insert OID = 595 (  "@"       6 0 l t f   0 701 701   0   0   0   0 float8abs - - ));
DATA(insert OID = 596 (  "|/"      6 0 l t f   0 701 701   0   0   0   0 dsqrt - - ));
DATA(insert OID = 597 (  "||/"     6 0 l t f   0 701 701   0   0   0   0 dcbrt - - ));
DATA(insert OID = 598 (  "%"       6 0 l t f   0 701 701   0   0   0   0 dtrunc - - ));
DATA(insert OID = 599 (  "%"       6 0 r t f 701   0 701   0   0   0   0 dround - - ));
DATA(insert OID = 600 (  "^"       6 0 b t f 701 701 701   0   0   0   0 dpow - - ));
DATA(insert OID = 601 (  ":"       6 0 l t f   0 701 701   0   0   0   0 dexp - - ));
DATA(insert OID = 602 (  ";"       6 0 l t f   0 701 701   0   0   0   0 dlog1 - - ));
DATA(insert OID = 603 (  "|"       6 0 l t f   0 704 702   0   0   0   0 intervalstart - - ));
DATA(insert OID = 607 (  "="       6 0 b t t  26  26  16 607 608  97  97 oideq eqsel eqjoinsel ));
DATA(insert OID = 608 (  "!="      6 0 b t f  26  26  16 608 607  97  97 oidneq eqsel eqjoinsel ));
DATA(insert OID = 609 (  "<"       6 0 b t f  26  26  16 610 612  97  97 int4lt intltsel intltjoinsel ));
DATA(insert OID = 610 (  ">"       6 0 b t f  26  26  16 609 611  97  97 int4gt intltsel intltjoinsel ));
DATA(insert OID = 611 (  "<="      6 0 b t f  26  26  16 612 610  97  97 int4le intltsel intltjoinsel ));
DATA(insert OID = 612 (  ">="      6 0 b t f  26  26  16 611 609  97  97 int4ge intltsel intltjoinsel ));
DATA(insert OID = 620 (  "="       6 0 b t t  700  700  16 620 621  622 622 float4eq eqsel eqjoinsel ));
DATA(insert OID = 621 (  "!="      6 0 b t f  700  700  16 621 620  622 622 float4ne eqsel eqjoinsel ));
DATA(insert OID = 622 (  "<"       6 0 b t f  700  700  16 623 625  622 622 float4lt intltsel intltjoinsel ));
DATA(insert OID = 623 (  ">"       6 0 b t f  700  700  16 622 624  622 622 float4gt intltsel intltjoinsel ));
DATA(insert OID = 624 (  "<="      6 0 b t f  700  700  16 625 623  622 622 float4le intltsel intltjoinsel ));
DATA(insert OID = 625 (  ">="      6 0 b t f  700  700  16 624 622  622 622 float4ge intltsel intltjoinsel ));
DATA(insert OID = 626 (  "!!="     6 0 b t f  23   19   16 0   0    0   0   int4notin "-"     "-"));
DATA(insert OID = 627 (  "!!="     6 0 b t f  26   19   16 0   0    0   0   oidnotin "-"     "-"));
DATA(insert OID = 630 (  "!="       6 0 b t f  18  18  16 630 92  0 0 charne eqsel eqjoinsel ));
DATA(insert OID = 631 (  "<"       6 0 b t f  18  18  16 631 634  0 0 charlt eqsel eqjoinsel ));
DATA(insert OID = 632 (  "<="       6 0 b t f  18  18  16 632 633  0 0 charle eqsel eqjoinsel ));
DATA(insert OID = 633 (  ">"       6 0 b t f  18  18  16 633 632  0 0 chargt eqsel eqjoinsel ));
DATA(insert OID = 634 (  ">="       6 0 b t f  18  18  16 634 631  0 0 charge eqsel eqjoinsel ));
DATA(insert OID = 635 (  "+"       6 0 b t f  18  18  16 0 0  0 0 charpl eqsel eqjoinsel ));
DATA(insert OID = 636 (  "-"       6 0 b t f  18  18  16 0 0  0 0 charmi eqsel eqjoinsel ));
DATA(insert OID = 637 (  "*"       6 0 b t f  18  18  16 0 0  0 0 charmul eqsel eqjoinsel ));
DATA(insert OID = 638 (  "/"       6 0 b t f  18  18  16 0 0  0 0 chardiv eqsel eqjoinsel ));

/* ----------------
 *	old definition of OperatorTupleForm
 * ----------------
 */
#ifndef OperatorTupleForm_Defined
#define OperatorTupleForm_Defined 1

typedef struct OperatorTupleFormD {
	NameData	oprname;
	ObjectId	oprowner;
	uint16		oprprec;
	char		oprkind;
	Boolean		oprisleft;
	Boolean		oprcanhash;
	ObjectId	oprleft;
	ObjectId	oprright;
	ObjectId	oprresult;
	ObjectId	oprcom;
	ObjectId	oprnegate;
	ObjectId	oprlsortop;
	ObjectId	oprrsortop;
	RegProcedure	oprcode;
	RegProcedure	oprrest;
	RegProcedure	oprjoin;
} OperatorTupleFormD;

typedef OperatorTupleFormD	*OperatorTupleForm;

#endif OperatorTupleForm_Defined

/* ----------------
 *	old definition of struct operator
 * ----------------
 */
#ifndef struct_operator_Defined
#define struct_operator_Defined 1

struct	operator {
	char	oprname[16];
	OID	oprowner;
	uint16	oprprec;
	char	oprkind;
	Boolean	oprisleft;
	Boolean	oprcanhash;
	OID	oprleft;
	OID	oprright;
	OID	oprresult;
	OID	oprcom;
	OID	oprnegate;
	OID	oprlsortop;
	OID	oprrsortop;
	REGPROC	oprcode;
	REGPROC	oprrest;
	REGPROC	oprjoin;
};

#endif struct_operator_Defined


/* ----------------
 *	old style compiler constants.  these are obsolete and
 *	should not be used -cim 6/17/90
 * ----------------
 */
#define OperatorNameAttributeNumber \
    Anum_pg_operator_oprname
#define OperatorKindAttributeNumber \
    Anum_pg_operator_oprkind
#define OperatorLeftAttributeNumber \
    Anum_pg_operator_oprleft
#define OperatorRightAttributeNumber \
    Anum_pg_operator_oprright
#define	OperatorResultAttributeNumber \
    Anum_pg_operator_oprresult
    
/* I'm not certain the next is correct -cim 6/17/90 */
#define	OperatorProcedureAttributeNumber \
    Anum_pg_operator_oprcode
    
#define	OperatorRestrictAttributeNumber \
    Anum_pg_operator_oprrest
#define	OperatorJoinAttributeNumber \
    Anum_pg_operator_oprjoin
    
#define	OperatorRelationNumberOfAttributes \
    Natts_pg_operator

#endif PgOperatorIncluded
