/* ----------------------------------------------------------------
 *   FILE
 *	result.c
 *	
 *   DESCRIPTION
 *	support for constant nodes needing special code.
 *
 *	Example: in constant queries where no relations are scanned,
 *	the planner generates result nodes.  Examples of such queries are:
 *
 *		retrieve (x = 1)
 *	and
 *		append emp (name = "mike", salary = 15000)
 *
 *	Result nodes are also used to optimise queries
 *	with tautological qualifications like:
 *
 *		retrieve (emp.all) where 2 > 1
 *
 *	In this case, the plan generated is
 *
 *			Result  (with 2 > 1 qual)
 *			/
 *		   SeqScan (emp.all)
 *
 *   INTERFACE ROUTINES
 *	ExecResult	- fetch a tuple from the node
 *   	ExecInitResult	- initialize the node
 *   	ExecEndResult	- shutdown the node
 *
 *   NOTES
 *	
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
 *	1) execdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h
 *	6) externs.h comes last
 * ----------------
 */
#include "executor/execdebug.h"

#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/externs.h"

/* ----------------------------------------------------------------
 *   	ExecResult(node)
 *
 *	returns the tuples from the outer plan which satisify the
 *	qualification clause.  Since result nodes with right
 *	subtrees are never planned, we ignore the right subtree
 *	entirely (for now).. -cim 10/7/89
 *
 * old comments:
 *   	Returns the tuple joined from inner and outer tuples which 
 *   	satisfies the qualification clause.
 *
 *	It scans the inner relation to join with current outer tuple.
 *
 *	If none is found, next tuple form the outer relation
 *	is retrieved and the inner relation is scanned from the
 *	beginning again to join with the outer tuple.
 *
 *   	Nil is returned if all the remaining outer tuples are tried and
 *   	all fail to join with the inner tuples.
 *   
 *   	If inner subtree is not present, only outer subtree is processed.
 *   	
 *   	Also, the qualification containing only constant clauses are
 *   	checked first before any processing is done. It always returns
 *    	'nil' if the constant qualification is not satisfied.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 *           ExecResult
 ****/
TupleTableSlot
ExecResult(node)
    Result node;
{
    ResultState		resstate;
    TupleTableSlot	outerTupleSlot;
    Plan   		outerPlan;
    TupleDescriptor 	outerType;
    Buffer   		outerBuffer;
    
    TupleDescriptor 	tupType;
    Pointer		tupValue;
    List		targetList;
    int			len;
    ExprContext		econtext;
    List		qual;
    bool		qualResult;
    int			resloop;
    
    /* ----------------
     *	initialize the result node's state
     * ----------------
     */
    resstate =  get_resstate(node);
    
    /* ----------------
     *	get the expression context
     * ----------------
     */
    econtext = get_cs_ExprContext(resstate);
    
    /* ----------------
     *   check tautological qualifications like (2 > 1)
     * ----------------
     */
    qual = get_resconstantqual(node);
    if (! lispNullp(qual)) {
	qualResult = ExecQual(qual, econtext);
	/* ----------------
	 *  if we failed the constant qual, then there
	 *  is no need to continue processing because regardless of
	 *  what happens, the constant qual will be false..
	 * ----------------
	 */
	if (qualResult == false)
	    return NULL;
	
	/* ----------------
	 *  our constant qualification succeeded so now we
	 *  throw away the qual because we know it will always
	 *  succeed.
	 * ----------------
	 */
	set_resconstantqual(node, LispNil);
    }
    
    /* ----------------
     *  retrieve tuples from the outer plan until there are no more.
     * ----------------
     */
    for(;;) {
	/* ----------------
	 *    if (resloop == 1) then it means that we were asked to return
	 *    a constant tuple and we alread did the last time ExecResult()
	 *    was called, so now we are through.
	 * ----------------
	 */
	resloop = get_rs_Loop(resstate);
	if (resloop == 1)
	    return NULL;
	    
	/* ----------------
	 *    get next outer tuple if necessary.
	 * ----------------
	 */
	outerPlan = (Plan) get_outerPlan(node);
	
	if (outerPlan != NULL) {
	    outerTupleSlot = ExecProcNode(outerPlan);
	    
	    if (TupIsNull(outerTupleSlot))
		return NULL;
	    
	    set_cs_OuterTupleSlot(resstate, outerTupleSlot);
	}
	
	/* ----------------
	 *    get the information to place into the expr context
	 * ----------------
	 */
	qual =	        get_resrellevelqual(node);
	resstate =      get_resstate(node);
	outerPlan =     (Plan) get_outerPlan(node);
	
	outerTupleSlot = get_cs_OuterTupleSlot(resstate);
	
	/* ----------------
	 *   fill in the information in the expression context
	 *   XXX gross hack. use outer tuple as scan tuple
	 * ----------------
	 */
	set_ecxt_outertuple(econtext,   outerTupleSlot);
	set_ecxt_scantuple(econtext,    outerTupleSlot);
	
	/* ----------------
	 *  if we don't have an outer plan, then it's probably
	 *  the case that we are doing a retrieve or an append
	 *  with a constant target list, so we should only return
	 *  the constant tuple once or never if we fail the qual.
	 * ----------------
	 */
	if (outerPlan == NULL)
	    set_rs_Loop(resstate, 1);
	    	
	/* ----------------
	 *    test qualification
	 * ----------------
	 */
	qualResult = ExecQual(qual, econtext);
	
	if (qualResult == true) {
	    /* ----------------
	     *   qualification passed, form the result tuple and
	     *   pass it back using ExecProject()
	     * ----------------
	     */
	    ProjectionInfo projInfo;

	    projInfo = get_cs_ProjInfo(resstate);
	    return
		ExecProject(projInfo);
	}
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecInitResult
 *   
 *   	Creates the run-time state information for the nestloop node
 *   	produced by the planner and initailizes inner and outer relations 
 *   	(child nodes).
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitResult(node, estate, parent)
    Plan   node;
    EState estate;
    Plan   parent;
{
    ResultState	    resstate;
    TupleDescriptor tupType;
    Pointer	    tupValue;
    List	    targetList;
    int		    len;
    ParamListInfo   paraminfo;
    ExprContext	    econtext;
    int		    baseid;
    
    /* ----------------
     *	assign execution state to node
     * ----------------
     */
    set_state(node, estate);
    
    /* ----------------
     *	create new ResultState for node
     * ----------------
     */
    resstate = MakeResultState();
    set_resstate(node, resstate);
        
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, resstate, parent);
    ExecAssignDebugHooks(node, resstate);
    ExecAssignExprContext(estate, resstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, resstate);
         
    /* ----------------
     *	then initialize children
     * ----------------
     */
    ExecInitNode((Plan) get_outerPlan(node), estate, node);
    ExecInitNode((Plan) get_innerPlan(node), estate, node);
        
    /* ----------------
     * 	initialize tuple type and projection info
     * ----------------
     */
    ExecAssignResultTypeFromTL(node, resstate);
    ExecAssignProjectionInfo(node, resstate);

    /* ----------------
     *	set "are we done yet" to false
     * ----------------
     */
    set_rs_Loop(resstate, 0);
    
    return LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndResult
 *   
 *   	fees up storage allocated through C routines
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndResult(node)
    Result node;
{
    ResultState	    resstate;
    Pointer	    tupValue;
    
    resstate = get_resstate(node);

    /* ----------------
     *	Free the projection info
     *
     *  Note: we don't ExecFreeResultType(resstate) 
     *        because the rule manager depends on the tupType
     *	      returned by ExecMain().  So for now, this
     *	      is freed at end-transaction time.  -cim 6/2/91     
     * ----------------
     */    
    ExecFreeProjectionInfo(resstate);
    
    /* ----------------
     *	shut down subplans
     * ----------------
     */
    ExecEndNode((Plan) get_outerPlan(node));
    ExecEndNode((Plan) get_innerPlan(node));

    /* ----------------
     *	clean out the tuple table
     * ----------------
     */
    ExecClearTuple(get_cs_ResultTupleSlot(resstate));
}
