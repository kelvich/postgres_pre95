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
 *	XXX do NOT break up DATA() statements into multiple lines!
 *	    the scripts are not as smart as you might think...
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
#include "tmp/postgres.h"

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

DATA(insert OID = 85 (  "!="        PGUID 0 b t t  16  16  16  85   91  0  0 boolne neqsel neqjoinsel ));
DATA(insert OID = 91 (  "="        PGUID 0 b t t  16  16  16  91   85  0  0 booleq eqsel eqjoinsel ));
#define BooleanEqualOperator   91

DATA(insert OID = 92 (  "="        PGUID 0 b t t  18  18  16  92 630  0  0 chareq eqsel eqjoinsel ));
DATA(insert OID = 93 (  "="        PGUID 0 b t t  19  19  16  93   0  0  0 char16eq eqsel eqjoinsel ));
DATA(insert OID = 94 (  "="        PGUID 0 b t t  21  21  16  94 519 95 95 int2eq eqsel eqjoinsel ));
DATA(insert OID = 95 (  "<"        PGUID 0 b t f  21  21  16 520 524 0 0 int2lt intltsel intltjoinsel ));
DATA(insert OID = 96 (  "="        PGUID 0 b t t  23  23  16  96 518 97 97 int4eq eqsel eqjoinsel ));
DATA(insert OID = 97 (  "<"        PGUID 0 b t f  23  23  16 521 525 0 0 int4lt intltsel intltjoinsel ));
DATA(insert OID = 98 (  "="        PGUID 0 b t t  25  25  16  98 531  0  0 texteq eqsel eqjoinsel ));
DATA(insert OID = 329 (  "="       PGUID 0 b t t  1000  1000  16  329 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 349 (  "="       PGUID 0 b t t  1001  1001  16  349 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 374 (  "="       PGUID 0 b t t  1002  1002  16  374 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 375 (  "="       PGUID 0 b t t  1003  1003  16  375 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 376 (  "="       PGUID 0 b t t  1004  1004  16  376 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 377 (  "="       PGUID 0 b t t  1005  1005  16  377 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 378 (  "="       PGUID 0 b t t  1006  1006  16  378 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 379 (  "="       PGUID 0 b t t  1007  1007  16  379 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 380 (  "="       PGUID 0 b t t  1008  1008  16  380 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 381 (  "="       PGUID 0 b t t  1009  1009  16  381 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 382 (  "="       PGUID 0 b t t  1028  1028  16  382 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 383 (  "="       PGUID 0 b t t  1010  1010  16  383 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 384 (  "="       PGUID 0 b t t  1011  1011  16  384 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 385 (  "="       PGUID 0 b t t  1012  1012  16  385 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 386 (  "="       PGUID 0 b t t  1013  1013  16  386 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 387 (  "="       PGUID 0 b t t  1014  1014  16  387 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 388 (  "="       PGUID 0 b t t  1015  1015  16  388 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 389 (  "="       PGUID 0 b t t  1016  1016  16  389 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 390 (  "="       PGUID 0 b t t  1017  1017  16  390 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 391 (  "="       PGUID 0 b t t  1018  1018  16  391 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 392 (  "="       PGUID 0 b t t  1019  1019  16  392 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 393 (  "="       PGUID 0 b t t  1020  1020  16  393 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 394 (  "="       PGUID 0 b t t  1021  1021  16  394 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 395 (  "="       PGUID 0 b t t  1022  1022  16  395 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 396 (  "="       PGUID 0 b t t  1023  1023  16  396 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 397 (  "="       PGUID 0 b t t  1024  1024  16  397 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 398 (  "="       PGUID 0 b t t  1025  1025  16  398 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 399 (  "="       PGUID 0 b t t  1026  1026  16  399 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 400 (  "="       PGUID 0 b t t  1027  1027  16  400 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 401 (  "="       PGUID 0 b t t  1034  1034  16  401 0  0  0 array_eq eqsel eqjoinsel ));
DATA(insert OID = 485 (  "<<"      PGUID 0 b t f 604 604  16   0   0   0   0 poly_left intltsel intltjoinsel ));
DATA(insert OID = 486 (  "&<"      PGUID 0 b t f 604 604  16   0   0   0   0 poly_overleft intltsel intltjoinsel ));
DATA(insert OID = 487 (  "&>"      PGUID 0 b t f 604 604  16   0   0   0   0 poly_overright intltsel intltjoinsel ));
DATA(insert OID = 488 (  ">>"      PGUID 0 b t f 604 604  16   0   0   0   0 poly_right intltsel intltjoinsel ));
DATA(insert OID = 489 (  "@"       PGUID 0 b t f 604 604  16   0   0   0   0 poly_contained intltsel intltjoinsel ));
DATA(insert OID = 490 (  "~"       PGUID 0 b t f 604 604  16   0   0   0   0 poly_contain intltsel intltjoinsel ));
DATA(insert OID = 491 (  "~="      PGUID 0 b t f 604 604  16   0   0   0   0 poly_same intltsel intltjoinsel ));
DATA(insert OID = 492 (  "&&"      PGUID 0 b t f 604 604  16   0   0   0   0 poly_overlap intltsel intltjoinsel ));
DATA(insert OID = 493 (  "<<"      PGUID 0 b t f 603 603  16   0   0   0   0 box_left intltsel intltjoinsel ));
DATA(insert OID = 494 (  "&<"      PGUID 0 b t f 603 603  16   0   0   0   0 box_overleft intltsel intltjoinsel ));
DATA(insert OID = 495 (  "&>"      PGUID 0 b t f 603 603  16   0   0   0   0 box_overright intltsel intltjoinsel ));
DATA(insert OID = 496 (  ">>"      PGUID 0 b t f 603 603  16   0   0   0   0 box_right intltsel intltjoinsel ));
DATA(insert OID = 497 (  "@"       PGUID 0 b t f 603 603  16   0   0   0   0 box_contained intltsel intltjoinsel ));
DATA(insert OID = 498 (  "~"       PGUID 0 b t f 603 603  16   0   0   0   0 box_contain intltsel intltjoinsel ));
DATA(insert OID = 499 (  "~="      PGUID 0 b t f 603 603  16   0   0   0   0 box_same intltsel intltjoinsel ));
DATA(insert OID = 500 (  "&&"      PGUID 0 b t f 603 603  16   0   0   0   0 box_overlap intltsel intltjoinsel ));
DATA(insert OID = 501 (  ">="      PGUID 0 b t f 603 603  16   0   0   0   0 box_ge areasel areajoinsel ));
DATA(insert OID = 502 (  ">"       PGUID 0 b t f 603 603  16   0   0   0   0 box_gt areasel areajoinsel ));
DATA(insert OID = 503 (  "="       PGUID 0 b t t 603 603  16   0   0   0   0 box_eq areasel areajoinsel ));
DATA(insert OID = 504 (  "<"       PGUID 0 b t f 603 603  16   0   0   0   0 box_lt areasel areajoinsel ));
DATA(insert OID = 505 (  "<="      PGUID 0 b t f 603 603  16   0   0   0   0 box_le areasel areajoinsel ));
DATA(insert OID = 506 (  "!^"      PGUID 0 b t f 600 600  16   0   0   0   0 point_above intltsel intltjoinsel ));
DATA(insert OID = 507 (  "!<"      PGUID 0 b t f 600 600  16   0   0   0   0 point_left intltsel intltjoinsel ));
DATA(insert OID = 508 (  "!>"      PGUID 0 b t f 600 600  16   0   0   0   0 point_right intltsel intltjoinsel ));
DATA(insert OID = 509 (  "!|"      PGUID 0 b t f 600 600  16   0   0   0   0 point_below intltsel intltjoinsel ));
DATA(insert OID = 510 (  "=|="     PGUID 0 b t f 600 600  16   0   0   0   0 point_eq intltsel intltjoinsel ));
DATA(insert OID = 511 (  "--->"    PGUID 0 b t f 600 603  16   0   0   0   0 on_pb intltsel intltjoinsel ));
DATA(insert OID = 512 (  "---`"    PGUID 0 b t f 600 602  16   0   0   0   0 on_ppath intltsel intltjoinsel ));
DATA(insert OID = 513 (  "@@"      PGUID 0 l t f   0 603 600   0   0   0   0 box_center intltsel intltjoinsel ));
DATA(insert OID = 514 (  "*"       PGUID 0 b t f  23  23  23 514   0   0   0 int4mul intltsel intltjoinsel ));
DATA(insert OID = 515 (  "!"       PGUID 0 r t f  23   0  23   0   0   0   0 int4fac intltsel intltjoinsel ));
DATA(insert OID = 516 (  "!!"      PGUID 0 l t f   0  23  23   0   0   0   0 int4fac intltsel intltjoinsel ));
DATA(insert OID = 517 (  "<--->"   PGUID 0 b t f 600 600  23   0   0   0   0 pointdist intltsel intltjoinsel ));
DATA(insert OID = 518 (  "!="      PGUID 0 b t f  23  23  16 518  96  0  0 int4ne neqsel neqjoinsel ));
DATA(insert OID = 519 (  "!="      PGUID 0 b t f  21  21  16 519  94  0  0 int2ne neqsel neqjoinsel ));
DATA(insert OID = 520 (  ">"       PGUID 0 b t f  21  21  16  95   0  0  0 int2gt intgtsel intgtjoinsel ));
DATA(insert OID = 521 (  ">"       PGUID 0 b t f  23  23  16  97   0  0  0 int4gt intgtsel intgtjoinsel ));
DATA(insert OID = 522 (  "<="      PGUID 0 b t f  21  21  16 524 520  0  0 int2le intltsel intltjoinsel ));
DATA(insert OID = 523 (  "<="      PGUID 0 b t f  23  23  16 525 521  0  0 int4le intltsel intltjoinsel ));
DATA(insert OID = 524 (  ">="      PGUID 0 b t f  21  21  16 522  95  0  0 int2ge intgtsel intgtjoinsel ));
DATA(insert OID = 525 (  ">="      PGUID 0 b t f  23  23  16 523  97  0  0 int4ge intgtsel intgtjoinsel ));
DATA(insert OID = 526 (  "*"       PGUID 0 b t f  21  21  21 526   0  0  0 int2mul intltsel intltjoinsel ));
DATA(insert OID = 527 (  "/"       PGUID 0 b t f  21  21  21   0   0  0  0 int2div intltsel intltjoinsel ));
DATA(insert OID = 528 (  "/"       PGUID 0 b t f  23  23  23   0   0  0  0 int4div intltsel intltjoinsel ));
DATA(insert OID = 529 (  "%"       PGUID 0 b t f  21  21  21   6   0  0  0 int2mod intltsel intltjoinsel ));
DATA(insert OID = 530 (  "%"       PGUID 0 b t f  23  23  23   6   0  0  0 int4mod intltsel intltjoinsel ));
DATA(insert OID = 531 (  "!="      PGUID 0 b t f  25  25  16 531  98   0   0 textne neqsel neqjoinsel ));
DATA(insert OID = 532 (  "="       PGUID 0 b t t  21  23  16 533 538  95  97 int24eq eqsel eqjoinsel ));
DATA(insert OID = 533 (  "="       PGUID 0 b t t  23  21  16 532 539  97  95 int42eq eqsel eqjoinsel ));
DATA(insert OID = 534 (  "<"       PGUID 0 b t f  21  23  16 537 542  0  0 int24lt intltsel intltjoinsel ));
DATA(insert OID = 535 (  "<"       PGUID 0 b t f  23  21  16 536 543  0  0 int42lt intltsel intltjoinsel ));
DATA(insert OID = 536 (  ">"       PGUID 0 b t f  21  23  16 535 540  0  0 int24gt intgtsel intgtjoinsel ));
DATA(insert OID = 537 (  ">"       PGUID 0 b t f  23  21  16 534 541  0  0 int42gt intgtsel intgtjoinsel ));
DATA(insert OID = 538 (  "!="      PGUID 0 b t f  21  23  16 539 532  0  0 int24ne neqsel neqjoinsel ));
DATA(insert OID = 539 (  "!="      PGUID 0 b t f  23  21  16 538 533  0  0 int42ne neqsel neqjoinsel ));
DATA(insert OID = 540 (  "<="      PGUID 0 b t f  21  23  16 543 536  0  0 int24le intltsel intltjoinsel ));
DATA(insert OID = 541 (  "<="      PGUID 0 b t f  23  21  16 542 537  0  0 int42le intltsel intltjoinsel ));
DATA(insert OID = 542 (  ">="      PGUID 0 b t f  21  23  16 541 534  0  0 int24ge intgtsel intgtjoinsel ));
DATA(insert OID = 543 (  ">="      PGUID 0 b t f  23  21  16 540 535  0  0 int42ge intgtsel intgtjoinsel ));
DATA(insert OID = 544 (  "*"       PGUID 0 b t f  21  23  23 545   0  0  0 int24mul intltsel intltjoinsel ));
DATA(insert OID = 545 (  "*"       PGUID 0 b t f  23  21  23 544   0  0  0 int42mul intltsel intltjoinsel ));
DATA(insert OID = 546 (  "/"       PGUID 0 b t f  21  23  23   0   0  0  0 int24div intltsel intltjoinsel ));
DATA(insert OID = 547 (  "/"       PGUID 0 b t f  23  21  23   0   0  0  0 int42div intltsel intltjoinsel ));
DATA(insert OID = 548 (  "%"       PGUID 0 b t f  21  23  23   6   0  0  0 int24mod intltsel intltjoinsel ));
DATA(insert OID = 549 (  "%"       PGUID 0 b t f  23  21  23   6   0  0  0 int42mod intltsel intltjoinsel ));
DATA(insert OID = 550 (  "+"       PGUID 0 b t f  21  21  21 550   0   0   0 int2pl intltsel intltjoinsel ));
DATA(insert OID = 551 (  "+"       PGUID 0 b t f  23  23  23 551   0   0   0 int4pl intltsel intltjoinsel ));
DATA(insert OID = 552 (  "+"       PGUID 0 b t f  21  23  23 553   0   0   0 int24pl intltsel intltjoinsel ));
DATA(insert OID = 553 (  "+"       PGUID 0 b t f  23  21  23 552   0   0   0 int42pl intltsel intltjoinsel ));
DATA(insert OID = 554 (  "-"       PGUID 0 b t f  21  21  21   0   0   0   0 int2mi intltsel intltjoinsel ));
DATA(insert OID = 555 (  "-"       PGUID 0 b t f  23  23  23   0   0   0   0 int4mi intltsel intltjoinsel ));
DATA(insert OID = 556 (  "-"       PGUID 0 b t f  21  23  23   0   0   0   0 int24mi intltsel intltjoinsel ));
DATA(insert OID = 557 (  "-"       PGUID 0 b t f  23  21  23   0   0   0   0 int42mi intltsel intltjoinsel ));
DATA(insert OID = 0   (  "-"       PGUID 0 l t f   0  23  23  0   0   0   0 int4um intltsel intltjoinsel ));
DATA(insert OID = 0   (  "-"       PGUID 0 l t f   0  21  21  0   0   0   0 int2um intltsel intltjoinsel ));
DATA(insert OID = 558 (  "="       PGUID 0 b t t  26  26  16 558 559   0   0 oideq intltsel intltjoinsel ));
DATA(insert OID = 559 (  "!="      PGUID 0 b t f  26  26  16 559 558   0   0 oidne neqsel neqjoinsel ));
DATA(insert OID = 560 (  "="       PGUID 0 b t t 702 702  16 560 561 562 562 abstimeeq eqsel eqjoinsel ));
DATA(insert OID = 561 (  "!="      PGUID 0 b t f 702 702  16 561 560 0 0 abstimene neqsel neqjoinsel ));
DATA(insert OID = 562 (  "<"       PGUID 0 b t f 702 702  16 563 565 0 0 abstimelt intltsel intltjoinsel ));
DATA(insert OID = 563 (  ">"       PGUID 0 b t f 702 702  16 562 564 0 0 abstimegt intltsel intltjoinsel ));
DATA(insert OID = 564 (  "<="      PGUID 0 b t f 702 702  16 565 563 0 0 abstimele intltsel intltjoinsel ));
DATA(insert OID = 565 (  ">="      PGUID 0 b t f 702 702  16 564 562 0 0 abstimege intltsel intltjoinsel ));
DATA(insert OID = 566 (  "="       PGUID 0 b t t 703 703  16 566 567 568 568 reltimeeq - - ));
DATA(insert OID = 567 (  "!="      PGUID 0 b t f 703 703  16 567 566 0 0 reltimene - - ));
DATA(insert OID = 568 (  "<"       PGUID 0 b t f 703 703  16 569 571 0 0 reltimelt - - ));
DATA(insert OID = 569 (  ">"       PGUID 0 b t f 703 703  16 568 570 0 0 reltimegt - - ));
DATA(insert OID = 570 (  "<="      PGUID 0 b t f 703 703  16 571 569 0 0 reltimele - - ));
DATA(insert OID = 571 (  ">="      PGUID 0 b t f 703 703  16 570 568 0 0 reltimege - - ));
DATA(insert OID = 572 (  "="       PGUID 0 b t t 704 704  16 572   0   0   0 intervaleq - - ));
DATA(insert OID = 573 (  "<<"      PGUID 0 b t f 704 704  16   0   0   0   0 intervalct - - ));
DATA(insert OID = 574 (  "&&"      PGUID 0 b t f 704 704  16   0   0   0   0 intervalov - - ));
DATA(insert OID = 575 (  "#="      PGUID 0 b t f 704 703  16   0 576   0 568 intervalleneq - - ));
DATA(insert OID = 576 (  "#!="     PGUID 0 b t f 704 703  16   0 575   0 568 intervallenne - - ));
DATA(insert OID = 577 (  "#<"      PGUID 0 b t f 704 703  16   0 580   0 568 intervallenlt - - ));
DATA(insert OID = 578 (  "#>"      PGUID 0 b t f 704 703  16   0 579   0 568 intervallengt - - ));
DATA(insert OID = 579 (  "#<="     PGUID 0 b t f 704 703  16   0 578   0 568 intervallenle - - ));
DATA(insert OID = 580 (  "#>="     PGUID 0 b t f 704 703  16   0 577   0 568 intervallenge - - ));
DATA(insert OID = 581 (  "+"       PGUID 0 b t f 702 703 702 581   0 0 0 timepl - - ));
DATA(insert OID = 582 (  "-"       PGUID 0 b t f 702 703 702   0   0 0 0 timemi - - ));
DATA(insert OID = 583 (  "<?>"     PGUID 0 b t f 702 704  16   0   0 562   0 ininterval - - ));
DATA(insert OID = 584 (  "-"       PGUID 0 l t f   0 700 700   0   0   0   0 float4um - - ));
DATA(insert OID = 585 (  "-"       PGUID 0 l t f   0 701 701   0   0   0   0 float8um - - ));
DATA(insert OID = 586 (  "+"       PGUID 0 b t f 700 700 700 586   0   0   0 float4pl - - ));
DATA(insert OID = 587 (  "-"       PGUID 0 b t f 700 700 700   0   0   0   0 float4mi - - ));
DATA(insert OID = 588 (  "/"       PGUID 0 b t f 700 700 700   0   0   0   0 float4div - - ));
DATA(insert OID = 589 (  "*"       PGUID 0 b t f 700 700 700 589   0   0   0 float4mul - - ));
DATA(insert OID = 590 (  "@"       PGUID 0 l t f   0 700 700   0   0   0   0 float4abs - - ));
DATA(insert OID = 591 (  "+"       PGUID 0 b t f 701 701 701 591   0   0   0 float8pl - - ));
DATA(insert OID = 592 (  "-"       PGUID 0 b t f 701 701 701   0   0   0   0 float8mi - - ));
DATA(insert OID = 593 (  "/"       PGUID 0 b t f 701 701 701   0   0   0   0 float8div - - ));
DATA(insert OID = 594 (  "*"       PGUID 0 b t f 701 701 701 594   0   0   0 float8mul - - ));
DATA(insert OID = 595 (  "@"       PGUID 0 l t f   0 701 701   0   0   0   0 float8abs - - ));
DATA(insert OID = 596 (  "|/"      PGUID 0 l t f   0 701 701   0   0   0   0 dsqrt - - ));
DATA(insert OID = 597 (  "||/"     PGUID 0 l t f   0 701 701   0   0   0   0 dcbrt - - ));
DATA(insert OID = 598 (  "%"       PGUID 0 l t f   0 701 701   0   0   0   0 dtrunc - - ));
DATA(insert OID = 599 (  "%"       PGUID 0 r t f 701   0 701   0   0   0   0 dround - - ));
DATA(insert OID = 600 (  "^"       PGUID 0 b t f 701 701 701   0   0   0   0 dpow - - ));
DATA(insert OID = 601 (  ":"       PGUID 0 l t f   0 701 701   0   0   0   0 dexp - - ));
DATA(insert OID = 602 (  ";"       PGUID 0 l t f   0 701 701   0   0   0   0 dlog1 - - ));
DATA(insert OID = 603 (  "|"       PGUID 0 l t f   0 704 702   0   0   0   0 intervalstart - - ));
DATA(insert OID = 607 (  "="       PGUID 0 b t t  26  26  16 607 608  97  97 oideq eqsel eqjoinsel ));
DATA(insert OID = 608 (  "!="      PGUID 0 b t f  26  26  16 608 607  0  0 oidne eqsel eqjoinsel ));
DATA(insert OID = 609 (  "<"       PGUID 0 b t f  26  26  16 610 612  0  0 int4lt intltsel intltjoinsel ));
DATA(insert OID = 610 (  ">"       PGUID 0 b t f  26  26  16 609 611  0  0 int4gt intgtsel intgtjoinsel ));
DATA(insert OID = 611 (  "<="      PGUID 0 b t f  26  26  16 612 610  0  0 int4le intltsel intltjoinsel ));
DATA(insert OID = 612 (  ">="      PGUID 0 b t f  26  26  16 611 609  0  0 int4ge intgtsel intgtjoinsel ));
DATA(insert OID = 620 (  "="       PGUID 0 b t t  700  700  16 620 621  622 622 float4eq eqsel eqjoinsel ));
DATA(insert OID = 621 (  "!="      PGUID 0 b t f  700  700  16 621 620  0 0 float4ne eqsel eqjoinsel ));
DATA(insert OID = 622 (  "<"       PGUID 0 b t f  700  700  16 623 625  0 0 float4lt intltsel intltjoinsel ));
DATA(insert OID = 623 (  ">"       PGUID 0 b t f  700  700  16 622 624  0 0 float4gt intgtsel intgtjoinsel ));
DATA(insert OID = 624 (  "<="      PGUID 0 b t f  700  700  16 625 623  0 0 float4le intltsel intltjoinsel ));
DATA(insert OID = 625 (  ">="      PGUID 0 b t f  700  700  16 624 622  0 0 float4ge intgtsel intgtjoinsel ));
DATA(insert OID = 626 (  "!!="     PGUID 0 b t f  23   19   16 0   0    0   0   int4notin "-"     "-"));
DATA(insert OID = 627 (  "!!="     PGUID 0 b t f  26   19   16 0   0    0   0   oidnotin "-"     "-"));
DATA(insert OID = 630 (  "!="      PGUID 0 b t f  18  18  16 630  92  0 0 charne eqsel eqjoinsel ));
    
DATA(insert OID = 631 (  "<"       PGUID 0 b t f  18  18  16 633 634  0 0 charlt intltsel intltjoinsel ));
DATA(insert OID = 632 (  "<="      PGUID 0 b t f  18  18  16 634 633  0 0 charle intltsel intltjoinsel ));
DATA(insert OID = 633 (  ">"       PGUID 0 b t f  18  18  16 631 632  0 0 chargt intltsel intltjoinsel ));
DATA(insert OID = 634 (  ">="      PGUID 0 b t f  18  18  16 632 631  0 0 charge intltsel intltjoinsel ));
    
DATA(insert OID = 635 (  "+"       PGUID 0 b t f  18  18  16 0 0  0 0 charpl eqsel eqjoinsel ));
DATA(insert OID = 636 (  "-"       PGUID 0 b t f  18  18  16 0 0  0 0 charmi eqsel eqjoinsel ));
DATA(insert OID = 637 (  "*"       PGUID 0 b t f  18  18  16 0 0  0 0 charmul eqsel eqjoinsel ));
DATA(insert OID = 638 (  "/"       PGUID 0 b t f  18  18  16 0 0  0 0 chardiv eqsel eqjoinsel ));

DATA(insert OID = 639 (  "~"       PGUID 0 b t f  19  19  16 0 640  0 0 char16regexeq eqsel eqjoinsel ));
DATA(insert OID = 640 (  "!~"      PGUID 0 b t f  19  19  16 0 639  0 0 char16regexne eqsel eqjoinsel ));
DATA(insert OID = 641 (  "~"       PGUID 0 b t f  25  25  16 0 642  0 0 textregexeq eqsel eqjoinsel ));
DATA(insert OID = 642 (  "!~"      PGUID 0 b t f  25  25  16 0 641  0 0 textregexne eqsel eqjoinsel ));
DATA(insert OID = 643 (  "!="      PGUID 0 b t f  19  19  16 93 643  0 0 char16ne eqsel eqjoinsel ));

DATA(insert OID = 660 (  "<"       PGUID 0 b t f  19  19  16 662 663  0 0 char16lt intltsel intltjoinsel ));
DATA(insert OID = 661 (  "<="      PGUID 0 b t f  19  19  16 663 662  0 0 char16le intltsel intltjoinsel ));
DATA(insert OID = 662 (  ">"       PGUID 0 b t f  19  19  16 660 661  0 0 char16gt intltsel intltjoinsel ));
DATA(insert OID = 663 (  ">="      PGUID 0 b t f  19  19  16 661 660  0 0 char16ge intltsel intltjoinsel ));
DATA(insert OID = 664 (  "<"       PGUID 0 b t f  25  25  16 666 667  0 0 text_lt intltsel intltjoinsel ));
DATA(insert OID = 665 (  "<="      PGUID 0 b t f  25  25  16 667 666  0 0 text_le intltsel intltjoinsel ));
DATA(insert OID = 666 (  ">"       PGUID 0 b t f  25  25  16 664 665  0 0 text_gt intltsel intltjoinsel ));
DATA(insert OID = 667 (  ">="      PGUID 0 b t f  25  25  16 665 664  0 0 text_ge intltsel intltjoinsel ));

DATA(insert OID = 670 (  "="       PGUID 0 b t f  701  701  16 670 671  0 0 float8eq eqsel eqjoinsel ));
DATA(insert OID = 671 (  "!="      PGUID 0 b t f  701  701  16 671 670  0 0 float8ne eqsel eqjoinsel ));
DATA(insert OID = 672 (  "<"       PGUID 0 b t f  701  701  16 674 675  0 0 float8lt intltsel intltjoinsel ));
DATA(insert OID = 673 (  "<="      PGUID 0 b t f  701  701  16 675 674  0 0 float8le intltsel intltjoinsel ));
DATA(insert OID = 674 (  ">"       PGUID 0 b t f  701  701  16 672 673  0 0 float8gt intltsel intltjoinsel ));
DATA(insert OID = 675 (  ">="      PGUID 0 b t f  701  701  16 673 672  0 0 float8ge intltsel intltjoinsel ));

DATA(insert OID = 676 (  "<"       PGUID 0 b t f  911  911  16 680 679  0 0 oidchar16lt intltsel intltjoinsel ));
DATA(insert OID = 677 (  "<="      PGUID 0 b t f  911  911  16 679 680  0 0 oidchar16le intltsel intltjoinsel ));
DATA(insert OID = 678 (  "="       PGUID 0 b t f  911  911  16 678 681  0 0 oidchar16eq intltsel intltjoinsel ));
DATA(insert OID = 679 (  ">="      PGUID 0 b t f  911  911  16 677 676  0 0 oidchar16ge intltsel intltjoinsel ));
DATA(insert OID = 680 (  ">"       PGUID 0 b t f  911  911  16 676 677  0 0 oidchar16gt intltsel intltjoinsel ));
DATA(insert OID = 681 (  "!="      PGUID 0 b t f  911  911  16 681 678  0 0 oidchar16ne intltsel intltjoinsel ));

DATA(insert OID = 830 (  "<"       PGUID 0 b t f  810  810  16 834 833  0 0 oidint2lt intltsel intltjoinsel ));
DATA(insert OID = 831 (  "<="      PGUID 0 b t f  810  810  16 833 834  0 0 oidint2le intltsel intltjoinsel ));
DATA(insert OID = 832 (  "="       PGUID 0 b t f  810  810  16 832 835  0 0 oidint2eq intltsel intltjoinsel ));
DATA(insert OID = 833 (  ">="      PGUID 0 b t f  810  810  16 831 830  0 0 oidint2ge intltsel intltjoinsel ));
DATA(insert OID = 834 (  ">"       PGUID 0 b t f  810  810  16 830 831  0 0 oidint2gt intltsel intltjoinsel ));
DATA(insert OID = 835 (  "!="      PGUID 0 b t f  810  810  16 835 832  0 0 oidint2ne intltsel intltjoinsel ));

DATA(insert OID = 930 (  "<"       PGUID 0 b t f  910  910  16 934 933  0 0 oidint4lt intltsel intltjoinsel ));
DATA(insert OID = 931 (  "<="      PGUID 0 b t f  910  910  16 933 934  0 0 oidint4le intltsel intltjoinsel ));
DATA(insert OID = 932 (  "="       PGUID 0 b t f  910  910  16 932 935  0 0 oidint4eq intltsel intltjoinsel ));
DATA(insert OID = 933 (  ">="      PGUID 0 b t f  910  910  16 931 930  0 0 oidint4ge intltsel intltjoinsel ));
DATA(insert OID = 934 (  ">"       PGUID 0 b t f  910  910  16 930 931  0 0 oidint4gt intltsel intltjoinsel ));
DATA(insert OID = 935 (  "!="      PGUID 0 b t f  910  910  16 935 932  0 0 oidint4ne intltsel intltjoinsel ));

DATA(insert OID =   0 (  "+"       PGUID 0 b t f 1034 1033 1034 0 0 0 0 aclinsert   intltsel intltjoinsel ));
DATA(insert OID =   0 (  "-"       PGUID 0 b t f 1034 1033 1034 0 0 0 0 aclremove   intltsel intltjoinsel ));
DATA(insert OID =   0 (  "~"       PGUID 0 b t f 1034 1033   16 0 0 0 0 aclcontains intltsel intltjoinsel ));

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
