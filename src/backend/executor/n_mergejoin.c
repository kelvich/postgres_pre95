/* ----------------------------------------------------------------
 *   FILE
 *	mergejoin.c
 *	
 *   DESCRIPTION
 *	routines supporting merge joins
 *   
 *   INTERFACE ROUTINES
 *   	ExecMergeJoin 		mergejoin outer and inner relations.
 *   	ExecInitMergeJoin	creates and initializes run time states
 *   	ExecEndMergeJoin  	cleand up the node.
 *
 *   NOTES
 *	Essential operation of the merge join algorithm is as follows:
 *	(** indicates the tuples satisify the merge clause).
 *
 *	Join {						       -
 *	    get initial outer and inner tuples		    INITIALIZE
 *	    Skip Inner					    SKIPINNER
 *	    mark inner position				    JOINMARK
 *	    do forever {				       -
 *	        while (outer ** inner) {		    JOINTEST
 *		    join tuples				    JOINTUPLES
 *	            advance inner position		    NEXTINNER
 *	        }					       -
 *	        advance outer position			    NEXTOUTER
 *	        if (outer ** mark) {			    TESTOUTER
 *                  restore inner position to mark          TESTOUTER
 *	            continue                                   -
 *	        } else {				       -
 *	            Skip Outer			            SKIPOUTER
 *		    mark inner position			    JOINMARK
 *	        }					       -
 *	    }						       -
 *      }						       -
 *
 *      Skip Outer {					    SKIPOUTER
 *	    if (inner ** outer)	Join Tuples		    JOINTUPLES
 *	    while (outer < inner)			    SKIPOUTER
 *		advance outer				    SKIPOUTER
 *	    if (outer > inner)				    SKIPOUTER
 *	        Skip Inner				    SKIPINNER
 *	}						       -
 *
 *	Skip Inner {					    SKIPINNER
 *	    if (inner ** outer)	Join Tuples		    JOINTUPLES
 *	    while (outer > inner)			    SKIPINNER
 *	        advance inner				    SKIPINNER
 *	    if (outer < inner)				    SKIPINNER
 *	        Skip Outer				    SKIPOUTER
 *	}						       -
 *
 *	Currently, the merge join operation is coded in the fashion
 *	of a state machine.  At each state, we do something and then
 *	proceed to another state.  This state is stored in the node's
 *	execution state information and is preserved across calls to
 *	ExecMergeJoin. -cim 10/31/89
 *
 *	Warning:  This code is known to fail for inequality operations
 *		  and is being redesigned.  Specifically, = and > work
 *		  but the logic is not correct for <.  Since mergejoins
 *		  are no better then nestloops for inequalitys, the planner
 *		  should not plan them anyways.  Alternatively, the
 *		  planner could just exchange the inner/outer relations
 *		  if it ever sees a <... -cim 7/1/90
 *
 *	Update:	  The executor tuple table has long since alleviated the
 *		  problem described above -cim 4/23/91
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
 *	MarkInnerTuple and RestoreInnerTuple macros
 *
 *	when we "mark" a tuple, we place a pointer to it
 *  	in the marked tuple slot.  now there are two pointers
 *  	to this tuple and we don't want it to be freed until
 *  	next time we mark a tuple, so we move the policy to
 *  	the marked tuple slot and set the inner tuple slot policy
 *  	to false.
 *
 *	But, when we restore the inner tuple, the marked tuple
 *	retains the policy.  Basically once a tuple is marked, it
 *	should only be freed when we mark another tuple.  -cim 9/27/90
 *
 *	Note:  now that we store buffers in the tuple table,
 *	       we have to also increment buffer reference counts
 *	       correctly whenever we propagate an additional pointer
 *	       to a buffer item.  Later, when ExecStoreTuple() is
 *	       called again on this slot, the refcnt is decremented
 *	       when the old tuple is replaced.
 * ----------------------------------------------------------------
 */
#define MarkInnerTuple(innerTupleSlot, mergestate) \
{ \
    bool	   shouldFree; \
    shouldFree = ExecSetSlotPolicy(innerTupleSlot, false); \
    ExecStoreTuple(ExecFetchTuple(innerTupleSlot), \
		   get_mj_MarkedTupleSlot(mergestate), \
		   ExecSlotBuffer(innerTupleSlot), \
		   shouldFree); \
    ExecIncrSlotBufferRefcnt(innerTupleSlot); \
}

#define RestoreInnerTuple(innerTupleSlot, markedTupleSlot) \
    ExecStoreTuple(ExecFetchTuple(markedTupleSlot), \
		   innerTupleSlot, \
		   ExecSlotBuffer(markedTupleSlot), \
		   false); \
    ExecIncrSlotBufferRefcnt(innerTupleSlot)

/* ----------------------------------------------------------------
 *   	MJFormOSortopI
 *
 *	This takes the mergeclause which is a qualification of the
 *	form ((= expr expr) (= expr expr) ...) and forms a new
 *	qualification like ((> expr expr) (> expr expr) ...) which
 *	is used by ExecMergeJoin() in order to determine if we should
 *	skip tuples.
 *
 * old comments
 *   	The 'qual' must be of the form:
 *   	   {(= outerkey1 innerkey1)(= outerkey2 innerkey2) ...}
 *   	The "sortOp outerkey innerkey" is formed by substituting the "="
 *   	by "sortOp".
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           MJFormISortopO
 *           ExecInitMergeJoin
 ****/
List
MJFormOSortopI(qualList, sortOp)
    List 	qualList;
    ObjectId 	sortOp;
{
    List qualCopy;
    List qualcdr;
    List qual;
    Oper op;
    Oper newop;
    
    /* ----------------
     *  qualList is a list: ((op .. ..) ...)
     *	first we make a copy of it.  CopyObject() makes a deep copy 
     *  so let's use it instead of the old fashoned lispCopy()...
     * ----------------
     */
    qualCopy = (List) CopyObject(qualList);
    
    foreach (qualcdr, qualCopy) {
	/* ----------------
	 *   first get the current (op .. ..) list
	 * ----------------
	 */
	qual = CAR(qualcdr);
	
	/* ----------------
	 *   now get at the op
	 * ----------------
	 */
	op = (Oper) CAR(qual);
	if (!ExactNodeType(op,Oper)) {
	    elog(DEBUG, "MJFormOSortopI: op not an Oper!");
	    return LispNil;
	}
	
	/* ----------------
	 *   change it's opid and since Op nodes now carry around a
	 *   cached pointer to the associated op function, we have
	 *   to make sure we invalidate this.  Otherwise you get bizarre
	 *   behavior when someone runs a mergejoin with _exec_repeat_ > 1
	 *   -cim 4/23/91
	 * ----------------
	 */
	set_opid(op, sortOp);
	set_op_fcache(op, NULL);
    }
    
    return qualCopy;
}
 
/* ----------------------------------------------------------------
 *   	MJFormISortopO
 *
 *	This does the same thing as MJFormOSortopI() except that
 *	it also reverses the expressions in the qualifications.
 *	For example: ((= expr1 expr2)) produces ((> expr2 expr1))
 *
 * old comments
 *   	The 'qual' must be of the form:
 *   	   {(= outerkey1 innerkey1) (= outerkey2 innerkey2) ...}
 *   	The 'sortOp innerkey1 outerkey" is formed by substituting the "="
 *   	by "sortOp" and reversing the positions of the keys.
 *  ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecInitMergeJoin
 ****/
List
MJFormISortopO(qualList,sortOp)
    List 	qualList;
    ObjectId 	sortOp;
{
    List 	ISortopO;
    List 	qualcdr;
    
    /* ----------------
     *	first generate OSortopI, a list of the form
     *  ((op outer inner) (op outer inner) ... )
     * ----------------
     */
    ISortopO = MJFormOSortopI(qualList, sortOp);
    
    /* ----------------
     *	now swap the cadr and caddr of each qual to form ISortopO,
     *  ((op inner outer) (op inner outer) ... )
     * ----------------
     */
    foreach (qualcdr, ISortopO) {
	List qual;
	List inner;
	List outer;
	qual =  CAR(qualcdr);
	
	inner = CAR(CDR(qual));
	outer = CAR(CDR(CDR(qual)));
	CAR(CDR(qual)) = outer;
	CAR(CDR(CDR(qual))) = inner;
    }
    
    return ISortopO;
}
 
/* ----------------------------------------------------------------
 *   	MergeCompare
 *   
 *   	Compare the keys according to 'compareQual' which is of the 
 *   	form: {(key1a > key2a)(key1b > key2b) ...}.
 *
 *   	(actually, it could also be the form (key1a < key2a)..)
 *   
 *   	This is different from calling ExecQual because ExecQual returns
 *   	true only if ALL the comparisions clauses are satisfied.
 *   	However, there is an order of significance among the keys with
 *   	the first keys being most significant. Therefore, the clauses
 *   	are evaluated in order and the 'compareQual' is satisfied
 *   	if (key1i > key2i) is true and (key1j = key2j) for 0 < j < i.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecMergeJoin
 ****/
bool
MergeCompare(eqQual, compareQual, econtext)
    List 	eqQual;
    List 	compareQual;
    ExprContext econtext;
{
    List   clause;
    List   eqclause;
    Const  expr_value;
    Datum  const_value;
	Boolean isNull;
    
    /* ----------------
     *	if we have no compare qualification, return nil
     * ----------------
     */
    if (lispNullp(compareQual))
	return false;
    
    /* ----------------
     *	for each pair of clauses, test them until
     *  our compare conditions are satisified
     * ----------------
     */
    eqclause = eqQual;
    foreach (clause, compareQual) {
	/* ----------------
	 *   first test if our compare clause is satisified.
	 *   if so then return true.
	 * ----------------
	 */
	const_value = (Datum)
	    ExecEvalExpr((Node) CAR(clause), econtext, &isNull);
    
	if (ExecCTrue(DatumGetInt32(const_value)))
	    return true;
	
	/* ----------------
	 *   ok, the compare clause failed so we test if the keys
	 *   are equal... if key1 != key2, we return false.
	 *   otherwise key1 = key2 so we move on to the next pair of keys.
	 * ----------------
	 */
	const_value = ExecEvalExpr((Node) CAR(eqclause), econtext, &isNull);
	
    
	if (! ExecCTrue(DatumGetInt32(const_value)))
	    return false;
	eqclause = CDR(eqclause);
    }
    
    /* ----------------
     *	if we get here then it means none of our key greater-than
     *  conditions were satisified so we return false.
     * ----------------
     */
    return false;
}
 
/* ----------------------------------------------------------------
 *	ExecMergeTupleDump
 *
 *	This function is called through the MJ_dump() macro
 *	when EXEC_MERGEJOINDEBUG is defined
 * ----------------------------------------------------------------
 */

void
ExecMergeTupleDumpInner(econtext)
    ExprContext	econtext;
{
    TupleTableSlot innerSlot;

    printf("==== inner tuple ====\n");
    innerSlot = get_ecxt_innertuple(econtext);
    if (TupIsNull(innerSlot))
	printf("(nil)\n");
    else
	debugtup(ExecFetchTuple(innerSlot), ExecSlotDescriptor(innerSlot));
}
 
void
ExecMergeTupleDumpOuter(econtext)
    ExprContext	econtext;
{
    TupleTableSlot outerSlot;

    printf("==== outer tuple ====\n");
    outerSlot = get_ecxt_outertuple(econtext);
    if (TupIsNull(outerSlot))
	printf("(nil)\n");
    else
	debugtup(ExecFetchTuple(outerSlot), ExecSlotDescriptor(outerSlot));
}
 
void
ExecMergeTupleDumpMarked(econtext, mergestate)
    ExprContext	econtext;
    MergeJoinState mergestate;
{
    TupleTableSlot markedSlot;

    printf("==== marked tuple ====\n");
    markedSlot = get_mj_MarkedTupleSlot(mergestate);

    if (TupIsNull(markedSlot))
	printf("(nil)\n");
    else
	debugtup(ExecFetchTuple(markedSlot), ExecSlotDescriptor(markedSlot));
}
 
void
ExecMergeTupleDump(econtext, mergestate)
    ExprContext	econtext;
    MergeJoinState mergestate;
{
    printf("******** ExecMergeTupleDump ********\n");
    
    ExecMergeTupleDumpInner(econtext);
    ExecMergeTupleDumpOuter(econtext);
    ExecMergeTupleDumpMarked(econtext, mergestate);
    
    printf("******** \n");
}
 
/* ----------------------------------------------------------------
 *   	ExecMergeJoin
 *
 * old comments
 *   	Details of the merge-join routines:
 *   	
 *   	(1) ">" and "<" operators
 *   
 *   	Merge-join is done by joining the inner and outer tuples satisfying 
 *   	the join clauses of the form ((= outerKey innerKey) ...).
 *   	The join clauses is provided by the query planner and may contain
 *   	more than one (= outerKey innerKey) clauses (for composite key).
 *   
 *   	However, the query executor needs to know whether an outer
 *   	tuple is "greater/smaller" than an inner tuple so that it can
 *   	"synchronize" the two relations. For e.g., consider the following
 *   	relations:
 *   
 *   		outer: (0 ^1 1 2 5 5 5 6 6 7)	current tuple: 1
 *   	 	inner: (1 ^3 5 5 5 5 6)	        current tuple: 3
 *   
 *   	To continue the merge-join, the executor needs to scan both inner
 *   	and outer relations till the matching tuples 5. It needs to know
 *   	that currently inner tuple 3 is "greater" than outer tuple 1 and
 *   	therefore it should scan the outer relation first to find a 
 *   	matching tuple and so on.
 *   	
 *   	Therefore, when initializing the merge-join node, the executor
 *   	creates the "greater/smaller" clause by substituting the "=" 
 *   	operator in the join clauses with the sort operator used to
 *   	sort the outer and inner relation forming (outerKey sortOp innerKey).
 *   	The sort operator is "<" if the relations are in ascending order  
 *   	otherwise, it is ">" if the relations are in descending order.
 *   	The opposite "smaller/greater" clause is formed by reversing the
 *   	outer and inner keys forming (innerKey sortOp outerKey).
 *   
 *   	(2) repositioning inner "cursor"
 *   
 *   	Consider the above relations and suppose that the executor has
 *   	just joined the first outer "5" with the last inner "5". The
 *   	next step is of course to join the second outer "5" with all
 *   	the inner "5's". This requires repositioning the inner "cursor"
 *   	to point at the first inner "5". This is done by "marking" the
 *   	first inner 5 and restore the "cursor" to it before joining
 *   	with the second outer 5. The access method interface provides
 *   	routines to mark and restore to a tuple.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecMergeJoin
 *           ExecProcNode
 ****/
TupleTableSlot
ExecMergeJoin(node)
    MergeJoin 	node;
{
    EState	   estate;
    MergeJoinState mergestate;
    ScanDirection  direction;
    List	   innerSkipQual;
    List	   outerSkipQual;
    List	   mergeclauses;
    List	   qual;
    bool	   qualResult;    
    bool	   compareResult;
    
    Plan	   innerPlan;
    TupleTableSlot innerTupleSlot;
    
    Plan	   outerPlan;
    TupleTableSlot outerTupleSlot;
    
    TupleTableSlot markedTupleSlot;
    
    ExprContext	   econtext;
    List	   targetList;
    int		   len;
    TupleDescriptor tupType;
    Pointer	   tupValue;
    
    HeapTuple	   resultTuple;
    TupleTableSlot resultSlot;
    
    /* ----------------
     *	get information from node
     * ----------------
     */
    mergestate =   get_mergestate(node);
    estate = (EState) get_state(node);
    direction =    get_es_direction(estate);
    innerPlan =    get_innerPlan(node);
    outerPlan =    get_outerPlan(node);
    econtext =     get_cs_ExprContext(mergestate);
    mergeclauses = get_mergeclauses(node);
    qual = 	   get_qpqual(node);
    
    if (direction == EXEC_FRWD) {
	outerSkipQual = get_mj_OSortopI(mergestate);
	innerSkipQual = get_mj_ISortopO(mergestate);
    } else {
	outerSkipQual = get_mj_ISortopO(mergestate);
	innerSkipQual = get_mj_OSortopI(mergestate);
    }

    /* ----------------
     *	ok, everything is setup.. let's go to work
     * ----------------
     */
    for (;;) {
	/* ----------------
	 *  get the current state of the join and do things accordingly.
	 *  Note: The join states are highlighted with 32-* comments for
	 *        improved readability.
	 * ----------------
	 */
	MJ_dump(econtext, mergestate);
	
	switch (get_mj_JoinState(mergestate)) {
	/* ********************************
	 *	EXEC_MJ_INITIALIZE means that this is the first time
	 *	ExecMergeJoin() has been called and so we have to
	 *	initialize the inner, outer and marked tuples as well
	 *	as various stuff in the expression context.
	 * ********************************
	 */
	case EXEC_MJ_INITIALIZE:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_INITIALIZE\n");
	    /* ----------------
	     *   Note: at this point, if either of our inner or outer
	     *   tuples are nil, then the join ends immediately because
	     *   we know one of the subplans is empty.
	     * ----------------
	     */
	    innerTupleSlot = ExecProcNode(innerPlan);
	    if (TupIsNull(innerTupleSlot)) {
		MJ_printf("ExecMergeJoin: **** inner tuple is nil ****\n");
		return NULL;
	    }
	    
	    outerTupleSlot = ExecProcNode(outerPlan);
	    if (TupIsNull(outerTupleSlot)) {
		MJ_printf("ExecMergeJoin: **** outer tuple is nil ****\n");
		return NULL;
	    }
	    
	    /* ----------------
	     *   store the inner and outer tuple in the merge state
	     * ----------------
	     */
	    set_ecxt_innertuple(econtext, innerTupleSlot);
	    set_ecxt_outertuple(econtext, outerTupleSlot);
	    
	    /* ----------------
	     *	 set the marked tuple to nil
	     * ----------------
	     */
	    ExecClearTuple(get_mj_MarkedTupleSlot(mergestate));
	    
	    /* ----------------
	     *  initialize merge join state to skip inner tuples.
	     * ----------------
	     */
	    set_mj_JoinState(mergestate, EXEC_MJ_SKIPINNER);
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_JOINMARK means we have just found a new
	 *  	outer tuple and a possible matching inner tuple.
	 *  	This is the case after the INITIALIZE, SKIPOUTER
	 *  	or SKIPINNER states.
	 * ********************************
	 */
	case EXEC_MJ_JOINMARK:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_JOINMARK\n");
	    ExecMarkPos(innerPlan);
	    
	    innerTupleSlot = get_ecxt_innertuple(econtext);
	    MarkInnerTuple(innerTupleSlot, mergestate);
	    
	    set_mj_JoinState(mergestate, EXEC_MJ_JOINTEST);
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_JOINTEST means we have two tuples which
	 *  	might satisify the merge clause, so we test them.
	 *
	 *  	If they do satisify, then we join them and move
	 *  	on to the next inner tuple (EXEC_MJ_JOINTUPLES).
	 *
	 *	If they do not satisify then advance to next outer tuple.
	 * ********************************
	 */
	case EXEC_MJ_JOINTEST:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_JOINTEST\n");
	    
	    qualResult = ExecQual(mergeclauses, econtext);
	    MJ_DEBUG_QUAL(mergeclauses, qualResult);
	    
	    if (qualResult)
		set_mj_JoinState(mergestate, EXEC_MJ_JOINTUPLES);
	    else 
		set_mj_JoinState(mergestate, EXEC_MJ_NEXTOUTER);
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_JOINTUPLES means we have two tuples which
	 *  	satisified the merge clause so we join them and then
	 * 	proceed to get the next inner tuple (EXEC_NEXT_INNER).
	 * ********************************
	 */
	case EXEC_MJ_JOINTUPLES:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_JOINTUPLES\n");
	    set_mj_JoinState(mergestate, EXEC_MJ_NEXTINNER);
	    
	    qualResult = ExecQual(qual, econtext);
	    MJ_DEBUG_QUAL(qual, qualResult);
	    
	    if (qualResult) {
		/* ----------------
		 *  qualification succeeded.  now form the desired
		 *  projection tuple and return the slot containing it.
		 * ----------------
		 */
		ProjectionInfo projInfo;
		
		MJ_printf("ExecMergeJoin: **** returning tuple ****\n");
		
		projInfo = get_cs_ProjInfo(mergestate);
		
		return
		    ExecProject(projInfo);
	    }
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_NEXTINNER means advance the inner scan
	 *  	to the next tuple.  If the tuple is not nil, we then
	 *  	proceed to test it against the join qualification.
	 * ********************************
	 */
	case EXEC_MJ_NEXTINNER:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_NEXTINNER\n");
	    
	    /* ----------------
	     *	now we get the next inner tuple, if any
	     * ----------------
	     */
	    innerTupleSlot = ExecProcNode(innerPlan);
	    MJ_DEBUG_PROC_NODE(innerTupleSlot);
	    set_ecxt_innertuple(econtext, innerTupleSlot);
	    
	    if (TupIsNull(innerTupleSlot))
		set_mj_JoinState(mergestate, EXEC_MJ_NEXTOUTER);
	    else
		set_mj_JoinState(mergestate, EXEC_MJ_JOINTEST);
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_NEXTOUTER means
	 *
	 *		  outer	inner
	 *   outer tuple -  5	  5  - marked tuple    
	 *		    5     5
	 *		    6     6  - inner tuple
	 *		    7     7
	 *
	 *	we know we just bumped into
	 *	the first inner tuple > current outer tuple
	 *  	so get a new outer tuple and then proceed to test
	 *	it against the marked tuple (EXEC_MJ_TESTOUTER)
	 * ********************************
	 */
	case EXEC_MJ_NEXTOUTER:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_NEXTOUTER\n");
	    
	    outerTupleSlot = ExecProcNode(outerPlan);
	    MJ_DEBUG_PROC_NODE(outerTupleSlot);
	    set_ecxt_outertuple(econtext, outerTupleSlot);
	    
	    /* ----------------
	     *  if the outer tuple is null then we know
	     *  we are done with the join
	     * ----------------
	     */
            if (TupIsNull(outerTupleSlot)) {
		MJ_printf("ExecMergeJoin: **** outer tuple is nil ****\n");
		return NULL;
	    }
	    
	    set_mj_JoinState(mergestate, EXEC_MJ_TESTOUTER);
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_TESTOUTER
	 *  	If the new outer tuple and the marked tuple satisify
	 *  	the merge clause then we know we have duplicates in
	 *  	the outer scan so we have to restore the inner scan
	 *  	to the marked tuple and proceed to join the new outer
	 *  	tuples with the inner tuples (EXEC_MJ_JOINTEST)
	 *
	 *	This is the case when
	 *
	 *			  outer	inner
	 *			    4	  5  - marked tuple    
	 *	     outer tuple -  5	  5  
	 *	 new outer tuple -  5     5
	 *			    6     8  - inner tuple
	 *			    7    12
	 *
	 *		new outer tuple = marked tuple
	 *
	 *  	If the outer tuple fails the test, then we know we have
	 *  	to proceed to skip outer tuples until outer >= inner
	 *  	(EXEC_MJ_SKIPOUTER).
	 *
	 *	This is the case when
	 *
	 *			  outer	inner
	 *	   		    5	  5  - marked tuple    
	 *	     outer tuple -  5     5
	 *	 new outer tuple -  6     8  - inner tuple
	 *			    7    12
	 *
	 *		new outer tuple > marked tuple
	 *
	 * ********************************
	 */
	case EXEC_MJ_TESTOUTER:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_TESTOUTER\n");
	    
	    /* ----------------
	     *  here we compare the outer tuple with the marked inner tuple
	     *  by using the marked tuple in place of the inner tuple.
	     * ----------------
	     */
	    innerTupleSlot =  get_ecxt_innertuple(econtext);
	    markedTupleSlot = get_mj_MarkedTupleSlot(mergestate);
	    set_ecxt_innertuple(econtext, markedTupleSlot);
	    
	    qualResult = ExecQual(mergeclauses, econtext);
	    MJ_DEBUG_QUAL(mergeclauses, qualResult);
	    
	    if (qualResult) {
		/* ----------------
		 *  the merge clause matched so now we juggle the slots
		 *  back the way they were and proceed to JOINTEST.
		 * ----------------
		 */
		set_ecxt_innertuple(econtext, innerTupleSlot);

		RestoreInnerTuple(innerTupleSlot, markedTupleSlot);
		
		ExecRestrPos(innerPlan);
		set_mj_JoinState(mergestate, EXEC_MJ_JOINTEST);
		
	    } else {
		/* ----------------
		 *  if the inner tuple was nil and the new outer
		 *  tuple didn't match the marked outer tuple then
		 *  we may have the case:
		 *
		 *		outer	inner
		 *		    4	  4   - marked tuple
		 *	new outer - 5     4
		 *		    6    nil  - inner tuple
		 *		    7
		 *
		 *  which means that all subsequent outer tuples will be
		 *  larger than our inner tuples.  
		 * ----------------
		 */
		if (TupIsNull(innerTupleSlot)) {
		    MJ_printf("ExecMergeJoin: **** wierd case 1 ****\n");
		    return NULL;
		}
		
		/* ----------------
		 *  restore the inner tuple and continue on to
		 *  skip outer tuples.
		 * ----------------
		 */
		set_ecxt_innertuple(econtext, innerTupleSlot);
		set_mj_JoinState(mergestate, EXEC_MJ_SKIPOUTER);
	    }
	    break;
	    
	/* ********************************
	 *	EXEC_MJ_SKIPOUTER means skip over tuples in the outer plan
	 *  	until we find an outer tuple > current inner tuple.
	 *
	 *	For example:
	 *
	 *			  outer	inner
	 *	     		    5	  5  
	 *	 		    5     5
	 *	     outer tuple -  6     8  - inner tuple
	 *			    7    12
	 *			    8    14
	 *
	 *		we have to advance the outer scan
	 *		until we find the outer 8.
	 *
	 * ********************************
	 */
	case EXEC_MJ_SKIPOUTER:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_SKIPOUTER\n");
	    /* ----------------
	     *	before we advance, make sure the current tuples
	     *  do not satisify the mergeclauses.  If they do, then
	     *  we update the marked tuple and go join them.
	     * ----------------
	     */
	    qualResult = ExecQual(mergeclauses, econtext);
	    MJ_DEBUG_QUAL(mergeclauses, qualResult);

	    if (qualResult) {
		ExecMarkPos(innerPlan);
		innerTupleSlot = get_ecxt_innertuple(econtext);

		MarkInnerTuple(innerTupleSlot, mergestate);
		
		set_mj_JoinState(mergestate, EXEC_MJ_JOINTUPLES);
		break;
	    }

	    /* ----------------
	     *	ok, now test the skip qualification
	     * ----------------
	     */
	    compareResult = MergeCompare(mergeclauses,
					 outerSkipQual,
					 econtext);
	    
	    MJ_DEBUG_MERGE_COMPARE(outerSkipQual, compareResult);

	    /* ----------------
	     *	compareResult is true as long as we should
	     *  continue skipping tuples.
	     * ----------------
	     */
	    if (compareResult) {
		
		outerTupleSlot = ExecProcNode(outerPlan);
		MJ_DEBUG_PROC_NODE(outerTupleSlot);
		set_ecxt_outertuple(econtext, outerTupleSlot);
		
		/* ----------------
		 *  if the outer tuple is null then we know
		 *  we are done with the join
		 * ----------------
		 */
		if (TupIsNull(outerTupleSlot)) {
		    MJ_printf("ExecMergeJoin: **** outerTuple is nil ****\n");
		    return NULL;
                }
		/* ----------------
		 *  otherwise test the new tuple against the skip qual.
		 *  (we remain in the EXEC_MJ_SKIPOUTER state)
		 * ----------------
		 */
		break;
	    }
	    
	    /* ----------------
	     *	now check the inner skip qual to see if we
	     *  should now skip inner tuples... if we fail the
	     *  inner skip qual, then we know we have a new pair
	     *  of matching tuples.
	     * ----------------
	     */
	    compareResult = MergeCompare(mergeclauses,
					 innerSkipQual,
					 econtext);

	    MJ_DEBUG_MERGE_COMPARE(innerSkipQual, compareResult);

	    if (compareResult)
		set_mj_JoinState(mergestate, EXEC_MJ_SKIPINNER);
	    else
		set_mj_JoinState(mergestate, EXEC_MJ_JOINMARK);
	    break;

	/* ********************************
	 *	EXEC_MJ_SKIPINNER means skip over tuples in the inner plan
	 *  	until we find an inner tuple > current outer tuple.
	 *
	 *	For example:
	 *
	 *			  outer	inner
	 *	     		    5	  5   
	 *	 		    5     5
	 *	     outer tuple - 12     8 - inner tuple
	 *			   14    10
	 *			   17    12
	 *
	 *		we have to advance the inner scan
	 *		until we find the inner 12.
	 *
	 * ********************************
	 */
	case EXEC_MJ_SKIPINNER:
	    MJ_printf("ExecMergeJoin: EXEC_MJ_SKIPINNER\n");
	    /* ----------------
	     *	before we advance, make sure the current tuples
	     *  do not satisify the mergeclauses.  If they do, then
	     *  we update the marked tuple and go join them.
	     * ----------------
	     */
	    qualResult = ExecQual(mergeclauses, econtext);
	    MJ_DEBUG_QUAL(mergeclauses, qualResult);
	    
	    if (qualResult) {
		ExecMarkPos(innerPlan);
		innerTupleSlot = get_ecxt_innertuple(econtext);

		MarkInnerTuple(innerTupleSlot, mergestate);
		
		set_mj_JoinState(mergestate, EXEC_MJ_JOINTUPLES);
		break;
	    }

	    /* ----------------
	     *	ok, now test the skip qualification
	     * ----------------
	     */
	    compareResult = MergeCompare(mergeclauses,
					 innerSkipQual,
					 econtext);
	    
	    MJ_DEBUG_MERGE_COMPARE(innerSkipQual, compareResult);
	    
	    /* ----------------
	     *	compareResult is true as long as we should
	     *  continue skipping tuples.
	     * ----------------
	     */
	    if (compareResult) {
		/* ----------------
		 *  now try and get a new inner tuple
		 * ----------------
		 */
		innerTupleSlot = ExecProcNode(innerPlan);
		MJ_DEBUG_PROC_NODE(innerTupleSlot);
		set_ecxt_innertuple(econtext, innerTupleSlot);
		
		/* ----------------
		 *  if the inner tuple is null then we know
		 *  we have to restore the inner scan
		 *  and advance to the next outer tuple
		 * ----------------
		 */
		if (TupIsNull(innerTupleSlot)) {
		    markedTupleSlot = get_mj_MarkedTupleSlot(mergestate);
		    if (TupIsNull(markedTupleSlot)) {
			/* ----------------
			 *  this is an interesting case.. all our
			 *  inner tuples are smaller then our outer
			 *  tuples so we never found an inner tuple
			 *  to mark.
			 *
			 *		  outer	inner
			 *   outer tuple -  5	  4     
			 *		    5     4
			 *		    6    nil  - inner tuple
			 *		    7
			 *
			 *  This means the join should end.
			 * ----------------
			 */
			MJ_printf("ExecMergeJoin: **** wierd case 2 ****\n");
			return NULL;
		    }
		    set_ecxt_innertuple(econtext, markedTupleSlot);
		    ExecRestrPos(innerPlan);
		    
		    set_mj_JoinState(mergestate, EXEC_MJ_NEXTOUTER);
		    break;
		}
		
		/* ----------------
		 *  otherwise test the new tuple against the skip qual.
		 *  (we remain in the EXEC_MJ_SKIPINNER state)
		 * ----------------
		 */
		break;
	    }
	    
	    /* ----------------
	     *  compare finally failed and we have stopped skipping
	     *  inner tuples so now check the outer skip qual
	     *  to see if we should now skip outer tuples...
	     * ----------------
	     */
	    compareResult = MergeCompare(mergeclauses,
					 outerSkipQual,
					 econtext);
	    
	    MJ_DEBUG_MERGE_COMPARE(outerSkipQual, compareResult);
	    
	    if (compareResult)
		set_mj_JoinState(mergestate, EXEC_MJ_SKIPOUTER);
	    else
		set_mj_JoinState(mergestate, EXEC_MJ_JOINMARK);
	    
	    break;
	    
	/* ********************************
	 *	if we get here it means our code is fucked up and
	 *  	so we just end the join prematurely.
	 * ********************************
	 */
	default:
	    elog(NOTICE, "ExecMergeJoin: invalid join state. aborting");
	    return NULL;
	}
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecInitMergeJoin
 *
 * old comments
 *   	Creates the run-time state information for the node and
 *   	sets the relation id to contain relevant decriptors.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitMergeJoin(node, estate, parent)
    MergeJoin 	node;
    EState 	estate;
    Plan	parent;
{
    MergeJoinState	mergestate;
    List		joinclauses;
    List		rightorder;
    List		leftorder;
    ObjectId		rightsortop;
    ObjectId		leftsortop;
    ObjectId		sortop;
    
    List		OSortopI;
    List		ISortopO;
    
    ParamListInfo       paraminfo;
    ExprContext	        econtext;
    int			baseid;
        
    MJ1_printf("ExecInitMergeJoin: %s\n",
	       "initializing node");
	
    /* ----------------
     *  assign the node's execution state and 
     *	get the range table and direction from it
     * ----------------
     */
    set_state(node, estate);
    
    /* ----------------
     *	create new merge state for node
     * ----------------
     */
    mergestate = MakeMergeJoinState();
    set_mergestate(node, mergestate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, mergestate, parent);
    ExecAssignDebugHooks(node, mergestate);
    ExecAssignExprContext(estate, mergestate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, mergestate);
    ExecInitMarkedTupleSlot(estate, mergestate);
    
    /* ----------------
     *	get merge sort operators.
     *
     *  XXX for now we assume all quals in the joinclauses were
     *      sorted with the same operator in both the inner and
     *      outer relations. -cim 11/2/89
     * ----------------
     */
    joinclauses = get_mergeclauses(node);
    rightorder =  get_mergerightorder(node);
    leftorder =   get_mergeleftorder(node);
    
    rightsortop = get_opcode(CAR(rightorder));
    leftsortop =  get_opcode(CAR(leftorder));
    
    if (leftsortop != rightsortop)
	elog(NOTICE, "ExecInitMergeJoin: %s",
	     "left and right sortop's are unequal!");
    
    sortop = rightsortop;
    
    /* ----------------
     *	form merge skip qualifications
     *
     *  XXX MJform routines need to be extended
     *	    to take a list of sortops.. -cim 11/2/89
     * ----------------
     */
    OSortopI = MJFormOSortopI(joinclauses, sortop);
    ISortopO = MJFormISortopO(joinclauses, sortop);
    set_mj_OSortopI(mergestate, OSortopI);
    set_mj_ISortopO(mergestate, ISortopO);

    MJ_printf("\nExecInitMergeJoin: OSortopI is ");
    MJ_lispDisplay(OSortopI);
    MJ_printf("\nExecInitMergeJoin: ISortopO is ");
    MJ_lispDisplay(ISortopO);
    MJ_printf("\n");
    
    /* ----------------
     *	initialize join state
     * ----------------
     */
    set_mj_JoinState(mergestate, EXEC_MJ_INITIALIZE);
    
    /* ----------------
     *	initialize subplans
     * ----------------
     */
    ExecInitNode((Plan) get_outerPlan(node), estate, node);
    ExecInitNode((Plan) get_innerPlan(node), estate, node);
        
    /* ----------------
     * 	initialize tuple type and projection info
     * ----------------
     */
    ExecAssignResultTypeFromTL(node, mergestate);
    ExecAssignProjectionInfo(node, mergestate);
    
    /* ----------------
     *	initialization successful
     * ----------------
     */
    MJ1_printf("ExecInitMergeJoin: %s\n",
	       "node initialized");
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndMergeJoin
 *
 * old comments
 *   	frees storage allocated through C routines.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndMergeJoin(node)
    MergeJoin 	node;
{
    MergeJoinState	mergestate;
    Pointer	    	tupValue;
    
    MJ1_printf("ExecEndMergeJoin: %s\n",
	       "ending node processing");
	    
    /* ----------------
     *	get state information from the node
     * ----------------
     */
    mergestate = get_mergestate(node);

    /* ----------------
     *	Free the projection info and the scan attribute info
     *
     *  Note: we don't ExecFreeResultType(mergestate) 
     *        because the rule manager depends on the tupType
     *	      returned by ExecMain().  So for now, this
     *	      is freed at end-transaction time.  -cim 6/2/91     
     * ----------------
     */    
    ExecFreeProjectionInfo(mergestate);
    
    /* ----------------
     *	shut down the subplans
     * ----------------
     */
    ExecEndNode((Plan) get_innerPlan(node));
    ExecEndNode((Plan) get_outerPlan(node));

    /* ----------------
     *	clean out the tuple table so that we don't try and
     *  pfree the marked tuples..  see HACK ALERT at the top of
     *  this file.
     * ----------------
     */
    ExecClearTuple(get_cs_ResultTupleSlot(mergestate));
    ExecClearTuple(get_mj_MarkedTupleSlot(mergestate));
    
    MJ1_printf("ExecEndMergeJoin: %s\n",
	       "node processing ended");
}
 
