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

/* ----------------------------------------------------------------
 *	MakeQueryDesc is a utility used by ProcessQuery and
 *	the rule manager to build a query descriptor..
 * ----------------------------------------------------------------
 */
List
MakeQueryDesc(operation, parsetree, plantree, state, feature)
    List  operation;
    List  parsetree;
    List  plantree;
    List  state;
    List  feature;
{
    return (List)
	lispCons(operation,				      /* operation */
		 lispCons(parsetree, 			      /* parse tree */
			  lispCons(plantree, 		      /* plan */
				   lispCons(state, 	      /* state */
					    lispCons(feature, /* feature */
						     LispNil)))));
}

/* ----------------------------------------------------------------
 *	CreateQueryDesc is a simpler interface to MakeQueryDesc
 * ----------------------------------------------------------------
 */
List
CreateQueryDesc(parsetree, plantree)
    List parsetree;
    List plantree;
{
    return MakeQueryDesc(CAR(CDR(CAR(parsetree))), 	/* operation */
			 parsetree,			/* parse tree */
			 plantree,			/* plan */
			 LispNil,			/* state */
			 LispNil);			/* feature */
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
 
/* ----------------------------------------------------------------
 *	ProcessPortal
 *
 *	Function to alleviate ProcessFragments of the code
 *	that deals with portal query processing.
 * ----------------------------------------------------------------
 */
 
void
ProcessPortal(operation, portalName, parseTree, plan, state, attinfo)
    int		operation;
    String 	portalName;
    List	parseTree;
    Plan	plan;
    EState	state;
    List	attinfo;
{
    Portal		portal;
    MemoryContext 	portalContext;

    /* ----------------
     *	sanity check
     * ----------------
     */
    AssertArg(operation == RETRIEVE);
    
    /*
     * Note:  "BeginCommand" is not called since no tuples are returned.
     */

    /* ----------------
     *   initialize the portal
     * ----------------
     */
    portal = BlankPortalAssignName(portalName);

    PortalSetQuery(portal,
		   CreateQueryDesc(parseTree, plan),
		   state,
		   PortalCleanup);

    /* ----------------
     * Return blank portal for now.
     *
     * Otherwise, this named portal will be cleaned.
     * Note: portals will only be supported within a BEGIN...END
     * block in the near future.  Later, someone will fix it to
     * do what is possible across transaction boundries. -hirohama
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
    List 	feature;
    List 	attinfo;
    
    List	parseRoot;
    bool	isIntoPortal;
    bool	isIntoRelation;
    bool	isIntoTemp;
    String	intoName;
    
    /* ----------------
     *	get info from the query desc
     * ----------------
     */
    parseTree = QdGetParseTree(queryDesc);
    plan =	QdGetPlan(queryDesc);
    
    operation = CAtom(GetOperation(queryDesc));
    tag = 	CreateOperationTag(operation);
    
    /* ----------------
     *	initialize portal/into relation status
     * ----------------
     */
    isIntoPortal =   false;
    isIntoRelation = false;
    isIntoTemp = false;
    
    if (operation == RETRIEVE) {
	List	resultDesc;
	int	dest;

	parseRoot = parse_tree_root(parseTree);
	resultDesc = root_result_relation(parseRoot);
	if (!lispNullp(resultDesc)) {
	    dest = CAtom(CAR(resultDesc));
	    if (dest == PORTAL) {
		isIntoPortal = true;
		intoName = CString(CADR(resultDesc));
	    } else if (dest == INTO || dest == INTOTEMP) {
		isIntoRelation = true;
		if (dest == INTOTEMP)
		   isIntoTemp = true;
	    }
	}
    }
    
    /* ----------------
     *	create a default executor state.. 
     * ----------------
     */
    state = CreateExecutorState();

    /* ----------------
     *	call ExecMain with EXEC_START to
     *  prepare the plan for execution
     * ----------------
     */
    feature = lispCons(lispInteger(EXEC_START), LispNil);

    
    attinfo = ExecMain(queryDesc, state, feature);
    
    /* ----------------
     *  Named portals do not do a "fetch all" initially, so now
     *  we return since ExecMain has been called with EXEC_START
     *  to initialize the query plan.
     * ----------------
     */
    if (isIntoPortal) {
	/* ----------------
	 * set executor portal memory context
	 *
	 * temporary hack till the executor-rule manager interface
	 * allocation is fixed.
	 * ----------------
	 */
	extern MemoryContext PortalExecutorHeapMemory;
	PortalExecutorHeapMemory = NULL;

   	ProcessPortal(operation,
		      intoName,
		      parseTree,
		      plan,
		      state,
		      attinfo);
	
	EndCommand(tag);
	return;
    }

    /* ----------------
     *   report the query's result type information
     *   back to the front end..
     *
     *   XXX this should be cleaned up (I have no idea
     *	     what the putxxxx functions are doing)
     *	     -cim 9/18/89
     * ----------------
     */
    if (operation == RETRIEVE) {
	if (isIntoRelation && IsUnderPostmaster) {
	    putnchar("P",1);
	    putint(0,4);
	    putstr("blank");
	} else {
	    if (!isIntoTemp)
	       BeginCommand("blank", attinfo);
	   }
    } else {
	if (IsUnderPostmaster) {
	    putnchar("P",1);
	    putint(0,4);
	    putstr("blank");
	} else
	    BeginCommand("blank",attinfo);
    }
    
    /* ----------------
     *   Now we get to the important call to ExecMain() where we
     *   actually run the plan..
     *
     *	 The trick here is we have to pass the appopriate
     *   feature depending on if we have a postmaster or if
     *   we are retrieving "into" a relation... This is a
     *   poor interface preserved from the lisp days, so we
     *   should do something better soon...
     * ----------------
     */
    
    if (isIntoRelation) {
	/* ----------------
	 *   creation of the into relation is now done in InitPlan()
	 *   so we nolonger pass the relation descriptor around in
	 *   the feature..  (it's in the EState struct now)
	 *   -cim 9/18/89
	 * ----------------
	 */
	feature = lispCons(lispInteger(EXEC_RESULT), LispNil);
    } else if (IsUnderPostmaster) {
	/* ----------------
	 *   I suggest that the IsUnderPostmaster test be
	 *   performed at a lower level in the code--at
	 *   least inside the executor to reduce the number
	 *   of externally visible requests. -hirohama
	 * ----------------
	 */
	feature = lispCons(lispInteger(EXEC_DUMP), LispNil);
    } else {
	feature = lispCons(lispInteger(EXEC_DEBUG), LispNil);
    }

    (void) ExecMain(queryDesc, state, feature);

    /* ----------------
     *   final call to ExecMain.. we close down all the scans
     *   and free allocated resources...
     * ----------------
     */

    feature = lispCons(lispInteger(EXEC_END), LispNil);
    (void) ExecMain(queryDesc, state, feature);

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
     *   not certain what this does.. -cim 8/29/89
     *
     * A: Notify the frontend of end of processing.
     * Perhaps it would be cleaner for ProcessQuery
     * and ProcessCommand to return the tag, and for
     * the "traffic cop" to call EndCommand on it.
     *	-hirohama
     * ----------------
     */
    EndCommand(tag);
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
ProcessQuery(parserOutput, originalPlan)
    List parserOutput;
    Plan originalPlan;
{
    List 	queryDesc;
    Plan 	currentPlan;
    Plan 	newPlan;
    Fragment	planFragments;
    List	currentFragments;
    
    currentPlan = originalPlan;
    queryDesc = CreateQueryDesc(parserOutput, currentPlan);
    if (ParallelExecutorEnabled()) {
       planFragments = InitialPlanFragments(originalPlan);
       for (;;) {
	   currentFragments = ParallelOptimize(queryDesc, planFragments);
	   planFragments = ExecuteFragments(queryDesc, 
					    currentFragments, 
					    planFragments);
	   if (planFragments == NULL)
	      return;
         }
       }
    else {
	ExecuteFragments(queryDesc, LispNil, NULL);
	return;
      }
}
