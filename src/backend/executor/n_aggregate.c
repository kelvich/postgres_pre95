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

#include "tmp/postgres.h"

 RcsId("Header: RCS/aggregate.c,v 1.1 91/07/28 15:02:49 caetta Exp $");

#include "executor/execdebug.h"

#include "parser/parsetree.h"
#include "utils/log.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_aggregate.h"
#include "access/tupmacs.h"

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
    char		*running_comp;
    char 		*final_value;
    TupleDescriptor 	aggtupdesc;
    TupleTableSlot	slot;
    Name 		aggname;
    bool		isNull = 0;
    extern char		*update_aggregate();
    extern char 	*finalize_aggregate();
    ObjectId		func1, func2, finalfunc;
    char 		nulls = ' ';
    Datum 		theNewVal;
    /* ------------
    *  get state info from node
    * ------------
    */
    aggstate = 		get_aggstate(node);
    estate = 		(EState) get_state(node);
    dir = 		get_es_direction(estate);

    /* the first time we call this we aggregate the tuples below.
     * subsequent calls return tuples from the temp relation
      */
 if(get_agg_Flag(aggstate) == false) {
    SO1_printf("ExecAgg: %s\n", "aggstate == false -> aggregating subplan");

    set_es_direction(estate, EXEC_FRWD);
    /* ---------------
     * if we couldn't create the temp or current relations then
     * we print a warning and return NULL.
     *----------------------
     */
     tempRelation = get_agg_TempRelation(aggstate);
     if (tempRelation == NULL) {
	elog(DEBUG, "ExecAggregate: temp relation is NULL! aborting...");
	return NULL;
     }

    /* ----------------------
     *  retrieve tuples from subplan
     * ----------------------
     */

     outerNode = get_outerPlan(node);

     aggtupdesc = (Attribute *)&tempRelation->rd_att;

     aggname = get_aggname(node);
     Caggname = CString(aggname);
     running_comp = (char *)palloc(2*sizeof(Datum));
     running_comp[0] = (char *)NULL;
     running_comp[1] = (char *)NULL;

     func1 = CInteger(SearchSysCacheGetAttribute(AGGNAME, 
		   AggregateIntFunc1AttributeNumber, Caggname,0,0,0));

     func2 = CInteger(SearchSysCacheGetAttribute(AGGNAME, 
		   AggregateIntFunc2AttributeNumber, Caggname,0,0,0));

     finalfunc = CInteger(SearchSysCacheGetAttribute(AGGNAME, 
      			AggregateFinFuncAttributeNumber, Caggname,0,0,0));

	     for(;;) {
		outerslot = ExecProcNode(outerNode);
	
		 outerTuple = (HeapTuple) ExecFetchTuple(outerslot);
		 if(outerTuple == NULL)
		    break;
		outerTupDesc = (TupleDescriptor)SlotTupleDescriptor(outerslot);
		/* continute to aggregate */
		theNewVal = (char *)fastgetattr(outerTuple, 1,
				outerTupDesc, &isNull);

		running_comp = update_aggregate(Caggname, running_comp,
						theNewVal, func1, func2);

		/* I'm allowed to do the t_hoff bit because this will be
		 * a single-datum tuple...*/
		/* sends running_comp and the new value to be updated.  this
		 * is generating an array for values[] for the tuple to be
	 	 * created when we are done aggregating.
	 	 */
		/* if this is not true, then we're running an aggregate with
		 * a clause (otherwise we would have "broke."  so, finalize
		 * this one and init another.
		 */
	}
	/* finalize this final aggregate*/
	final_value = finalize_aggregate(running_comp, finalfunc);
	heapTuple = heap_formtuple(1,
				aggtupdesc,
				&final_value,
				&nulls);

	slot = (TupleTableSlot) get_css_ScanTupleSlot(aggstate);
	ExecSetSlotDescriptor(slot, &tempRelation->rd_att);
		/* for now, */
	set_agg_Flag(aggstate, true);
	return (TupleTableSlot) 
		ExecStoreTuple(heapTuple, slot, buffer, false);
   }
   else {
	return (NULL); 
   }
}

/*-----------------
 * ExecInitAgg
 *
 *  Creates the run-time information for the agg node produced by the
 *  planner and initializes its outer subtree
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

       SO1_printf("ExecInitAgg: %s\n",
		      "initializing agg node");
   /* assign the node's execution state */

   set_state(node, estate);

   /* create state structure */

   aggstate = MakeAggState();
   set_aggstate(node, aggstate);

   /* Should Aggregate nodes initialize their ExprContexts?  not for
    * now until we can do quals */

    ExecAssignNodeBaseInfo(estate, aggstate, parent);
    ExecAssignDebugHooks(node, aggstate);

    /* tuple table initialization
     *
     * agg nodes only return scan tuples from their sorted relation
     */
     ExecInitScanTupleSlot(estate, aggstate);

     /* initializes child nodes */
     outerPlan = get_outerPlan(node);
     ExecInitNode(outerPlan,estate,node);

     /* initialize aggstate information */
     set_agg_Flag(aggstate, false);

     /* initialize tuple type.  this node does no projection */

     ExecAssignScanTypeFromOuterPlan(node, aggstate);
     set_cs_ProjInfo(aggstate, NULL);

     /* get type information needed for ExecCreatR*/

     tupType = ExecGetScanType(aggstate);
     tempOid = get_tempid(node);
     aggOid = -1;

     /* create temporary relations */

     len = 			length(get_qptargetlist(node));
     tempDesc = 		ExecCreatR(len, tupType, tempOid);

     /* save the relation descriptor in the sortstate */

     set_agg_TempRelation(aggstate, tempDesc);
     SO1_printf("ExecInitSort: %s\n",
		    "sort node initialized");

     return
	LispTrue;
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
    if (FileNameUnlink(relpath(&tempRelation->rd_rel->relname)) < 0)
	elog(WARN, "ExecEndSort: unlink: %m");
    amclose(tempRelation);

    ExecCloseR( (Plan)node);
    /* shut down subplan */
    outerPlan = get_outerPlan(node);
    ExecEndNode(outerPlan);

    /* clean up tuple table */
    ExecClearTuple(get_css_ScanTupleSlot(aggstate));

    SO1_printf("ExecEndSort: %s\n",
		   "sort node shutdown");
}
/* end ExecEndAgg */


char *
update_aggregate(Caggname, running_comp, attr_data, func1, func2)
    char  *Caggname;
    char  *running_comp[]; /* full of char *'s */
    Datum  *attr_data;
    ObjectId func1, func2;
{
    static int nargs[2];
    static func_ptr functionptrarray[2];
    int i;
    Datum args[2];

    if(running_comp[0] == (char *)NULL) {
	/* first time being called */

	running_comp[0] = (char *)AggNameGetInitAggVal(Caggname);
	running_comp[1] = (char *)AggNameGetInitSecVal(Caggname);
	/* equals the initial conditions for the specific aggregate */

	fmgr_info(func1, &functionptrarray[0], &nargs[0]);
	fmgr_info(func2, &functionptrarray[1], &nargs[1]);

	/* fill up function pointer info */
    }
	/* for now, the xition func will be an array of functions for
	 * each object in the running comp.  for example, for the average
	 * aggregate, the running comp would look like:
	 * [sum, number], and the function array would look like [+, +1].
	 * if the function needs args, it gets it from the running comp
	 * and attr_data.  otherwise, it just updates the running comp.
	 * Actually, the OID numbers for the procedures are stored.
	 */
    /* now, functionptrarray and nargs are full */
	for(i=0; i<2; i++) {
	if(nargs[i] > 1) {
	    /* make array of appropriate args */
	    args[0] = running_comp[i];
	    args[1] = attr_data;
	    running_comp[i] = fmgr_by_ptr_array_args
						(functionptrarray[i],
						    nargs[i], args);
	 }
	 else {
	    running_comp[i] = fmgr_by_ptr_array_args(functionptrarray[i],
						    nargs[i],
						    &running_comp[i]);

	/* running_comp[1] will be holding the "incrementer", or the arg
	 * which does not need to know the new value
	 */
    	 }
 	}  
    /* now the values in running_comp are updated */
    return (running_comp);
}

char *
finalize_aggregate(running_comp, func)
    char  *running_comp[];
    ObjectId func;
{
    char *fvalue;
    int numargs;
    func_ptr fptr;
    fmgr_info(func, &fptr, &numargs);
    /* we'll assume that the aggregates have at most 2 objects that they
     * can create
     */
     fvalue = fmgr_by_ptr_array_args(fptr, numargs, running_comp);

     /* this should be ok regardless of nargs; all arguments come from
      * running comp, and in the same order that the first argument is
      * the principle aggregate
      */
     pfree(running_comp);
     return (fvalue);
}



