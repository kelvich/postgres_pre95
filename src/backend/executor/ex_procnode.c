/* ----------------------------------------------------------------
 *   FILE
 *	ex_procnode.c
 *	
 *   DESCRIPTION
 *	contains dispatch functions which call the appropriate
 *	"initialize", "get a tuple", and "cleanup" routines for the
 *	given node type.  If the node has children, then it will
 *	presumably call ExecInitNode, ExecProcNode, or ExecEndNode
 *	on it's subnodes and do the appropriate processing..
 *
 *   INTERFACE ROUTINES
 *	ExecInitNode	-	initialize a plan node and it's subplans
 *	ExecProcNode	-	get a tuple by executing the plan node
 *	ExecEndNode	-	shut down a plan node and it's subplans
 *
 *   NOTES
 *	This used to be three files.  It is now all combined into
 *	one file so that it is easier to keep ExecInitNode, ExecProcNode,
 *	and ExecEndNode in sync when new nodes are added.
 *	
 *   EXAMPLE
 *	suppose we want the age of the manager of the shoe department and
 *	the number of employees in that department.  so we have the query:
 *
 *		retrieve (DEPT.no_emps, EMP.age)
 *		where EMP.name = DEPT.mgr and
 *		      DEPT.name = "shoe"
 *	
 *	Suppose the planner gives us the following plan:
 *	
 *			Nest Loop (DEPT.mgr = EMP.name)
 *			/	\ 
 *		       /         \
 *		   Seq Scan	Seq Scan
 *		    DEPT	  EMP
 *		(name = "shoe")
 *	
 *	ExecMain() is eventually called three times.  The first time
 *	it is called it calls InitPlan() which calls ExecInitNode() on
 *	the root of the plan -- the nest loop node.
 *
 *    *	ExecInitNode() notices that it is looking at a nest loop and
 *	as the code below demonstrates, it calls ExecInitNestLoop().
 *	Eventually this calls ExecInitNode() on the right and left subplans
 *	and so forth until the entire plan is initialized.
 *	
 *    *	The second time ExecMain() is called, it calls ExecutePlan() which
 *	calls ExecProcNode() repeatedly on the top node of the plan.
 *	Each time this happens, ExecProcNode() will end up calling
 *	ExecNestLoop(), which calls ExecProcNode() on its subplans.
 *	Each of these subplans is a sequential scan so ExecSeqScan() is
 *	called.  The slots returned by ExecSeqScan() may contain
 *	tuples which contain the attributes ExecNestLoop() uses to
 *	form the tuples it returns.
 *
 *    *	Eventually ExecSeqScan() stops returning tuples and the nest
 *	loop join ends.  Lastly, ExecMain() calls ExecEndNode() which
 *	calls ExecEndNestLoop() which in turn calls ExecEndNode() on
 *	its subplans which result in ExecEndSeqScan().
 *
 *	This should show how the executor works by having
 *	ExecInitNode(), ExecProcNode() and ExecEndNode() dispatch
 *	their work to the appopriate node support routines which may
 *	in turn call these routines themselves on their subplans.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "executor/executor.h"
 RcsId("$Header$");

/* ----------------------------------------------------------------
 *	Hook processing macros.  these are used to simplify
 *	the node processing routines below.  For example,
 *
 *	    case classTag(Result):
 *		END_WITH_HOOKS(Result,resstate);
 *		break;
 *
 *	becomes
 *	----------------
 *	    case classTag(Result):
 *	        {
 *		    Pointer p = (Pointer) get_resstate(node);
 *		    DO_END_BEFORE_HOOK(p);
 *		    ExecEndResult((Result) node);
 *		    DO_END_AFTER_HOOK(p);
 *		}
 *		break;
 *
 *	which in turn expands to:
 *	----------------
 *	    case classTag(Result):
 *	        {
 *		    Pointer p = (Pointer) get_resstate(node);
 *		    hook = get_base_hook(p);
 *		    if (hook != NULL) {
 *		    	proc = get_hook_pre_endnode(hook);
 *			(*proc)(node, state);
 *		    }
 *		    ExecEndResult((Result) node);
 *		    if (hook != NULL) { 
 *			proc = get_hook_post_endnode(hook);
 *			(*proc)(node, state);
 *		    }
 *		}
 *		break;
 *
 *	Hook functions are currently only used for debugging and
 *	generating plan traces.  If EXEC_ASSIGNDEBUGHOOKS is not defined
 *	in H/executor/execdebug.h then the macros only expand to something
 *	like:
 *
 *	    case classTag(Result):
 *		ExecEndResult((Result) node);
 *		break;
 *	
 * ----------------------------------------------------------------
 */

/* ----------------
 *	do_hooks macros
 * ----------------
 */
#define DO_INIT_AFTER_HOOK(state) \
    hook = get_base_hook(state); \
    if (hook != NULL) { \
	proc = get_hook_post_initnode(hook); \
	(*proc)(node, state); \
    }

#define DO_PROC_BEFORE_HOOK(state) \
    hook = get_base_hook(state); \
    if (hook != NULL) { \
	proc = get_hook_pre_procnode(hook); \
	(*proc)(node, state); \
    }

#define DO_PROC_AFTER_HOOK(state) \
    if (hook != NULL) { \
	proc = get_hook_post_procnode(hook); \
	(*proc)(node, state); \
    }

#define DO_END_BEFORE_HOOK(state) \
    hook = get_base_hook(state); \
    if (hook != NULL) { \
	proc = get_hook_pre_endnode(hook); \
	(*proc)(node, state); \
    }

#define DO_END_AFTER_HOOK(state) \
    if (hook != NULL) { \
	proc = get_hook_post_endnode(hook); \
	(*proc)(node, state); \
    }

/* ----------------
 *	with_hooks macros
 * ----------------
 */
#ifdef EXEC_ASSIGNDEBUGHOOKS

#define INIT_WITH_HOOKS(n,s) \
{ \
    Pointer p; \
    result = (List) CppIdentity(ExecInit)n((n) node, estate, parent); \
    p = (Pointer) CppIdentity(get_)s(node); \
    DO_INIT_AFTER_HOOK(p); \
}

#define PROC_WITH_HOOKS(f,n,s) \
{ \
    Pointer p = (Pointer) CppIdentity(get_)s(node); \
    DO_PROC_BEFORE_HOOK(p); \
    result = (TupleTableSlot) f((n) node); \
    DO_PROC_AFTER_HOOK(p); \
}

#define END_WITH_HOOKS(n,s) \
{ \
    Pointer p = (Pointer) CppIdentity(get_)s(node); \
    DO_END_BEFORE_HOOK(p); \
    CppIdentity(ExecEnd)n((n) node); \
    DO_END_AFTER_HOOK(p); \
}

#else /*EXEC_ASSIGNDEBUGHOOKS*/

#define INIT_WITH_HOOKS(n,s) \
    result = (List) CppIdentity(ExecInit)n((n) node, estate, parent);

#define PROC_WITH_HOOKS(f,n,s) \
    result = (TupleTableSlot) f((n) node);

#define END_WITH_HOOKS(n,s) \
    CppIdentity(ExecEnd)n((n) node);
	
#endif /*EXEC_ASSIGNDEBUGHOOKS*/

/* ----------------------------------------------------------------
 *   	ExecInitNode
 *   
 *   	Recursively initializes all the nodes in the plan rooted
 *   	at 'node'. 
 *   
 *   	Initial States:
 *   	  'node' is the plan produced by the query planner
 * ----------------------------------------------------------------
 */
List
ExecInitNode(node, estate, parent)
    Plan 	node;
    EState 	estate;
    Plan 	parent;
{
    List	   	result;
    HookNode 		hook;
    HookFunction 	proc;

    /* ----------------
     *	do nothing when we get to the end
     *  of a leaf on tree.
     * ----------------
     */   
    if (node == NULL)
	return LispNil;
    
    switch(NodeType(node)) {
	/* ----------------
	 *	control nodes
	 * ----------------
	 */
    case classTag(Result):
/*	INIT_WITH_HOOKS( Result,resstate); type casting problem*/ 
	result = (List) ExecInitResult((Plan) node, estate, parent);	
	break;
	
    case classTag(Append):
	INIT_WITH_HOOKS(Append,unionstate);
	break;
              
	/* ----------------
	 *	scan nodes
	 * ----------------
	 */
    case classTag(SeqScan):
/*	INIT_WITH_HOOKS((Plan) SeqScan,scanstate); type casting problem */
	result = (List) ExecInitSeqScan((Plan) node, estate, parent);	
	break;

    case classTag(IndexScan):
	INIT_WITH_HOOKS(IndexScan,indxstate);
	break;
	
	/* ----------------
	 *	join nodes
	 * ----------------
	 */
    case classTag(NestLoop):
	INIT_WITH_HOOKS(NestLoop,nlstate);
	break;

    case classTag(MergeJoin):
	INIT_WITH_HOOKS(MergeJoin,mergestate);
	break;
	
	/* ----------------
	 *	materialization nodes
	 * ----------------
	 */
    case classTag(Material):
	INIT_WITH_HOOKS(Material,matstate);
	break;

    case classTag(Sort):
	INIT_WITH_HOOKS(Sort,sortstate);
	break;

    case classTag(Unique):
	INIT_WITH_HOOKS(Unique,uniquestate);
	break;

    case classTag(Agg):
	INIT_WITH_HOOKS(Agg, aggstate);
	break;
		
	/* ----------------
	 *	XXX add hooks to these
	 * ----------------
	 */
    case classTag(Hash):
	result = (List) ExecInitHash((Hash) node,
				     estate,
				     parent);
	break;
	
    case classTag(HashJoin):
	result = (List) ExecInitHashJoin((HashJoin) node,
					 estate,
					 parent);
	break;
	
    case classTag(ScanTemps):
	result = (List) ExecInitScanTemps((ScanTemps) node,
					  estate,
					  parent);
	break;
	
    default:
	elog(DEBUG, "ExecInitNode: node not yet supported: %d",
	     NodeGetTag(node));
	result = LispNil;
    }
    
    return result;
}
 

/* ----------------------------------------------------------------
 *   	ExecProcNode
 *   
 *   	Initial States:
 *   	  the query tree must be initialized once by calling ExecInit.
 * ----------------------------------------------------------------
 */
TupleTableSlot
ExecProcNode(node)
    Plan node;
{
    TupleTableSlot	result;
    HookNode 		hook;
    HookFunction 	proc;

    extern int testFlag;
    extern bool _exec_collect_stats_;

    /* ----------------
     *	deal with NULL nodes..
     * ----------------
     */
    if (node == NULL)
	return NULL;

    switch(NodeType(node)) {
	/* ----------------
	 *	control nodes
	 * ----------------
	 */
    case classTag(Result):
	PROC_WITH_HOOKS(ExecResult,Result,resstate);
	break;
	
    case classTag(Append):
	PROC_WITH_HOOKS(ExecProcAppend,Append,unionstate);
	break;
           
	/* ----------------
	 *	scan nodes
	 * ----------------
	 */
    case classTag(SeqScan):
	PROC_WITH_HOOKS(ExecSeqScan,SeqScan,scanstate);
	break;

    case classTag(IndexScan):
	PROC_WITH_HOOKS(ExecIndexScan,IndexScan,indxstate);
	break;

	/* ----------------
	 *	join nodes
	 * ----------------
	 */
    case classTag(NestLoop):
	PROC_WITH_HOOKS(ExecNestLoop,NestLoop,nlstate);
	break;

    case classTag(MergeJoin):
	PROC_WITH_HOOKS(ExecMergeJoin,MergeJoin,mergestate);
	break;

	/* ----------------
	 *	materialization nodes
	 * ----------------
	 */
    case classTag(Material):
	PROC_WITH_HOOKS(ExecMaterial,Material,matstate);
	break;

    case classTag(Sort):
	PROC_WITH_HOOKS(ExecSort,Sort,sortstate);
	break;

    case classTag(Unique):
	PROC_WITH_HOOKS(ExecUnique,Unique,uniquestate);
	break;

    case classTag(Agg):
	PROC_WITH_HOOKS(ExecAgg, Agg, aggstate);
	break;


    /* ----------------
     *  XXX add hooks to these
     * ----------------
     */
    case classTag(Hash):
	result = (TupleTableSlot) ExecHash((Hash) node);
	break;
   
    case classTag(HashJoin):
	result = (TupleTableSlot) ExecHashJoin((HashJoin) node);
	break;
	
    case classTag(ScanTemps):
	result = (TupleTableSlot) ExecScanTemps((ScanTemps) node);
	break;

    default:
	elog(DEBUG, "ExecProcNode: node not yet supported: %d",
	     NodeGetTag(node));
	result = (TupleTableSlot) NULL;
    }
    
    if (_exec_collect_stats_ && testFlag && !TupIsNull((Pointer) result))
	set_plan_size(node, get_plan_size(node) + 1);
    
    return result;
}

/* ----------------------------------------------------------------  
 *   	ExecEndNode
 *   
 *   	Recursively cleans up all the nodes in the plan rooted
 *   	at 'node'.
 *
 *   	After this operation, the query plan will not be able to
 *	processed any further.  This should be called only after
 *	the query plan has been fully executed.
 * ----------------------------------------------------------------  
 */
void
ExecEndNode(node)
    Plan node;
{
    HookNode 		hook;
    HookFunction 	proc;

    /* ----------------
     *	do nothing when we get to the end
     *  of a leaf on tree.
     * ----------------
     */
    if (node == NULL) 
	return;

    switch(NodeType(node)) {
	/* ----------------
	 *  control nodes
	 * ----------------
	 */
    case classTag(Result):
	END_WITH_HOOKS(Result,resstate);
	break;
	
    case classTag(Append):
	END_WITH_HOOKS(Append,unionstate);
	break;
	
	/* ----------------
	 *	scan nodes
	 * ----------------
	 */
    case classTag(SeqScan):
	END_WITH_HOOKS(SeqScan,scanstate);
	break;
    
    case classTag(IndexScan):
	END_WITH_HOOKS(IndexScan,indxstate);
	break;
    
	/* ----------------
	 *	join nodes
	 * ----------------
	 */
    case classTag(NestLoop):
	END_WITH_HOOKS(NestLoop,nlstate);
	break;
	
    case classTag(MergeJoin):
	END_WITH_HOOKS(MergeJoin,mergestate);
	break;
	
	/* ----------------
	 *	materialization nodes
	 * ----------------
	 */
    case classTag(Material):
	END_WITH_HOOKS(Material,matstate);
	break;
    
    case classTag(Sort):
	END_WITH_HOOKS(Sort,sortstate);
	break;
    
    case classTag(Unique):
	END_WITH_HOOKS(Unique,uniquestate);
	break;

    case classTag(Agg):
	END_WITH_HOOKS(Agg, aggstate);
	break;


	/* ----------------
	 *	XXX add hooks to these
	 * ----------------
	 */
    case classTag(Hash):
	ExecEndHash((Hash) node);
	break;
	
    case classTag(HashJoin):
	ExecEndHashJoin((HashJoin) node);
	break;
    
    case classTag(ScanTemps):
	ExecEndScanTemps((ScanTemps) node);
	break;

    default:
	elog(DEBUG, "ExecEndNode: node not yet supported",
	     NodeGetTag(node));
	break;
    }
}
