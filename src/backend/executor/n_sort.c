/* ----------------------------------------------------------------
 *   FILE
 *	sort.c
 *	
 *   DESCRIPTION
 *	Routines to handle sorting of relations into temporaries.
 *
 *   INTERFACE ROUTINES
 *     	ExecSort	- generate a sorted temporary relation
 *     	ExecInitSort	- initialize node and subnodes..
 *     	ExecEndSort	- shutdown node and subnodes
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
 *    	FormSortKeys(node)
 *    
 *    	Forms the structure containing information used to sort the relation.
 *    
 *    	Returns an array of 'struct skey'.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecInitSort
 ****/
Pointer
FormSortKeys(sortnode)
    Sort sortnode;
{
    struct skey		*sortkeys;
    List    		targetList;
    List   		tl;
    int			keycount;
    Resdom    		resdom;
    AttributeNumber 	resno;
    Index   		reskey;
    OperatorTupleForm	reskeyop;
    
    /* ----------------
     *	get information from the node
     * ----------------
     */
    targetList = get_qptargetlist((Plan)sortnode);
    keycount =   get_keycount((Temp)sortnode);
    
    /* ----------------
     *	first allocate space for scan keys
     * ----------------
     */
    sortkeys = (struct skey*)ExecMakeSkeys(keycount);
    if (sortkeys == NULL)
	elog(DEBUG, "FormSortKeys: ExecMakeSkeys(keycount) returns NULL!");
    
    /* ----------------
     *	form each scan key from the resdom info in the target list
     * ----------------
     */
    foreach(tl, targetList) {
	resdom =  (Resdom) tl_resdom(CAR(tl));
	resno =   get_resno(resdom);
	reskey =  get_reskey(resdom);
	reskeyop = get_reskeyop(resdom);
	    
	if (reskey > 0) {
	    ExecSetSkeys(reskey - 1,
			 (ScanKeyEntry)sortkeys,
			 (AttributeNumber) resno,
			 (RegProcedure) reskeyop,
			 (Datum) 0);
	}
    }
    
    return (Pointer)sortkeys;
}
 
/* ----------------------------------------------------------------
 *   	ExecSort
 *
 * old comments
 *   	Retrieves tuples fron the outer subtree and insert them into a 
 *   	temporary relation. The temporary relation is then sorted and
 *   	the sorted relation is stored in the relation whose ID is indicated
 *   	in the 'tempid' field of this node.
 *   	Assumes that heap access method is used.
 *   
 *   	Conditions:
 *   	  -- none.
 *   
 *   	Initial States:
 *   	  -- the outer child is prepared to return the first tuple.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot
ExecSort(node)
    Sort 	   node;
{
    EState	   estate;
    SortState	   sortstate;
    Plan 	   outerNode;
    ScanDirection  dir;
    int		   keycount;
    Pointer	   sortkeys;
    Relation	   tempRelation;
    Relation	   currentRelation;
    HeapScanDesc   currentScanDesc;
    HeapTuple      heapTuple;
    TupleTableSlot slot;
    Buffer	   buffer;
    int		   tupCount = 0;
    
    /* ----------------
     *	get state info from node
     * ----------------
     */
    SO1_printf("ExecSort: %s\n",
	       "entering routine");
    
    sortstate =   get_sortstate(node);
    estate =      (EState) get_state((Plan) node);
    dir =   	  get_es_direction(estate);
    
    /* ----------------
     *	the first time we call this, we retrieve all tuples
     *  from the subplan into a temporary relation and then
     *  we sort the relation.  Subsequent calls return tuples
     *  from the temporary relation.
     * ----------------
     */
    
    if (get_sort_Flag(sortstate) == false) {
	SO1_printf("ExecSort: %s\n",
		   "sortstate == false -> sorting subplan");
	/* ----------------
	 *  set all relations to be scanned in the forward direction 
	 *  while creating the temporary relation.
	 * ----------------
	 */
	set_es_direction(estate, EXEC_FRWD);
	
	/* ----------------
	 *   if we couldn't create the temp or current relations then
	 *   we print a warning and return LispNil.
	 * ----------------
	 */
	tempRelation =  get_sort_TempRelation(sortstate);
	if (tempRelation == NULL) {
	    elog(DEBUG, "ExecSort: temp relation is NULL! aborting...");
	    return NULL;
	}
	
	currentRelation = get_css_currentRelation((CommonScanState)sortstate);
	if (currentRelation == NULL) {
	    elog(DEBUG, "ExecSort: current relation is NULL! aborting...");
	    return NULL;
	}
	
	/* ----------------
	 *   retrieve tuples from the subplan and
	 *   insert them in the temporary relation
	 * ----------------
	 */
	outerNode =     get_outerPlan((Plan) node);
	SO1_printf("ExecSort: %s\n",
		   "inserting tuples into tempRelation");
	
	for (;;) {
	    slot = ExecProcNode(outerNode);

	    if (TupIsNull((Pointer) slot))
		break;

	    tupCount++;
	    
	    heapTuple = (HeapTuple) ExecFetchTuple((Pointer)slot);
	    
	    aminsert(tempRelation, 	/* relation desc */
		     heapTuple,		/* heap tuple to insert */
		     0);		/* return: offset */
	    
	    ExecClearTuple((Pointer) slot);
	}
   
	/* ----------------
	 *   now sort the tuples in our temporary relation
	 *   into a new sorted relation using psort()
	 *
	 *   psort() seems to require that the relations
	 *   are created and opened in advance.
	 *   -cim 1/25/90
	 * ----------------
	 */
	keycount = get_keycount((Temp) node);
	sortkeys = get_sort_Keys(sortstate);
	SO1_printf("ExecSort: %s\n",
		   "calling psort");
	
	/*
	 * If no tuples were fetched from the proc node return NULL now
	 * psort dumps it if 0 tuples are in the relation and I don't want
	 * to try to debug *that* routine!!
	 */
	if (tupCount == 0)
	    return NULL;

	psort(tempRelation,	/* old relation */
	      currentRelation,	/* new relation */
	      keycount,		/* number keys */
	      sortkeys);	/* keys */
	
	if (currentRelation == NULL) {
	    elog(DEBUG, "ExecSort: sorted relation is NULL! aborting...");
	    return NULL;
	}

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
	
	set_css_currentRelation((CommonScanState) sortstate, currentRelation);
	set_css_currentScanDesc((CommonScanState)  sortstate, currentScanDesc);
	    
	/* ----------------
	 *  make sure the tuple descriptor is up to date
	 * ----------------
	 */
	slot = (TupleTableSlot)
	    get_css_ScanTupleSlot((CommonScanState) sortstate);
    
	ExecSetSlotDescriptor((Pointer) slot, &currentRelation->rd_att);
	
	/* ----------------
	 *  finally set the sorted flag to true
	 * ----------------
	 */
	set_sort_Flag(sortstate, true);
    }
    else {
	slot = (TupleTableSlot)
	    get_css_ScanTupleSlot((CommonScanState) sortstate);
    }
    
    SO1_printf("ExecSort: %s\n",
	       "retrieveing tuple from sorted relation");
    
    /* ----------------
     *	at this point we know we have a sorted relation so
     *  we preform a simple scan on it with amgetnext()..
     * ----------------
     */
    currentScanDesc = get_css_currentScanDesc((CommonScanState) sortstate);
    
    heapTuple = amgetnext(currentScanDesc, 	/* scan desc */
			  (dir == EXEC_BKWD), 	/* bkwd flag */
			  &buffer); 		/* return: buffer */

    return (TupleTableSlot)
	ExecStoreTuple((Pointer) heapTuple,   	/* tuple to store */
		       (Pointer) slot,   	/* slot to store in */
		       buffer,		/* this tuple's buffer */
		       false); 		/* don't free stuff from amgetnext */
}
 
/* ----------------------------------------------------------------
 *   	ExecInitSort
 *
 * old comments
 *   	Creates the run-time state information for the sort node
 *   	produced by the planner and initailizes its outer subtree.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitSort(node, estate, parent)
    Sort 	node;
    EState 	estate;
    Plan	parent;
{
    SortState		sortstate;
    Plan		outerPlan;
    Pointer		sortkeys;
    TupleDescriptor	tupType;
    ObjectId		tempOid;
    ObjectId		sortOid;
    Relation		tempDesc;
    Relation		sortedDesc;
    ParamListInfo       paraminfo;
    ExprContext	        econtext;
    int			baseid;
    int			len;
    
    SO1_printf("ExecInitSort: %s\n",
	       "initializing sort node");
    
    /* ----------------
     *  assign the node's execution state
     * ----------------
     */
    set_state((Plan)node, (EStatePtr)estate);
    
    /* ----------------
     * create state structure
     * ----------------
     */
    sortstate = MakeSortState(0,NULL, NULL);
    set_sortstate(node, sortstate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks
     *
     *  Sort nodes don't initialize their ExprContexts because
     *  they never call ExecQual or ExecTargetList.
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) sortstate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) sortstate);
    
    /* ----------------
     *	tuple table initialization
     *
     *  sort nodes only return scan tuples from their sorted
     *  relation.
     * ----------------
     */
    ExecInitScanTupleSlot(estate, (CommonScanState) sortstate);
    
    /* ----------------
     * initializes child nodes
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecInitNode(outerPlan, estate, (Plan) node);
    
    /* ----------------
     *	initialize sortstate information
     * ----------------
     */
    sortkeys = FormSortKeys(node);
    set_sort_Keys(sortstate, sortkeys);
    set_sort_Flag(sortstate, false);
    
    /* ----------------
     * 	initialize tuple type.  no need to initialize projection
     *  info because this node doesn't do projections.
     * ----------------
     */
    ExecAssignScanTypeFromOuterPlan((Plan) node, (CommonState)sortstate);
    set_cs_ProjInfo((CommonState) sortstate, NULL);
    
    /* ----------------
     *	get type information needed for ExecCreatR
     * ----------------
     */
    tupType = ExecGetScanType((CommonScanState) sortstate);
    
    /* ----------------
     *	ExecCreatR wants it's third argument to be an object id of
     *  a relation in the range table or a negative number like -1
     *  indicating that the relation is not in the range table.
     *
     *  In the second case ExecCreatR creates a temp relation.
     *  (currently this is the only case we support -cim 10/16/89)
     * ----------------
     */
    tempOid = 	get_tempid((Temp) node);
    sortOid =	-1;
    
    /* ----------------
     *	create the temporary relations
     * ----------------
     */
    len = 		length(get_qptargetlist((Plan) node));
    tempDesc = 		ExecCreatR(len, tupType, tempOid);
    sortedDesc = 	ExecCreatR(len, tupType, sortOid);
    
    /* ----------------
     *	save the relation descriptor in the sortstate
     * ----------------
     */
    set_sort_TempRelation(sortstate, tempDesc);
    set_css_currentRelation((CommonScanState) sortstate, sortedDesc);
    SO1_printf("ExecInitSort: %s\n",
	       "sort node initialized");
    
    /* ----------------
     *  return relation oid of temporary sort relation in a list
     *	(someday -- for now we return LispTrue... cim 10/12/89)
     * ----------------
     */
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndSort(node)
 *
 * old comments
 *   	destroys the temporary relation.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndSort(node)
    Sort node;
{
    SortState	sortstate;
    Relation	tempRelation;
    Relation	sortedRelation;
    Plan	outerPlan;
    
    /* ----------------
     *	get info from the sort state 
     * ----------------
     */
    SO1_printf("ExecEndSort: %s\n",
	       "shutting down sort node");
    
    sortstate =      get_sortstate(node);
    tempRelation =   get_sort_TempRelation(sortstate);
    sortedRelation = get_css_currentRelation((CommonScanState) sortstate);
    
    /* ----------------
     *	the temporary relations are not cataloged so
     *  we can't call DestroyRelation() so we unlink the
     *  unix file explicitly.  Yet another hack -- the
     *  amclose routine should explicitly destroy temp relations
     *  but it doesn't yet -cim 1/25/90
     * ----------------
     */
    /* ----------------
     * release the buffers of the temporary relations so that at commit
     * time the buffer manager will not try to flush them.
     * ----------------
     */
    ReleaseTmpRelBuffers(tempRelation);
    ReleaseTmpRelBuffers(sortedRelation);
    if (FileNameUnlink((char *)
	relpath((char *) &(tempRelation->rd_rel->relname))) < 0)
	elog(WARN, "ExecEndSort: unlink: %m");
    if (FileNameUnlink((char *) relpath((char *)
			       &(sortedRelation->rd_rel->relname))) < 0)
	elog(WARN, "ExecEndSort: unlink: %m");
    
    amclose(tempRelation);
    amclose(sortedRelation);
    
    /* ----------------
     *	close the sorted relation and shut down the scan.
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
    ExecClearTuple((Pointer)
		   get_css_ScanTupleSlot((CommonScanState) sortstate));
        
    SO1_printf("ExecEndSort: %s\n",
	       "sort node shutdown");
} 

/* ----------------------------------------------------------------
 *	ExecSortMarkPos
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecMarkPos
 ****/
List
ExecSortMarkPos(node)
    Plan node;
{
    SortState 	 sortstate;
    HeapScanDesc sdesc;
    
    /* ----------------
     *	if we haven't sorted yet, just return LispNil.
     * ----------------
     */
    sortstate =     get_sortstate((Sort) node);
    if (get_sort_Flag(sortstate) == false)
	return LispNil;
    
    /* ----------------
     *  XXX access methods don't return positions yet so
     *      for now we return LispNil.  It's possible that
     *      they will never return positions for all I know -cim 10/16/89
     * ----------------
     */
    sdesc = get_css_currentScanDesc((CommonScanState) sortstate);
    ammarkpos(sdesc);
    return LispNil;
}

/* ----------------------------------------------------------------
 *	ExecSortRestrPos
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecRestrPos
 ****/
void
ExecSortRestrPos(node)
    Plan node;
{
    SortState	 sortstate;
    HeapScanDesc sdesc;
    
    /* ----------------
     *	if we haven't sorted yet, just return.
     * ----------------
     */
    sortstate =  get_sortstate((Sort) node);
    if (get_sort_Flag(sortstate) == false)
	return;
    
    /* ----------------
     *	restore the scan to the previously marked position
     * ----------------
     */
    sdesc = get_css_currentScanDesc((CommonScanState) sortstate);
    amrestrpos(sdesc);
}
