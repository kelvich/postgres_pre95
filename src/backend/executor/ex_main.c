/* ----------------------------------------------------------------
 *   FILE
 *	ex_main.c
 *	
 *   DESCRIPTION
 *	top level executor interface routines
 *
 *   INTERFACE ROUTINES
 *   	ExecMain	- main interface to executor
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#define EXEC_RULEMANAGER 1

#include "tcop/slaves.h"
#include "executor/executor.h"

 RcsId("$Header$");

/* ----------------------------------------------------------------
 *	random junk
 * ----------------------------------------------------------------
 */
bool  ExecIsInitialized = false;
Const ConstTrue;
Const ConstFalse;

extern int Quiet;

extern 	List rule;

/* ----------------------------------------------------------------
 *	InitializeExecutor
 *
 *	This is only called once - by ExecMain, the first time
 *	the user executes a query.
 * ----------------------------------------------------------------
 */
void
InitializeExecutor()
{
    MemoryContext oldContext;
    ObjectId    boolType = 16;	/* see local.bki */
    Size	boolLen = 1;	/* these should come from lookups */
    Datum	datumTrue;
    Datum	datumFalse;

    /* ----------------
     *	make certain we are in the top memory context
     *  or else this stuff will go away after the first transaction.
     * ----------------
     */
    oldContext = MemoryContextSwitchTo(TopMemoryContext);

    /* ----------------
     *	initialize ConstTrue and ConstFalse
     * ----------------
     */
    datumTrue  = Int32GetDatum((int32) true);
    datumFalse = Int32GetDatum((int32) false);
    
    ConstTrue =  MakeConst(boolType, boolLen, datumTrue,  false, true);
    ConstFalse = MakeConst(boolType, boolLen, datumFalse, false, true);
    
#ifdef EXEC_DEBUGVARIABLEFILE
    /* ----------------
     *	initialize debugging variables
     * ----------------
     */
    if (! Quiet)
	printf("==== using %s\n", EXEC_DEBUGVARIABLEFILE);
    
    DebugVariableFileSet(EXEC_DEBUGVARIABLEFILE);
#endif EXEC_DEBUGVARIABLEFILE
    
    /* ----------------
     *	return to old memory context
     * ----------------
     */
    (void) MemoryContextSwitchTo(oldContext);

    ExecIsInitialized = true;
}
 

/* ----------------------------------------------------------------
 *   	ExecMain
 *   
 *   	This is the main routine of the executor module. It accepts
 *   	the query descriptor from the traffic cop and executes the
 *   	query plan.
 *   
 *   	Parameter:
 *   	   argList --(commandType parseTree queryPlan queryState queryFeature)
 * ----------------------------------------------------------------
 */
 
List
ExecMain(queryDesc, estate, feature)
    List 	queryDesc;
    EState 	estate;
    List 	feature;
{
    int		operation;
    List 	parseTree;
    Plan	plan;
    List 	result;
    CommandDest dest;
    void	(*destination)();
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(!lispNullp(queryDesc));
    
    /* ----------------
     *	initialize if necessary
     * ----------------
     */
    if (!ExecIsInitialized)
	InitializeExecutor();
    
    /* ----------------
     *	extract information from the query descriptor
     *  and the query feature.
     * ----------------
     */
    operation =   CAtom(GetOperation(queryDesc));
    parseTree =   QdGetParseTree(queryDesc);
    plan =	  QdGetPlan(queryDesc);
    dest =	  QdGetDest(queryDesc);
    destination = (void (*)()) DestToFunction(dest);
	
    /* ----------------
     *	ExecMain is typically called from the tcop three
     *   times:
     *
     *	first time,   command = EXEC_START		initialize
     *   second time, command = EXEC_RUN		run the plan
     *   last time,   command = EXEC_END		cleanup
     * ----------------
     */
    result =    LispNil;
    
    switch(CInteger(FeatureGetCommand(feature))) {
	/* ----------------
	 *	EXEC_START, EXEC_RUN, EXEC_END are the
	 *	most general cases..
	 * ----------------
	 */
    case EXEC_START:
	/* result is a tuple descriptor */
	result = InitPlan(operation,
			  parseTree,
			  plan,
			  estate);
	/* reset buffer refcount.  the current refcounts
	 * are saved and will be restored at the end of
	 * this ExecMain cycle.
	 *
	 * this makes sure that when ExecMain's are
	 * called recursively as for tuple level rules
	 * or postquel functions, the buffers pinned
	 * by one ExecMain will not be unpinned by another
	 * ExecMain.
	 */
	BufferRefCountReset(get_es_refcount(estate));
	break;
		
    case EXEC_RUN:
	/* result is a tuple or LispNil */
	result = (List) ExecutePlan(estate,
				    plan,
				    parseTree,
				    operation,
				    ALL_TUPLES,
				    EXEC_FRWD,
				    destination);
	break;
	
    case EXEC_END:
	/* result is a tuple or LispNil */
	result = EndPlan(plan, estate);
	/* restore saved refcounts. */
	BufferRefCountRestore(get_es_refcount(estate));
	break;

	/* ----------------
	 *	retrieve next n "forward" tuples
	 * ----------------
	 */
    case EXEC_FOR:
	/* result is a tuple or LispNil */
	result = (List) ExecutePlan(estate,
				    plan,
				    parseTree,
				    operation,
				    CInteger(FeatureGetCount(feature)),
				    EXEC_FRWD,
				    destination);
	break;

	/* ----------------
	 *	retrieve next n "backward" tuples
	 * ----------------
	 */
    case EXEC_BACK:
	/* result is a tuple or LispNil */
	result = (List) ExecutePlan(estate,
				    plan,
				    parseTree,
				    operation,
				    CInteger(FeatureGetCount(feature)),
				    EXEC_BKWD,
				    destination);
	break;
	
	/* ----------------
	 *	return one tuple but don't "retrieve" it.
	 *	(this is used by the rule manager..) -cim 9/14/89
	 * ----------------
	 */
    case EXEC_RETONE:
	/* result is a tuple or LispNil */
	result = (List) ExecutePlan(estate,
				    plan,
				    parseTree,
				    operation,
				    ONE_TUPLE,
				    EXEC_FRWD,
				    destination);
	break;
    default:
	elog(DEBUG, "ExecMain: Unknown command.");
	break;
    }
    
    return result;
}
 
/* ----------------------------------------------------------------
 *   	InitPlan
 *   
 *   	Initialises the query plan: open files, allocate storage
 *   	and start up the rule manager
 * ----------------------------------------------------------------
 */
extern Relation    CopyRelDescUsing();

List
InitPlan(operation, parseTree, plan, estate)
    int			operation;
    EState 		estate;
    List 		parseTree;
    Plan	 	plan;
{	
    List		parseRoot;
    List 		rangeTable;
    List		resultRelation;
    Relation 		relationRelationDesc;
    Relation            intoRelationDesc;
    
    TupleDescriptor 	tupType;
    List	 	targetList;
    int		 	len;
    List	 	attinfo;
    extern List		MakeList();
    
#ifdef PALLOC_DEBUG
    {
	extern int query_tuple_count;
	query_tuple_count = 0;
    }
#endif PALLOC_DEBUG    
    
    /* ----------------
     *  get information from query descriptor
     * ----------------
     */
    parseRoot =   	parse_root(parseTree);
    rangeTable =	parse_tree_range_table(parseTree);
    resultRelation = 	parse_tree_result_relation(parseTree);
    
    /* ----------------
     *  initialize the node's execution state
     * ----------------
     */
    set_es_range_table(estate, rangeTable);
    set_es_error_message(estate, (Name) "Foo bar");
    
    /* ----------------
     *	initialize the BaseId counter so node base_id's
     *  are assigned correctly.  Someday baseid's will have to
     *  be stored someplace other than estate because they
     *  should be unique per query planned.
     * ----------------
     */
    set_es_BaseId(estate, 1);
    
    /* ----------------
     *	initialize result relation stuff
     * ----------------
     */
    set_es_result_rel_scanstate(estate, NULL);

    if (resultRelation != NULL && operation != RETRIEVE) {
	/* ----------------
	 *    if we have a result relation, open the it and
	 *    initialize the result relation info stuff.
	 * ----------------
	 */
	RelationInfo	resultRelationInfo;
	Index 		resultRelationIndex;
	List		rtentry;
	ObjectId	resultRelationOid;
	Relation 	resultRelationDesc;
	RelationRuleInfo resRelRuleInfo;
	
	resultRelationIndex = CInteger(resultRelation);
	rtentry =	      rt_fetch(resultRelationIndex, rangeTable);
	resultRelationOid =   CInteger(rt_relid(rtentry));
	resultRelationDesc =  heap_open(resultRelationOid);
	resultRelationInfo = MakeRelationInfo(resultRelationIndex,
					      resultRelationDesc,
					      0,      /* num indices */
					      NULL,   /* index descs */
					      NULL);  /* index key info */
	/* ----------------
	 *  open indices on result relation and save descriptors
	 *  in the result relation information..
	 * ----------------
	 */
	ExecOpenIndices(resultRelationOid, resultRelationInfo);
	
	set_es_result_relation_info(estate, resultRelationInfo);

	/* ----------------
	 *  Initialize the result relation rule info.
	 * ----------------
	 */
	resRelRuleInfo = prs2MakeRelationRuleInfo(resultRelationDesc,
						  operation);
	
	set_es_result_rel_ruleinfo(estate, resRelRuleInfo);
    } else {
	/* ----------------
	 * 	if no result relation, then set state appropriately
	 * ----------------
	 */
	set_es_result_relation_info(estate, NULL);
    }
    
    relationRelationDesc = heap_openr(RelationRelationName);
    set_es_relation_relation_descriptor(estate, relationRelationDesc);
    
    /* ----------------
     *    initialize the executor "tuple" table.
     * ----------------
     */
    {
	int        nSlots     = ExecCountSlotsNode(plan);
	TupleTable tupleTable = ExecCreateTupleTable(nSlots+10);

	set_es_tupleTable(estate, tupleTable);
    }

    /* ----------------
     *     initialize the private state information for
     *	   all the nodes in the query tree.  This opens
     *	   files, allocates storage and leaves us ready
     *     to start processing tuples..
     * ----------------
     */
    ExecInitNode(plan, estate, NULL);
    
    /* ----------------
     *     get the tuple descriptor describing the type
     *	   of tuples to return.. (this is especially important
     *	   if we are creating a relation with "retrieve into")
     * ----------------
     */
    tupType =    ExecGetTupType(plan);             /* tuple descriptor */
    targetList = get_qptargetlist(plan);
    len = 	 ExecTargetListLength(targetList); /* number of attributes */

    /* ----------------
     *    now that we have the target list, initialize the junk filter
     *    if this is a REPLACE or a DELETE query.
     *    We also init the junk filter if this is an append query
     *    (there might be some rule lock info there...)
     *    NOTE: in the future we might want to initialize the junk
     *	  filter for all queries.
     * ----------------
     */
    if (operation == REPLACE || operation == DELETE || operation == APPEND) {
	JunkFilter j = (JunkFilter) ExecInitJunkFilter(targetList);
	set_es_junkFilter(estate, j);
    } else
	set_es_junkFilter(estate, NULL);
    
    /* ----------------
     *	initialize the "into" relation
     * ----------------
     */
    intoRelationDesc = (Relation) NULL;
    
    if (operation == RETRIEVE) {
	List 	    resultDesc;
	int  	    dest;
	String 	    intoName;
	char	    archiveMode;
	ObjectId    intoRelationId;
	
	resultDesc = root_result_relation(parseRoot);
	if (! lispNullp(resultDesc)) {
	    dest = CAtom(CAR(resultDesc));
	    if (dest == INTO) {
		/* ----------------
		 *  create the "into" relation
		 *
		 *  note: there is currently no way for the user to
		 *	  specify the desired archive mode of the
		 *	  "into" relation...
		 * ----------------
		 */
		intoName = CString(CADR(resultDesc));
		archiveMode = 'n';
		
		intoRelationId = heap_create(intoName,
					     archiveMode,
					     len,
					     DEFAULT_SMGR,
					     (struct attribute **)tupType);
		
		/* ----------------
		 *  XXX rather than having to call setheapoverride(true)
		 *	and then back to false, we should change the
		 *	arguments to heap_open() instead..
		 * ----------------
		 */
		setheapoverride(true);
		
		intoRelationDesc = heap_open(intoRelationId);
		local_heap_open(intoRelationDesc);
		
		setheapoverride(false);
		
	    } else if (dest == INTOTEMP) {
		/* ----------------
		 *  this is only for the parallel backends to save results in
		 *  temporary relations and collect them later
		 * ----------------
		 */

		intoRelationDesc = ExecCreatR(len, tupType, -1);
		if (! IsMaster) {
		    SlaveTmpRelDescInit();
		    SlaveInfoP[MyPid].resultTmpRelDesc =
		       CopyRelDescUsing(intoRelationDesc, SlaveTmpRelDescAlloc);
		}
	    }
	}
    }

    set_es_into_relation_descriptor(estate, intoRelationDesc);
    
    /* ----------------
     *	return the type information..
     * ----------------
     */
    attinfo = MakeList(lispInteger(len), tupType, -1);
    
    return
	attinfo;
}
 
/* ----------------------------------------------------------------
 *   	EndPlan
 *   
 *   	Cleans up the query plan -- closes files and free up storages
 * ----------------------------------------------------------------
 */
 
List
EndPlan(plan, estate)
    Plan		plan;
    EState		estate;
{
    RelationInfo 	resultRelationInfo;
    List		resultRelationInfoList;
    Relation		intoRelationDesc;
   
    /* ----------------
     *	get information from state
     * ----------------
     */
    resultRelationInfo =  get_es_result_relation_info(estate);
    resultRelationInfoList = get_es_result_relation_info_list(estate);
    intoRelationDesc =	  get_es_into_relation_descriptor(estate);
   
    /* ----------------
     *   shut down the query
     * ----------------
     */
    ExecEndNode(plan);
      
    /* ----------------
     *    destroy the executor "tuple" table.
     * ----------------
     */
    {
	TupleTable tupleTable = (TupleTable) get_es_tupleTable(estate);
	ExecDestroyTupleTable(tupleTable,true);	/* was missing last arg */
	set_es_tupleTable(estate, NULL);
    }
        
    /* ----------------
     *   close the result relations if necessary
     * ----------------
     */
    if (resultRelationInfo != NULL) {
	Relation resultRelationDesc;
	
	resultRelationDesc = get_ri_RelationDesc(resultRelationInfo);
	heap_close(resultRelationDesc);
	
	/* ----------------
	 *  close indices on the result relation
	 * ----------------
	 */
	ExecCloseIndices(resultRelationInfo);
    }

    while (resultRelationInfoList != LispNil) {
	Relation resultRelationDesc;

	resultRelationInfo = (RelationInfo) CAR(resultRelationInfoList);
	resultRelationDesc = get_ri_RelationDesc(resultRelationInfo);
	heap_close(resultRelationDesc);
	resultRelationInfoList = CDR(resultRelationInfoList);
    }
         
    /* ----------------
     *   close the "into" relation if necessary
     * ----------------
     */
    if (intoRelationDesc != NULL)
	if (ParallelExecutorEnabled())
	    /* have to use shared buffer pool for paralle backends */
	    heap_close(intoRelationDesc);
	else
	    local_heap_close(intoRelationDesc);
    
    /* ----------------
     *	return successfully
     * ----------------
     */
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecutePlan
 *   
 *   	processes the query plan to retrieve 'tupleCount' tuples in the
 *   	direction specified.
 *   	Retrieves all tuples if tupleCount is 0
 *
 *|	XXX direction seems not to be used (I hope this isn't
 *|         another example of lisp global scoping) -cim 8/7/89
 * ----------------------------------------------------------------
 */

TupleTableSlot
ExecutePlan(estate, plan, parseTree, operation, numberTuples,
	    direction, printfunc)
    EState 	estate;
    Plan	plan;
    LispValue	parseTree;
    int		operation;
    int 	numberTuples;
    int 	direction;
    void 	(*printfunc)();
{
    Relation		intoRelationDesc;
    JunkFilter		junkfilter;
    
    TupleTableSlot	slot;
    ItemPointer		tupleid = NULL;
    ItemPointerData	tuple_ctid;
    int	 		current_tuple_count;
    TupleTableSlot	result;	/* result to return to ExecMain */
    RuleLock		locks;
    
    /* ----------------
     *  get information
     * ----------------
     */
    intoRelationDesc =	get_es_into_relation_descriptor(estate);
    
    /* ----------------
     *	initialize local variables
     * ----------------
     */
    slot 		= NULL;
    current_tuple_count = 0;
    result 		= NULL;

    /* ----------------
     *	Set the direction.
     * ----------------
     */
    set_es_direction(estate, direction);

    /* ----------------
     *	Loop until we've processed the proper number
     *  of tuples from the plan..
     * ----------------
     */
    
    for(;;) {
	/* ----------------
	 *	Execute the plan and obtain a tuple
	 * ----------------
	 */
	 if (operation != NOTIFY)
		slot = ExecProcNode(plan);
	
	/* ----------------
	 *	if the tuple is null, then we assume
	 *	there is nothing more to process so
	 *	we just return null...
	 * ----------------
	 */
	if (TupIsNull((Pointer) slot) && operation != NOTIFY) {
	    result = NULL;
	    break;
	}
	
	/* ----------------
	 *	if we have a junk filter, then project a new
	 *	tuple with the junk removed.
	 *
	 *	Store this new "clean" tuple in the place of the 
	 *	original tuple.
	 *
	 *      Also, extract all the junk ifnormation we need.
	 * ----------------
	 */
	if ((junkfilter = get_es_junkFilter(estate)) != (JunkFilter)NULL) {
	    Datum 	datum;
	    NameData 	attrName;
	    HeapTuple 	newTuple;
	    Boolean 	isNull;

	    /* ---------------
	     * extract the 'ctid' junk attribute.
	     * ---------------
	     */
	    if (operation == REPLACE || operation == DELETE) {
		strcpy(&(attrName), "ctid");
		if (! ExecGetJunkAttribute(junkfilter,
					   slot,
					   &attrName,
					   &datum,
					   &isNull))
		    elog(WARN,"ExecutePlan: NO (junk) `ctid' was found!");
		
		if (isNull) 
		    elog(WARN,"ExecutePlan: (junk) `ctid' is NULL!");
		
		tupleid = (ItemPointer) DatumGetPointer(datum);
		tuple_ctid = *tupleid; /* make sure we don't free the ctid!! */
		tupleid = &tuple_ctid;
	    }

	    /* ---------------
	     * Find the "rule lock" info. (if it is there!)
	     * ---------------
	     */
	    strcpy(&(attrName), "lock");
	    if (! ExecGetJunkAttribute(junkfilter,
				       slot,
				       &attrName,
				       &datum,
				       &isNull))
		locks = InvalidRuleLock;
	    else {
		if (isNull) {
		    /*
		     * this is an error (I think...)
		     * If we have a "lock" junk attribute, then it better
		     * have a value!
		     * NOTE: an empty lock i.e. "(numOfLocks: 0)"::lock
		     * is a good non-null value, i.e. still 'isNull' must
		     * be false
		     */
		    elog(WARN, "ExecutePlan: (junk) rule lock is NULL!");
		}
		
		locks = (RuleLock) DatumGetPointer(datum);
		
		/*
		 * Make a copy of the lock, because when we call
		 * amreplace, we will (probably?) free it and the
		 * "locks" as returned by "ExecGetJunkAttribute" is
		 * something that -for some reason- causes problems when
		 * you try to pfree it! (believe me, I've tried it!).
		 */
		locks = prs2CopyLocks(locks);
	    }

	    /* ---------------
	     * Finally create a new "clean" tuple with all junk attributes
	     * removed and with the new "rule lock" info.
	     * ---------------
	     */
	    newTuple = ExecRemoveJunk(junkfilter, slot);
	    
	    slot = (TupleTableSlot)
		ExecStoreTuple((Pointer) newTuple, /* tuple to store */
			       (Pointer) slot,     /* destination slot */
			       InvalidBuffer, 	/* this tuple has no buffer */
			       true); 		/* tuple should be pfreed */
	} else {
	    locks = InvalidRuleLock;
	}

#ifdef PALLOC_DEBUG	
	/* ----------------
	 *	if debugging memory allocations, print memory dump
	 *	every so often
	 * ----------------
	 */
	{
	    extern int query_tuple_count, query_tuple_max;
	    if (query_tuple_max && query_tuple_max == ++query_tuple_count) {
		query_tuple_count = 0;
		dump_palloc_list("ExecutePlan", true);
	    }
	}
#endif PALLOC_DEBUG
	
	/* ----------------
	 *	now that we have a tuple, do the appropriate thing
	 *	with it.. either return it to the user, add
	 *	it to a relation someplace, delete it from a
	 *	relation, or modify some of it's attributes.
	 * ----------------
	 */
	
	switch(operation) {
	case RETRIEVE:
	    result = ExecRetrieve(slot, 	  /* slot containing tuple */
				  printfunc,	  /* print function */
				  intoRelationDesc); /* "into" relation */
	    break;
	    
	case APPEND:
	    result = ExecAppend(slot, tupleid, estate, locks);
	    break;
	    
	case DELETE:
	    result = ExecDelete(slot, tupleid, estate);
	    break;
	    
	case REPLACE:
	    result = ExecReplace(slot, tupleid, estate, parseTree, locks);
	    break;

	    /* Total hack. I'm ignoring any accessor functions for
	       Relation, RelationTupleForm, NameData.
	       Assuming that NameData.data has offset 0.
	       */
	case NOTIFY: {
	    RelationInfo rInfo = get_es_result_relation_info(estate);
	    Relation rDesc = get_ri_RelationDesc(rInfo);
	    Async_Notify(&rDesc->rd_rel->relname);
	    result = NULL;
	    current_tuple_count = 0;
	    numberTuples = 1;
	    elog(DEBUG, "ExecNotify %s",&rDesc->rd_rel->relname);
	}
	    break;
	    
	default:
	    elog(DEBUG, "ExecutePlan: unknown operation in queryDesc");
	    result = NULL;
	    break;
	}
	/* ----------------
	 *	check our tuple count.. if we've returned the
	 *	proper number then return, else loop again and
	 *	process more tuples..
	 * ----------------
	 */
	current_tuple_count += 1;
	if (numberTuples == current_tuple_count)
	    break;
    }

    /* ----------------
     *	here, result is either a slot containing a tuple in the case
     *  of a RETRIEVE or NULL otherwise.
     * ----------------
     */
    return result;	    
}
 
/* ----------------------------------------------------------------
 *	ExecRetrieve
 *
 *	RETRIEVEs are easy.. we just pass the tuple to the appropriate
 *	print function.  The only complexity is when we do a
 *	"retrieve into", in which case we insert the tuple into
 *	the appropriate relation (note: this is a newly created relation
 *	so we don't need to worry about indices or locks.)
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecRetrieve(slot, printfunc, intoRelationDesc)
    TupleTableSlot	slot;
    void	 	(*printfunc)();
    Relation	 	intoRelationDesc;
{
    HeapTuple	 	tuple;
    TupleDescriptor 	attrtype;

    /* ----------------
     *	get the heap tuple out of the tuple table slot
     * ----------------
     */
    tuple = (HeapTuple) ExecFetchTuple((Pointer) slot);
    attrtype =		ExecSlotDescriptor((Pointer) slot);
    
    /* ----------------
     *	insert the tuple into the "into relation"
     * ----------------
     */
    if (intoRelationDesc != NULL) {
	if (ParallelExecutorEnabled())
	    /* for parallel executor, has to insert through shared buffer */
	    (void) heap_insert(intoRelationDesc, tuple, NULL);
	else
	    (void) local_heap_insert(intoRelationDesc, tuple);
	IncrAppended();
    }
    
    /* ----------------
     *	send the tuple to the front end (or the screen)
     * ----------------
     */
    (*printfunc)(tuple, attrtype);
    IncrRetrieved();
    
    /* ----------------
     *	return the tuple slot back to ExecutePlan
     * ----------------
     */
    return
	slot;
}
 
/* ----------------------------------------------------------------
 *	ExecAppend
 *
 *	APPENDs are trickier.. we have to insert the tuple into
 *	the base relation, insert appropriate tuples into the
 *	index relations and if we step on some locks along the way,
 *	run the rule manager..
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecAppend(slot, tupleid, estate, newlocks)
    TupleTableSlot	slot;
    ItemPointer		tupleid;
    EState		estate;
    RuleLock		newlocks;
{
    HeapTuple	 tuple;
    RelationInfo resultRelationInfo;
    Relation	 resultRelationDesc;
    int		 numIndices;
    RuleLock 	 locks;
    RuleLock	 indexLocks;
    HeapTuple	 returnedTuple;
    Buffer	 returnedBuffer;
    Prs2Status   status;
    Prs2Stub	 ruleStubs;
    RuleLock 	 stubLocks;
    HeapTuple	 tempTuple;
    ObjectId	 newId;

    /* ----------------
     *	get the heap tuple out of the tuple table slot
     * ----------------
     */
    tuple = (HeapTuple) ExecFetchTuple((Pointer) slot);
    
    /* ----------------
     *	get information on the result relation
     * ----------------
     */
    resultRelationInfo = get_es_result_relation_info(estate);
    resultRelationDesc = get_ri_RelationDesc(resultRelationInfo);
    
    /* ----------------
     *	process rules
     * ----------------
     */
#ifdef EXEC_RULEMANAGER
    /*
     * NOTE: the check for rule stubs etc. is done by the rule manager.
     */

    /*
     * Now call the rule manager, but only if necessary.
     */
    if (prs2MustCallRuleManager(get_es_result_rel_ruleinfo(estate),
				(HeapTuple) NULL,
				InvalidBuffer,
				APPEND))
    {
	status = prs2Main(estate,
			  (RelationRuleInfo) NULL,/*only aplies to retrieves*/
			  APPEND,
			  0,
			  resultRelationDesc,
			  (HeapTuple) NULL,
			  InvalidBuffer,
			  tuple,
			  InvalidBuffer,
			  (HeapTuple) NULL,
			  InvalidBuffer,
			  (AttributeNumberPtr) NULL,
			  0,
			  &returnedTuple,
			  &returnedBuffer);
	if (status == PRS2_STATUS_INSTEAD) {
	    /*
	     * do not append the tuple
	     */
	    return NULL;
	    
	} else if (status == PRS2_STATUS_TUPLE_CHANGED) {
	    /* ----------------
	     *  store the returnedTuple in the slot and free the old
	     *  copy of the tuple.
	     * ----------------
	     */
	    ExecStoreTuple((Pointer) returnedTuple, /* tuple to store */
			   (Pointer) slot,           /* destination slot */
			   returnedBuffer,
			   true);		/* tuple should be pfreed */
	    
	    tuple = returnedTuple;
	}
    } /*if prs2MustCallRuleManager*/

#endif EXEC_RULEMANAGER
    
    /* ----------------
     *	have to add code to preform unique checking here.
     *  cim -12/1/89
     * ----------------
     */
    
    /* ----------------
     * If the user has explicitly specified some locks in the "replace"
     * statement, ignore all rule stubs & stuff and just put these
     * locks in the tuple. Obviously this is guaranteed to mess things
     * up if the user does not *really* know what he is doing,
     * i.e. if it is not me!  _sp.
     * ----------------
     */
    if (newlocks != InvalidRuleLock)
	HeapTupleSetRuleLock(tuple, InvalidBuffer, newlocks);
    
    /* ----------------
     *	insert the tuple
     * ----------------
     */
    newId = heap_insert(resultRelationDesc, /* relation desc */
			tuple,		    /* heap tuple */
			0);		    /* return: offset */
    IncrAppended();
    UpdateAppendOid(newId);
    
    /* ----------------
     *	process indices
     * 
     *	Note: heap_insert adds a new tuple to a relation.  As a side
     *  effect, the tupleid of the new tuple is placed in the new
     *  tuple's t_ctid field.
     * ----------------
     */
    numIndices = get_ri_NumIndices(resultRelationInfo);
    if (numIndices > 0) {
	indexLocks = ExecInsertIndexTuples(slot,
					   &(tuple->t_ctid),
					   estate);
    }

    /* ----------------
     *	XXX will we ever return anything meaningful?
     * ----------------
     */
    return NULL;
}
 
/* ----------------------------------------------------------------
 *	ExecDelete
 *
 *	DELETE is like append, we delete the tuple and its
 *	index tuples and then run the rule manager if necessary.
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecDelete(slot, tupleid, estate)
    TupleTableSlot	slot;
    ItemPointer		tupleid;
    EState		estate;
{
    HeapTuple	 tuple;
    RelationInfo resultRelationInfo;
    Relation	 resultRelationDesc;
    
    RuleLock	 locks;
    RuleLock	 indexLocks;
    Prs2Status   status;
    HeapTuple    oldTuple;
    Buffer       oldTupleBuffer;
    HeapTuple    rawTuple;
    Buffer	 rawTupleBuffer;
    HeapTuple    dummyTuple;
    Buffer       dummyBuffer;

    /* ----------------
     *	get the heap tuple out of the tuple table slot
     * ----------------
     */
    tuple = (HeapTuple) ExecFetchTuple((Pointer) slot);

    /* ----------------
     *	get the result relation information
     * ----------------
     */
    resultRelationInfo = get_es_result_relation_info(estate);
    resultRelationDesc = get_ri_RelationDesc(resultRelationInfo);

#ifdef EXEC_RULEMANAGER
    /* ----------------
     *        process rules
     * ----------------
     */
    
    /* ----------------
     * find the old & raw tuples
     *
     * NOTE: currently we do not store them anywhere!
     * therefore we will use the 'tupleid' to find the original
     * tuple (the "raw" tuple") and we will assume that "old" tuple
     * is the same as the "raw" tuple.
     * The only problem is the extra time spent to reactivate
     * backward chaining rules already activated but... what
     * else can we do ?
     */
    rawTuple = heap_fetch(resultRelationDesc,
			  NowTimeQual,
			  tupleid,
			  &rawTupleBuffer);
    
    oldTuple = rawTuple;
    oldTupleBuffer = rawTupleBuffer;

    /* ----------------
     * XXX!!
     * NOTE: in the current implementation where only RelationLevel
     * locks are used, the rule manager must look in pg_relation
     * to find the locks of "oldTuple".
     */
    if (prs2MustCallRuleManager(get_es_result_rel_ruleinfo(estate),
				oldTuple, oldTupleBuffer, DELETE))
    {
	status = prs2Main(estate,
			  (RelationRuleInfo) NULL,/*only aplies to retrieves*/
			  DELETE,
			  0,      /* userid */
			  resultRelationDesc,
			  oldTuple,
			  oldTupleBuffer,
			  (HeapTuple) NULL,
			  InvalidBuffer,
			  rawTuple,
			  rawTupleBuffer,
			  (AttributeNumberPtr) NULL,
			  0,
			  &dummyTuple,
			  &dummyBuffer);
	
	if (status == PRS2_STATUS_INSTEAD) {
	  /*
	   * there was an instead rule, do not do the delete...
	   */
	  return NULL;
	}
    } /*if prs2MustCallRuleManager*/
    if (BufferIsValid(rawTupleBuffer)) ReleaseBuffer(rawTupleBuffer);

#endif EXEC_RULEMANAGER
    
    /* ----------------
     *	delete the tuple
     * ----------------
     */
    locks = heap_delete(resultRelationDesc, /* relation desc */
			tupleid);	    /* item pointer to tuple */
    
    IncrDeleted();
    
    /* ----------------
     *	Note: Normally one would think that we have to
     *	      delete index tuples associated with the
     *	      heap tuple now..
     *
     *	      ... but in POSTGRES, we have no need to do this
     *        because the vaccuum daemon automatically
     *	      opens an index scan and deletes index tuples
     *	      when it finds deleted heap tuples. -cim 9/27/89
     * ----------------
     */
    
    /* ----------------
     *	XXX will we ever return anything meaningful?
     * ----------------
     */
    return NULL;
}
 
/* ----------------------------------------------------------------
 *	ExecReplace
 *
 *	note: we can't run replace queries with transactions
 *  	off because replaces are actually appends and our
 *  	scan will mistakenly loop forever, replacing the tuple
 *  	it just appended..  This should be fixed but until it
 *  	is, we don't want to get stuck in an infinite loop
 *  	which corrupts your database..
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecReplace(slot, tupleid, estate, parseTree, newlocks)
    TupleTableSlot	slot;
    ItemPointer		tupleid;
    EState		estate;
    LispValue		parseTree;
    RuleLock		newlocks;
{
    HeapTuple		tuple;
    RelationInfo 	resultRelationInfo;
    Relation	 	resultRelationDesc;
    RuleLock	 	locks;
    RuleLock	 	indexLocks;
    int		 	numIndices;
    HeapTuple	 	rawTuple;
    Buffer		rawTupleBuffer = InvalidBuffer;
    HeapTuple 		oldTuple;
    Buffer 		oldTupleBuffer;
    HeapTuple 		changedTuple;
    Buffer    		changedTupleBuffer;
    AttributeNumberPtr 	attributeArray;
    AttributeNumber    	numberOfReplacedAttributes;
    LispValue 		targetList;
    LispValue		t;
    Resdom    		resdom;
    Prs2Status        	status;
    Prs2Stub     	ruleStubs;
    RuleLock     	stubLocks;
    HeapTuple    	tempTuple;


    /* ----------------
     *	abort the operation if not running transactions
     * ----------------
     */
    if (IsBootstrapProcessingMode()) {
	elog(DEBUG, "ExecReplace: replace can't run without transactions");
	return NULL;
    }
    
    /* ----------------
     *	get the heap tuple out of the tuple table slot
     * ----------------
     */
    tuple = (HeapTuple) ExecFetchTuple((Pointer) slot);

    /* ----------------
     *	get the result relation information
     * ----------------
     */
    resultRelationInfo = get_es_result_relation_info(estate);
    resultRelationDesc = get_ri_RelationDesc(resultRelationInfo);

    /* ----------------
     *  process rules
     * ----------------
     */
#ifdef EXEC_RULEMANAGER    


    /* ----------------
     * find the old & raw tuples
     *
     * NOTE: currently we do not store them anywhere!
     * therefore we will use the 'tupleid' to find the original
     * tuple (the "raw" tuple") and we will assume that "old" tuple
     * is the same as the "raw" tuple.
     * The only problem is the extra time spent to reactivate
     * backward chaining rules already activated but... what
     * else can we do ?
     */
    rawTuple = heap_fetch(resultRelationDesc,
			  NowTimeQual,
			  tupleid,
			  &rawTupleBuffer);
    
    oldTuple = rawTuple;
    oldTupleBuffer = rawTupleBuffer;


    if (prs2MustCallRuleManager(get_es_result_rel_ruleinfo(estate),
				oldTuple, oldTupleBuffer, REPLACE))
    {
				    
	/* ----------------
	 * find the attributes replaced...
	 */
	attributeArray = (AttributeNumberPtr)
	    palloc( sizeof(AttributeNumber)	*
		    RelationGetNumberOfAttributes(resultRelationDesc));
	
	numberOfReplacedAttributes = 0;
	targetList = parse_targetlist(parseTree);
	foreach (t, targetList) {
	    resdom = (Resdom) CAR(CAR(t));
	    attributeArray[numberOfReplacedAttributes] = get_resno(resdom);
	    numberOfReplacedAttributes += 1;
	}

	status = prs2Main(estate,
			  (RelationRuleInfo) NULL,/*only aplies to retrieves*/
			  REPLACE,
			  0,      /* user id */
			  resultRelationDesc,
			  oldTuple,
			  oldTupleBuffer,
			  tuple,
			  InvalidBuffer,
			  rawTuple,
			  oldTupleBuffer,
			  attributeArray,
			  numberOfReplacedAttributes,
			  &changedTuple,
			  &changedTupleBuffer);
	
	if (status == PRS2_STATUS_INSTEAD) {
	    /*
	     * An instead rule has been found, do not do the replace...
	     */
	    return NULL;
	}	
	

	if (status == PRS2_STATUS_TUPLE_CHANGED) {
	    ExecStoreTuple((Pointer) changedTuple, /* tuple to store */
			   (Pointer) slot,	/* destination slot */
			   InvalidBuffer, 	/* this tuple has no buffer */
			   true);		/* tuple should be pfreed */
	    
	    tuple = changedTuple;
	}
    } /*if prs2MustCallRuleManager */
    if (BufferIsValid(oldTupleBuffer)) ReleaseBuffer(oldTupleBuffer);
	
#endif EXEC_RULEMANAGER
    
    /* ----------------
     *	have to add code to preform unique checking here.
     *  in the event of unique tuples, this becomes a deletion
     *  of the original tuple affected by the replace.
     *  cim -12/1/89
     * ----------------
     */
    
    /* ----------------
     * If the user has explicitly specified some locks in the "replace"
     * statement, ignore all rule stubs & stuff and just put these
     * locks in the tuple. Obviously this is guaranteed to mess things
     * up if the user does not *really* know what he is doing,
     * i.e. if it is not me!  _sp.
     * ----------------
     */
    if (newlocks != InvalidRuleLock)
	HeapTupleSetRuleLock(tuple, InvalidBuffer, newlocks);

    /* ----------------
     *	replace the heap tuple
     * ----------------
     */
    locks = heap_replace(resultRelationDesc, /* relation desc */
			 tupleid,	     /* item ptr of tuple to replace */
			 tuple);	     /* replacement heap tuple */
    
    IncrReplaced();
    
    /* ----------------
     *	Note: instead of having to update the old index tuples
     *        associated with the heap tuple, all we do is form
     *	      and insert new index tuples..  This is because
     *        replaces are actually deletes and inserts and
     *	      index tuple deletion is done automagically by
     *	      the vaccuum deamon.. All we do is insert new
     *	      index tuples.  -cim 9/27/89
     * ----------------
     */
    
    /* ----------------
     *	process indices
     *
     *	heap_replace updates a tuple in the base relation by invalidating
     *  it and then appending a new tuple to the relation.  As a side
     *  effect, the tupleid of the new tuple is placed in the new
     *  tuple's t_ctid field.  So we now insert index tuples using
     *  the new tupleid stored there.
     * ----------------
     */
    numIndices = get_ri_NumIndices(resultRelationInfo);
    if (numIndices > 0) {
	indexLocks = ExecInsertIndexTuples(slot,
					   &(tuple->t_ctid),
					   estate);
    }
    
    /* ----------------
     *	XXX will we ever return anything meaningful?
     * ----------------
     */
    return NULL;
}
