/* ----------------------------------------------------------------
 *   FILE
 *	ex_utils.c
 *	
 *   DESCRIPTION
 *	miscellanious executor utility routines
 *
 *   INTERFACE ROUTINES
 *	ExecAssignNodeBaseInfo	\
 *	ExecAssignDebugHooks	 >  preforms misc work done in all the
 *	ExecAssignExprContext	/   init node routines.
 *
 *	ExecGetTypeInfo	  	\
 *	ExecMakeTypeInfo  	 \
 *	ExecOrderTypeInfo  	  \
 *	ExecSetTypeInfo    	  |  old execCStructs interface
 *	ExecFreeTypeInfo 	  |  code from the version 1
 *	ExecMakeSkeys 	  	  |  lisp system.  These should
 *	ExecSetSkeys 	  	  |  go away or be updated soon.
 *	ExecSetSkeysValue 	  |  -cim 11/1/89
 *	ExecFreeSkeys 	  	  |
 *	ExecMakeTLValues 	  |
 *	ExecSetTLValues		  /
 *	ExecFreeTLValues	 /
 *	ExecTupleAttributes	/
 *
 *	get_innerPlan	\  convience accessors to eliminate confusion
 *	get_outerPlan	/    between right/left inner/outer
 *
 *	QueryDescGetTypeInfo - moved here from main.c
 *				am not sure what uses it -cim 10/12/89
 *
 *	ExecCollect		   \
 *	ExecUniqueCons		    \
 *	ExecGetVarAttlistFromExpr    >	support routines for
 *	ExecGetVarAttlistFromTLE    /	ExecInitScanAttributes
 *	ExecMakeAttsFromList	   /
 *
 *	ExecInitScanAttributes	\    creates/destroys array of attribute
 *	ExecFreeScanAttributes	/    numbers actually examined by the executor
 *					(needed by the rule manager -cim)
 *
 *   NOTES
 *	This file has traditionally been the place to stick misc.
 *	executor support stuff that doesn't really go anyplace else.
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
 
#include "executor/executor.h"
#include "nodes/relation.h"	/* needed before including keys.h */
#include "planner/keys.h"	/* needed for definition of INNER */

 RcsId("$Header$");
/* ----------------------------------------------------------------
 *      global counters for number of tuples processed, retrieved,
 *      appended, replaced, deleted.
 * ----------------------------------------------------------------
 */
int     NTupleProcessed;
int     NTupleRetrieved;
int     NTupleReplaced;
int     NTupleAppended;
int     NTupleDeleted;
int	NIndexTupleInserted;
extern int NIndexTupleProcessed;  /* have to be defined in the access
			             method level so that the cinterface.a
			             will link ok. */

/* ----------------------------------------------------------------
 *		    	statistic functions
 * ----------------------------------------------------------------
 */
 
/* ----------------------------------------------------------------
 *	ResetTupleCount
 * ----------------------------------------------------------------
 */
void
ResetTupleCount()
{
    NTupleProcessed = 0;
    NTupleRetrieved = 0;
    NTupleAppended = 0;
    NTupleDeleted = 0;
    NTupleReplaced = 0;
    NIndexTupleProcessed = 0;
}

/* ----------------------------------------------------------------
 *	PrintTupleCount
 * ----------------------------------------------------------------
 */
void
DisplayTupleCount(statfp)
FILE *statfp;
{
    if (NTupleProcessed > 0)
	fprintf(statfp, "!\t%d tuple%s processed, ", NTupleProcessed,
	       (NTupleProcessed == 1) ? "" : "s");
    else {
	fprintf(statfp, "!\tno tuples processed.\n");
	return;
      }
    if (NIndexTupleProcessed > 0)
	fprintf(statfp, "%d indextuple%s processed, ", NIndexTupleProcessed,
	       (NIndexTupleProcessed == 1) ? "" : "s");
    if (NIndexTupleInserted > 0)
	fprintf(statfp, "%d indextuple%s inserted, ", NIndexTupleInserted,
	       (NIndexTupleInserted == 1) ? "" : "s");
    if (NTupleRetrieved > 0)
	fprintf(statfp, "%d tuple%s retrieved. ", NTupleRetrieved,
	       (NTupleRetrieved == 1) ? "" : "s");
    if (NTupleAppended > 0)
	fprintf(statfp, "%d tuple%s appended. ", NTupleAppended,
	       (NTupleAppended == 1) ? "" : "s");
    if (NTupleDeleted > 0)
	fprintf(statfp, "%d tuple%s deleted. ", NTupleDeleted,
	       (NTupleDeleted == 1) ? "" : "s");
    if (NTupleReplaced > 0)
	fprintf(statfp, "%d tuple%s replaced. ", NTupleReplaced,
	       (NTupleReplaced == 1) ? "" : "s");
    fprintf(statfp, "\n");
}

/* ----------------------------------------------------------------
 *		 miscellanious init node support functions
 *
 *	ExecAssignNodeBaseInfo	- assigns the baseid field of the node
 *	ExecAssignDebugHooks	- assigns the node's debugging hooks
 *	ExecAssignExprContext	- assigns the node's expression context
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecGetPrivateState
 *
 *	This is used to get a pointer to a node's private state.
 * ----------------
 */
BaseNode
ExecGetPrivateState(node)
    Plan node;
{
    switch(NodeType(node)) {
    case classTag(Result):
	return (BaseNode) get_resstate((Result) node);
    
    case classTag(Append):
	return (BaseNode) get_unionstate((Append) node);
    
    case classTag(NestLoop):
	return (BaseNode) get_nlstate((NestLoop) node);
    
    case classTag(MergeJoin):
	return (BaseNode) get_mergestate((MergeJoin) node);
    
    case classTag(HashJoin):
	return (BaseNode) get_hashjoinstate((HashJoin) node);
    
    case classTag(SeqScan):
	return (BaseNode) get_scanstate((Scan) node);
    
    case classTag(IndexScan):
	return (BaseNode) get_indxstate((IndexScan) node);
    
    case classTag(Material):
	return (BaseNode) get_matstate((Material) node);
    
    case classTag(Sort):
	return (BaseNode) get_sortstate((Sort) node);
    
    case classTag(Unique):
	return (BaseNode) get_uniquestate((Unique) node);
    
    case classTag(Hash):
	return (BaseNode) get_hashstate((Hash) node);

    default:
	return NULL;
    }
}

/* ----------------
 *	ExecAssignNodeBaseInfo
 *
 *	as it says, this assigns the baseid field of the node and
 *	increments the counter in the estate.  In addition, it initializes
 *	the base_parent field of the basenode.
 * ----------------
 */
void
ExecAssignNodeBaseInfo(estate, basenode, parent)
    EState	estate;
    BaseNode	basenode;
    Plan	parent;
{
    int		baseid;
    BaseNode	parentstate;
	
    baseid = get_es_BaseId(estate);
    set_base_id(basenode, baseid);
    baseid++;
    set_es_BaseId(estate, baseid);
    
    set_base_parent(basenode, (Pointer) parent);

    if (parent != NULL) 
	parentstate = ExecGetPrivateState(parent);
    else
	parentstate = (BaseNode) NULL;
    
    set_base_parent_state(basenode, (Pointer) parentstate);
}

/* ----------------
 *	ExecAssignDebugHooks
 *
 *	this assigns the executor's debugging hooks, if we compile
 *	with EXEC_ASSIGNDEBUGHOOKS set.
 * ----------------
 */
void
ExecAssignDebugHooks(node, basenode)
    Plan	node;
    BaseNode	basenode;
{
#ifdef EXEC_ASSIGNDEBUGHOOKS
    HookNode	 hook;
    HookFunction proc;
    
    if (_debug_hook_id_ > 0) {
	hook = (HookNode) _debug_hooks_[ _debug_hook_id_ ];
	set_base_hook(basenode, hook); 
	proc = (HookFunction) get_hook_at_initnode(hook);
	if (proc != NULL)
	    (*proc)(node, basenode);
    }
#endif EXEC_ASSIGNDEBUGHOOKS
}

/* ----------------
 *	ExecAssignExprContext
 *
 *	This initializes the ExprContext field.  It is only necessary
 *	to do this for nodes which use ExecQual or ExecTargetList
 *	because those routines depend on econtext.  Other nodes which
 *	dont have to evaluate expressions don't need to do this.
 * ----------------
 */
void
ExecAssignExprContext(estate, commonstate)
    EState	estate;
    CommonState	commonstate;
{
    ExprContext    econtext;
    ParamListInfo  paraminfo;
    List           rangeTable;
    
    paraminfo = get_es_param_list_info(estate);
    rangeTable = get_es_range_table(estate);

    econtext = MakeExprContext(NULL,		/* scan tuple slot */
			       NULL,		/* inner tuple slot */
			       NULL,		/* outer tuple slot */
			       NULL,		/* relation */
			       0,		/* relid */
			       paraminfo,	/* param list info */
			       rangeTable);	/* range table */

    set_cs_ExprContext(commonstate, econtext);
}

/* ----------------------------------------------------------------
 *	Result slot tuple type and ProjectionInfo support
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecAssignResultType
 * ----------------
 */
void
ExecAssignResultType(commonstate, execTupDesc, tupDesc)
    CommonState		commonstate;
    ExecTupDescriptor	execTupDesc;
    TupleDescriptor	tupDesc;
{
    TupleTableSlot	slot;
    
    slot = get_cs_ResultTupleSlot(commonstate);
    ExecSetSlotExecDescriptor(slot, execTupDesc);
    (void) ExecSetSlotDescriptor((Pointer) slot, tupDesc);
}

/* ----------------
 *	ExecAssignResultTypeFromOuterPlan
 * ----------------
 */
void
ExecAssignResultTypeFromOuterPlan(node, commonstate)
    Plan 	node;
    CommonState	commonstate;
{
    Plan		outerPlan;
    ExecTupDescriptor	execTupDesc;
    TupleDescriptor	tupDesc;
    
    outerPlan =   get_outerPlan(node);
    execTupDesc = ExecGetExecTupDesc(outerPlan);
    tupDesc = ExecGetTupType(outerPlan);
    
    ExecAssignResultType(commonstate, execTupDesc, tupDesc);
}

/* ----------------
 *	ExecAssignResultTypeFromTL
 * ----------------
 */
void
ExecAssignResultTypeFromTL(node, commonstate)
    Plan 	node;
    CommonState	commonstate;
{
    List	        targetList;
    Var			tlvar;
    int			i;
    int			len;
    List		tl, tle;
    List		fjtl;
    ExecTupDescriptor	execTupDesc;
    TupleDescriptor	tupDesc;
    int			fjcount;
    int			varlen;
    TupleDescriptor	varTupDesc;
    TupleDescriptor	origTupDesc;
    
    targetList =  get_qptargetlist(node);
    origTupDesc = ExecTypeFromTL(targetList);
    if ((len = ExecTargetListLength(targetList)) != 0)
	execTupDesc = ExecMakeExecTupDesc(len);
    else
	execTupDesc = (ExecTupDescriptor)NULL;

    fjtl = LispNil;
    tl = targetList;
    i = 0;
    while (!lispNullp(tl) || !lispNullp(fjtl)) {
	if (!lispNullp(fjtl)) {
	    tle = CAR(fjtl);
	    fjtl = CDR(fjtl);
	  }
	else {
	    tle = CAR(tl);
	    tl = CDR(tl);
	  }
	if (!tl_is_resdom(tle)) {
	    /* it is a FJoin */
	    fjtl = CDR(tle);
	    tle = get_fj_innerNode((Fjoin)CAR(tle));
	  }
	if (get_rescomplex((Resdom)CAR(tle))) {
	    /* if it is a composite type, i.e., a tuple */
	    tlvar = (Var)get_expr(tle);
	    Assert(IsA(tlvar,Var));
	    varlen = ExecGetVarLen(node, commonstate, tlvar);
	    varTupDesc = ExecGetVarTupDesc(node, commonstate, tlvar);
	    execTupDesc->data[i] = MakeExecAttDesc(ATTTUP, varlen, varTupDesc);
	  }
	else {
	    /* this means that it is a base type */
	    execTupDesc->data[i] = ExecMakeExecAttDesc(ATTVAL, 1);
	    execTupDesc->data[i]->attdesc->data[0] = origTupDesc->data[i];
	  }
	i++;
      }
    if (len > 0) {
	tupDesc = ExecTupDescToTupDesc(execTupDesc, len);
	ExecAssignResultType(commonstate, execTupDesc, tupDesc);
    }
    else
	ExecAssignResultType(commonstate,
			     (ExecTupDescriptor)NULL,
			     (TupleDescriptor)NULL);
}

/* ----------------
 *	ExecGetResultType
 * ----------------
 */
TupleDescriptor
ExecGetResultType(commonstate)
    CommonState	commonstate;
{
    TupleTableSlot	slot = get_cs_ResultTupleSlot(commonstate);
    
    return 
	ExecSlotDescriptor((Pointer) slot);
}

/* ----------------
 *	ExecFreeResultType
 * ----------------
 */
void
ExecFreeResultType(commonstate)
    CommonState	commonstate;
{
    TupleTableSlot	slot;
    TupleDescriptor	tupType;
    
    slot = 	  get_cs_ResultTupleSlot(commonstate);
    tupType = 	  ExecSlotDescriptor((Pointer) slot);
    
    ExecFreeTypeInfo((struct attribute **) tupType);	/* BUG -- glass */
}


/* ----------------
 *	ExecAssignProjectionInfo
 * ----------------
 */
void
ExecAssignProjectionInfo(node, commonstate)
    Plan 	node;
    CommonState	commonstate;
{
    ProjectionInfo	projInfo;
    List	        targetList;
    int      		len;
    Pointer	 	tupValue;
	
    targetList =  get_qptargetlist(node);
    len = 	  ExecTargetListLength(targetList);
    tupValue =    (Pointer) ExecMakeTLValues(len);

    projInfo = 	MakeProjectionInfo(targetList, len, tupValue,
				   get_cs_ExprContext(commonstate),
				   get_cs_ResultTupleSlot(commonstate));
    set_cs_ProjInfo(commonstate, projInfo);
/*
    set_pi_targetlist(projInfo, targetList);
    set_pi_len(projInfo, len);
    set_pi_tupValue(projInfo, tupValue);
    set_pi_exprContext(projInfo, get_cs_ExprContext(commonstate));
    set_pi_slot(projInfo, get_cs_ResultTupleSlot(commonstate));
*/
}


/* ----------------
 *	ExecFreeProjectionInfo
 * ----------------
 */
void
ExecFreeProjectionInfo(commonstate)
    CommonState	commonstate;
{
    ProjectionInfo	projInfo;
    Pointer	 	tupValue;

    /* ----------------
     *	get projection info.  if NULL then this node has
     *  none so we just return.
     * ----------------
     */
    projInfo = get_cs_ProjInfo(commonstate);
    if (projInfo == NULL)
	return;

    /* ----------------
     *	clean up memory used.
     * ----------------
     */
    tupValue = get_pi_tupValue(projInfo);
    ExecFreeTLValues(tupValue);
    
    pfree(projInfo);
    set_cs_ProjInfo(commonstate, NULL);
}

/* ----------------------------------------------------------------
 *	the following scan type support functions are for
 *	those nodes which are stubborn and return tuples in
 *	their Scan tuple slot instead of their Result tuple
 *	slot..  luck fur us, these nodes do not do projections
 *	so we don't have to worry about getting the ProjectionInfo
 *	right for them...  -cim 6/3/91
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecGetScanType
 * ----------------
 */
TupleDescriptor
ExecGetScanType(csstate)
    CommonScanState	csstate;
{
    TupleTableSlot slot = (TupleTableSlot)
	get_css_ScanTupleSlot(csstate);
    return ExecSlotDescriptor((Pointer) slot);
}

/* ----------------
 *	ExecFreeScanType
 * ----------------
 */
void
ExecFreeScanType(csstate)
    CommonScanState csstate;
{
    TupleTableSlot	slot;
    TupleDescriptor	tupType;
    
    slot = 	  (TupleTableSlot) get_css_ScanTupleSlot(csstate);
    tupType = 	  ExecSlotDescriptor((Pointer) slot);
    
    ExecFreeTypeInfo((struct attribute **) tupType); /* bug -- glass */
}

/* ----------------
 *	ExecAssignScanType
 * ----------------
 */
void
ExecAssignScanType(csstate, execTupDesc, tupDesc)
    CommonScanState	csstate;
    ExecTupDescriptor	execTupDesc;
    TupleDescriptor	tupDesc;
{
    TupleTableSlot	slot;
    
    slot = (TupleTableSlot) get_css_ScanTupleSlot(csstate);
    (void) ExecSetSlotDescriptor((Pointer) slot, tupDesc);
    ExecSetSlotExecDescriptor(slot, execTupDesc);
}

/* ----------------
 *	ExecAssignScanTypeFromOuterPlan
 * ----------------
 */
void
ExecAssignScanTypeFromOuterPlan(node, commonstate)
    Plan 	node;
    CommonState	commonstate;
{
    Plan		outerPlan;
    TupleDescriptor	tupDesc;
    ExecTupDescriptor	execTupDesc;
    
    outerPlan =   get_outerPlan(node);
    tupDesc = 	  ExecGetTupType(outerPlan);
    execTupDesc = ExecGetExecTupDesc(outerPlan);
    
    ExecAssignScanType((CommonScanState) commonstate, execTupDesc, tupDesc);
}


/* ----------------------------------------------------------------
 *			stuff from execCStructs
 *
 *	execCStructs was a file used in the Lisp system to do
 *	stuff with C-allocated structures.  These routines were
 *	called from the lisp executor across the foreign function
 *	interface.  Since it was easier to just keep these when
 *	we converted from lisp to C, that's what we did.  The usefulness
 *	of these functions should be reevaluated sometime soon and
 *	the code rewritten accordingly once an interface is settled on.
 *	-cim 11/1/89
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *	ExecTypeFromTL support routines.
 *
 *	these routines are used mainly from ExecTypeFromTL.
 *	-cim 6/12/90
 *
 * old comments
 *	Routines dealing with the structure 'attribute' which conatains
 *	the type information about attributes in a tuple:
 *
 *	ExecGetTypeInfo(relDesc) --
 *		returns the structure 'attribute' from the relation desc.
 *	ExecMakeTypeInfo(noType) --
 *		returns pointer to array of 'noType' structure 'attribute'.
 *	ExecSetTypeInfo(index, typeInfo, attNum, attLen) --
 *		sets the element indexed by 'index' in typeInfo with
 *		the values: attNum, attLen.
 *	ExecFreeTypeInfo(typeInfo) --
 *		frees the structure 'typeInfo'.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecGetTypeInfo
 * ----------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
Attribute
ExecGetTypeInfo(relDesc)
    Relation 	relDesc;
{
    return
	((Attribute) &relDesc->rd_att);
}

/* ----------------
 *	ExecMakeTypeInfo
 *
 *	This creates a pre-initialized array of pointers to
 *	attribute structures.  The structures themselves are
 *	expected to be initialized via calls to ExecSetTypeInfo.
 *
 *	The structure returned is a single area of memory which
 *	looks like:
 *
 *      +------+------+-----+------------+------------+-----+
 *	| ptr0 | ptr1 | ... | attribute0 | attribute1 | ... |
 *      +------+------+-----+------------+------------+-----+
 *	
 *	where ptr0 = &attribute0, ptr1 = &attribute1, etc..
 * ----------------
 */
TupleDescriptor
ExecMakeTypeInfo(nelts)
    int	nelts;
{
    struct attribute 	*ap;
    struct attribute	**info;
    int     		i;
    int     		size;

    /* ----------------
     *	calculate the size of the entire structure
     * ----------------
     */
    if (nelts <= 0)
	return NULL;

    size = (nelts * sizeof(struct attribute)) +
	((nelts + 1) * sizeof(struct attribute *));

    /* ----------------
     *	allocate the whole structure and initialize all
     *  the pointers in the initial array. The array is made
     *  one element too large so that it is NULL terminated,
     *  just in case..
     * ----------------
     */
    info = (struct attribute **) palloc(size);
    ap =   (struct attribute *) (info + (nelts + 1));
	
    for (i=0; i<nelts; i++) {
	info[i] = ap++;
    }
    info[i] = NULL;

    return
	(TupleDescriptor) info;
}

/* ----------------
 *	ExecSetTypeInfo
 *
 *	This initializes fields of a single attribute in a
 *	tuple descriptor from the specified parameters.
 *
 *	XXX this duplicates much of the functionality of TupleDescInitEntry.
 *	    the routines should be moved to the same place and be rewritten
 *	    to share common code.
 * ----------------
 */
void
ExecSetTypeInfo(index, typeInfo, typeID, attNum, attLen, attName, attbyVal)
    int 		index;
    struct attribute 	**typeInfo;
    ObjectId			typeID;
    int			attNum;
    int			attLen;
    char		*attName;
    Boolean		attbyVal;
{
    struct attribute	*att;

    /* ----------------
     *	get attribute pointer and preform a sanity check..
     * ----------------
     */
    att = typeInfo[index];
    if (att == NULL)
	elog(WARN, "ExecSetTypeInfo: trying to assign through NULL");
    
    /* ----------------
     *	assign values to the tuple descriptor, being careful not
     *  to copy a null attName..
     *
     *  XXX it is unknown exactly what information is needed to
     *      initialize the attribute struct correctly so for now
     *	    we use 0.  this should be fixed -- otherwise we run the
     *	    risk of using garbage data. -cim 5/5/91
     * ----------------
     */
    att->attrelid  = 0;				/* dummy value */
    
    if (attName != (char *) NULL)
	strncpy(att->attname, attName, 16);
    else
	bzero(att->attname, 16);
    
    att->atttypid = 	typeID;
    att->attdefrel = 	0;			/* dummy value */
    att->attnvals  = 	0;			/* dummy value */
    att->atttyparg = 	0;			/* dummy value */
    att->attlen =   	attLen;
    att->attnum =   	attNum;
    att->attbound = 	0;			/* dummy value */
    att->attbyval = 	attbyVal;
    att->attcanindex = 	0;			/* dummy value */
    att->attproc = 	0;			/* dummy value */
    att->attnelems = 	0;			/* dummy value */
    att->attcacheoff = 	-1;
}

ExecTupDescriptor
ExecMakeExecTupDesc(len)
int len;
{
    ExecTupDescriptor retval;

    retval = (ExecTupDescriptor)palloc(len * sizeof(ExecTupDescriptorData));
    return retval;
}

ExecAttDesc
ExecMakeExecAttDesc(tag, len)
AttributeTag tag;
int len;
{
    ExecAttDesc attdesc;
    attdesc = (ExecAttDesc)palloc(sizeof(ExecAttDescData));
    attdesc->tag = tag;
    attdesc->len = len;
    attdesc->attdesc = CreateTemplateTupleDesc(len);
    return attdesc;
}

ExecAttDesc
MakeExecAttDesc(tag, len, tupdesc)
AttributeTag tag;
int len;
TupleDescriptor tupdesc;
{
    ExecAttDesc attdesc;
    attdesc = (ExecAttDesc)palloc(sizeof(ExecAttDescData));
    attdesc->tag = tag;
    attdesc->len = len;
    attdesc->attdesc = tupdesc;
    return attdesc;
}

/* ----------------
 *	ExecFreeTypeInfo frees the array of attrbutes
 *	created by ExecMakeTypeInfo and returned by ExecTypeFromTL...
 * ----------------
 */
void
ExecFreeTypeInfo(typeInfo)
    struct attribute **typeInfo;
{
    /* ----------------
     *	do nothing if asked to free a null pointer
     * ----------------
     */
    if (typeInfo == NULL)
	return;

    /* ----------------
     *	the entire array of typeinfo pointers created by
     *  ExecMakeTypeInfo was allocated with a single palloc()
     *  so we can deallocate the whole array with a single pfree().
     *  (we should not try and free all the elements in the array)
     *  -cim 6/12/90
     * ----------------
     */
    pfree(typeInfo);
}
 
/* ----------------------------------------------------------------
 *	QueryDescGetTypeInfo
 *
 *|	I don't know how this is used, all I know is that it
 *|	appeared one day in main.c so I moved it here. -cim 11/1/89
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
List
QueryDescGetTypeInfo(queryDesc)
    List queryDesc;
{
    Plan            plan;
    TupleDesc       tupleType;
    List            targetList;
    
    plan = 		    QdGetPlan(queryDesc);
    tupleType = (TupleDesc) ExecGetTupType(plan);
    targetList = 	    get_qptargetlist(plan);
    
    return (List)
	MakeList(lispInteger(ExecTargetListLength(targetList)), tupleType, -1);
}

/* ----------------------------------------------------------------
 *		   ExecInitScanAttributes support
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	ExecCollect
 *
 *	This function calls its collect function on the results
 *	of calling its apply function on successive elements
 *	of the list passed to it.
 * --------------------------------
 */
/**** xxref:
 *           ExecGetVarAttlistFromExpr
 *           ExecInitScanAttributes
 ****/
List
ExecCollect(l, applyFunction, collectFunction, applyParameters)
    List l;
    List (*applyFunction)();
    List (*collectFunction)();
    List applyParameters;
{
    List cdrEntry;
    List entry;
    List applyResult;
    List collection;
    collection = LispNil;
    
    foreach(cdrEntry, l) {
	entry = CAR(cdrEntry);
	applyResult = (*applyFunction)(entry, applyParameters);
	collection = (*collectFunction)(applyResult, collection);
    }
    
    return collection;
}
 
/* --------------------------------
 *	ExecUniqueCons
 *
 *	This function returns a list of unique integers, given
 *	two lists as input.  Since our lists are short and this
 *	is only done once each query, we don't bother worring
 *	about efficiency.. (this should probably be done in the
 *	planner anyways..)
 *
 *      Note: list2 had better be free of duplicates already.
 *            list1 can have duplicates - they get tossed.
 *
 *	Final list consists elts of list1 on the head of list2,
 *	unless one of the lists is nil, in which case the other
 *	is returned..
 * --------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
List
ExecUniqueCons(list1, list2)
    List list1;
    List list2;
{
    List 	current1;
    List 	current2;
    LispValue 	current1Int;
    LispValue 	current2Int;
    bool	foundDuplicate;
    
    /* ----------------
     *	make sure both lists contain something
     * ----------------
     */
    if (lispNullp(list1))
	return list2;
    
    if (lispNullp(list2))
	return list1;
    
    /* ----------------
     *	for each entry in the first list 
     *     see if it's already in the second list
     *     if so, then move onto the next entry in the first list
     *     if not, then add the entry to the front of the second list.
     *
     *  note: this trashes list1.. non-duplicates are moved to list2
     *        and duplicates are left stranded in memory..
     * ----------------
     */
    
    current1 = list1;
    while (! lispNullp(current1)) {
	current1Int = CAR(current1);
	current2 = list2;
	foundDuplicate = false;
	
	/* ----------------
	 *  see if current item from list 1 is already in list2
	 * ----------------
	 */
	while (! lispNullp(current2)) {
	    current2Int = CAR(current2);
	    if (CInteger(current2Int) == CInteger(current1Int)) {
		foundDuplicate = true;
		break;
	    } 
	    current2 = CDR(current2);
	}
	
	/* ----------------
	 *  foundDuplicate is now true if current item is in list2
	 *  and is false if we traversed all of list2 and no item there
	 *  matched it..
	 * ----------------
	 */
	if (! foundDuplicate) {
	    List temp;
	    /* ----------------
	     *  since current1 is not in list2, move current1 onto the head
	     *  of list2 and then move to the next element in list1.
	     * ----------------
	     */
	    temp =          CDR(current1);
	    CDR(current1) = list2;
	    list2 =         current1;
	    current1 =      temp;
	} else {
	    /* ----------------
	     *  current1 is already in list2 so we just
	     *	advance current1 to the next element in list1
	     * ----------------
	     */
	    current1 = CDR(current1);
	}
    }
    
    /* ----------------
     *	at this point, all elts of list1 not originally in list2
     *  are on the head of list2 so we can just return list2.
     * ----------------
     */
    return list2;
}
 
/* --------------------------------
 *	ExecGetVarAttlistFromExpr
 *
 *	This is a recursive function which returns a list
 *	containing the attribute numbers of all var nodes
 * --------------------------------
 */
/**** xxref:
 *           ExecGetVarAttlistFromExpr
 *           ExecGetVarAttlistFromTLE
 ****/
List
ExecGetVarAttlistFromExpr(expr, relationNum)
    Node expr;
    List relationNum;
{
    /* ----------------
     *	const nodes have no information for us, so we return nil.
     * ----------------
     */
    if (ExactNodeType(expr,Const))
	return LispNil;
    
    /* ----------------
     *	var nodes are what we want.. so we check if
     *  the var node applies to the relation we want
     *  and if it does, we return the var's attribute number
     *  in a list.  If the var is not associated with
     *  the relation we are interested in then we just return nil.
     * ----------------
     */
    if (ExactNodeType(expr,Var)) {
	Index varno;
	AttributeNumber relno;
	AttributeNumber attno;
    
	relno = CInteger(CAR(relationNum));
	varno = get_varno((Var)expr);
	attno = get_varattno((Var) expr);
	
	if (varno == relno)
	    return lispCons(lispInteger(attno), LispNil);
	else
	    return LispNil;
    }
    /* ----------------
     *	operator expressions may have var nodes in their
     *  opargs so we recursively search for var nodes there..
     * ----------------
     */
    if (is_clause(expr)) {
	List opargs;
	opargs = (List) get_opargs(expr);
	return ExecCollect(opargs, 		      /* list */
			   ExecGetVarAttlistFromExpr, /* apply function */
			   ExecUniqueCons,	      /* collect function */
			   relationNum); 	      /* apply parameters */
    } 
    /* ----------------
     *	function expressions are just like operator expressions..
     *  we scan the function expression's arg list to find the var nodes
     *  from which we get attribute numbers
     * ----------------
     */
    if (is_funcclause(expr)) {
	List funcargs;
	funcargs = (List) get_funcargs(expr);
	return ExecCollect(funcargs, 		      /* list */
			   ExecGetVarAttlistFromExpr, /* apply function */
			   ExecUniqueCons,	      /* collect function */
			   relationNum); 	      /* apply parameters */
    }
    /* ----------------
     *	for or clauses, we scan each of the or expressions
     *  because they may contain var nodes also..
     * ----------------
     */
    if (or_clause(expr)) {
	List clauseargs;
	clauseargs = (List) get_orclauseargs(expr);
	return ExecCollect(clauseargs, 		      /* list */
			   ExecGetVarAttlistFromExpr, /* apply function */
			   ExecUniqueCons,	      /* collect function */
			   relationNum); 	      /* apply parameters */
    }
    /* ----------------
     *	finally, not clauses are easy, we just return the
     *  attributes from the not clause's argument..
     * ----------------
     */
    if (not_clause(expr)) {
	List notclausearg;
	notclausearg = (List) get_notclausearg(expr);
	return
	    ExecGetVarAttlistFromExpr((Node) notclausearg, relationNum);
    }
    
    /* ----------------
     *	if expr is not one of the above, our structure is likely
     *  corrupted but then we know there ain't no var nodes so
     *  we return nil and keep-a-processing...
     * ----------------
     */
    elog(DEBUG, "ExecGetVarAttlistFromExpr: invalid node type in expr!");
    return LispNil;
}
 
/* --------------------------------
 *	ExecGetVarAttlistFromTLE
 *
 *	This is simple.. Just extract the expression from
 *	the target list entry and have ExecGetAttlistFromExpr
 *	do our work for us..
 * --------------------------------
 */
/**** xxref:
 *           <apparently-unused>
 ****/
List
ExecGetVarAttlistFromTLE(tle, relationNum)
    List tle;
    List relationNum;    
{
    Node expr;
    
    expr =  (Node) tl_expr(tle);
    return
	ExecGetVarAttlistFromExpr(expr, relationNum);
}
 
/* --------------------------------
 *	ExecMakeAttsFromList
 *
 *	Returns an array of AttributeNumber's given a list,
 *      and returns the size of the array in it's second parameter.
 * --------------------------------
 */
/**** xxref:
 *           ExecInitScanAttributes
 ****/
AttributeNumberPtr
ExecMakeAttsFromList(attlist, numAttsPtr)
    List attlist;
    int	 *numAttsPtr;
{
    List		cdrEntry;
    List		entry;
    int 		len;
    AttributeNumberPtr  attnums;
    int			i;
    
    len = length(attlist);
    CXT1_printf("ExecMakeAttsFromList: context is %d\n", CurrentMemoryContext);
    attnums = (AttributeNumberPtr)
	palloc(len * sizeof(AttributeNumber));
    i = 0;
    foreach (cdrEntry, attlist) {
	entry = CAR(cdrEntry);
	attnums[i++] = CInteger(entry);
    }
    if (numAttsPtr != NULL)
	(*numAttsPtr) = i;
    
    return attnums;
}
 
/* --------------------------------
 *	ExecInitScanAttributes
 *
 *	This inspects the node's target list and initializes
 *	an array of attribute numbers stored in the node.
 *
 *	This array describe which attributes of the scanned
 *	tuple are ever examined or returned.  The rule manager
 *	uses this information to know which attributes are going
 *	to be inspected.
 *
 *	This is nontrivial because we have to walk the
 *	entire target list structure and find all the var
 *	nodes with varno's equal to the index (in the range table)
 *	of the relation being scanned by the node..
 *
 *	XXX:  If a user defined function/operator references
 *	      attributes in some wierd indirect way, then the
 *	      rule manager might not know to do the right thing
 *	      because we don't detect this.
 *
 *	      In these circumstance, we have to have the rule manager
 *	      message every attribute of the tuple and we need to
 *	      know somehow that the function can manipulate the tuple
 *	      in some unexpected manner..
 *
 *	Note: the memory used by the cons cells in our
 *	temporary lists is not reclaimed until the end of transaction.
 *	(A better setup would free the memory as we go.)
 *
 *	Here is one instance where it would be much easier
 *	to write this in lisp..
 * --------------------------------
 */
/**** xxref:
 *           ExecInitIndexScan
 *           ExecInitSeqScan
 ****/
void
ExecInitScanAttributes(node)
    Plan node;
{
    ScanState		scanstate;
    List		targetList;
    AttributeNumberPtr 	scanAtts;
    int			numScanAtts;
    List		attlist;
    
    /* ----------------
     *	make sure we have a scan node
     * ----------------
     */
    if (!ExecIsSeqScan(node) && !ExecIsIndexScan(node)) {
	elog(DEBUG, "ExecInitScanAttributes: called on non scan node");
	return;
    }
    
    /* ----------------
     *	XXX currently we assign attlist = LispNil which forces
     *      ExecScan() to generate the attribute information
     *      at runtime when the first tuple is retrieved.
     *
     *	    This is a poor long-term solution because that
     *	    means the rule-manager ends up inspecting every
     *	    attribute of every tuple scanned..  But this will
     *	    work for now.. -cim 10/12/89
     * ----------------
     */
    attlist = LispNil;

    /* ----------------
     *  get information from the node.. 
     *	Note: both indexscans and seqscans have scanstates since
     *  indexscans inherit from seqscans.
     * ----------------
     */
    scanstate =  get_scanstate((Scan) node);
    
#ifdef PERHAPSNEVER
    /* ----------------
     *	need the range table index of the scanned relation
     *  into relno so we know which var nodes to inspect.
     *  ExecCollect expects a list so we put it in one..
     * ----------------
     */
    targetList =  get_qptargetlist(node);
    
    relno = 	  get_scanrelid(node);
    relationNum = lispCons(lispInteger(relno), LispNil);
    
    /* ----------------
     *	construct a list of all the attributes using ExecCollect
     * ----------------
     */
    attlist = ExecCollect(targetList, 		    /* list */
			  ExecGetVarAttlistFromTLE, /* apply function */
			  ExecUniqueCons,	    /* collect function */
			  relationNum); 	    /* apply parameters */
    
    if (ExecIsIndexScan(node)) {
	/* ----------------
	 *  we might also inspect the indxqual clauses in the
	 *  index node for additional attribute number information
	 *  but I don't understand how to do this now.. -cim 9/21/89
	 * ----------------
	 */
    }

    /* XXX should also inspect qpqual */
#endif PERHAPSNEVER
    
    /* ----------------
     *	make attribute array from attlist.
     * ----------------
     */
    if (lispNullp(attlist)) {
	scanAtts = NULL;
	numScanAtts = 0;
    } else
	scanAtts = ExecMakeAttsFromList(attlist, &numScanAtts);
    
    /* ----------------
     *	set the node's attribute information
     * ----------------
     */
    set_cs_ScanAttributes((CommonState) scanstate, scanAtts);
    set_cs_NumScanAttributes((CommonState) scanstate, numScanAtts);
}

/* --------------------------------
 *	ExecMakeBogusScanAttributes
 *
 *	This makes an array of size natts containing the
 *	attribute numbers from 1 to natts.
 * --------------------------------
 */
AttributeNumberPtr
ExecMakeBogusScanAttributes(natts)
    int natts;
{
    extern MemoryContext PortalExecutorHeapMemory;
    AttributeNumberPtr   scanAtts;
    int			 scanAttSize;
    int			 i;

    /* ----------------
     *	calculate size of our array
     * ----------------
     */
    scanAttSize = natts * sizeof(AttributeNumber);
	    
    /* ----------------
     *  if we are running in a portal, we have to make
     *  sure our allocations occur in the proper context
     *  lest they go away unexpectedly.  This is a hack
     *  to preserve this memory for the lifetime of the
     *  portal (I think).  -cim 3/15/90
     * ----------------
     */
    if (PortalExecutorHeapMemory != NULL)
	scanAtts = (AttributeNumberPtr)
	    MemoryContextAlloc(PortalExecutorHeapMemory, scanAttSize);
    else
	scanAtts = (AttributeNumberPtr)
	    palloc(scanAttSize);
    
    /* ----------------
     *	fill the scan attributes array
     * ----------------
     */
    for (i=1; i<=natts; i++) {
	scanAtts[i-1] = i;
    }	

    return scanAtts;
}
    

/* --------------------------------
 *	ExecFreeScanAttributes
 *
 *	This frees the allocated array of attribute numbers.
 *
 *	Note: it is not an error to pass NULL.  That just means
 *	that no attribute information was allocated.
 * --------------------------------
 */
 
/**** xxref:
 *           ExecEndIndexScan
 *           ExecEndSeqScan
 ****/
void
ExecFreeScanAttributes(ptr)
    AttributeNumberPtr ptr;
{
    extern MemoryContext PortalExecutorHeapMemory;

    /* ----------------
     *	don't free an invalid pointer
     * ----------------
     */
    if (ptr == NULL)
	return;

    /* ----------------
     *	make certain we free the attribute list in the proper
     *  memory context.  Again, this is a crappy hack which comes
     *  from retrofitting portals into the executor.
     * ----------------
     */
    if (PortalExecutorHeapMemory != NULL)
	MemoryContextFree(PortalExecutorHeapMemory, (Pointer) ptr);
    else
	pfree(ptr);
}
 
/* function that returns number of atts in a var */
int
ExecGetVarLen(node, commonstate, var)
Plan node;
CommonState commonstate;
Var var;
{
    int varno, varattno;
    TupleTableSlot    slot;
    ExecTupDescriptor   execTupDesc;
    int len;
    Plan varplan;

    if (node == NULL) return 0;
    varno = get_varno(var);
    varattno = get_varattno(var);
    if (varno == INNER)
	varplan = get_innerPlan(node);
    else
	varplan = get_outerPlan(node);
    if (varattno == 0) {
	if (varplan == NULL) {
	    /* this must be a scan */
	    Relation curRel;
	    curRel = get_css_currentRelation((CommonScanState)commonstate);
	    len = curRel->rd_rel->relnatts;
	  }
	else
	    len = ExecTargetListLength(get_qptargetlist(varplan));
      }
    else {
	    slot = NodeGetResultTupleSlot(varplan);
	    execTupDesc = ExecSlotExecDescriptor(slot);
	    len = execTupDesc->data[varattno-1]->len;
      }
    return len;
}

/* function that returns the Tuple Descriptor for a var */
TupleDescriptor
ExecGetVarTupDesc(node, commonstate, var)
Plan node;
CommonState commonstate;
Var var;
{
    int varno, varattno;
    TupleTableSlot    slot;
    ExecTupDescriptor   execTupDesc;
    TupleDescriptor tupdesc;
    Plan varplan;

    if (node == NULL) return 0;
    varno = get_varno(var);
    varattno = get_varattno(var);
    if (varno == INNER)
	varplan = get_innerPlan(node);
    else
	varplan = get_outerPlan(node);
    if (varattno == 0) {
	if (varplan == NULL) {
	    /* this must be a scan */
	    Relation curRel;
	    curRel = get_css_currentRelation((CommonScanState)commonstate);
	    tupdesc = &curRel->rd_att;
	  }
	else
	    tupdesc = ExecGetTupType(varplan);
      }
    else {
	    execTupDesc = ExecGetExecTupDesc(varplan);
	    tupdesc = execTupDesc->data[varattno-1]->attdesc;
      }
    return tupdesc;
}
