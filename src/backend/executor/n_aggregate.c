/* ------------------------------------------------
 *   FILE
 *     aggregate.c
 *
 *   DESCRIPTION
 *	Routines to handle aggregate nodes.
 *
 *   INTERFACE ROUTINES
 *	ExecAgg			-generate a temporary relation
 *	ExecInitAgg		-initialize node and subnodes
 *	ExecEndAgg		-shutdown node and subnodes
 *
 *  NOTES  
 *
 *   IDENTIFICATION
 *   	$Header$ *?*
 * ------------------------------------------------
 */

#include "executor/executor.h"
#include "catalog/pg_protos.h"

RcsId("$Header$");

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
 *	a query. -jc
 *
 *	XXX Aggregates should probably have another option: what to do 
 *	with transfn2 if we hit a null value.  "count" (transfn1 = null,
 *	transfn2 = increment) will want to have transfn2 called; "avg"
 *	(transfn1 = add, transfn2 = increment) will not. -pma 1/3/93
 *
 *	Initial State:
 *
 *	ExecAgg expects a tempRelation to have been created to hold
 *	the aggregated tuples whose id is stored in the aggregate node.
 * ------------------------------------------
 */

/**** xxref:
 *		ExecProcNode
 ****/
TupleTableSlot		/* this is debatable */
ExecAgg(node)
	Agg node;
{
	AggState		aggstate;
	EState			estate;
	Plan			outerNode;
	HeapTuple		outerTuple, newTuple, aggTuple;
	TupleDescriptor		outerTupDesc, aggtupdesc;
	TupleTableSlot		outerslot, newSlot;
	char	 		nulls = ' ';
	Relation		tempRelation;
	String	 		aggname;
	char			*xfn1_val, *xfn2_val, *finalfn_val;
	ObjectId		xfn1_oid, xfn2_oid, finalfn_oid;
	func_ptr		xfn1_ptr, xfn2_ptr, finalfn_ptr;
	int			xfn1_nargs, xfn2_nargs, finalfn_nargs;
	bool			isNull = 0, isNull1 = 0, isNull2 = 0;
	char	 		*theNewVal;
	char			*args[2];
	long			nTuplesAgged = 0;
	Form_pg_aggregate	aggp;
	extern char	 	*fastgetattr();
	extern char		*AggNameGetInitVal();

	/* ---------------------
	 *  get state info from node
	 * ---------------------
	 */
	aggstate = get_aggstate((Agg) node);
	estate = (EState) get_state((Plan) node);

	/* ---------------------
	 * the first time we call this we aggregate the tuples below.
	 * subsequent calls return tuples from the temp relation
	 * ---------------------
	 */
	if (get_agg_Flag(aggstate) == true)
		return((TupleTableSlot) NULL);

	SO1_printf("ExecAgg: %s\n", "aggstate == false -> aggregating subplan");

	set_es_direction(estate, EXEC_FRWD);

	/* ---------------------
	 * if we couldn't create the temp or current relations then
	 * we print a warning and return NULL.
	 * ----------------------
	 */
	tempRelation = get_agg_TempRelation((AggState) aggstate);
	if (!RelationIsValid(tempRelation))
		elog(WARN, "ExecAgg: could not open temporary relation");
	
	outerNode = get_outerPlan((Plan) node);

	aggname = (String) get_aggname(node);
	aggTuple = SearchSysCacheTuple(AGGNAME, aggname, (char *) NULL,
				       (char *) NULL, (char *) NULL);
	if (!HeapTupleIsValid(aggTuple))
		elog(WARN, "ExecAgg: cache lookup failed for \"%-*s\"",
		     sizeof(NameData), aggname);
	aggp = (Form_pg_aggregate) GETSTRUCT(aggTuple);
	xfn1_oid = aggp->aggtransfn1;
	xfn2_oid = aggp->aggtransfn2;
	finalfn_oid = aggp->aggfinalfn;

	if (ObjectIdIsValid(finalfn_oid))
		fmgr_info(finalfn_oid, &finalfn_ptr, &finalfn_nargs);
	if (ObjectIdIsValid(xfn2_oid)) {
		fmgr_info(xfn2_oid, &xfn2_ptr, &xfn2_nargs);
		xfn2_val = AggNameGetInitVal(aggname,
					     Anum_pg_aggregate_agginitval2,
					     &isNull2);
		/* ------------------------------------------
		 * If there is a second transition function, its initial
		 * value must exist -- as it does not depend on data values,
		 * we have no other way of determining an initial value.
		 * ------------------------------------------
		 */
		if (isNull2)
			elog(WARN, "ExecAgg: agginitval2 is null");
	}
	if (ObjectIdIsValid(xfn1_oid)) {
		fmgr_info(xfn1_oid, &xfn1_ptr, &xfn1_nargs);
		xfn1_val = AggNameGetInitVal(aggname,
					     Anum_pg_aggregate_agginitval1,
					     &isNull1);
		/* ------------------------------------------
		 * If the initial value for the first transition function
		 * doesn't exist in the pg_aggregate table then we let
		 * the first value returned from the outer procNode become
		 * the initial value. (This is useful for aggregates like
		 * max{} and min{}.)
		 * ------------------------------------------
		 */
		while (isNull1) {
			outerslot = ExecProcNode(outerNode);
			outerTuple = (HeapTuple)
				ExecFetchTuple((Pointer) outerslot);
			if (!HeapTupleIsValid(outerTuple))
				elog(WARN, "ExecAgg: no initial value given and no valid tuples found");
			outerTupDesc = (TupleDescriptor)
				SlotTupleDescriptor(outerslot);
			xfn1_val = fastgetattr(outerTuple, 1, outerTupDesc,
					       &isNull1);

			if (ObjectIdIsValid(xfn2_oid)) {
				xfn2_val = fmgr_c(xfn2_ptr, xfn2_oid,
						  xfn2_nargs, &xfn2_val,
						  &isNull2);
				Assert(!isNull2);
			}
			if (!isNull1)
				++nTuplesAgged;
		}
	}
	
	for (;;) {
		isNull = isNull1 = isNull2 = 0;
		outerslot = ExecProcNode(outerNode);
		outerTuple = (HeapTuple)
			ExecFetchTuple((Pointer) outerslot);
		if (!HeapTupleIsValid(outerTuple))
			break;
		outerTupDesc = (TupleDescriptor)
			SlotTupleDescriptor(outerslot);
		theNewVal = fastgetattr(outerTuple, 1, outerTupDesc, &isNull);
		
		if (!isNull && ObjectIdIsValid(xfn1_oid)) {
			args[0] = xfn1_val;
			args[1] = theNewVal;
			xfn1_val = fmgr_c(xfn1_ptr, xfn1_oid,
					  xfn1_nargs, args, &isNull1);
			Assert(!isNull1);
		}
		if (ObjectIdIsValid(xfn2_oid) &&
		    !(isNull && ObjectIdIsValid(xfn1_oid))) {
			xfn2_val = fmgr_c(xfn2_ptr, xfn2_oid,
					  xfn2_nargs, &xfn2_val,
					  &isNull2);
			Assert(!isNull2);
		}
		++nTuplesAgged;
	}

	/* --------------
	 * finalize the aggregate (if necessary), and get the resultant value
	 * --------------
	 */
	if (ObjectIdIsValid(finalfn_oid) && nTuplesAgged > 0) {
		if (finalfn_nargs > 1) {
			args[0] = xfn1_val;
			args[1] = xfn2_val;
		} else if (ObjectIdIsValid(xfn1_oid)) {
			args[0] = xfn1_val;
		} else if (ObjectIdIsValid(xfn2_oid)) {
			args[0] = xfn2_val;
		} else
			elog(WARN, "ExecAgg: no valid transition functions??");
		finalfn_val = fmgr_c(finalfn_ptr, finalfn_oid,
				     finalfn_nargs, args, &isNull1);
	} else if (ObjectIdIsValid(xfn1_oid)) {
		finalfn_val = xfn1_val;
	} else if (ObjectIdIsValid(xfn2_oid)) {
		finalfn_val = xfn2_val;
	} else
		elog(WARN, "ExecAgg: no valid transition functions??");

	aggtupdesc = ExecGetResultType((CommonState) aggstate);
	newTuple = heap_formtuple(1, aggtupdesc, &finalfn_val, &nulls);

	newSlot = (TupleTableSlot)
		get_cs_ResultTupleSlot((CommonState) aggstate);
	set_agg_Flag(aggstate, true);
	return((TupleTableSlot) ExecStoreTuple((Pointer) newTuple,
					       (Pointer) newSlot,
					       InvalidBuffer,
					       false));
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
	AggState	aggstate;
	Plan		outerPlan;
	TupleDescriptor	tupType;
	ObjectId	tempOid;
	ObjectId	aggOid;
	Relation	tempDesc;
	ParamListInfo	paraminfo;
	ExprContext	econtext;
	int		baseid;
	int		len;

	SO1_printf("ExecInitAgg: %s\n", "initializing agg node");

	/* 
	 * assign the node's execution state
	 */

	set_state((Plan) node,  (EStatePtr)estate);

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

#define AGG_NSLOTS 2
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

	len = ExecTargetListLength(get_qptargetlist((Plan) node));
	tempDesc = ExecCreatR(len, tupType, tempOid);

	/*
	 * save the relation descriptor in the sortstate
	 */

	set_agg_TempRelation(aggstate, tempDesc);
	SO1_printf("ExecInitSort: %s\n", "sort node initialized");

	return LispTrue;
}

int
ExecCountSlotsAgg(node)
	Plan node;
{
	return ExecCountSlotsNode(get_outerPlan(node)) +
		ExecCountSlotsNode(get_innerPlan(node)) +
			AGG_NSLOTS;
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

