/* ----------------------------------------------------------------
 *   FILE
 *	ex_scan.c
 *	
 *   DESCRIPTION
 *	This code provides support for generalized relation scans.
 *	ExecScan is passed a node and a pointer to a function to
 *	"do the right thing" and return a tuple from the relation.
 *	ExecScan then does the tedious stuff - checking the
 *	qualification and projecting the tuple appropriately.
 *
 *   INTERFACE ROUTINES
 *   	ExecScan
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <sys/file.h>
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

#include "parser/parse.h"
#include "utils/log.h"
#include "rules/prs2.h"

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
 *   	ExecScan
 *   
 *   	Scans the relation using the 'access method' indicated and 
 *   	returns the next qualifying tuple in the direction specified
 *   	in the global variable ExecDirection.
 *   	The access method returns the next tuple and execScan() is 
 *   	responisble for checking the tuple returned against the qual-clause.
 *   	
 *   	If the tuple retrieved contains locks set by rules, the tuple is 
 *   	passed to the Tuple Rule Manager and the rule descriptor returned 
 *   	is kept. Also, a flag is set (set_ss_RuleFlag) so that subsequent 
 *   	calls know that a rule is activated and the Tuple Rule Manager 
 *   	is called to return any possible tuple formed from the set of rules.
 *   	The flag is cleared if the Tuple Rule Manager fails to return any
 *   	tuple and the relation is scanned instead to retrieve the next tuple.
 *   
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
 *	May need to put startmmgr  and endmmgr in here.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitScanAttributes
 *           ExecIndexScan
 *           ExecSeqScan
 ****/
TupleTableSlot
ExecScan(node, accessMtd)
    Plan    node;
    Pointer (*accessMtd)();	/* function returning a tuple */
{
    ScanState		scanstate;
    EState		estate;
    List		qual;
    bool		qualResult;
    List		targetList;
    int			len;
    TupleDescriptor	tupType;
    Pointer		tupValue;
    Relation		scanRelation;
    Index		scanRelid;
    AttributeNumberPtr	scanAtts;
    int			scanNumAtts;
    int			i;
    bool		madeBogusScanAtts = false;
    
    TupleTableSlot	slot;
    TupleTableSlot	rawTupleSlot;
    TupleTableSlot	resultSlot;
    HeapTuple 		newTuple;
    HeapTuple		changedTuple;
    HeapTuple		rawTuple;
    HeapTuple		viewTuple;

    int			status;
    ItemPointer		tuple_tid;
    Index		oldRelId;
    RelationInfo	resultRelationInfo;
    Index		resultRelationIndex;
    Prs2EStateInfo	prs2EStateInfo;
    RelationRuleInfo	relRuleInfo;
    ExprContext		econtext;
    ProjectionInfo	projInfo;
    
    scanstate =     	get_scanstate(node);
    
    /* ----------------
     *	initialize misc variables
     * ----------------
     */
    newTuple = 	NULL;
    slot =	NULL;
    
    estate = 	(EState) get_state(node);
    
    /* ----------------
     *	get the expression context
     * ----------------
     */
    econtext = 		get_cs_ExprContext(scanstate);
    prs2EStateInfo = 	get_es_prs2_info(estate);
    relRuleInfo = 	get_css_ruleInfo(scanstate);
    rawTupleSlot = 	(TupleTableSlot) get_css_RawTupleSlot(scanstate);
    
    /* ----------------
     *	initialize fields in ExprContext which don't change
     *	in the course of the scan..
     * ----------------
     */
    qual = 		get_qpqual(node);
    scanRelation =	get_css_currentRelation(scanstate);
    scanRelid =		get_scanrelid(node);

    set_ecxt_relation(econtext, scanRelation);
    set_ecxt_relid(econtext, scanRelid);
	    
    /* ----------------
     *	get a tuple from the access method and have the
     *  rule manager process it..  loop until we obtain
     *  a tuple which passes the qualification.
     * ----------------
     */
    for(;;) {
	/* ----------------
	 * First check to see if we have 'view' rules.
	 * If yes, proccess all the tuples returned by them before
	 * you process any of the "real" tuples (the ones returned
	 * by the access method).
	 *
	 * NOTE: If this is a parent of another node (e.g. sort)
	 * and not an actual relation scan, then RelationRuleInfo == NULL!
	 * ----------------
	 */
	if (relRuleInfo != NULL &&
	    prs2MustCallRuleManager(relRuleInfo, (HeapTuple) NULL,
				    InvalidBuffer, RETRIEVE))
	{
	    viewTuple = prs2GetOneTupleFromViewRules(
			    relRuleInfo,
			    prs2EStateInfo,
			    scanRelation,
			    get_es_explain_relation(estate));
	    
	    /* ----------------
	     * note that the data pointed by 'relRuleInfo'
	     * might change, but as 'relRuleInfo' is a pointer,
	     * there is no need to do a 
	     *     set_css_ruleInfo(scanState, relRuleInfo);
	     * ----------------
	     */
	    if (viewTuple != NULL) {
		/* ----------------
		 * We've found a 'view' tuple
		 * ----------------
		 */
		ExecStoreTuple(viewTuple,
			       get_css_ScanTupleSlot(scanstate),
			       InvalidBuffer,
			       true);
		
	    } else if (relRuleInfo->insteadViewRuleFound) {
		/* ----------------
		 * No more view rules, but (at least) one of them,
		 * was an "instead" rule. So, do not call the access method
		 * for more tuples, and pretend that that was the end
		 * of the scan
		 * ----------------
		 */
		return (TupleTableSlot)
		    ExecClearTuple(get_css_ScanTupleSlot(scanstate));
		
	    } else {
		/* ----------------
		 * No more view rules and no instead rule found.
		 * Put clear the ScanTupleSlot to show that...
		 * ----------------
		 */
		ExecClearTuple(get_css_ScanTupleSlot(scanstate));
	    }
	} else {
	    /*
	     * RelationRuleInfo == NULL, so there are no rules...
	     */
	    ExecClearTuple(get_css_ScanTupleSlot(scanstate));
	}

	if (TupIsNull(get_css_ScanTupleSlot(scanstate))) {
	    /* ----------------
	     * No (more) rules and no "instead" rules....
	     * Call the access method for more tuples...
	     * ----------------
	     */
	    slot = (TupleTableSlot) (*accessMtd)(node);
	    
	    /* ----------------
	     *  if the slot returned by the accessMtd contains
	     *  NULL, then it means there is nothing more to scan
	     *  so we just return the empty slot.
	     * ----------------
	     */
	    if (TupIsNull(slot))
		return slot;
	    
	} else {
	    /* ----------------
	     * use the "view" tuple
	     * ----------------
	     */
	    slot = (TupleTableSlot) get_css_ScanTupleSlot(scanstate);
	}
	
	/*
	 * call the rule manager only if necessary...
	 */
	if (relRuleInfo != NULL &&
	    prs2MustCallRuleManager(relRuleInfo,
				    (HeapTuple) ExecFetchTuple(slot),
				    ExecSlotBuffer(slot),
				    RETRIEVE))
	{

	    /* ----------------
	     *  get the attribute information about the tuple
	     * ----------------
	     */
	    scanNumAtts =	get_cs_NumScanAttributes(scanstate);
	    scanAtts =	get_cs_ScanAttributes(scanstate);

	    /* ----------------
	     *  if the system didn't precalculate the scan attribute
	     *  information (which is the right thing), then we generate
	     *  a list of all the attributes.  This means the rule manager
	     *  does much more work than it should because our current
	     *  interface is lame.  -cim 3/15/90
	     * ----------------
	     */
	    if (scanAtts == NULL) {
		scanNumAtts = ((HeapTuple) ExecFetchTuple(slot))->t_natts;
		scanAtts = ExecMakeBogusScanAttributes(scanNumAtts);
		madeBogusScanAtts = true;
	    }
	    
	    /* ----------------
	     *  have the rule manager process the tuple
	     *  if it returns a new tuple, then we use the
	     *  tuple it returns in place of our original tuple
	     *
	     *  XXX the current rule manager interface only allows
	     *      the rule system to return one tuple per scan tuple.
	     *      This will have to change so that a single scan tuple
	     *      will return many tuples by calling prs2Main repeatedly.
	     *      -cim 3/15/90
	     * ----------------
	     */
	    rawTuple = (HeapTuple)
		ExecFetchTuple(slot); /* raw tuple with no rules applied*/
	
	    status = prs2Main(estate,
			      relRuleInfo,
			      RETRIEVE, 			/* operation */
			      0, 				/* userid */
			      scanRelation,			/* base rel */
			      (HeapTuple) ExecFetchTuple(slot), /* tuple */
			      ExecSlotBuffer(slot), 	/* tuple's buffer */
			      NULL,			/* update tuple */
			      InvalidBuffer,	/* update tuple buffer */
			      rawTuple,		/* 'raw' tuple */
			      InvalidBuffer,
			      scanAtts,		/* atts of interest */
			      scanNumAtts, 	/* number of atts */
			      &changedTuple,	/* return: tuple */
			      NULL);		/* return: buffer */
	    
	    /*
	     * pfree the bogus scan atts.
	     */
	    if (madeBogusScanAtts)
		ExecFreeScanAttributes(scanAtts);

	    if (status == PRS2_STATUS_TUPLE_CHANGED) {
		/* ----------------
		 *  remember, "tuple" is really the slot containing
		 *  the tuple.  Once the tuple table stuff works, we
		 *  should change the variable names.
		 * ----------------
		 */
		ExecStoreTuple(rawTuple,
			       rawTupleSlot,
			       ExecSlotBuffer(slot),
			       ExecSlotPolicy(slot));
		
		ExecStoreTuple(changedTuple,
			       slot,
			       InvalidBuffer,
			       true);
	    } else {
		/* ----------------
		 *  leave original tuple alone and clear the
		 *  "raw" tuple slot.
		 * ----------------
		 */
		ExecClearTuple(rawTupleSlot);
	    } 
	} /* if rerel != NULL */
	
	/* ----------------
	 *   place the current tuple into the expr context
	 * ----------------
	 */
	set_ecxt_scantuple(econtext, slot);
	
	/* ----------------
	 *  check that the current tuple satisifies the qual-clause
	 * ----------------
	 */
	qualResult = ExecQual(qual, econtext);
	
	/* ----------------
	 *  if our qualification succeeds then we
	 *  leave the loop.
	 * ----------------
	 */
	if (qualResult == true)
	    break;
    }
	
    /* ----------------
     *  save the qualifying tuple's tid 
     * ----------------
     */
    oldRelId =       	  get_ss_OldRelId(scanstate);
    resultRelationInfo =  get_es_result_relation_info(estate);
    
    tuple_tid = (ItemPointer)
	&(((HeapTuple) ExecFetchTuple(slot))->t_ctid);

    /* ----------------
     *	form a projection tuple, store it in the result tuple
     *  slot and return it.
     * ----------------
     */
    projInfo = get_cs_ProjInfo(scanstate);
    
    return (TupleTableSlot)
	ExecProject(projInfo);
}
