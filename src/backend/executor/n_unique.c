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
    
    h1 = (HeapTuple) ExecFetchTuple(t1);
    h2 = (HeapTuple) ExecFetchTuple(t2);
    
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
    outerPlan =     	get_outerPlan(node);
    resultTupleSlot =   get_cs_ResultTupleSlot(uniquestate);
    
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
	if (TupIsNull(slot))
	    return NULL;
	
	/* ----------------
	 *   we use the result tuple slot to hold our saved tuples.
	 *   if we haven't a saved tuple to compare our new tuple with,
	 *   then we exit the loop. This new tuple as the saved tuple
	 *   the next time we get here.  
	 * ----------------
	 */
	if (TupIsNull(resultTupleSlot))
	    break;
	
	/* ----------------
	 *   now test if the new tuple and the previous
	 *   tuple match.  If so then we loop back and fetch
	 *   another new tuple from the subplan.
	 * ----------------
	 */
	if (! ExecIdenticalTuples(slot, resultTupleSlot))
	    break;
    }
    
    /* ----------------
     *	we have a new tuple different from the previous saved tuple
     *  so we save it in the saved tuple slot.  Also bump the refcnt
     *  for the slot's buffer.
     * ----------------
     */
    shouldFree = ExecSetSlotPolicy(slot, false); 
    
    ExecStoreTuple(ExecFetchTuple(slot),
		   resultTupleSlot,
		   ExecSlotBuffer(slot),
		   shouldFree);
    
    ExecIncrSlotBufferRefcnt(resultTupleSlot);
    
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
    set_state(node, estate);
    
    /* ----------------
     *	create new UniqueState for node
     * ----------------
     */
    uniquestate = MakeUniqueState();
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
    ExecAssignNodeBaseInfo(estate, uniquestate, parent);
    ExecAssignDebugHooks(node, uniquestate);
    ExecInitResultTupleSlot(estate, uniquestate);
    
    /* ----------------
     *	then initialize outer plan
     * ----------------
     */
    outerPlan = get_outerPlan(node);
    ExecInitNode(outerPlan, estate, node);
    
    /* ----------------
     *	unique nodes do no projections, so initialize
     *  projection info for this node appropriately
     * ----------------
     */
    ExecAssignResultTypeFromOuterPlan(node, uniquestate);
    set_cs_ProjInfo(uniquestate, NULL);
       
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
    ExecEndNode((Plan) get_outerPlan(node));
    ExecClearTuple(get_cs_ResultTupleSlot(uniquestate));
} 
