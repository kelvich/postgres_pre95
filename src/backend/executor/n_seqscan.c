/* ----------------------------------------------------------------
 *   FILE
 *	seqscan.c
 *	
 *   DESCRIPTION
 *	Support routines for sequential scans of relations.
 *
 *   INTERFACE ROUTINES
 *   	ExecSeqScan		sequentially scans a relation.
 *    	ExecSeqNext 		retrieve next tuple in sequential order.
 *   	ExecInitSeqScan 	creates and initializes a seqscan node.
 *   	ExecEndSeqScan		releases any storage allocated.
 *   	ExecSeqReScan		rescans the relation
 *   	ExecMarkPos		marks scan position
 *   	ExecRestrPos		restores scan position
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
 *	1) execdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h
 *	6) externs.h comes last
 * ----------------
 */
#include "executor/execdebug.h"

#include "parser/parsetree.h"
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
#include "parser/parse.h"	/* for RETRIEVE */
#include "tcop/slaves.h"

/* ----------------------------------------------------------------
 *   			Scan Support
  * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *	SeqNext
 *
 *|	This is a workhorse for ExecSeqScan
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
TupleTableSlot
SeqNext(node)
    SeqScan node;
{
    HeapTuple	 	tuple;
    HeapScanDesc  	scandesc;
    ScanState	 	scanstate;
    EState	 	estate;
    ScanDirection	direction;
    int			parallel;
    TupleTableSlot	slot;
    Buffer		buffer;
    
    /* ----------------
     *	get information from the estate and scan state
     * ----------------
     */
    estate =        (EState) get_state(node);
    scanstate =     get_scanstate(node);
    scandesc =      get_css_currentScanDesc(scanstate);
    direction =     get_es_direction(estate);
    parallel =      get_parallel(node);

    if (ParallelExecutorEnabled()) {
	scandesc->pageskip = parallel;
	scandesc->initskip = SlaveInfoP[MyPid].groupPid;
      }
    else {
	scandesc->pageskip = 1;
	scandesc->initskip = 0;
      }

    /* ----------------
     *	get the next tuple from the access methods
     * ----------------
     */
    tuple = heap_getnext(scandesc, 		   /* scan desc */
			 (direction == EXEC_BKWD), /* backward flag */
			 &buffer); 	           /* return: buffer */

    /* ----------------
     *	save the tuple and the buffer returned to us by the access methods
     *  in our scan tuple slot and return the slot.  Note: we pass 'false'
     *  because tuples returned by heap_getnext() are pointers onto
     *  disk pages and were not created with palloc() and so should not
     *  be pfree()'d.
     * ----------------
     */
    slot = (TupleTableSlot)
	get_css_ScanTupleSlot(scanstate);

    slot = (TupleTableSlot)
	ExecStoreTuple(tuple,  /* tuple to store */
		       slot,   /* slot to store in */
		       buffer, /* buffer associated with this tuple */
		       false); /* don't pfree this pointer */
    
    /* ----------------
     *  XXX -- mao says:  The sequential scan for heap relations will
     *  automatically unpin the buffer this tuple is on when we cross
     *  a page boundary.  The clearslot code also does this.  We bump
     *  the pin count on the page here, since we actually have two
     *  pointers to it -- one in the scan desc and one in the tuple
     *  table slot.  --mar 20 91
     * ----------------
     */
    ExecIncrSlotBufferRefcnt(slot);

    return slot;
}
 
/* ----------------------------------------------------------------
 *	ExecSeqScan(node)
 *
 * old comments
 *   	Scans the realtion sequentially and returns the next qualifying 
 *   	tuple in the direction specified in  execDireciton.
 *   	It calls the ExecScan() routine ans passes it the access method
 *   	which retrieve tuples sequentially.
 *   
 *   	Conditions:
 *   	  -- the "cursor" maintained by the AMI is positioned at the tuple
 *   	     returned previously.
 *   
 *   	Initial States:
 *   	  -- the relation indicated is opened for scanning so that the 
 *   	     "cursor" is positioned before the first qualifying tuple.
 *   	  -- state variable ruleFlag = nil.
 *
 *	uses SeqNext() as access method
 *
 *    XXX
 *    fix buffer-page stuff. probably put something in state node
 *    sequential scan calling AMI's getnext
 *
 * ----------------------------------------------------------------
 */
#define DEBUG_ExecSeqScan_1 \
    S_printf("ExecSeqScan: scanning node: "); \
    S_lispDisplay(node)

#define DEBUG_ExecSeqScan_2 \
    S1_printf("ExecSeqScan: returned tuple slot: %d\n", slot)

/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot
ExecSeqScan(node)
    SeqScan node;
{
    TupleTableSlot	slot;
    ScanState		scanstate;
    bool		procOuterFlag;
    Plan  		outerPlan;
    
    DEBUG_ExecSeqScan_1;
    
    /* ----------------
     *	get information from node
     * ----------------
     */
    scanstate =     get_scanstate(node);
    procOuterFlag = get_ss_ProcOuterFlag(scanstate);
    
   /* ----------------
    *	get a tuple from either the outer subplan or
    *   from the relation depending on the procOuterFlag.
    * ----------------
    */
    if (procOuterFlag) {
	outerPlan = get_outerPlan(node);
	slot = ExecProcNode(outerPlan);
    } else {
	slot = ExecScan((Plan) node,
			SeqNext); 	/* SeqNext is a function */
    }
    
    DEBUG_ExecSeqScan_2;
    
    return
	slot;
}
 
/* ----------------------------------------------------------------
 *   	InitScanRelation
 *
 *	This does the initialization for scan relations and
 *	subplans of scans.
 * ----------------------------------------------------------------
 */

ObjectId
InitScanRelation(node, estate, scanstate, outerPlan)
    Plan 		node;
    EState		estate;
    ScanState	        scanstate;
    Plan	        outerPlan;
{
    Index	        relid;
    List	        rangeTable;
    List	        rtentry;
    ObjectId		reloid;
    TimeQual		timeQual;
    ScanDirection   	direction;
    Relation		currentRelation;
    HeapScanDesc    	currentScanDesc;
    RelationInfo	resultRelationInfo;
    
    if (outerPlan == NULL) {
	/* ----------------
	 * if the outer node is nil then we are doing a simple
	 * sequential scan of a relation...
	 *
	 * get the relation object id from the relid'th entry
	 * in the range table, open that relation and initialize
	 * the scan state...
	 * ----------------
	 */
	relid =   		get_scanrelid(node);
	rangeTable = 		get_es_range_table(estate);
	rtentry = 		rt_fetch(relid, rangeTable);
	reloid =  		CInteger(rt_relid(rtentry));
	timeQual = 		(TimeQual) CInteger(rt_time(rtentry));
	direction =  		get_es_direction(estate);
	resultRelationInfo = 	get_es_result_relation_info(estate);
	
	ExecOpenScanR(reloid,		  /* relation */
		      0,		  /* nkeys */
		      NULL,		  /* scan key */
		      0,		  /* is index */
		      direction,          /* scan direction */
		      timeQual, 	  /* time qual */
		      &currentRelation,   /* return: rel desc */
		      &currentScanDesc);  /* return: scan desc */
	
	set_css_currentRelation(scanstate, currentRelation);
	set_css_currentScanDesc(scanstate, currentScanDesc);
	
	/* ----------------
	 *   Initialize rule info for this scan node
	 *
	 *   Note:  when we have a result relation which is actually
	 *	    a view managed by the tuple level rule system, the
	 *	    rule manager needs to be able to get at the raw tuples
	 *	    associated with the result relation.  To facilitate
	 *	    this, the estate now contains a pointer to the result
	 *	    relation's scan state.  -cim 3/17/91
	 * ----------------
	 */
	set_css_ruleInfo(scanstate,
	    prs2MakeRelationRuleInfo(currentRelation, RETRIEVE));
	
	if (resultRelationInfo != NULL &&
	    relid == get_ri_RangeTableIndex(resultRelationInfo)) {
	    set_es_result_rel_scanstate(estate, scanstate);
	}
	    
	ExecAssignScanType(scanstate, &currentRelation->rd_att);
	
	set_ss_ProcOuterFlag(scanstate, false);
    } else {
	/* ----------------
	 *   otherwise we are scanning tuples from the
	 *   outer subplan so we initialize the outer plan
	 *   and nullify 
	 * ----------------
	 */
	ExecInitNode(outerPlan, estate, node);
	
	set_scanrelid(node, 0);
	set_css_currentRelation(scanstate, NULL);
	set_css_currentScanDesc(scanstate, NULL);
	set_css_ruleInfo(scanstate, NULL);
	ExecAssignScanType(scanstate, NULL);
	set_ss_ProcOuterFlag(scanstate, true);

	reloid = InvalidObjectId;
    }

    /* ----------------
     *	return the relation
     * ----------------
     */
    return
	reloid;
}

 
/* ----------------------------------------------------------------
 *   	ExecInitSeqScan
 *
 * old comments
 *   	Creates the run-time state information for the seqscan node 
 *   	and sets the relation id to contain relevant descriptors.
 *   
 *   	If there is a outer subtree (hash or sort), the outer subtree
 *   	is initialized and the relation id is set to the descriptors
 *   	returned by the subtree.
 * ----------------------------------------------------------------
 */

/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitSeqScan(node, estate, parent)
    Plan 	node;
    EState	estate;
    Plan	parent;
{
    ScanState	        scanstate;
    Plan	        outerPlan;
    ObjectId		reloid;
    
    /* ----------------
     *  assign the node's execution state
     * ----------------
     */
    set_state(node, estate);
    
    /* ----------------
     *   create new ScanState for node
     * ----------------
     */
    scanstate = MakeScanState();
    set_scanstate(node, scanstate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, scanstate, parent);
    ExecAssignDebugHooks(node, scanstate);
    ExecAssignExprContext(estate, scanstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, scanstate);
    ExecInitScanTupleSlot(estate, scanstate);
    ExecInitRawTupleSlot(estate, scanstate);
    
    /* ----------------
     *	initialize scanrelid
     * ----------------
     */
    {
	Index relid;

	relid = get_scanrelid(node);
	set_ss_OldRelId(scanstate, relid);
    }
    
    /* ----------------
     * 	initialize tuple type
     * ----------------
     */
    ExecAssignResultTypeFromTL(node, scanstate);
    ExecAssignProjectionInfo(node, scanstate);
    
    /* ----------------
     *	initialize scanAttributes (used by rule_manager)
     * ----------------
     */
    ExecInitScanAttributes(node);
    
    /* ----------------
     *	initialize scan relation or outer subplan
     * ----------------
     */
    outerPlan = (Plan) get_outerPlan(node);
    
    reloid = InitScanRelation(node,
			      estate,
			      scanstate,
			      outerPlan);

    
    /* ----------------
     * return the object id of the relation
     * (I don't think this is ever used.. -cim 10/16/89)
     * ----------------
     */
    {
	List returnOid = (List)
	    MakeList(lispInteger(reloid), -1);
	
	return
	    returnOid;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecEndSeqScan
 *   
 *   	frees any storage allocated through C routines.
 *|	...and also closes relations and/or shuts down outer subplan
 *|	-cim 8/14/89
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndSeqScan(node)
    SeqScan node;
{
    ScanState		scanstate;
    Plan		outerPlan;
    AttributeNumberPtr	scanAtts;
    RelationRuleInfo	ruleInfo;
    
    /* ----------------
     *	get information from node
     * ----------------
     */
    scanstate = get_scanstate(node);
    scanAtts =	get_cs_ScanAttributes(scanstate);

    /* ----------------
     * Restore the relation level rule stubs.
     * ----------------
     */    
    ruleInfo = get_css_ruleInfo(scanstate);
    if (ruleInfo != NULL && ruleInfo->relationStubsHaveChanged) {
	ObjectId reloid;
	reloid = RelationGetRelationId(get_css_currentRelation(scanstate));
	prs2ReplaceRelationStub(reloid, ruleInfo->relationStubs);
    }

    /* ----------------
     *	Free the projection info and the scan attribute info
     *
     *  Note: we don't ExecFreeResultType(scanstate) 
     *        because the rule manager depends on the tupType
     *	      returned by ExecMain().  So for now, this
     *	      is freed at end-transaction time.  -cim 6/2/91     
     * ----------------
     */    
    ExecFreeProjectionInfo(scanstate);
    ExecFreeScanAttributes(scanAtts);
    
    /* ----------------
     * close scan relation
     * ----------------
     */
    ExecCloseR(node);
    
    /* ----------------
     * clean up outer subtree (does nothing if there is no outerPlan)
     * ----------------
     */
    outerPlan = get_outerPlan(node);
    ExecEndNode(outerPlan);
  
    /* ----------------
     *	clean out the tuple table
     * ----------------
     */
    ExecClearTuple(get_cs_ResultTupleSlot(scanstate));
    ExecClearTuple(get_css_ScanTupleSlot(scanstate));
    ExecClearTuple(get_css_RawTupleSlot(scanstate));
}
 
/* ----------------------------------------------------------------
 *   			Join Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *   	ExecSeqReScan(node)
 *   
 *   	Rescans the relation.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecReScan
 ****/
void
ExecSeqReScan(node)
    Plan node;
{
    ScanState 	  scanstate;
    EState	  estate;
    Plan	  outerPlan;
    Relation      rdesc;
    HeapScanDesc  sdesc;
    ScanDirection direction;
    
    scanstate = get_scanstate(node);
    estate =    (EState) get_state(node);
    
    if (get_ss_ProcOuterFlag(scanstate) == false) {
	/* ----------------
	 *   if the ProcOuterFlag is false then it means
	 *   we are scanning a relation so we use ExecReScanR()
	 * ----------------
	 */
	rdesc = 	get_css_currentRelation(scanstate);
	sdesc = 	get_css_currentScanDesc(scanstate);
	direction = 	get_es_direction(estate);
	sdesc = 	ExecReScanR(rdesc, sdesc, direction, 0, NULL);
	set_css_currentScanDesc(scanstate, sdesc);
    } else {
	/* ----------------
	 *   if ProcOuterFlag is true then we are scanning
	 *   a subplan so we just call ExecReScan on the subplan.
	 * ----------------
	 */
	outerPlan = get_outerPlan(node);
	ExecReScan(outerPlan);
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecSeqMarkPos(node)
 *   
 *   	Marks scan position.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecMarkPos
 ****/
List
ExecSeqMarkPos(node)
    Plan node;
{
    ScanState 	 scanstate;
    Plan	 outerPlan;
    bool	 procOuterFlag;
    HeapScanDesc sdesc;
    
    scanstate =     get_scanstate(node);
    procOuterFlag = get_ss_ProcOuterFlag(scanstate);
    
    /* ----------------
     *	if we are scanning a subplan then propagate
     *  the ExecMarkPos() request to the subplan
     * ----------------
     */
    if (procOuterFlag) {
	outerPlan = get_outerPlan(node);
	return
	    ExecMarkPos(outerPlan);
    }
    
    /* ----------------
     *  otherwise we are scanning a relation so mark the
     *  position using the access methods..
     *
     *  XXX access methods don't return positions yet so
     *      for now we return LispNil.  It's possible that
     *      they will never return positions for all I know -cim 10/16/89
     * ----------------
     */
    sdesc = get_css_currentScanDesc(scanstate);
    ammarkpos(sdesc);
    
    return LispNil;
}
 
/* ----------------------------------------------------------------
 *   	ExecSeqRestrPos
 *   
 *   	Restores scan position.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecRestrPos
 ****/
void
ExecSeqRestrPos(node)
    Plan node;
{
    ScanState	 scanstate;
    Plan	 outerPlan;
    bool	 procOuterFlag;
    HeapScanDesc sdesc;
    
    scanstate = get_scanstate(node);
    procOuterFlag = get_ss_ProcOuterFlag(scanstate);
    
    /* ----------------
     *	if we are scanning a subplan then propagate
     *  the ExecRestrPos() request to the subplan
     * ----------------
     */
    if (procOuterFlag) {
	outerPlan = get_outerPlan(node);
	ExecRestrPos(outerPlan);
	return;
    }
    
    /* ----------------
     *  otherwise we are scanning a relation so restore the
     *  position using the access methods..
     * ----------------
     */
    sdesc = get_css_currentScanDesc(scanstate);
    amrestrpos(sdesc);
}
