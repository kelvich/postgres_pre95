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

#include "executor/executor.h"

 RcsId("$Header$");

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
    TupleTableSlot	resultSlot;
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
    bool		isDone;
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
    econtext = get_cs_ExprContext((CommonState) resstate);
    
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
    
    if (get_cs_TupFromTlist((CommonState) resstate)) {
	ProjectionInfo projInfo;

	projInfo = get_cs_ProjInfo((CommonState)resstate);
	resultSlot = ExecProject(projInfo, &isDone);
	if (!isDone)
	    return resultSlot;
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
	outerPlan = (Plan) get_outerPlan((Plan) node);
	
	if (outerPlan != NULL) {
	    outerTupleSlot = ExecProcNode(outerPlan);
	    
	    if (TupIsNull((Pointer) outerTupleSlot))
		return NULL;
	    
	    set_cs_OuterTupleSlot((CommonState) resstate, outerTupleSlot);
	}
	
	/* ----------------
	 *    get the information to place into the expr context
	 * ----------------
	 */
	qual =	        get_resrellevelqual(node);
	resstate =      get_resstate(node);
	outerPlan =     (Plan) get_outerPlan((Plan) node);
	
	outerTupleSlot = get_cs_OuterTupleSlot((CommonState)resstate);
	
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

	    projInfo = get_cs_ProjInfo((CommonState)resstate);
	    resultSlot = ExecProject(projInfo, &isDone);
	    set_cs_TupFromTlist((CommonState) resstate, !isDone);
	    return resultSlot;
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
    set_state( node, (EStatePtr)estate);
    
    /* ----------------
     *	create new ResultState for node
     * ----------------
     */
    resstate = MakeResultState(0);
    set_resstate((Result) node, resstate);
        
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) resstate, parent);
    ExecAssignDebugHooks( node, (BaseNode) resstate);
    ExecAssignExprContext(estate, (CommonState) resstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, (CommonState) resstate);
         
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
    ExecAssignResultTypeFromTL(node, (CommonState)resstate);
    ExecAssignProjectionInfo(node, (CommonState) resstate);

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
    ExecFreeProjectionInfo((CommonState)resstate);
    
    /* ----------------
     *	shut down subplans
     * ----------------
     */
    ExecEndNode((Plan) get_outerPlan((Plan)node));
    ExecEndNode((Plan) get_innerPlan((Plan)node));

    /* ----------------
     *	clean out the tuple table
     * ----------------
     */
    ExecClearTuple((Pointer)get_cs_ResultTupleSlot((CommonState)resstate));
}
