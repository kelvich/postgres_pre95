/* ----------------------------------------------------------------
 *   FILE
 *	material.c
 *	
 *   DESCRIPTION
 *	Routines to handle materialization nodes.
 *
 *   INTERFACE ROUTINES
 *     	ExecMaterial		- generate a temporary relation
 *     	ExecInitMaterial	- initialize node and subnodes..
 *     	ExecEndMaterial		- shutdown node and subnodes
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "executor/executor.h"

 RcsId("$Header$");

/* ----------------------------------------------------------------
 *   	ExecMaterial
 *
 *	The first time this is called, ExecMaterial retrieves tuples
 *	this node's outer subplan and inserts them into a temporary
 *	relation.  After this is done, a flag is set indicating that
 *	the subplan has been materialized.  Once the relation is
 *	materialized, the first tuple is then returned.  Successive
 *	calls to ExecMaterial return successive tuples from the temp
 *	relation.
 *
 *	Initial State:
 *
 *	ExecMaterial assumes the temporary relation has been
 *	created and openend by ExecInitMaterial during the prior
 *	InitPlan() phase.
 *
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot			/* result tuple from subplan */
ExecMaterial(node)
    Material node;
{
    EState	  	estate;
    MaterialState	matstate;
    Plan 	  	outerNode;
    ScanDirection 	dir;
    Relation	  	tempRelation;
    Relation	  	currentRelation;
    HeapScanDesc  	currentScanDesc;
    HeapTuple     	heapTuple;
    TupleTableSlot  	slot;
    Buffer		buffer;
    
    /* ----------------
     *	get state info from node
     * ----------------
     */
    matstate =   	get_matstate(node);
    estate =      	(EState) get_state((Plan) node);
    dir =   	  	get_es_direction(estate);
    
    /* ----------------
     *	the first time we call this, we retrieve all tuples
     *  from the subplan into a temporary relation and then
     *  we sort the relation.  Subsequent calls return tuples
     *  from the temporary relation.
     * ----------------
     */
    
    if (get_mat_Flag(matstate) == false) {
	/* ----------------
	 *  set all relations to be scanned in the forward direction 
	 *  while creating the temporary relation.
	 * ----------------
	 */
	set_es_direction(estate, EXEC_FRWD);
	
	/* ----------------
	 *   if we couldn't create the temp or current relations then
	 *   we print a warning and return NULL.
	 * ----------------
	 */
	tempRelation =  get_mat_TempRelation(matstate);
	if (tempRelation == NULL) {
	    elog(DEBUG, "ExecMaterial: temp relation is NULL! aborting...");
	    return NULL;
	}
	
	currentRelation = get_css_currentRelation((CommonScanState)matstate);
	if (currentRelation == NULL) {
	    elog(DEBUG, "ExecMaterial: current relation is NULL! aborting...");
	    return NULL;
	}
	
	/* ----------------
	 *   retrieve tuples from the subplan and
	 *   insert them in the temporary relation
	 * ----------------
	 */
	outerNode =     get_outerPlan((Plan) node);
	for (;;) {
	    slot = ExecProcNode(outerNode);
	    
	    heapTuple = (HeapTuple) ExecFetchTuple((Pointer) slot);
	    if (heapTuple == NULL)
		break;
	    
	    aminsert(tempRelation, 	/* relation desc */
		     heapTuple,		/* heap tuple to insert */
		     0);		/* return: offset */
	    
	    ExecClearTuple((Pointer) slot);
	}
	currentRelation = tempRelation;
	
	/* ----------------
	 *   restore to user specified direction
	 * ----------------
	 */
	set_es_direction(estate, dir);
	
	/* ----------------
	 *   now initialize the scan descriptor to scan the
	 *   sorted relation and update the sortstate information
	 * ----------------
	 */
	currentScanDesc = ambeginscan(currentRelation,    /* relation */
				      (dir == EXEC_BKWD), /* bkwd flag */
				      NowTimeQual,        /* time qual */
				      0, 		  /* num scan keys */
				      NULL); 		  /* scan keys */
	set_css_currentRelation((CommonScanState)matstate, currentRelation);
	set_css_currentScanDesc((CommonScanState)matstate, currentScanDesc);
	
	ExecAssignScanType((CommonScanState)matstate,
			   &currentRelation->rd_att);
	
	/* ----------------
	 *  finally set the sorted flag to true
	 * ----------------
	 */
	set_mat_Flag(matstate, true);
    }
    
    /* ----------------
     *	at this point we know we have a sorted relation so
     *  we preform a simple scan on it with amgetnext()..
     * ----------------
     */
    currentScanDesc = get_css_currentScanDesc((CommonScanState)matstate);
    
    heapTuple = amgetnext(currentScanDesc, 	/* scan desc */
			  (dir == EXEC_BKWD), 	/* bkwd flag */
			  &buffer); 		/* return: buffer */

    /* ----------------
     *	put the tuple into the scan tuple slot and return the slot.
     *  Note: since the tuple is really a pointer to a page, we don't want
     *  to call pfree() on it..
     * ----------------
     */
    slot = (TupleTableSlot)
	get_css_ScanTupleSlot((CommonScanState)matstate);

    return (TupleTableSlot)
	ExecStoreTuple((Pointer) heapTuple,  /* tuple to store */
		       (Pointer) slot,      /* slot to store in */
		       buffer,	   /* buffer for this tuple */
		       false);     /* don't pfree this pointer */
    
}
 
/* ----------------------------------------------------------------
 *   	ExecInitMaterial
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List	/* initialization status */
ExecInitMaterial(node, estate, parent)
    Material 	node;
    EState 	estate;
    Plan	parent;
{
    MaterialState	matstate;
    Plan		outerPlan;
    TupleDescriptor	tupType;
    Relation		tempDesc;
    int			baseid;
    int			len;
    
    /* ----------------
     *  assign the node's execution state
     * ----------------
     */
    set_state((Plan) node, (EStatePtr)estate);
    
    /* ----------------
     * create state structure
     * ----------------
     */
    matstate = MakeMaterialState(false, NULL);
    set_matstate(node, matstate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +  assign result tuple slot
     *
     *  Materialization nodes don't need ExprContexts because
     *  they never call ExecQual or ExecTargetList.
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) matstate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) matstate);
    ExecInitScanTupleSlot(estate, (CommonScanState)matstate);
    
    /* ----------------
     * initializes child nodes
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecInitNode(outerPlan, estate, (Plan) node);
    
    /* ----------------
     *	initialize matstate information
     * ----------------
     */
    set_mat_Flag(matstate, false);

    /* ----------------
     * 	initialize tuple type.  no need to initialize projection
     *  info because this node doesn't do projections.
     * ----------------
     */
    ExecAssignScanTypeFromOuterPlan((Plan) node, (CommonState)matstate);
    set_cs_ProjInfo((CommonState)matstate, NULL);
    
    /* ----------------
     *	get type information needed for ExecCreatR
     * ----------------
     */
    tupType = ExecGetScanType((CommonScanState)matstate);

    /* ----------------
     *	ExecCreatR wants it's third argument to be an object id of
     *  a relation in the range table or a negative number like -1
     *  indicating that the relation is not in the range table.
     *
     *  In the second case ExecCreatR creates a temp relation.
     *  (currently this is the only case we support -cim 10/16/89)
     * ----------------
     */
    /* ----------------
     *	create the temporary relation
     * ----------------
     */
    len = length(get_qptargetlist((Plan) node));
    tempDesc = 	ExecCreatR(len, tupType, -1);
    
    /* ----------------
     *	save the relation descriptor in the sortstate
     * ----------------
     */
    set_mat_TempRelation(matstate, tempDesc);
    set_css_currentRelation((CommonScanState)matstate, tempDesc);
    
    /* ----------------
     *  return relation oid of temporary relation in a list
     *	(someday -- for now we return LispTrue... cim 10/12/89)
     * ----------------
     */
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndMaterial
 *
 * old comments
 *   	destroys the temporary relation.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndMaterial(node)
    Material node;
{
    MaterialState	matstate;
    Relation		tempRelation;
    Plan		outerPlan;
    
    /* ----------------
     *	get info from the material state 
     * ----------------
     */
    matstate = get_matstate(node);
    tempRelation = get_mat_TempRelation(matstate);
    
    /* ----------------
     *	the temporary relations are not cataloged so
     *  we can't call DestroyRelation() so we unlink the
     *  unix file explicitly.  Yet another hack -- the
     *  amclose routine should explicitly destroy temp relations
     *  but it doesn't yet -cim 1/25/90
     * ----------------
     */
    if (FileNameUnlink((char *) relpath((char *)
					&(tempRelation->rd_rel->relname))) < 0)
	elog(WARN, "ExecEndMaterial: unlink: %m");
    amclose(tempRelation);
    
    /* ----------------
     *	close the temp relation and shut down the scan.
     * ----------------
     */
    ExecCloseR((Plan) node);
    
    /* ----------------
     *	shut down the subplan
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecEndNode(outerPlan);
    
    /* ----------------
     *	clean out the tuple table
     * ----------------
     */
    ExecClearTuple((Pointer) get_css_ScanTupleSlot((CommonScanState)matstate));
} 
 
/* ----------------------------------------------------------------
 *	ExecMaterialMarkPos
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
List /* nothing of interest */
ExecMaterialMarkPos(node)
    Material node;
{
    MaterialState 	matstate;
    HeapScanDesc 	sdesc;
    
    /* ----------------
     *	if we haven't materialized yet, just return LispNil.
     * ----------------
     */
    matstate =          get_matstate(node);
    if (get_mat_Flag(matstate) == false)
	return LispNil;
    
    /* ----------------
     *  XXX access methods don't return positions yet so
     *      for now we return LispNil.  It's possible that
     *      they will never return positions for all I know -cim 10/16/89
     * ----------------
     */
    sdesc = get_css_currentScanDesc((CommonScanState)matstate);
    ammarkpos(sdesc);
    
    return LispNil;
}
 
/* ----------------------------------------------------------------
 *	ExecMaterialRestrPos
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
void
ExecMaterialRestrPos(node)
    Material node;
{
    MaterialState	matstate;
    HeapScanDesc 	sdesc;
    
    /* ----------------
     *	if we haven't materialized yet, just return.
     * ----------------
     */
    matstate =  get_matstate(node);
    if (get_mat_Flag(matstate) == false)
	return;
    
    /* ----------------
     *	restore the scan to the previously marked position
     * ----------------
     */
    sdesc = get_css_currentScanDesc((CommonScanState)matstate);
    amrestrpos(sdesc);
}
 
