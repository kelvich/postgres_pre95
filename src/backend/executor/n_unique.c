/* ----------------------------------------------------------------
 *   FILE
 *	unique.c
 *	
 *   DESCRIPTION
 *	Routines to handle unique'ing of queries where appropriate
 *
 *   INTERFACE ROUTINES
 *     	ExecUnique	- generate a unique'd temporary relation
 *     	ExecInitUnique	- initialize node and subnodes..
 *     	ExecEndUnique	- shutdown node and subnodes
 *
 *   NOTES
 *	Assumes tuples returned from subplan arrive in
 *	sorted order.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "executor/executor.h"
#include "planner/clauses.h"

 RcsId("$Header$");


/* ----------------------------------------------------------------
 *   	ExecIdenticalTuples
 *
 *	This is a hack function used by ExecUnique to see if
 *	two tuples are identical.  This should be provided
 *	by the heap tuple code but isn't.  The real problem
 *	is that we assume we can byte compare tuples to determine
 *	if they are "equal".  In fact, if we have user defined
 *	types there may be problems because it's possible that
 *	an ADT may have multiple representations with the
 *	same ADT value. -cim
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecUnique
 ****/
bool	/* true if tuples are identical, false otherwise */
ExecIdenticalTuples(t1, t2)
    List  t1;
    List  t2;
{
    HeapTuple 	h1;
    HeapTuple 	h2;
    char 	*d1;
    char 	*d2;
    int 	len;
    
    h1 = (HeapTuple) ExecFetchTuple((Pointer) t1);
    h2 = (HeapTuple) ExecFetchTuple((Pointer) t2);
    
    /* ----------------
     *	if tuples aren't the same length then they are 
     *  obviously different (one may have null attributes).
     * ----------------
     */
    if (h1->t_len != h2->t_len)
	return false;
    
    /* ----------------
     *	ok, now get the pointers to the data and the
     *  size of the attribute portion of the tuple.
     * ----------------
     */
    d1 = (char *) GETSTRUCT(h1);
    d2 = (char *) GETSTRUCT(h2);
    len = (int) h1->t_len - (int) h1->t_hoff;
    
    /* ----------------
     *	byte compare the data areas and return the result.
     * ----------------
     */
    if (bcmp(d1, d2, len) != 0)
	return false;
    
    return true;
}
 
/* ----------------------------------------------------------------
 *   	ExecUnique
 *
 *	This is a very simple node which filters out duplicate
 *	tuples from a stream of sorted tuples from a subplan.
 *
 *	XXX see comments below regarding freeing tuples.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot 		/* return: a tuple or NULL */
ExecUnique(node)
    Unique node;
{
    UniqueState		uniquestate;
    TupleTableSlot	resultTupleSlot;
    TupleTableSlot	slot;
    Plan   		outerPlan;
    bool		shouldFree;
    
    /* ----------------
     *	get information from the node
     * ----------------
     */
    uniquestate =   	get_uniquestate(node);
    outerPlan =     	get_outerPlan((Plan) node);
    resultTupleSlot =   get_cs_ResultTupleSlot((CommonState) uniquestate);
    
    /* ----------------
     *	now loop, returning only non-duplicate tuples.
     *  We assume that the tuples arrive in sorted order
     *  so we can detect duplicates easily.
     * ----------------
     */
    for (;;) {
	/* ----------------
	 *   fetch a tuple from the outer subplan
	 * ----------------
	 */
	slot = ExecProcNode(outerPlan);
	if (TupIsNull((Pointer) slot))
	    return NULL;
	
	/* ----------------
	 *   we use the result tuple slot to hold our saved tuples.
	 *   if we haven't a saved tuple to compare our new tuple with,
	 *   then we exit the loop. This new tuple as the saved tuple
	 *   the next time we get here.  
	 * ----------------
	 */
	if (TupIsNull((Pointer) resultTupleSlot))
	    break;
	
	/* ----------------
	 *   now test if the new tuple and the previous
	 *   tuple match.  If so then we loop back and fetch
	 *   another new tuple from the subplan.
	 * ----------------
	 */
	if (! ExecIdenticalTuples((List) slot, (List) resultTupleSlot))
	    break;
    }
    
    /* ----------------
     *	we have a new tuple different from the previous saved tuple
     *  so we save it in the saved tuple slot.  Also bump the refcnt
     *  for the slot's buffer.
     * ----------------
     */
    shouldFree = ExecSetSlotPolicy((Pointer) slot, false); 
    
    ExecStoreTuple(ExecFetchTuple((Pointer) slot),
		   (Pointer) resultTupleSlot,
		   ExecSlotBuffer((Pointer) slot),
		   shouldFree);
    
    ExecIncrSlotBufferRefcnt((Pointer) resultTupleSlot);
    
    return
	slot;
}
 
/* ----------------------------------------------------------------
 *   	ExecInitUnique
 *
 *	This initializes the unique node state structures and
 *	the node's subplan.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List	/* return: initialization status */
ExecInitUnique(node, estate, parent)
    Unique 	node;
    EState 	estate;
    Plan	parent;
{
    UniqueState	    uniquestate;
    Plan   	    outerPlan;
    
    /* ----------------
     *	assign execution state to node
     * ----------------
     */
    set_state((Plan) node,  estate);
    
    /* ----------------
     *	create new UniqueState for node
     * ----------------
     */
    uniquestate = MakeUniqueState(0);
    set_uniquestate(node, uniquestate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *
     *  Unique nodes have no ExprContext initialization because
     *  they never call ExecQual or ExecTargetList.
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) uniquestate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) uniquestate);
    ExecInitResultTupleSlot(estate, (CommonState) uniquestate);
    
    /* ----------------
     *	then initialize outer plan
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecInitNode(outerPlan, estate, (Plan) node);
    
    /* ----------------
     *	unique nodes do no projections, so initialize
     *  projection info for this node appropriately
     * ----------------
     */
    ExecAssignResultTypeFromOuterPlan((Plan) node, (CommonState) uniquestate);
    set_cs_ProjInfo((CommonState) uniquestate, NULL);
       
    /* ----------------
     *	all done.
     * ----------------
     */
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndUnique
 *
 *	This shuts down the subplan and frees resources allocated
 *	to this node.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndUnique(node)
    Unique node;
{
    UniqueState	    uniquestate;
    
    uniquestate = get_uniquestate(node);
    ExecEndNode((Plan) get_outerPlan((Plan) node));
    ExecClearTuple((Pointer)
		   get_cs_ResultTupleSlot((CommonState) uniquestate));
} 
