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

/* ----------------
 * executor debugging definitions are kept in a separate file
 * so people can customize what debugging they want to see and not
 * have this information clobbered every time a new version of
 * executor.h is checked in -cim 10/26/89
 * ----------------
 */
#include "execdebug.h"

#include "skey.h"
#include "anum.h"
#include "align.h"
#include "buf.h"
#include "cat.h"
#include "catname.h"
#include "clib.h"
#include "executor/execdefs.h"
#include "executor/tuptable.h"
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

#include "printtup.h"
#include "primnodes.h"
#include "plannodes.h"
#include "execnodes.h"
#include "parsetree.h"

#include "prs2.h"
#include "prs2stub.h"

/* ----------------
 * .h files made from running pgdecs over generated .c files in obj/lib/C
 * ----------------
 */
#include "primnodes.a.h"
#include "plannodes.a.h"
#include "execnodes.a.h"

/* ----------------------------------------------------------------
 *	executor shared memory segment header definition
 *
 *	After the executor's shared memory segment is allocated
 *	the initial bytes are initialized to contain 2 pointers.
 *	the first pointer points to the "low" end of the available
 *	area and the second points to the "high" end.
 *
 *	When part of the shared memory is reserved by a call to
 *	ExecSMAlloc(), the "low" pointer is advanced to point
 *	beyond the newly allocated area to the available free
 *	area.  If an allocation would cause "low" to exceed "high",
 *	then the allocation fails and an error occurrs.
 *
 *	Note:  Since no heap management is done, there is no
 *	provision for freeing shared memory allocated via ExecSMAlloc()
 *	other than wipeing out the entire space with ExecSMClean().
 * ----------------------------------------------------------------
 */

typedef struct ExecSMHeaderData {
    Pointer	low;		/* pointer to low water mark */
    Pointer	high;		/* pointer to high water mark */
} ExecSMHeaderData;

typedef ExecSMHeaderData *ExecSMHeader;

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
 *     #define macros
 * ----------------------------------------------------------------
 */

/* ----------------
 *	the reverse alignment macros are for use with the executor
 *	shared memory allocator's ExecSMHighAlloc routine which
 *	allocates memory from the high-area "down" to the low-area.
 *	In this case we need alignment in the "other" direction than
 *	provided by SHORTALIGN and LONGALIGN
 * ----------------
 */
#define	REVERSESHORTALIGN(LEN)\
    (((long)(LEN)) & ~01)

#ifndef	sun
#define	REVERSELONGALIGN(LEN)\
    (((long)(LEN)) & ~03)
#else
#define	REVERSELONGALIGN(LEN)	REVERSESHORTALIGN(LEN)
#endif

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
    CAR(CDR(CDR(CDR(CDR(CDR((parse_tree_root)))))))


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

/* ----------------
 *	DebugVariable is a structure holding a string
 *	specifying the name of a debugging variable and
 *	a pointer specifying the address of the object
 *	holding the variable's value.  This is used only
 *	in debug.c
 * ----------------
 */
typedef struct DebugVariable {
    String	debug_variable;
    Pointer	debug_address;
} DebugVariable;
 
typedef DebugVariable *DebugVariablePtr;
 

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

extern Pointer		get_cs_ResultTupleSlot();
extern Pointer		get_css_ScanTupleSlot();

extern bool		ExecIsInitialized;
extern Const		ConstTrue;
extern Const		ConstFalse;

extern SLList		ExecAllocDebugList;

extern int		_debug_hook_id_;
extern HookNode		_debug_hooks_[];

/* 
 *	public executor functions
 *
 *	XXX reorganize me.
 */
#include "executor/x_append.h"
#include "executor/x_debug.h"
#include "executor/x_endnode.h"
#include "executor/x_eutils.h"
#include "executor/x_execam.h"
#include "executor/x_execfmgr.h"
#include "executor/x_execinit.h"
#include "executor/x_execmain.h"
#include "executor/x_execmmgr.h"
#include "executor/x_execstats.h"
#include "executor/x_exist.h"
#include "executor/hashjoin.h"
#include "executor/x_indexscan.h"
#include "executor/x_initnode.h"
#include "executor/x_main.h"
#include "executor/x_material.h"
#include "executor/x_mergejoin.h"
#include "executor/x_nestloop.h"
#include "executor/x_parallel.h"
#include "executor/x_procnode.h"
#include "executor/x_qual.h"
#include "executor/recursive.h"
#include "executor/x_result.h"
#include "executor/x_scan.h"
#include "executor/x_seqscan.h"
#include "executor/x_sort.h"
#include "executor/x_tuples.h"
#include "executor/x_unique.h"
#include "executor/x_xdebug.h"

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecutorIncluded
