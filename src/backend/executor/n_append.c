/* ----------------------------------------------------------------
 *   FILE
 *      append.c
 *      
 *   DESCRIPTION
 *      routines to handle append nodes.
 *
 *   INTERFACE ROUTINES
 *      ExecInitAppend  - initialize the append node
 *      ExecProcAppend  - retrieve the next tuple from the node
 *      ExecEndAppend   - shut down the append node
 *
 *   NOTES
 *      Each append node contains a list of one or more subplans which
 *      must be iteratively processed (forwards or backwards).
 *      Tuples are retrieved by executing the 'whichplan'th subplan
 *      until the subplan stops returning tuples, at which point that
 *      plan is shut down and the next started up.
 *
 *      Append nodes don't make use of their left and right
 *      subtrees, rather they maintain a list of subplans so
 *      a typical append node looks like this in the plan tree:
 *
 *                 ...
 *                 /
 *              Append -------+------+------+--- nil
 *              /   \         |      |      |
 *            nil   nil      ...    ...    ...
 *                               subplans
 *
 *      Append nodes are currently used to support inheritance
 *      queries, where several relations need to be scanned.
 *      For example, in our standard person/student/employee/student-emp
 *      example, where student and employee inherit from person
 *      and student-emp inherits from student and employee, the
 *      query:
 *
 *              retrieve (e.name) from e in person*
 *
 *      generates the plan:
 *
 *                |
 *              Append -------+-------+--------+--------+
 *              /   \         |       |        |        |
 *            nil   nil      Scan    Scan     Scan     Scan
 *                            |       |        |        |
 *                          person employee student student-emp
 *
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */

#include "executor/executor.h"

 RcsId("$Header$");
/* ----------------------------------------------------------------
 *      exec-append-initialize-next
 *    
 *      Sets up the append node state (i.e. the append state node)
 *      for the "next" scan.
 *    
 *      Returns t iff there is a "next" scan to process.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitAppend
 *           ExecProcAppend
 ****/
List
exec_append_initialize_next(node)
    Append      node;
{
    EState         estate;
    AppendState    unionstate;
    TupleTableSlot result_slot;
    List           rangeTable;
    
    int            whichplan;
    int            nplans;
    List           rtentries;
    List           rtentry;
    
    Index          unionrelid;
    
    /* ----------------
     *  get information from the append node
     * ----------------
     */
    estate =    (EState) get_state((Plan) node);
    unionstate =         get_unionstate(node);
    result_slot =        get_cs_ResultTupleSlot((CommonState) unionstate);
    rangeTable =         get_es_range_table(estate);
    
    whichplan = get_as_whichplan(unionstate);
    nplans =    get_as_nplans(unionstate);
    rtentries = get_unionrtentries(node);
    
    if (whichplan < 0) {
        /* ----------------
         *      if scanning in reverse, we start at
         *      the last scan in the list and then
         *      proceed back to the first.. in any case
         *      we inform ExecProcAppend that we are
         *      at the end of the line by returning LispNil
         * ----------------
         */
        set_as_whichplan(unionstate, 0);
        return LispNil;
	
    } else if (whichplan >= nplans) {
        /* ----------------
         *      as above, end the scan if we go beyond
         *      the last scan in our list..
         * ----------------
         */
        set_as_whichplan(unionstate, nplans - 1);
        return LispNil;
        
    } else {
        /* ----------------
         *      initialize the scan
         *      (and update the range table appropriately)
         * ----------------
         */
	if (get_unionrelid(node) > 0) {
	    rtentry = nth(whichplan, rtentries);
	    if (lispNullp(rtentry))
		elog(DEBUG, "exec_append_initialize_next: rtentry is nil");
	  
	    unionrelid = get_unionrelid(node);
	    rt_store(unionrelid, rangeTable, rtentry);
	    set_ttc_whichplan( result_slot, whichplan );
	}
      
	return LispTrue;
    }
}
 
/* ----------------------------------------------------------------
 *      ExecInitAppend
 *    
 *      Begins all of the subscans of the append node, storing the
 *      scan structures in the 'initialized' vector of the append-state
 *      structure.
 *
 *     (This is potentially wasteful, since the entire result of the
 *      append node may not be scanned, but this way all of the
 *      structures get allocated in the executor's top level memory
 *      block instead of that of the call to ExecProcAppend.)
 *    
 *      Returns the scan result of the first scan.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitAppend(node, estate, parent)
    Append      node;
    EState      estate;
    Plan 	parent;
{
    AppendState unionstate;
    List        rangeTable;     /* LLL unused */
    
    int         nplans;
    List        rtentries;
    List        unionplans;
    ListPtr     initialized;
    int         i;
    Plan        initNode;
    List        result;
    List        junkList;
    int         baseid;
    
    /* ----------------
     *  assign execution state to node and get information
     *  for append state
     * ----------------
     */
    set_state((Plan) node,  (EStatePtr)estate);
    
    unionplans =        get_unionplans(node);
    nplans =            length(unionplans);
    rtentries =         get_unionrtentries(node);
    
    CXT1_printf("ExecInitAppend: context is %d\n", CurrentMemoryContext);
    initialized = (ListPtr)
        palloc(nplans * sizeof(List));
    
    /* ----------------
     *  create new AppendState for our append node
     * ----------------
     */   
    unionstate =  MakeAppendState(0, nplans, initialized, rtentries);
    set_unionstate(node, unionstate);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks
     *
     *  Append plans don't have expression contexts because they
     *  never call ExecQual or ExecTargetList.
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) unionstate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) unionstate);

    /* ----------------
     *	append nodes still have Result slots, which hold pointers
     *  to tuples, so we have to initialize them..
     * ----------------
     */
    ExecInitResultTupleSlot(estate, (CommonState) unionstate);
    
    if (get_es_result_relation_info(estate))
    {
	RelationInfo rri;
	RelationInfo es_rri = get_es_result_relation_info(estate);
	List         resultList = LispNil;
	List         rtentryP;

	foreach(rtentryP,rtentries)
	{
	    ObjectId reloid;
	    List     rtentry = CAR(rtentryP);

	    reloid = CInteger(rt_relid(rtentry));
	    rri = MakeRelationInfo(get_ri_RangeTableIndex(es_rri),
				   heap_open(reloid),
				   0,		/* num indices */
				   NULL,	/* index descs */
				   NULL);	/* index key info */
	    resultList = nappend1(resultList, rri);
	    ExecOpenIndices(reloid, rri);
	}
	set_es_result_relation_info_list(estate, resultList);
    }
    /* ----------------
     *  call ExecInitNode on each of the plans in our list
     *  and save the results into the array "initialized"
     * ----------------
     */       
    junkList = LispNil;
    for(i = 0; i < nplans ; i++ ) {
	JunkFilter j;
	List       targetList;
        /* ----------------
         *  NOTE: we first modify range table in 
         *        exec_append_initialize_next() and
         *        then initialize the subnode,
         *        since it may use the range table.
         * ----------------
         */
        set_as_whichplan(unionstate, i);
        exec_append_initialize_next(node);
	
        initNode = (Plan) nth(i, unionplans);
        result =          ExecInitNode(initNode, estate, (Plan) node);
	
        initialized[i] = result;
	
	/* ---------------
	 *  Each targetlist in the subplan may need its own junk filter
	 * ---------------
	 */
	targetList = get_qptargetlist(initNode);
	j = (JunkFilter) ExecInitJunkFilter(targetList);
	junkList = nappend1(junkList, j);

    }
    set_es_junkFilter_list(estate, junkList);
    
    /* ----------------
     *	initialize the return type from the appropriate subplan.
     * ----------------
     */
    initNode = (Plan) nth(0, unionplans);
    ExecAssignResultType((CommonState) unionstate, ExecGetTupType(initNode));
    set_cs_ProjInfo((CommonState) unionstate, NULL);
    
    /* ----------------
     *  return the result from the first subplan's initialization
     * ----------------
     */       
    set_as_whichplan(unionstate, 0);
    exec_append_initialize_next(node);
    result = (List) initialized[0];
    
    return
        result;
}
 
/* ----------------------------------------------------------------
 *     ExecProcAppend
 *    
 *      Handles the iteration over the multiple scans.
 *    
 *     NOTE: Can't call this ExecAppend, that name is used in execMain.l
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcAppend
 *           ExecProcNode
 ****/
TupleTableSlot
ExecProcAppend(node)
    Append              node;
{
    EState              estate;
    AppendState         unionstate;
    
    int                 whichplan;
    List                unionplans;
    Plan                subnode;
    TupleTableSlot      result;
    TupleTableSlot      result_slot;
    ScanDirection       direction;
    
    /* ----------------
     *  get information from the node
     * ----------------
     */
    unionstate =      get_unionstate(node);
    estate = (EState) get_state((Plan) node);
    direction =       get_es_direction(estate);
    
    unionplans =      get_unionplans(node);
    whichplan =       get_as_whichplan(unionstate);
    result_slot =     get_cs_ResultTupleSlot((CommonState) unionstate);
	
    /* ----------------
     *  figure out which subplan we are currently processing
     * ----------------
     */
    subnode = (Plan) nth(whichplan, unionplans);
    
    if (lispNullp(subnode))
        elog(DEBUG, "ExecProcAppend: subnode is NULL");
    
    /* ----------------
     *  get a tuple from the subplan
     * ----------------
     */
    result = ExecProcNode(subnode);
    
    if (! TupIsNull((Pointer) result)) {
        /* ----------------
         *  if the subplan gave us something then place a copy of
	 *  whatever we get into our result slot and return it, else..
         * ----------------
         */
	return
	    (TupleTableSlot)ExecStoreTuple(ExecFetchTuple((Pointer) result),
					   (Pointer) result_slot,
			   		    ExecSlotBuffer((Pointer) result),
			   		    false);
    
    } else {
        /* ----------------
         *  .. go on to the "next" subplan in the appropriate
         *  direction and try processing again (recursively)
         * ----------------
         */
        whichplan = get_as_whichplan(unionstate);
        
        if (direction == EXEC_FRWD)
	{
            set_as_whichplan(unionstate, whichplan + 1);
	}
        else
	{
            set_as_whichplan(unionstate, whichplan - 1);
	}

	/* ----------------
	 *  return something from next node or an empty slot
	 *  all of our subplans have been exhausted.
	 * ----------------
	 */
        if (exec_append_initialize_next(node)) {
	    ExecSetSlotDescriptorIsNew((Pointer) result_slot, true);
            return
		ExecProcAppend(node);
        } else
	    return
		(TupleTableSlot)ExecClearTuple((Pointer) result_slot);
    }
}
 
/* ----------------------------------------------------------------
 *      ExecEndAppend
 *    
 *      Shuts down the subscans of the append node.
 *    
 *      Returns nothing of interest.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndAppend(node)
    Append      node;
{
    AppendState unionstate;
    int         nplans;
    List        unionplans;
    ListPtr     initialized;
    int         i;
    
    /* ----------------
     *  get information from the node
     * ----------------
     */
    unionstate =  get_unionstate(node);
    unionplans =  get_unionplans(node);
    nplans =      get_as_nplans(unionstate);
    initialized = get_as_initialized(unionstate);
    
    /* ----------------
     *  shut down each of the subscans
     * ----------------
     */
    for(i = 0; i < nplans; i++) {
        if (! lispNullp(initialized[i])) {
            ExecEndNode( (Plan) nth(i, unionplans) );
        }
    }
}
 
