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

#include "anum.h"
#include "buf.h"
#include "catalog.h"
#include "catname.h"
#include "itup.h"
#include "log.h"
#include "parse.h"
#include "pmod.h"
#include "rel.h"
#include "strings.h"
#include "syscache.h"
#include "tqual.h"
#include "tuple.h"

#include "printtup.h"
#include "primnodes.h"
#include "plannodes.h"
#include "execnodes.h"
#include "parsetree.h"

/*
 * .h files made from running pgdecs over generated .c files in obj/lib/C
 */
#include "primnodes.a.h"
#include "plannodes.a.h"
#include "execnodes.a.h"

/* ----------------------------------------------------------------
 *     constant #defines
 * ----------------------------------------------------------------
 */

/*
 *     memory management defines
 */

#define M_STATIC 		0 		/* Static allocation mode */
#define M_DYNAMIC 		1 		/* Dynamic allocation mode */

#define EXEC_FRWD		1		/* Scan forward */
#define EXEC_BKWD		-1		/* Scan backward */

#define ALL_TUPLES		0		/* return all tuples */
#define ONE_TUPLE		1		/* return only one tuple */

/*
 *    boolean value returned by C routines
 */

#define EXEC_C_TRUE 		1	/* C language boolean truth constant */
#define EXEC_C_FALSE		0      	/* C language boolean false constant */

/*
 *     attribute constants and definitions
 */

#ifdef vax
#define EXEC_MAXATTR		1584
#endif
#ifndef vax
#define EXEC_MAXATTR		1600		/* Max # attributes */
#endif

#define EXEC_T_CTID		-1
#define EXEC_I_TID		-3
#define EXEC_T_LOCK		-2

/*
 *	constants used by ExecMain
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

#ifdef NOTYET
/*
 *	index scan constants
 *
 * 	Offset definitions to access fields in 'states' of index nodes.
 */

#define EXEC_INDEXSCAN_IndexPtr 	0
#define EXEC_INDEXSCAN_RuleFlag 	1
#define EXEC_INDEXSCAN_RuleDesc 	2

/*
 *	seqscan constants
 *
 *    	The folowing offsets are used to access state information from the
 *    	'states' field of the seqscan nodes
 */

#define EXEC_SEQSCAN_RuleFlag 	0
#define EXEC_SEQSCAN_RuleDesc 	1

/*
 *	rule lock constants
 */

#define EXEC_EARLY 	0
#define EXEC_LATE 	1
#define EXEC_REFUSE 	2
#define EXEC_ALWAYS 	3

/*
 *      Constants taken from RuleLock.h
 */

#define RuleLockTypeWrite	0		/* Rule lock Write 0 */
#define RuleLockTypeRead1	1		/* Rule lock Write 1 */
#define RuleLockTypeRead2	2		/* Rule Lock Read 2 */
#define RuleLockTypeRead3	3		/* Rule Lock Read 3 */

/*
 *      RuleLockIntermediate.ruletype is one of the following
 */

#define RuleTypeRetrieve	0		/* Retrieve rule */
#define RuleTypeReplace		1		/* Replace rule */
#define RuleTypeAppend		2		/* Append rule */
#define RuleTypeDelete		3		/* Delete rule */
#define RuleTypeRefuseRetrieve	4		/* Refuse retrieve rule */
#define RuleTypeRefuseReplace	5		/* Refuse replace rule */
#define RuleTypeRefuseAppend	6		/* Refuse append rule */
#define RuleTypeRefuseDelete	7		/* Refuse delete rule */

#endif NOTYET

/* ----------------------------------------------------------------
 *     #defines
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

/* ----------------
 *	query feature accessors
 *
 *	BEWARE: the query "feature" can contain different sorts
 *		of things depending on the operation.  In particular,
 *		in the case of "retrieve into", it is a list
 *
 *			('result intoRelationDesc)
 *
 *		where 'result == lispInteger(EXEC_RESULT) and
 *		      intoRelationDesc == lispInteger(relation descriptor)
 *
 *		It can also contain other stuff..
 *
 *	this is a real crappy interface.. we should fix it soon -cim 8/29/89
 * ----------------
 */
#define QdGetCommand(queryDesc)	    	CAR(QdGetFeature(queryDesc))
#define QdGetCount(queryDesc)  	    	CAR(CDR(QdGetFeature(queryDesc)))

#define QdGetIntoRelation(queryDesc) \
    ((Relation) CInteger(CAR(CDR(QdGetFeature(queryDesc)))))

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
#define ExecIsNestLoop(node)	IsA(node,NestLoop)
#define ExecIsMergeJoin(node)	IsA(node,MergeSort)
#define ExecIsHashJoin(node)	IsA(node,HashJoin)
#define ExecIsSeqScan(node)	IsA(node,SeqScan)
#define ExecIsIndexScan(node)	IsA(node,IndexScan)
#define ExecIsSort(node)	IsA(node,Sort)
#define ExecIsHash(node)	IsA(node,Hash)


#ifdef NOTYET
/* ----------------
 *   	index scan accessor macros
 *
 * 	IGetRelID(node)		    get the id of the relation to be scanned
 *	IGetRelDesc(node)	    get the relation descriptor from relation id
 *	IGetScanDesc(node)	    get the scan descriptor from relation id
 *	IGetQual(node)		    get the qualification clause
 *	IGetTarget(node)	    get the target list
 *	IGetRuleFlag(node)	    get the flag indicating rule activation
 *	ISetRuleFlag(node, value)   set the flag indicating rule activation
 *	IGetRuleDesc(node)	    get rule descriptor
 *	ISetRuleDesc(node, value)   set rule descriptor
 *	IGetIndices(node)	    get indices
 *	IGetIndexPtr(node)	    get index pointer
 *	ISetIndexPtr(node, value)   set index pointer
 *	IGetIndexQual(node)	    get index qualifications
 * ----------------
 */

#define IGetRelID(node)		get_scanrelid(node)
#define IGetRelDesc(node) 	ExecGetRelDesc(IGetRelID(node))
#define IGetScanDesc(node) 	ExecGetScanDesc(IGetRelID(node))
#define IGetQual(node)		get_qpqual(node)
#define IGetTarget(node)	get_qptargetlist(node)

#define IGetRuleFlag(node) \
    ExecGetStates(EXEC_INDEXSCAN_RuleFlag, get_states(node))
     
#define ISetRuleFlag(node, value) \
    ExecSetStates(EXEC_INDEXSCAN_RuleFlag, get_states(node)), value)

#define IGetRuleDesc(node) \
    ExecGetStates(EXEC_INDEXSCAN_RuleDesc, get_states(node))

#define ISetRuleDesc(node, value) \
    ExecSetStates(EXEC_INDEXSCAN_RuleDesc, get_states(node), value)

#define IGetIndices(node)	get_indxid(node)

#define IGetIndexPtr(node) \
    ExecGetStates(EXEC_INDEXSCAN_IndexPtr, get_states(node))

#define ISetIndexPtr(node, value) \
    ExecSetStates(EXEC_INDEXSCAN_IndexPtr, get_states(node), value)

#define IGetIndexQual(node) \
    getindxqual(node)

/*
 * XXX automatic conversion failed on this one but we might not need it
 * -cim 8/4/89
 *
 * get the scan descriptor from index id = (relDesc, scanDesc)
 *
 * #define IGetIScanDesc,indexID (),ExecGetScanDesc (indexID)
 */
#endif NOTYET


#ifdef NOTYET
/* ----------------
 *    Merge Join Macros
 * ----------------
 */

#define MJGetOuterTuple(node)		GetMJOuterTuple(get_state(node))
#define MJSetOuterTuple(node,value)	setf(MJGetOuterTuple(node),value)

#define MJGetTupType(node)		GetMJTupType(get_state(node))
#define MJSetTupType(node,value)	setf(MJGetTupType(node),value)

#define MJGetTupValue(node)		GetMJTupValue(get_state(node))
#define MJSetTupValue(node,value)	setf(MJGetTupValue(node),value)

#define MJGetBufferPage(node)		GetMJBufferPage(get_state(node))
#define MJSetBufferPage(node,value)	setf(MJGetBufferPage(node),value)

#define MJGetOSortopI(node)		GetMJOSortopI(get_state(node))
#define MJSetOSortopI(node,value)	setf(MJGetOSortopI(node),value)

#define MJGetISortopO(node)		GetMJISortopO(get_state(node))
#define MJSetISortopO(node,value)	setf(MJGetISortopO(node),value)

#define MJGetMarkFlag(node)		GetMJMarkFlag(get_state(node))
#define MJSetMarkFlag(node,value)	setf(MJGetMarkFlag(node),value)

#define MJGetFrwdMarkPos(node)		GetMJFrwdMarkPos(get_state(node))
#define MJSetFrwdMarkPos(node,value)	setf(MJGetFrwdMarkPos(node),value)

#define MJGetBkwdMarkPos(node)		GetMJBkwdMarkPos(get_state(node))
#define MJSetBkwdMarkPos(node,value)    setf(MJGetBkwdMarkPos(node),value)

#define MJGetMarkPos(node) \
    (execDirection == EXEC_FRWD ? \
     MJGetFrwdMarkPos(node) : MJGetBkwdMarkPos(node))

#define MJSetMarkPos(node,scanPos) \
    (execDirection == EXEC_FRWD  ? \
     MJSetFrwdMarkPos(node,scanPos) : MJSetBkwdMarkPos(node,scanPos))
#endif NOTYET




#ifdef NOTYET
/* ----------------
 *	sort node defines
 * ----------------
 */

#define SortGetSortedFlag(node)		GetSortFlag(get_state(node))
#define SortSetSortedFlag(node,value)	setf(SortGetSortedFlag (node),value)
#define SortGetSortKeys(node)		GetSortKeys(get_state(node))
#define SortSetSortKeys(node,value)	setf(SortGetSortKeys(node),value)
#endif NOTYET




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

extern GeneralRetrieveIndexResult AMgettuple();
extern HeapTuple		  RelationGetHeapTupleByItemPointer();

extern Const		ConstTrue;
extern Const		ConstFalse;

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
#include "executor/mergejoin.h"
#include "executor/nestloop.h"
#include "executor/procnode.h"
#include "executor/qual.h"
#include "executor/result.h"
#include "executor/rulelocks.h"
#include "executor/rulemgr.h"
#include "executor/scan.h"
#include "executor/seqscan.h"
#include "executor/sort.h"
#include "executor/tuples.h"

/* ----------------------------------------------------------------
 *	shit whose time is to die
 *	(but we keep it around for reference purposes)
 * ----------------------------------------------------------------
 */

#ifdef TIMETODIE

/* ----------------
 *	append node macros
 * ----------------
 */

#define append_nplans_is_valid(nplans) \
    (nplans > 0)

#define append_whichplan_is_valid(whichplan, nplans) \
    (whichplan >= 0 && whichplan < nplans)

#define append_initialized_is_valid(initialized) \
    (lispVectorp(initialized))

#define append_plans_is_valid(plans) \
    (lispAtomp(plans))

#define append_rtentries_is_valid(rtentries) \
    (lispAtomp(rtentries))

/* ----------------
 *    nest loop macros
 * ----------------
 */

#define NLGetOuterTuple(node)		GetNLOuterTuple(get_state(node))
#define NLSetOuterTuple(node,value) \
    setf(NLGetOuterTuple(node), ppreserve (value))
     
#define NLGetPortalFlag(node)		GetNLPortalFlag(get_state(node))
#define NLSetPortalFlag(node,value)	setf(NLGetPortalFlag(node), value)

#define NLGetTupType(node)		GetNLTupType(get_state(node))
#define NLSetTupType(node,value)	setf(NLGetTupType(node), value)

#define NLGetTupValue(node)		GetNLTupValue(get_state(node))
#define NLSetTupValue(node,value)	setf(NLGetTupValue(node), value)

/* ----------------
 *	scan/relation descriptor accessors
 * ----------------
 */

#define SGetScanDesc(node) \
    ExecGetScanDesc(get_scanrelid(node))
#define SSetScanDesc(node, scanDesc) \
    ExecSetScanDesc(get_scanrelid(node), scanDesc)

#define SGetRelDesc(node) \
    ExecGetRelDesc(get_scanrelid(node))
#define SSetRelDesc(node,relDesc) \
    ExecSetRelDesc(get_scanrelid(node), relDesc)

/* ----------------
 *	Get/Set scan/relation descriptor from index id
 * ----------------
 */

#define SGetIScanDesc(indexID)		 ExecGetScanDesc(indexID)
#define SSetIScanDesc(indexID,indexDesc) ExecSetScanDesc(indexID, indexDesc)
#define SGetIRelDesc(indexID)		 ExecGetRelDesc(indexID)
#define SSetIRelDesc(indexID)		 ExecSetRelDesc(indexID)

/* ----------------
 *	access state information of SCAN nodes
 * ----------------
 */

#define SGetBufferPage(node)		GetScanBufferPage(get_state(node))
#define SSetBufferPage(node,value)	setf(SGetBufferPage(node),value)
#define SGetTupType(node)		GetScanTupType(get_state(node))
#define SSetTupType(node,value)		setf(SGetTupType(node),value)
#define SGetTupValue(node)		GetScanTupValue(get_state(node))
#define SSetTupValue(node,value)	setf(SGetTupValue(node),value)
#define SGetOldRelId(node)		GetScanOldRelId(get_state(node))
#define SSetOldRelId(node,value)	setf(SGetOldRelId(node),value)

/*
 *	this is the attr types for tuples being scanned, not those returned
 *	according to the target list
 */
#define SGetScanType(node) \
    ExecGetTypeInfo(ExecGetRelDesc(get_scanrelid(node)))
     
#define SGetRuleFlag(node)		GetScanRuleFlag(get_state(node))
#define SSetRuleFlag(node,value)	setf(SGetRuleFlag(node),value)
#define SGetRuleDesc(node)		GetScanRuleDesc(get_state (node))
#define SSetRuleDesc(node,value)	setf(SGetRuleDesc(node),value)
#define SGetProcLeftFlag(node)		GetScanProcLeftFlag(get_state (node))
#define SSetProcLeftFlag(node,value)	setf(SGetProcLeftFlag(node),value)
#define SGetIndexPtr(node)		GetScanIndexPtr(get_state (node))
#define SSetIndexPtr(node,value)	setf(SGetIndexPtr(node),value)
#define SGetSkeys(node)			GetScanSkeys(get_state (node))
#define SSetSkeys(node,value)		setf(SGetSkeys(node),value)
#define SGetSkeysCount(node)		GetScanSkeysCount(get_state (node))
#define SSetSkeysCount(node,value)	setf(SGetSkeysCount(node),value)

/* ----------------
 *	get/set index id pointed by the index ptr
 * ----------------
 */

#define SGetIndexID(indexPtr,node) \
    nth(indexPtr, get_indxid(node))
     
#define SSetIndexID(indexPtr,node,value) \
    setf(SGetIndexPtr(indexPtr, get_indxid(node)), value)

/* ----------------
 *   	sequential scan node accessors
 * ----------------
 */

#define SGetRelID(node)		get_scanrelid(node)
#define SGetQual(node)		get_qpqual(node)
#define SGetTarget(node)	get_qptargetlist(node)

/* ----------------
 *	result node accessors
 * ----------------
 */

#define ResGetLoop(node) 		get_Loop(get_resstate(node))
#define ResSetLoop(node,value)		set_Loop(get_resstate(node), value)
#define ResGetLeftTuple(node)		get_LeftTuple(get_resstate(node))
#define ResSetLeftTuple(node,value)	set_LeftTuple(get_resstate(node), value)
#define ResGetTupType(node) 		get_TupType(get_resstate(node))
#define ResSetTupType(node,value) 	set_TupType(get_resstate(node), value)
#define ResGetTupValue(node) 		get_TupValue(get_resstate(node))
#define ResSetTupValue(node,value)	set_TupValue(get_resstate(node), value)
#define ResGetLevel(node)		get_Level(get_resstate(node))
#define ResSetLevel(node,value)		set_Level(get_resstate(node), value)

/* ----------------
 *    Retrieve/Construct the components in relation ID.
 *    Relation ID which is the relation name
 *	 is changed into (RelDesc ScanDesc)
 *    after initialization.
 * ----------------
 */
#define ExecMakeRelID(relDesc,scanDesc)	\
    lispList(relDesc,scanDesc)

#define ExecGetRelDesc(relID) \
    (CAR(relID))

#define ExecSetRelDesc(relID,relDesc) \
   (ExecGetRelDesc(relID) = (relDesc)))

#define ExecGetScanDesc(relID) \
   (CAR(CDR(relID)))

#define ExecSetScanDesc(relID,relDesc) \
   (ExecGetScanDesc(relID) = (relDesc))

/* ----------------
 *     Things for tuple tid.
 * 
 * (initializtion is in in executor.c as follows)
 * extern List _qual_tuple_tid_		= 0;
 * extern List _qual_tuple_		= LispNil;
 * extern List _result_relation_		= LispNil;
 * extern List _resdesc_			= LispNil;
 * ----------------
 */

extern int 		_qual_tuple_tid_;
extern List 	_qual_tuple_;
extern Relation 	_result_relation_;
extern List 	_resdesc_;

/* ----------------
 *     Global variables
 *    
 *     TODO
 *    	Put into root node to allow concurrent executors
 *	
 * initializtion is in in executor.c as follows:
 *
 * extern List execDirection		= EXEC_FRWD;
 * extern List execTime			= 0;
 * extern List execOwner		= 0;
 * extern List execSPlanInfo		= LispNil;
 * extern List execErrorMssg		= LispNil;		
 * extern List execRangeTbl		= LispNil;
 * extern List _relation_relation_	= LispNil;
 * ----------------
 */

extern int 		execDirection;
extern int 		execTime;
extern int 		execOwner;
extern char 		*execErrorMssg;
extern List 		execSPlanInfo;
extern List 		execRangeTbl;
extern Relation 	_relation_relation_;  

/* ----------------
 *    Variables global to all concurrent executors 
 * ----------------
 *
 * Queue of rule plans that want to be run as a Now time qual transacation
 * (initializtion is in in executor.c as follows)
 *
 *  	extern List _rule_now_plans_queue_	 =  LispNil;
 *
 * Queue of rule plans that want to be run as a Self Time qual transaction
 *
 *  	extern List _rule_self_plans_queue_ =  LispNil;
 */

#endif TIMETODIE

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecutorIncluded
