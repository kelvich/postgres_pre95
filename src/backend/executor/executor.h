/* ----------------------------------------------------------------
 *      FILE
 *     	executor.h
 *     
 *      DESCRIPTION
 *     	support for the POSTGRES executor module
 *
 *	$Header$"
 * ----------------------------------------------------------------
 */

#ifndef ExecutorIncluded
#define ExecutorIncluded

/* ----------------------------------------------------------------
 *     #includes
 * ----------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>

#include "c.h"
#include "pg_lisp.h"

#include "access.h"
#include "anum.h"
#include "buf.h"
#include "catalog.h"
#include "catname.h"
#include "clib.h"
#include "fmgr.h"
#include "ftup.h"
#include "heapam.h"
#include "htup.h"
#include "itup.h"
#include "log.h"
#include "mcxt.h"
#include "parse.h"
#include "pmod.h"
#include "recursion_a.h"
#include "recursion.h"
#include "rel.h"
#include "rproc.h"
#include "simplelists.h"
#include "strings.h"
#include "syscache.h"
#include "tqual.h"
#include "tuple.h"

#include "printtup.h"
#include "primnodes.h"
#include "plannodes.h"
#include "execnodes.h"
#include "parsetree.h"

/* ----------------
 * executor debugging definitions are kept in a separate file
 * so people can customize what debugging they want to see and not
 * have this information clobbered every time a new version of
 * executor.h is checked in -cim 10/26/89
 * ----------------
 */
#include "execdebug.h"

/* ----------------
 * .h files made from running pgdecs over generated .c files in obj/lib/C
 * ----------------
 */
#include "primnodes.a.h"
#include "plannodes.a.h"
#include "execnodes.a.h"



/* ----------------------------------------------------------------
 * 	parallel fragment definition
 *
 *	The parallel optimiser returns a FragmentInfo structure
 *	which contains an array of fragment structures which
 *	the executor executes in parallel.  The Fragment structure
 *	contains a pair of pointers - one to a current node in
 *	the query plan and one to a replacement parallel node.
 *
 *	ExecuteFragements() executes the fragment plans and replaces
 *	the corresponding nodes in the query plan with materialized
 *	results.
 * ----------------------------------------------------------------
 */
typedef struct FragmentData {
    Plan	current;	/* pointer to node in query plan */
    Plan	replacement;	/* pointer to parallel replacement node */
} FragmentData;

typedef FragmentData *Fragment;

typedef struct FragmentInfo {
    Fragment	fragments;	/* fragments to execute in parallel */
    int		numFragments;	/* size of fragements array */
} FragmentInfo;



/* ----------------------------------------------------------------
 *     constant #defines
 * ----------------------------------------------------------------
 */

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
#define EXEC_DUMP			3
#define EXEC_FOR 			4
#define EXEC_BACK			5
#define EXEC_FDEBUG  			6
#define EXEC_BDEBUG  			7
#define EXEC_DEBUG   			8
#define EXEC_RETONE  			9
#define EXEC_RESULT  			10

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

/* ----------------------------------------------------------------
 *     #define macros
 * ----------------------------------------------------------------
 */

/* ----------------
 *	miscellany
 * ----------------
 */

#define ExecCNull(Cptr)		(Cptr == NULL)
#define ExecCTrue(bval)		(bval != 0)
#define ExecCFalse(bval)	(bval == 0)

#define ExecCNot(bval) \
    (ExecCFalse(bval) ? EXEC_C_TRUE : EXEC_C_FALSE)

#define ExecRevDir(direction)	(direction - 1) 

/* ----------------
 *	query descriptor accessors
 * ----------------
 */
#define GetOperation(queryDesc)	     (List) CAR(queryDesc)
#define QdGetParseTree(queryDesc)    (List) CAR(CDR(queryDesc))
#define QdGetPlan(queryDesc)	     (Plan) CAR(CDR(CDR(queryDesc)))
#define QdGetState(queryDesc)	   (EState) CAR(CDR(CDR(CDR(queryDesc))))
#define QdGetFeature(queryDesc)      (List) CAR(CDR(CDR(CDR(CDR(queryDesc)))))

#define QdSetState(queryDesc, s) \
    (CAR(CDR(CDR(CDR(queryDesc)))) = (List) s)

#define QdSetFeature(queryDesc, f) \
    (CAR(CDR(CDR(CDR(CDR(queryDesc))))) = (List) f)

/* ----------------
 *	query feature accessors
 * ----------------
 */
#define FeatureGetCommand(feature)	CAR(feature)
#define FeatureGetCount(feature)	CAR(CDR(feature))

#define QdGetCommand(queryDesc)	FeatureGetCommand(QdGetFeature(queryDesc))
#define QdGetCount(queryDesc)	FeatureGetCount(QdGetFeature(queryDesc))

/* ----------------
 *	parse tree accessors
 * ----------------
 */
#define parse_tree_root(parse_tree) \
    CAR(parse_tree)

#define parse_tree_target_list(parse_tree) \
    CAR(CDR(parse_tree))

#define parse_tree_qualification(parse_tree) \
    CAR(CDR(CDR(parse_tree)))

#define parse_tree_root_num_levels(parse_tree_root) \
    CAR(parse_tree_root)

#define parse_tree_root_command_type(parse_tree_root) \
    CAR(CDR(parse_tree_root))

#define parse_tree_root_result_relation(parse_tree_root) \
    CAR(CDR(CDR(parse_tree_root)))

#define parse_tree_root_range_table(parse_tree_root) \
    CAR(CDR(CDR(CDR(parse_tree_root))))

#define parse_tree_root_priority(parse_tree_root) \
    CAR(CDR(CDR(CDR(CDR(parse_tree_root)))))

#define parse_tree_root_rule_info(parse_tree_root) \
    CAR(CDR(CDR(CDR(CDRCDR((parse_tree_root))))))


#define parse_tree_range_table(parse_tree)	\
    parse_tree_root_range_table(parse_tree_root(parse_tree))

#define parse_tree_result_relation(parse_tree) \
    parse_tree_root_result_relation(parse_tree_root(parse_tree))

/* ----------------
 * 	macros to determine node types
 * ----------------
 */

#define ExecIsRoot(node) 	LispNil
#define ExecIsResult(node)	IsA(node,Result)
#define ExecIsExistential(node)	IsA(node,Existential)
#define ExecIsAppend(node)	IsA(node,Append)
#define ExecIsParallel(node)	IsA(node,Parallel)
#define ExecIsRecursive(node)	IsA(node,Recursive)
#define ExecIsNestLoop(node)	IsA(node,NestLoop)
#define ExecIsMergeJoin(node)	IsA(node,MergeJoin)
#define ExecIsHashJoin(node)	IsA(node,HashJoin)
#define ExecIsSeqScan(node)	IsA(node,SeqScan)
#define ExecIsIndexScan(node)	IsA(node,IndexScan)
#define ExecIsMaterial(node)	IsA(node,Material)
#define ExecIsSort(node)	IsA(node,Sort)
#define ExecIsUnique(node)	IsA(node,Unique)
#define ExecIsHash(node)	IsA(node,Hash)

/* ----------------------------------------------------------------
 *	debugging structures
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecAllocDebugData is used by ExecAllocDebug() and ExecFreeDebug()
 *	to keep track of memory allocation.
 * ----------------
 */
typedef struct ExecAllocDebugData {
    Pointer	pointer;
    Size	size;
    String	routine;
    String	file;
    int		line;
    SLNode 	Link;
} ExecAllocDebugData;

/* ----------------------------------------------------------------
 *	externs
 * ----------------------------------------------------------------
 */

/* 
 *	miscellany
 */

extern Relation		amopen();
extern Relation		AMopen();
extern Relation		amopenr();
extern HeapScanDesc 	ambeginscan();
extern HeapTuple 	amgetnext();
extern char *		amgetattr();
extern RuleLock		aminsert();
extern RuleLock		amdelete();
extern RuleLock		amreplace();

extern GeneralInsertIndexResult   AMinsert();
extern GeneralRetrieveIndexResult AMgettuple();
extern HeapTuple		  RelationGetHeapTupleByItemPointer();

extern Const		ConstTrue;
extern Const		ConstFalse;

extern SLList		ExecAllocDebugList;

extern int		_debug_hook_id_;
extern HookNode		_debug_hooks_[];

/* 
 *	public executor functions
 */

#include "executor/append.h"
#include "executor/endnode.h"
#include "executor/eutils.h"
#include "executor/exist.h"
#include "executor/indexscan.h"
#include "executor/initnode.h"
#include "executor/main.h"
#include "executor/material.h"
#include "executor/mergejoin.h"
#include "executor/nestloop.h"
#include "executor/parallel.h"
#include "executor/procnode.h"
#include "executor/qual.h"
#include "executor/recursive.h"
#include "executor/result.h"
#include "executor/scan.h"
#include "executor/seqscan.h"
#include "executor/sort.h"
#include "executor/tuples.h"
#include "executor/unique.h"

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecutorIncluded
