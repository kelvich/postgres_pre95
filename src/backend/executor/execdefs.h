/* ----------------------------------------------------------------
 *   FILE
 *	execdefs.h
 *	
 *   DESCRIPTION
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ExecdefsIncluded
#define ExecdefsIncluded

/* ----------------
 *     memory management defines
 * ----------------
 */
#define M_STATIC 		0 		/* Static allocation mode */
#define M_DYNAMIC 		1 		/* Dynamic allocation mode */

/* ----------------
 *	executor scan direction definitions
 * ----------------
 */
#define EXEC_FRWD		1		/* Scan forward */
#define EXEC_BKWD		-1		/* Scan backward */

/* ----------------
 *	ExecutePlan() tuplecount definitions
 * ----------------
 */
#define ALL_TUPLES		0		/* return all tuples */
#define ONE_TUPLE		1		/* return only one tuple */

/* ----------------
 *    boolean value returned by C routines
 * ----------------
 */
#define EXEC_C_TRUE 		1	/* C language boolean truth constant */
#define EXEC_C_FALSE		0      	/* C language boolean false constant */

/* ----------------
 *	constants used by ExecMain
 * ----------------
 */
#define EXEC_START 			1
#define EXEC_END 			2
#define EXEC_RUN		        3
#define EXEC_FOR 			4
#define EXEC_BACK			5
#define EXEC_RETONE  			6
#define EXEC_RESULT  			7

#define EXEC_INSERT_RULE 		11
#define EXEC_DELETE_RULE 		12
#define EXEC_DELETE_REINSERT_RULE 	13
#define EXEC_MAKE_RULE_EARLY 		14
#define EXEC_MAKE_RULE_LATE 		15
#define EXEC_INSERT_CHECK_RULE 		16
#define EXEC_DELETE_CHECK_RULE 		17

/* ----------------
 *	Merge Join states
 * ----------------
 */
#define EXEC_MJ_INITIALIZE		1
#define EXEC_MJ_JOINMARK		2
#define EXEC_MJ_JOINTEST		3
#define EXEC_MJ_JOINTUPLES		4
#define EXEC_MJ_NEXTOUTER		5
#define EXEC_MJ_TESTOUTER		6
#define EXEC_MJ_NEXTINNER		7
#define EXEC_MJ_SKIPINNER		8
#define EXEC_MJ_SKIPOUTER		9

#endif ExecdefsIncluded
