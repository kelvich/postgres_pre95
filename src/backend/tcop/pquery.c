/* ----------------------------------------------------------------
 *   FILE
 *	pquery.c
 *	
 *   DESCRIPTION
 *	POSTGRES process query command code
 *
 *   INTERFACE ROUTINES
 *	ProcessQuery
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "tcop/tcopdebug.h"

#include "tcop/dest.h"
#include "commands/command.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/mnodes.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/x_execmain.h"

extern List MakeList();

/* ----------------------------------------------------------------
 *	MakeQueryDesc is a utility used by ProcessQuery and
 *	the rule manager to build a query descriptor..
 * ----------------------------------------------------------------
 */
List
MakeQueryDesc(operation, parsetree, plantree, state, feature, dest)
    List  	operation;
    List  	parsetree;
    List  	plantree;
    List  	state;
    List  	feature;
    CommandDest dest;
{
    return
	MakeList(operation,  	    /* operation */
		 parsetree, 	    /* parse tree */
		 plantree, 	    /* plan */
		 state, 	    /* state */
		 feature, 	    /* feature */
		 lispInteger(dest), /* output dest */
		 -1);

}

/* ----------------------------------------------------------------
 *	CreateQueryDesc is a simpler interface to MakeQueryDesc
 * ----------------------------------------------------------------
 */
List
CreateQueryDesc(parsetree, plantree, dest)
    List 	parsetree;
    List 	plantree;
    CommandDest dest;
{
    return MakeQueryDesc(CAR(CDR(CAR(parsetree))), 	/* operation */
			 parsetree,			/* parse tree */
			 plantree,			/* plan */
			 LispNil,			/* state */
			 LispNil,			/* feature */
			 dest);				/* output dest */
}

/* ----------------------------------------------------------------
 *	CreateExecutorState
 *
 *	Note: this may someday take parameters -cim 9/18/89
 * ----------------------------------------------------------------
 */
EState
CreateExecutorState()
{
    EState		state;
    TupleCount		tuplecount;

    /* ----------------
     *	create a new executor state
     * ----------------
     */
    state = (EState) RMakeEState();
		       
    /* ----------------
     *	initialize the Executor State structure
     * ----------------
     */
    set_es_direction(state, EXEC_FRWD);
    set_es_time(state, 0);
    set_es_owner(state, 0);
    set_es_locks(state, LispNil);
    set_es_subplan_info(state, LispNil);
    set_es_error_message(state, NULL);
    set_es_range_table(state, LispNil);
    
    set_es_qualification_tuple(state, NULL);
    set_es_qualification_tuple_id(state, NULL);
    set_es_qualification_tuple_buffer(state, InvalidBuffer);
    set_es_raw_qualification_tuple(state, NULL);
    
    set_es_relation_relation_descriptor(state, NULL);
    set_es_into_relation_descriptor(state, NULL);
    set_es_result_relation_info(state, NULL);

    tuplecount = MakeTupleCount(0, 	/* retrieved */
				0, 	/* appended */
				0, 	/* deleted */
				0, 	/* replaced */
				0, 	/* inserted */
				0); 	/* processed */
    
    set_es_tuplecount(state, tuplecount);
    
    set_es_param_list_info(state, NULL);
    set_es_prs2_info(state, NULL);
    set_es_explain_relation(state, NULL);
    
    set_es_BaseId(state, 0);
    set_es_tupleTable(state, NULL);

    set_es_junkFilter(state, NULL);
    set_es_result_rel_scanstate(state, NULL);
    set_es_result_rel_ruleinfo(state, NULL);

    /* ----------------
     *	return the executor state structure
     * ----------------
     */
    return state;
}

/* ----------------------------------------------------------------
 *	CreateOperationTag
 *
 *	utility to get a string representation of the
 *	query operation.
 * ----------------------------------------------------------------
 */
String
CreateOperationTag(operationType)
    int	operationType;
{
    String tag;
    
    switch (operationType) {
    case RETRIEVE:
	tag = "RETRIEVE";
	break;
    case APPEND:
	tag = "APPEND";
	break;
    case DELETE:
	tag = "DELETE";
	break;
    case EXECUTE:
	tag = "EXECUTE";
	break;
    case REPLACE:
	tag = "REPLACE";
	break;
    default:
	elog(DEBUG, "CreateOperationTag: unknown operation type %d", 
	     operationType);
	tag = NULL;
	break;
    }
    
    return tag;
}

/* ----------------
 *	ProcessPortal
 * ----------------
 */
 
void
ProcessPortal(portalName, parseTree, plan, state, attinfo, dest)
    String 	portalName;
    List	parseTree;
    Plan	plan;
    EState	state;
    List	attinfo;
    CommandDest dest;
{
    Portal		portal;
    MemoryContext 	portalContext;

    /* ----------------
     *   convert the current blank portal into the user-specified
     *   portal and initialize the state and query descriptor.
     * ----------------
     */
    portal = BlankPortalAssignName(portalName);

    PortalSetQuery(portal,
		   CreateQueryDesc(parseTree, plan, dest),
		   state,
		   PortalCleanup);

    /* ----------------
     *	now create a new blank portal and switch to it. 
     *	Otherwise, the new named portal will be cleaned.
     *
     *  Note: portals will only be supported within a BEGIN...END
     *  block in the near future.  Later, someone will fix it to
     *  do what is possible across transaction boundries. -hirohama
     * ----------------
     */
    portalContext = (MemoryContext)
	PortalGetHeapMemory(GetPortalByName(NULL));
    
    MemoryContextSwitchTo(portalContext);
    
    StartPortalAllocMode(DefaultAllocMode, 0);
}

 
/* ----------------------------------------------------------------
 *	ProcessQueryDesc
 *
 *	Read the comments for ProcessQuery() below...
 * ----------------------------------------------------------------
 */
void
ProcessQueryDesc(queryDesc)
    List 	queryDesc;
{
    List 	parseTree;
    Plan 	plan;
    int		operation;
    String	tag;
    EState 	state;
    List 	feature_start;
    List 	feature_run;
    List 	feature_end;
    List 	attinfo;
    
    List	parseRoot;
    bool	isRetrieveIntoPortal;
    bool	isRetrieveIntoRelation;
    bool	isRetrieveIntoTemp;
    String	intoName;
    CommandDest dest;

    /* ----------------
     *	get info from the query desc
     * ----------------
     */
    parseTree = QdGetParseTree(queryDesc);
    plan =	QdGetPlan(queryDesc);
    
    operation = CAtom(GetOperation(queryDesc));
    tag = 	CreateOperationTag(operation);
    dest = 	QdGetDest(queryDesc);
    
    /* ----------------
     *	initialize portal/into relation status
     * ----------------
     */
    isRetrieveIntoPortal =   false;
    isRetrieveIntoRelation = false;
    isRetrieveIntoTemp =     false;
    
    if (operation == RETRIEVE) {
	List	resultDesc;
	int	resultDest;

	parseRoot = parse_tree_root(parseTree);
	resultDesc = root_result_relation(parseRoot);
	if (!lispNullp(resultDesc)) {
	    resultDest = CAtom(CAR(resultDesc));
	    if (resultDest == PORTAL) {
		isRetrieveIntoPortal = true;
		intoName = CString(CADR(resultDesc));
	    } else if (resultDest == INTO || resultDest == INTOTEMP) {
		isRetrieveIntoRelation = true;
		if (resultDest == INTOTEMP)
		   isRetrieveIntoTemp = true;
	    }
	}
    }

    /* ----------------
     *	when preforming a retrieve into, we override the normal
     *  communication destination during the processing of the
     *  the query.  This only affects the tuple-output function
     *  - the correct destination will still see BeginCommand()
     *  and EndCommand() messages.
     * ----------------
     */
    if (isRetrieveIntoRelation)
    	QdSetDest(queryDesc, None);
    
    /* ----------------
     *	create a default executor state.. 
     * ----------------
     */
    state = CreateExecutorState();

    /* ----------------
     *	create the executor "features"
     * ----------------
     */
    feature_start = MakeList(lispInteger(EXEC_START), -1);
    feature_run =   MakeList(lispInteger(EXEC_RUN), -1);
    feature_end =   MakeList(lispInteger(EXEC_END), -1);
    
    /* ----------------
     *	call ExecMain with EXEC_START to
     *  prepare the plan for execution
     * ----------------
     */
    attinfo = ExecMain(queryDesc, state, feature_start);
    
    /* ----------------
     *   report the query's result type information
     *   back to the front end or to whatever destination
     *   we're dealing with.
     * ----------------
     */
    BeginCommand(NULL,
		 operation,
		 attinfo,
		 isRetrieveIntoRelation,
		 isRetrieveIntoPortal,
		 tag,
		 dest);

    /* ----------------
     *  Named portals do not do a "fetch all" initially, so now
     *  we return since ExecMain has been called with EXEC_START
     *  to initialize the query plan.  
     *
     *  Note: ProcessPortal transforms the current "blank" portal
     *        into a named portal and creates a new blank portal so
     *	      everything we allocated in the current "blank" memory
     *	      context will be preserved across queries.  -cim 2/22/91
     * ----------------
     */
    if (isRetrieveIntoPortal) {
	extern MemoryContext PortalExecutorHeapMemory;
	PortalExecutorHeapMemory = NULL;
	
	ProcessPortal(intoName,
		      parseTree,
		      plan,
		      state,
		      attinfo,
		      dest);

	EndCommand(tag, dest);
	return;
    }
    
    /* ----------------
     *   Now we get to the important call to ExecMain() where we
     *   actually run the plan..
     * ----------------
     */
    (void) ExecMain(queryDesc, state, feature_run);

    /* ----------------
     *   final call to ExecMain.. we close down all the scans
     *   and free allocated resources...
     * ----------------
     */
    (void) ExecMain(queryDesc, state, feature_end);

#ifdef EXEC_TUPLECOUNT	    
    /* ----------------
     *	print tuple counts.. note this is only a temporary
     *  place for this because statistics are stored in the
     *  execution state.. it should be moved someplace else
     *  someday -cim 10/16/89
     * ----------------
     */
    DisplayTupleCount(state);
#endif EXEC_TUPLECOUNT	    

    /* ----------------
     *  Notify the destination of end of processing.
     * ----------------
     */
    EndCommand(tag, dest);
}

/* ----------------------------------------------------------------
 *	ProcessQuery
 *
 * ----------------------------------------------------------------
 */

extern List ParallelOptimize();
extern Fragment InitialPlanFragments();
extern Fragment ExecuteFragments();

void
ProcessQuery(parserOutput, originalPlan, dest)
    List 	parserOutput;
    Plan 	originalPlan;
    CommandDest dest;
{
    List 	queryDesc;
    Plan 	currentPlan;
    Plan 	newPlan;
    Fragment	planFragments;
    List	currentFragments;
    extern int	testFlag;
    
    currentPlan = originalPlan;
    if (testFlag) dest = None;
    queryDesc = CreateQueryDesc(parserOutput, currentPlan, dest);
    
    if (ParallelExecutorEnabled()) {
	planFragments = InitialPlanFragments(originalPlan);
	for (;;) {
	    currentFragments = ParallelOptimize(queryDesc, planFragments);
	    planFragments =    ExecuteFragments(queryDesc,
						currentFragments, 
						planFragments);
	    if (planFragments == NULL)
		return;
	}
    } else
	ExecuteFragments(queryDesc, LispNil, NULL);
}
