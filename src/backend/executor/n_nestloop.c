/* ----------------------------------------------------------------
 *   FILE
 *	nestloop.c
 *	
 *   DESCRIPTION
 *	routines to support nest-loop joins
 *
 *   INTERFACE ROUTINES
 *   	ExecNestLoop	 - process a nestloop join of two plans
 *   	ExecInitNestLoop - initialize the join
 *   	ExecEndNestLoop	 - shut down the join
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
 *   	ExecNestLoop(node)
 *
 * old comments
 *   	Returns the tuple joined from inner and outer tuples which 
 *   	satisfies the qualification clause.
 *
 *	It scans the inner relation to join with current outer tuple.
 *
 *	If none is found, next tuple form the outer relation is retrieved
 *	and the inner relation is scanned from the beginning again to join
 *	with the outer tuple.
 *
 *   	Nil is returned if all the remaining outer tuples are tried and
 *   	all fail to join with the inner tuples.
 *
 *   	Nil is also returned if there is no tuple from inner realtion.
 *   
 *   	Conditions:
 *   	  -- outerTuple contains current tuple from outer relation and
 *   	     the right son(inner realtion) maintains "cursor" at the tuple
 *   	     returned previously.
 *              This is achieved by maintaining a scan position on the outer
 *              relation.
 *   
 *   	Initial States:
 *   	  -- the outer child and the inner child 
 *             are prepared to return the first tuple.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot
ExecNestLoop(node)
    NestLoop node;
{
    NestLoopState 	nlstate;
    Plan	  	innerPlan;
    Plan	  	outerPlan;
    bool	  	portalFlag;
    bool 	  	needNewOuterTuple;
    
    TupleTableSlot  	outerTupleSlot;
    TupleTableSlot  	innerTupleSlot;
    
    TupleDescriptor  	tupType;
    Pointer	  	tupValue;
    List	  	targetList;
    int		  	len;
    
    List	  	qual;
    bool	  	qualResult;
    ExprContext	  	econtext;
    JoinRuleInfo	ruleInfo;
    
    /* ----------------
     *	get information from the node
     * ----------------
     */
    ENL1_printf("getting info from node");
    
    nlstate =    get_nlstate(node);
    qual =       get_qpqual((Plan) node);
    outerPlan =  get_outerPlan((Plan) node);
    innerPlan =  get_innerPlan((Plan) node);
    ruleInfo =   (JoinRuleInfo) get_ruleinfo((Join) node);
    
    /* ----------------
     *	initialize expression context
     * ----------------
     */
    econtext = get_cs_ExprContext((CommonState)nlstate);
    
    /* ----------------
     *	get the current outer tuple
     * ----------------
     */
    outerTupleSlot = get_cs_OuterTupleSlot((CommonState)nlstate);
    set_ecxt_outertuple(econtext, outerTupleSlot);
    
    /* ----------------
     *  Ok, everything is setup for the join so now loop until
     *  we return a qualifying join tuple..
     * ----------------
     */
    ENL1_printf("entering main loop");
    for(;;) {
	/* ----------------
	 *  The essential idea now is to get the next inner tuple
	 *  and join it with the current outer tuple.
	 * ----------------
	 */
	needNewOuterTuple = false;
	
	/* ----------------
	 *  If outer tuple is not null then that means
	 *  we are in the middle of a scan and we should
	 *  restore our previously saved scan position.
	 * ----------------
	 */
	if (! TupIsNull((Pointer) outerTupleSlot)) {	    
	    ENL1_printf("have outer tuple, restoring outer plan");
	    ExecRestrPos(outerPlan);
	} else {
	    ENL1_printf("outer tuple is nil, need new outer tuple");
	    needNewOuterTuple = true;
	}
	
	/* ----------------
	 *  portalFlag can be thought of as a
	 *  "need to rescan outer relation" flag.
	 *  I don't really understand it beyond that..  -cim 8/31/89
	 * ----------------
	 */
	portalFlag =  get_nl_PortalFlag(nlstate);
	if (portalFlag == false) {
	    ENL1_printf("portal flag false, need new outer tuple");
	    needNewOuterTuple = true;
	}
	
	/* ----------------
	 *  if we have an outerTuple, try to get the next inner tuple.
	 * ----------------
	 */
	if (!needNewOuterTuple) {
	    ENL1_printf("getting new inner tuple");
	
	    innerTupleSlot = ExecProcNode(innerPlan);
	    set_ecxt_innertuple(econtext, innerTupleSlot);
	    
	    if (TupIsNull((Pointer) innerTupleSlot)) {
		ENL1_printf("no inner tuple, need new outer tuple");
		needNewOuterTuple = true;
	    }
	}
	
	/* ----------------
	 *  loop until we have a new outer tuple and a new
	 *  inner tuple.
	 * ----------------
	 */
	while (needNewOuterTuple) {
	    /* ----------------
	     * If portalFlag is nil, rescan outer relation.
	     * ----------------
	     */
	    if (portalFlag == false)
		ExecReScan(outerPlan);
	    
	    /* ----------------
	     *	now try to get the next outer tuple
	     * ----------------
	     */
	    ENL1_printf("getting new outer tuple");
	    outerTupleSlot = ExecProcNode(outerPlan);
	    set_ecxt_outertuple(econtext, outerTupleSlot);
	    
	    /* ----------------
	     *  if there are no more outer tuples, then the join
	     *  is complete..
	     * ----------------
	     */
	    if (TupIsNull((Pointer) outerTupleSlot)) {
		ENL1_printf("no outer tuple, ending join");
		return NULL;
	    }
	    
	    /* ----------------
	     *  we have a new outer tuple so we mark our position
	     *  in the outer scan and save the outer tuple in the
	     *  NestLoop state
	     * ----------------
	     */
	    ENL1_printf("saving new outer tuple information");
	    ExecMarkPos(outerPlan);
	    set_nl_PortalFlag(nlstate, true);
	    set_cs_OuterTupleSlot((CommonState)nlstate, outerTupleSlot);
	    
	    /* ----------------
	     *	now rescan the inner plan and get a new inner tuple
	     * ----------------
	     */
	    
	    /* ----------------
	     *	  Nest Loop joins with indexscans in the inner plan
	     *	  are treated specially (hack hack hack).. since the
	     *    idea is to first get an outer tuple and then do
	     *	  an index scan on the inner relation to find a matching
	     *	  inner tuple, this means the inner indexscan's scan
	     *    key is now a function of the values in the outer tuple.
	     *    This means we have to recalculate the proper scan key
	     *    values so the index scan will work correctly.
	     *
	     *    Note: the proper thing to do would be to redesign
	     *    things so qualifications could refer to current attribute
	     *    values of tuples elsewhere in the plan..  In fact, this
	     *    may have to be done when we start implementing
	     *    bushy trees..  But for now our plans are simple and
	     *    we can get by doing cheezy stuff.
	     * ----------------
	     */
	    if (ExecIsIndexScan(innerPlan)) {
		ENL1_printf("recalculating inner scan keys");
		ExecUpdateIndexScanKeys((IndexScan) innerPlan, econtext);
	    }
	    
	    ENL1_printf("rescanning inner plan");
	    ExecReScan(innerPlan);
	    
	    /* ----------------
	     *    If we are running in a 'insert rule lock & stubs'
	     *    mode, insert the appropriate rule stubs to the
	     *    inner relation.
	     * ----------------
	     */
	    if (ruleInfo != (JoinRuleInfo) NULL) {
		/*
		 * NOTE: the inner plan must be a scan!
		 */
		ScanState 	  scanState;
		RelationRuleInfo  relRuleInfo;
		Relation 	  innerRelation;
		Prs2OneStub 	  oneStub;
		HeapTuple 	  outerHeapTuple;
		Buffer 		  buf;
		TupleDescriptor	  outerTupleDesc;
		TupleDescriptor   innerTupleDesc;
		
		if (!ExecIsSeqScan(innerPlan) && !ExecIsIndexScan(innerPlan)){
		    elog(WARN,"Rule stub code: inner plan not a scan");
		}
		
		outerHeapTuple = (HeapTuple) ExecFetchTuple((Pointer)
							    outerTupleSlot);
		buf = ExecSlotBuffer((Pointer)outerTupleSlot);

		/*
		 * add a rule stub in the inner relation.
		 */
		scanState = 		get_scanstate((Scan) innerPlan);
		relRuleInfo =
		    get_css_ruleInfo((CommonScanState) scanState);
		innerRelation =
		    get_css_currentRelation((CommonScanState)scanState);

		/*
		 * get tuple descs for the rule manager.  XXX why can't
		 * the rule manager just take the econtext as an argument
		 * and get this information itself?  ExecGetTupType is
		 * an obsolete way of doing things. -cim 6/3/91
		 */
		outerTupleDesc = ExecGetTupType(outerPlan);
		innerTupleDesc = ExecGetTupType(innerPlan);
		
		oneStub =
		    prs2MakeStubForInnerRelation(ruleInfo,
						 outerHeapTuple,
						 buf,
						 outerTupleDesc,
						 innerTupleDesc);
		
		prs2AddOneStub(relRuleInfo->relationStubs, oneStub);
		relRuleInfo->relationStubsHaveChanged = true;
		set_css_ruleInfo((CommonScanState)scanState, relRuleInfo);
		
		/*
		 * also store this stub to the 'ruleInfo'
		 */
		set_jri_stub(ruleInfo, oneStub);
		
		/*
		 * update the statistics
		 */
		prs2UpdateStats(ruleInfo, PRS2_ADDSTUB);
	    }

	    ENL1_printf("getting new inner tuple");
	    
	    innerTupleSlot = ExecProcNode(innerPlan);
	    set_ecxt_innertuple(econtext, innerTupleSlot);
	    
	    if (TupIsNull((Pointer) innerTupleSlot)) {
		ENL1_printf("couldn't get inner tuple - need new outer tuple");
	    } else {
		ENL1_printf("got inner and outer tuples");
		needNewOuterTuple = false;
	    }
	}
	
	/* ----------------
	 *   at this point we have a new pair of inner and outer
	 *   tuples so we test the inner and outer tuples to see
	 *   if they satisify the node's qualification.
	 * ----------------
	 */
	ENL1_printf("testing qualification");
	qualResult = ExecQual(qual, econtext);
	
	/* ----------------
	 *   if we are in a 'set rule locks' mode, and the inner tuple
	 *   satisfies the appropriate qualification, put a new
	 *   lock on it.
	 * ----------------
	 */
	if (ruleInfo != (JoinRuleInfo) NULL) {
	    RuleLock 		lock;
	    Relation 		innerRelation;
	    Prs2OneStub 	oneStub;
	    HeapTuple 		innerHeapTuple;
	    ScanState 		scanState;
	    TupleTableSlot 	scanTupleSlot;
	    bool		newExpLocks;
	    Buffer 		buf;

	    if (!ExecIsSeqScan(innerPlan) && !ExecIsIndexScan(innerPlan)) {
		elog(WARN,"Rule stub code: inner plan not a scan");
	    }
	    scanState = 	get_scanstate((Scan) innerPlan);
	    innerRelation = 	get_css_currentRelation((CommonScanState)
							scanState);
	    oneStub = 		get_jri_stub(ruleInfo);
	    scanTupleSlot = 	(TupleTableSlot)
		get_css_ScanTupleSlot((CommonScanState)scanState);
	    
	    innerHeapTuple = (HeapTuple)
		ExecFetchTuple((Pointer)scanTupleSlot);
	    
	    buf = ExecSlotBuffer((Pointer) scanTupleSlot);
	    
	    if (prs2AddLocksAndReplaceTuple(innerHeapTuple,
					    buf,
					    innerRelation,
					    oneStub,
					    &newExpLocks)) {
		prs2UpdateStats(ruleInfo, PRS2_ADDLOCK);
	    }
	    if (!newExpLocks)
		qualResult = false;
	}

	if (qualResult) {
	    /* ----------------
	     *  qualification was satisified so we project and
	     *  return the slot containing the result tuple
	     *  using ExecProject().
	     * ----------------
	     */
	    ProjectionInfo projInfo;
	    
	    ENL1_printf("qualification succeeded, projecting tuple");
	    
	    projInfo = get_cs_ProjInfo((CommonState)nlstate);
	    return
		ExecProject(projInfo);
	} 
	
	/* ----------------
	 *  qualification failed so we have to try again..
	 * ----------------
	 */
	ENL1_printf("qualification failed, looping");
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecInitNestLoop
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
ExecInitNestLoop(node, estate, parent)
    NestLoop node;
    EState   estate;
    Plan     parent;
{
    NestLoopState   nlstate;
    TupleDescriptor tupType;
    Pointer	    tupValue;
    List	    targetList;
    int		    len;
    ExprContext	    econtext;
    ParamListInfo   paraminfo;
    int		    baseid;
    
    NL1_printf("ExecInitNestLoop: %s\n",
	       "initializing node");
	
    /* ----------------
     *	assign execution state to node
     * ----------------
     */
    set_state((Plan) node, estate);
    
    /* ----------------
     *    create new nest loop state
     * ----------------
     */
    nlstate = MakeNestLoopState(false);
    set_nlstate(node, nlstate);
        
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) nlstate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode)nlstate);
    ExecAssignExprContext(estate, (CommonState)nlstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, (CommonState)nlstate);
         
    /* ----------------
     *    now initialize children
     * ----------------
     */
    ExecInitNode(get_outerPlan((Plan)node), estate, (Plan)node);
    ExecInitNode(get_innerPlan((Plan)node), estate, (Plan) node);
    set_nl_PortalFlag(nlstate, true);
        
    /* ----------------
     * 	initialize tuple type and projection info
     * ----------------
     */
    ExecAssignResultTypeFromTL((Plan) node, (CommonState)nlstate);
    ExecAssignProjectionInfo((Plan) node, (CommonState) nlstate);
    
    /* ----------------
     *  finally, wipe the current outer tuple clean.
     * ----------------
     */
    set_cs_OuterTupleSlot((CommonState)nlstate, NULL);
    
    NL1_printf("ExecInitNestLoop: %s\n",
	       "node initialized");
    return LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndNestLoop
 *   
 *   	closes down scans and frees allocated storage
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
List
ExecEndNestLoop(node)
    NestLoop node;
{
    NestLoopState   nlstate;
    Pointer	    tupValue;
    
    NL1_printf("ExecEndNestLoop: %s\n",
	       "ending node processing");
    
    /* ----------------
     *	get info from the node
     * ----------------
     */
    nlstate =  get_nlstate(node);
    
    /* ----------------
     *	Free the projection info
     *
     *  Note: we don't ExecFreeResultType(nlstate) 
     *        because the rule manager depends on the tupType
     *	      returned by ExecMain().  So for now, this
     *	      is freed at end-transaction time.  -cim 6/2/91     
     * ----------------
     */    
    ExecFreeProjectionInfo((CommonState)nlstate);

    /* ----------------
     *	close down subplans
     * ----------------
     */
    ExecEndNode(get_outerPlan((Plan) node));
    ExecEndNode(get_innerPlan((Plan) node));

    /* ----------------
     *	clean out the tuple table 
     * ----------------
     */
    ExecClearTuple((Pointer) get_cs_ResultTupleSlot((CommonState)nlstate));
    
    NL1_printf("ExecEndNestLoop: %s\n",
	       "node processing ended");
}
