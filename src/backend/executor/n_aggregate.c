/* ------------------------------------------------
 *   FILE
 *     aggregate.c
 *
 *   DESCRIPTION
 *	Routines to handle aggregate nodes.
 *
 *   INTERFACE ROUTINES
 *	ExecAgg			-generate a temporary relation
 *	ExecInitAgg		-initialize node and subnodes..
 *	ExecEndAgg		-shutdown node and subnodes
 *
 *  NOTES  
 *	this is a new file, jc
 *
 *   IDENTIFICATION
 *   	$Header$ *?*
 * ------------------------------------------------
 */

#include "executor/executor.h"

 RcsId("Header: RCS/aggregate.c,v 1.1 91/07/28 15:02:49 caetta Exp $");
/* ---------------------------------------
 *    ExecAgg
 *
 *	ExecAgg receives tuples from its outer subplan and
 * 	aggregates over the appropriate attribute until the clause
 *	attribute contents change, if there is a clause attribute.
 *	It then finalizes and stores the aggregated tuple into a second
 *	temp class, and goes on to aggregate the next set.  When the outer
 *	subplan runs out of tuples, the procedure exits and returns the
 *	name of the temporary relation that holds the aggregated tuples.
 *	I'm not relying on the negative relid to identify it because of
 *	the possibility of more than one temp relation being created during
 *	a query.
 *	Initial State:
 *
 *	ExecAgg expects a tempRelation to have been created to hold
 *	the aggregated tuples whose id is stored in the aggregate node.
 * ------------------------------------------
 */
/* convenient routine...
*/

/*-----------------------------------------------------------------------
 * find the attribute No from the tuple descriptor and the
 * attribute name. 'nattrs' is the # of attributes in the relation.
 *-------------------------------------------------------

/*
AttributeNumber
findAttrNoFromName(tupdesc, nattrs, name)
TupleDescriptor tupdesc;
AttributeNumbers nattrs;
Name name;
{
    int i;

	for(i=0; i<nattrs; i++) {
          if (!strcmp (&(name->data[0]), &(tupdesc->data[i]->attname.data[0])))
		return((AttributeNumber)(i+1));
	}

	elog(WARN, "findAttrNoFromName: No such attribute (name = %s)",
		name);
}
*/

/**** xxref:
 *		ExecProcNode
 ****/
TupleTableSlot		/* this is debatable */
ExecAgg(node)
	Agg node;
{
    EState		estate;
    AggState		aggstate;
    Plan		outerNode;
    ScanDirection	dir;
    HeapTuple		outerTuple;
    char		*Caggname;
    TupleDescriptor	outerTupDesc;
    HeapTuple		temp_tuple;
    HeapTuple		heapTuple;
    TupleTableSlot	outerslot;
    Buffer		buffer;
    Relation		tempRelation;
    Relation 		rel;
    HeapScanDesc	currentScanDesc;
    extern Datum 	fastgetattr();
    char		*running_comp[2];
    char		*AggNameGetInitAggVal(),*AggNameGetInitAggVal();
    char 		*final_value;
    TupleDescriptor 	aggtupdesc;
    TupleTableSlot	slot;
    Name 		aggname;
    bool		isNull = 0;
    extern char		*update_aggregate();
    extern char 	*finalize_aggregate();
    ObjectId		func1, func2, finalfunc;
    char 		nulls = ' ';
    char 		*theNewVal;
    int			nargs[3];
    func_ptr		functionptrarray[3];
    char 		*args[2];

    /* ------------
     *  get state info from node
     * ------------
     */
    aggstate = 		get_aggstate((Agg) node);
    estate = 		(EState) get_state((Plan) node);
    dir = 		get_es_direction(estate);

    /* the first time we call this we aggregate the tuples below.
     * subsequent calls return tuples from the temp relation
     */
    if(get_agg_Flag(aggstate) != false)
	return (NULL); 

    SO1_printf("ExecAgg: %s\n", "aggstate == false -> aggregating subplan");

    set_es_direction(estate, EXEC_FRWD);

    /* ---------------------
     * if we couldn't create the temp or current relations then
     * we print a warning and return NULL.
     *----------------------
     */
    tempRelation = get_agg_TempRelation((AggState) aggstate);
    if (tempRelation == NULL) 
    {
	elog(DEBUG, "ExecAggregate: temp relation is NULL! aborting...");
	return NULL;
    }

    /* ----------------------
     *  retrieve tuples from subplan
     * ----------------------
     */

    outerNode = get_outerPlan((Plan) node);

    aggtupdesc = ExecGetResultType((CommonState) aggstate);

    aggname = get_aggname(node);
    Caggname = CString(aggname);

    running_comp[0] = (char *)
	AggNameGetInitVal(Caggname, Anum_pg_aggregate_initaggval, &isNull);
    while (isNull)
    {
	outerslot = ExecProcNode(outerNode);
	outerTuple = (HeapTuple) ExecFetchTuple((Pointer) outerslot);
	if(outerTuple == NULL)
	    elog(WARN, "No initial value *and* no values to aggregate");
 	outerTupDesc = (TupleDescriptor)SlotTupleDescriptor(outerslot);
	/* 
	 * find an initial value.
	 */
	running_comp[0] = (char *)
	    fastgetattr(outerTuple, 1, outerTupDesc, &isNull);
    }
	
    running_comp[1] = (char *)
	AggNameGetInitVal(Caggname, Anum_pg_aggregate_initsecval, &isNull);

    func1 = CInteger(SearchSysCacheGetAttribute(AGGNAME,
						Anum_pg_aggregate_xitionfunc1,
						Caggname,
						0,0,0));
    if (!func1)
	elog(WARN, "Missing xition function 1 for aggregate %s", Caggname);

    func2 = CInteger(SearchSysCacheGetAttribute(AGGNAME,
						Anum_pg_aggregate_xitionfunc2,
						Caggname,
						0,0,0));

    finalfunc = CInteger(SearchSysCacheGetAttribute(AGGNAME, 
						    Anum_pg_aggregate_finalfunc,
						    Caggname,
						    0,0,0));

    fmgr_info(func1, &functionptrarray[0], &nargs[0]);
    if (func2)
	fmgr_info(func2, &functionptrarray[1], &nargs[1]);
    if (finalfunc)
	fmgr_info(finalfunc, &functionptrarray[2], &nargs[2]);

    for(;;) 
    {
	outerslot = ExecProcNode(outerNode);
	
	outerTuple = (HeapTuple) ExecFetchTuple((Pointer) outerslot);
	if(outerTuple == NULL)
	    break;
 	outerTupDesc = (TupleDescriptor)SlotTupleDescriptor(outerslot);

	/* 
	 * continute to aggregate 
	 */
	theNewVal = (char *)fastgetattr(outerTuple, 1, outerTupDesc, &isNull);

	args[0] = running_comp[0];
	args[1] = theNewVal;
	running_comp[0] = (char *) 
	    fmgr_by_ptr_array_args( functionptrarray[0],
				    nargs[0],
				    &args[0] );
	if (func2) 
	{
	    running_comp[1] = (char *)
		fmgr_by_ptr_array_args( functionptrarray[1],
					nargs[1],
					&running_comp[1] );
	}
    }

    /* 
     * finalize the aggregate (if necessary), and get the resultant value
     */

    if (finalfunc)
    {
	final_value = (char *)
	    fmgr_by_ptr_array_args( functionptrarray[2],
				    nargs[2],
				    &running_comp[0] );
    }
    else
	final_value = running_comp[0];

    heapTuple = heap_formtuple( 1,
				aggtupdesc,
				&final_value,
				&nulls );

    slot = (TupleTableSlot) get_cs_ResultTupleSlot((CommonState) aggstate);
/*
 * Unclear why this is being done -- mer 4 Nov. 1991
 *
 *  ExecSetSlotDescriptor(slot, &tempRelation->rd_att);
 */
    set_agg_Flag(aggstate, true);
    return (TupleTableSlot)
	ExecStoreTuple((Pointer) heapTuple, (Pointer) slot, buffer, false);
}

/* -----------------
 * ExecInitAgg
 *
 *  Creates the run-time information for the agg node produced by the
 *  planner and initializes its outer subtree
 * -----------------
 */
/* xxref
 * 	ExecInitNode
 */
List
ExecInitAgg(node, estate, parent)
    Agg 	node;
    EState	estate;
    Plan	parent;
{
    AggState		aggstate;
    Plan		outerPlan;
    TupleDescriptor	tupType;
    ObjectId		tempOid;
    ObjectId		aggOid;
    Relation		tempDesc;
    ParamListInfo	paraminfo;
    ExprContext		econtext;
    int			baseid;
    int			len;

    SO1_printf("ExecInitAgg: %s\n", "initializing agg node");

    /* 
     * assign the node's execution state
     */

    set_state((Plan) node,  estate);

    /*
     * create state structure
     */

    aggstate = MakeAggState(false,NULL,NULL); /* values will be squished
					  *  anyway
					  */
    set_aggstate(node, aggstate);

    /*
     * Should Aggregate nodes initialize their ExprContexts?
     * not for now until we can do quals
     */

    ExecAssignNodeBaseInfo(estate, (BaseNode) aggstate, (Plan) parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) aggstate);

    /*
     * tuple table initialization
     */
    ExecInitScanTupleSlot(estate, (CommonScanState) aggstate);
    ExecInitResultTupleSlot(estate, (CommonState) aggstate);

    /*
     * initializes child nodes
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecInitNode( outerPlan,estate,(Plan)node);

    /*
     * initialize aggstate information
     */
    set_agg_Flag(aggstate, false);

    /*
     * Initialize tuple type for both result and scan.
     * This node does no projection
     */
    ExecAssignResultTypeFromTL((Plan) node, (CommonState) aggstate);
    ExecAssignScanTypeFromOuterPlan((Plan) node, (CommonState) aggstate);
    set_cs_ProjInfo((CommonState) aggstate, (ProjectionInfo) NULL);

    /*
     * get type information needed for ExecCreatR
     */

    tupType = ExecGetScanType((CommonScanState) aggstate);
    tempOid = get_tempid((Temp)node);
    aggOid = -1;

    /*
     * create temporary relations
     */

    len =		length(get_qptargetlist((Plan) node));
    tempDesc =		ExecCreatR(len, tupType, tempOid);

    /*
     * save the relation descriptor in the sortstate
     */

    set_agg_TempRelation(aggstate, tempDesc);
    SO1_printf("ExecInitSort: %s\n", "sort node initialized");

    return LispTrue;
}

/* ------------------------
 *	ExecEndAgg(node)
 *
 * 	destroys the temporary relation
 * -----------------------
 */

/*** xxref:
 *	ExecEndNode
 ****/
void
ExecEndAgg(node)
	Agg node;
{
    AggState	aggstate;
    Relation	tempRelation;
    Plan	outerPlan;

    /* get info from aggstate */

    SO1_printf("ExecEndSort: %s\n",
		   "shutting down sort node");

    aggstate = 	   get_aggstate(node);
    tempRelation = get_agg_TempRelation(aggstate);

    /* release the buffers of the temp relations so that at commit time
       the buffer manager will not try to flush them.  see hack comments
       for sort nodes */

    ReleaseTmpRelBuffers(tempRelation);
    if (FileNameUnlink((char *)
		       relpath((char *) &tempRelation->rd_rel->relname)) < 0)
	elog(WARN, "ExecEndSort: unlink: %m");
    amclose(tempRelation);

    ExecCloseR( (Plan)node);
    /* shut down subplan */
    outerPlan = get_outerPlan((Plan) node);
    ExecEndNode(outerPlan);

    /* clean up tuple table */
    ExecClearTuple((Pointer)
		   get_css_ScanTupleSlot((CommonScanState) aggstate));

    SO1_printf("ExecEndSort: %s\n",
		   "sort node shutdown");
}
/* end ExecEndAgg */

