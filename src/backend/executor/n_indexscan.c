/* ----------------------------------------------------------------
 *   FILE
 *	indexscan.c
 *	
 *   DESCRIPTION
 *	Routines to support indexes and indexed scans of relations
 *
 *   INTERFACE ROUTINES
 *	ExecInsertIndexTuples	inserts tuples into indices on result relation
 *
 *   	ExecIndexScan 		scans a relation using indices
 *   	ExecIndexNext 		using index to retrieve next tuple
 *   	ExecInitIndexScan	creates and initializes state info. 
 *   	ExecIndexReScan		rescans the indexed relation.
 *   	ExecEndIndexScan 	releases all storage.
 *   	ExecIndexMarkPos	marks scan position.
 *   	ExecIndexRestrPos	restores scan position.
 *
 *   NOTES
 *	the code supporting ExecInsertIndexTuples should be
 *	collected and merged with the genam stuff.
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

#include "access/ftup.h"
#include "parser/parsetree.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "catalog/catname.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/externs.h"

#include "parser/parse.h"	/* for RETRIEVE */
#include "tcop/slaves.h"

/* ----------------
 *	Misc stuff to move to executor.h soon -cim 6/5/90
 * ----------------
 */
#define NO_OP		0
#define LEFT_OP		1
#define RIGHT_OP	2

/* ----------------------------------------------------------------
 *		  ExecInsertIndexTuples support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *	ExecGetIndexKeyInfo
 *
 *	Extracts the index key attribute numbers from
 *	an index tuple form (i.e. a tuple from the pg_index relation)
 *	into an array of attribute numbers.  The array and the
 *	size of the array are returned to the caller via return
 *	parameters.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecOpenIndices
 ****/
void
ExecGetIndexKeyInfo(indexTuple, numAttsOutP, attsOutP)
    IndexTupleForm	indexTuple;
    int			*numAttsOutP;
    AttributeNumberPtr	*attsOutP;
{
    int			i;
    int 		numKeys;
    AttributeNumberPtr 	attKeys;
    
    /* ----------------
     *	check parameters
     * ----------------
     */
    if (numAttsOutP == NULL && attsOutP == NULL) {
	elog(DEBUG, "ExecGetIndexKeyInfo: %s",
	     "invalid parameters: numAttsOutP and attsOutP must be non-NULL");
    }
    
    /* ----------------
     *	first count the number of keys..
     * ----------------
     */
    numKeys = 0;
    for (i=0; i<8 && indexTuple->indkey[i] != 0; i++)
	numKeys++;
    
    /* ----------------
     *	place number keys in callers return area
     * ----------------
     */
    (*numAttsOutP) = numKeys;
    if (numKeys < 1) {
	elog(DEBUG, "ExecGetIndexKeyInfo: %s",
	     "all index key attribute numbers are zero!");
	(*attsOutP) = NULL;
	return;
    }
    
    /* ----------------
     *	allocate and fill in array of key attribute numbers
     * ----------------
     */
    CXT1_printf("ExecGetIndexKeyInfo: context is %d\n", CurrentMemoryContext);
    
    attKeys = (AttributeNumberPtr)
	palloc(numKeys * sizeof(AttributeNumber));
    
    for (i=0; i<numKeys; i++)
	attKeys[i] = indexTuple->indkey[i];
    
    /* ----------------
     *	return array to caller.
     * ----------------
     */
    (*attsOutP) = attKeys;
}
 
/* ----------------------------------------------------------------
 *	ExecOpenIndices
 *
 *	Here we scan the pg_index relation to find indices
 *	associated with a given heap relation oid.  Since we
 *	don't know in advance how many indices we have, we
 *	form lists containing the information we need from
 *	pg_index and then process these lists.
 *
 *	Note: much of this code duplicates effort done by
 *	the IndexCatalogInformation function in plancat.c
 *	because IndexCatalogInformation is poorly written.
 *
 *	It would be much better the functionality provided
 *	by this function and IndexCatalogInformation was
 *	in the form of a small set of orthogonal routines..
 *	If you are trying to understand this, I suggest you
 *	look at the code to IndexCatalogInformation and
 *	FormIndexTuple.. -cim 9/27/89
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           InitPlan
 ****/
void
ExecOpenIndices(resultRelationOid, resultRelationInfo)
    ObjectId		resultRelationOid;
    RelationInfo	resultRelationInfo;
{
    Relation		indexRd;
    HeapScanDesc	indexSd;
    ScanKeyData		key;
    HeapTuple		tuple;
    IndexTupleForm	indexStruct;
    ObjectId  	   	indexOid;
    List		oidList;
    List		nkeyList;
    List		keyList;
    List		indexoid;
    List		numkeys;
    List		indexkeys;
    int			len;
    
    RelationPtr		relationDescs;
    IndexInfoPtr	indexInfoArray;
    int		   	numKeyAtts;
    AttributeNumberPtr 	indexKeyAtts;
    int			i;
    
    /* ----------------
     *	open pg_index
     * ----------------
     */
    indexRd = amopenr(IndexRelationName);
    
    /* ----------------
     *	form a scan key
     * ----------------
     */
	ScanKeyEntryInitialize(&key.data[0], 0, IndexHeapRelationIdAttributeNumber,
						   ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(resultRelationOid));
    
    /* ----------------
     *	scan the index relation, looking for indices for our
     *  result relation..
     * ----------------
     */
    indexSd = ambeginscan(indexRd, 		/* scan desc */
			  false, 		/* scan backward flag */
			  NowTimeQual, 		/* time qual */
			  1,			/* number scan keys */
			  &key); 		/* scan keys */
    
    oidList =  LispNil;
    nkeyList = LispNil;
    keyList =  LispNil;
    
    while(tuple = amgetnext(indexSd, 		/* scan desc */
			    false,		/* scan backward flag */
			    NULL),    		/* return: buffer */
	  HeapTupleIsValid(tuple)) {
	
	/* ----------------
	 *  For each index relation we find, extract the information
	 *  we need and store it in a list..
	 * 
	 *  first get the oid of the index relation from the tuple
	 * ----------------
	 */
	indexStruct = (IndexTupleForm) GETSTRUCT(tuple);
	indexOid = indexStruct->indexrelid;
	
	/* ----------------
	 *  next get the index key information from the tuple
	 * ----------------
	 */
	ExecGetIndexKeyInfo(indexStruct,
			    &numKeyAtts,
			    &indexKeyAtts);
	
	/* ----------------
	 *  save the index information into lists
	 * ----------------
	 */
	oidList =  lispCons(lispInteger(indexOid), oidList);
	nkeyList = lispCons(lispInteger(numKeyAtts), nkeyList);
	keyList =  lispCons(indexKeyAtts, keyList);
    }
    
    /* ----------------
     *	we have the info we need so close the pg_index relation..
     * ----------------
     */
    amendscan(indexSd);
    amclose(indexRd);
    
    /* ----------------
     *	Now that we've collected the index information into three
     *  lists, we open the index relations and store the descriptors
     *  and the key information into arrays.
     * ----------------
     */
    len = length(oidList);
    if (len > 0) {
	/* ----------------
	 *   allocate space for relation descs
	 * ----------------
	 */
	CXT1_printf("ExecOpenIndices: context is %d\n", CurrentMemoryContext);
	relationDescs = (RelationPtr)
	    palloc(len * sizeof(Relation));
	
	/* ----------------
	 *   initialize index info array
	 * ----------------
	 */
	CXT1_printf("ExecOpenIndices: context is %d\n", CurrentMemoryContext);
	indexInfoArray = (IndexInfoPtr)
	    palloc(len * sizeof(IndexInfo));
	
	for (i=0; i<len; i++)
	    indexInfoArray[i] = MakeIndexInfo();
	
	/* ----------------
	 *   attempt to open each of the indices.  If we succeed,
	 *   then store the index relation descriptor into the
	 *   relation descriptor array.
	 * ----------------
	 */
	i = 0;
	foreach (indexoid, oidList) {
	    Relation  indexDesc;
	    
	    indexOid =  CInteger(CAR(indexoid));
	    indexDesc = AMopen(indexOid);
	    if (indexDesc != NULL)
		relationDescs[i++] = indexDesc;
	}
	
	/* ----------------
	 *   store the relation descriptor array and number of
	 *   descs into the result relation info.
	 * ----------------
	 */
	set_ri_NumIndices(resultRelationInfo, i);
	set_ri_IndexRelationDescs(resultRelationInfo, relationDescs);
	
	/* ----------------
	 *   store the index key information collected in our
	 *   lists into the index info array
	 * ----------------
	 */
	i = 0;
	foreach (numkeys, nkeyList) {
	    numKeyAtts = CInteger(CAR(numkeys));
	    set_ii_NumKeyAttributes(indexInfoArray[i++], numKeyAtts);
	}
	
	i = 0;
	foreach (indexkeys, keyList) {
	    indexKeyAtts = (AttributeNumberPtr) CAR(indexkeys);
	    set_ii_KeyAttributeNumbers(indexInfoArray[i++], indexKeyAtts);
	}
	
	/* ----------------
	 *   store the index info array into relation info
	 * ----------------
	 */
	set_ri_IndexRelationInfo(resultRelationInfo, indexInfoArray);
    }
    
    /* ----------------
     *	All done,  resultRelationInfo now contains complete information
     *  on the indices associated with the result relation.
     * ----------------
     */
    
    /* should free oidList, nkeyList and keyList here */
}
 
/* ----------------------------------------------------------------
 *	ExecCloseIndices
 *
 *	Close the index relations stored in resultRelationInfo
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           EndPlan
 ****/
void
ExecCloseIndices(resultRelationInfo)
    RelationInfo	resultRelationInfo;
{
    int 	i;
    int 	numIndices;
    RelationPtr	relationDescs;
    
    numIndices = get_ri_NumIndices(resultRelationInfo);
    relationDescs = get_ri_IndexRelationDescs(resultRelationInfo);
    
    for (i=0; i<numIndices; i++)
	if (relationDescs[i] != NULL)
	    AMclose(relationDescs[i]);
    /*
     * XXX should free indexInfo array here too.
     */
}
 
/* ----------------------------------------------------------------
 *	ExecFormIndexTuple
 *
 *	Most of this code is cannabilized from DefaultBuild().
 *	As said in the comments for ExecOpenIndices, most of
 *	this functionality should be rearranged into a proper
 *	set of routines..
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInsertIndexTuples
 ****/
IndexTuple
ExecFormIndexTuple(heapTuple, heapRelation, indexRelation, indexInfo)
    HeapTuple	heapTuple;
    Relation	heapRelation;
    Relation	indexRelation;
    IndexInfo	indexInfo;
{
    IndexTuple		indexTuple;
    TupleDescriptor	heapDescriptor;
    TupleDescriptor	indexDescriptor;
    Datum		*datum;
    char		*nulls;
    
    int			numberOfAttributes;
    AttributeNumberPtr  keyAttributeNumbers;
    
    /* ----------------
     *	get information from index info structure
     * ----------------
     */
    numberOfAttributes =  get_ii_NumKeyAttributes(indexInfo);
    keyAttributeNumbers = get_ii_KeyAttributeNumbers(indexInfo);
    
    /* ----------------
     *	datum and null are arrays in which we collect the index attributes
     *  when forming a new index tuple.
     * ----------------
     */
    CXT1_printf("ExecFormIndexTuple: context is %d\n", CurrentMemoryContext);
    datum = (Datum *)	palloc(numberOfAttributes * sizeof *datum);
    nulls =  (char *)	palloc(numberOfAttributes * sizeof *nulls);
    
    /* ----------------
     *	get the tuple descriptors from the relations so we know
     *  how to form the index tuples..
     * ----------------
     */
    heapDescriptor =  RelationGetTupleDescriptor(heapRelation);
    indexDescriptor = RelationGetTupleDescriptor(indexRelation);
    
    /* ----------------
     *  FormIndexDatum fills in its datum and null parameters
     *  with attribute information taken from the given heap tuple.
     * ----------------
     */
    FormIndexDatum(numberOfAttributes,  /* num attributes */
		   keyAttributeNumbers,	/* array of att nums to extract */
		   heapTuple,	        /* tuple from base relation */
		   heapDescriptor,	/* heap tuple's descriptor */
		   InvalidBuffer,	/* buffer associated with heap tuple */
		   datum,		/* return: array of attributes */
		   nulls);		/* return: array of char's */
    
    indexTuple = FormIndexTuple(numberOfAttributes,
				indexDescriptor,
				datum,
				nulls);
    
    /* ----------------
     *	free temporary arrays
     *
     *  XXX should store these in the IndexInfo instead of allocating
     *     and freeing on every insertion, but efficency here is not
     *     that important and FormIndexTuple is wasteful anyways..
     *     -cim 9/27/89
     * ----------------
     */
    pfree(nulls);
    pfree(datum);
    
    return indexTuple;
}
 
/* ----------------------------------------------------------------
 *	ExecInsertIndexTuples
 *
 *	This routine takes care of inserting index tuples
 *	into all the relations indexing the result relation
 *	when a heap tuple is inserted into the result relation.
 *	Much of this code should be moved into the genam
 *	stuff as it only exists here because the genam stuff
 *	doesn't provide the functionality needed by the
 *	executor.. -cim 9/27/89
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecAppend
 *           ExecReplace
 ****/
RuleLock
ExecInsertIndexTuples(heapTuple, tupleid, estate)
    HeapTuple		heapTuple;
    ItemPointer 	tupleid;
    EState		estate;
{
    RelationInfo	        resultRelationInfo;
    int 			i;
    int 			numIndices;
    RelationPtr		    	relationDescs;
    Relation			heapRelation;
    IndexInfoPtr		indexInfoArray;
    IndexTuple		     	indexTuple;
    GeneralInsertIndexResult 	result;
    
    /* ----------------
     *	get information from the result relation info structure.
     * ----------------
     */
    resultRelationInfo = get_es_result_relation_info(estate);
    numIndices =         get_ri_NumIndices(resultRelationInfo);
    relationDescs =      get_ri_IndexRelationDescs(resultRelationInfo);
    indexInfoArray =     get_ri_IndexRelationInfo(resultRelationInfo);
    heapRelation =       get_ri_RelationDesc(resultRelationInfo);
    
    /* ----------------
     *	for each index, form and insert the index tuple
     * ----------------
     */
    for (i=0; i<numIndices; i++) {
	if (relationDescs[i] != NULL) {
	    
	    indexTuple = ExecFormIndexTuple(heapTuple,
					    heapRelation,
					    relationDescs[i],
					    indexInfoArray[i]);
	    
	    indexTuple->t_tid = (*tupleid);     /* structure assignment */
	    
	    result = AMinsert(relationDescs[i], /* index relation */
			      indexTuple, 	/* index tuple */
			      0,		/* scan (not used) */
			      0);		/* return: offset */
	    
	    /* XXX should inspect result for locks */
	    
	    /* ----------------
	     *	keep track of index inserts for debugging
	     * ----------------
	     */
	    IncrIndexInserted();
	    
	    /* ----------------
	     *	free index tuple after insertion
	     * ----------------
	     */
	    pfree(indexTuple);
	}
    }
    
    /* ----------------
     *	return rule locks from insertion.. (someday)
     * ----------------
     */
    return NULL;
}
 
/* ----------------------------------------------------------------
 *			index scan support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *   	IndexNext
 *
 *	Retrieve a tuple from the IndexScan node's currentRelation
 *	using the indices in the IndexScanState information.
 *
 *	note: the old code mentions 'Primary indices'.  to my knowledge
 *	we only support a single secondary index. -cim 9/11/89
 *
 * old comments:
 *   	retrieve a tuple from relation using the indices given.
 *   	The indices are used in the order they appear in 'indices'.
 *   	The indices may be primary or secondary indices:
 *   	  * primary index -- 	scan the relation 'relID' using keys supplied.
 *   	  * secondary index -- 	scan the index relation to get the 'tid' for
 *   			       	a tuple in the relation 'relID'.
 *   	If the current index(pointed by 'indexPtr') fails to return a
 *   	tuple, the next index in the indices is used.
 *   
 *        bug fix so that it should retrieve on a null scan key.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           UseNextIndex
 ****/
TupleTableSlot
IndexNext(node)
    IndexScan node;
{
    EState	   		estate;
    ScanState	   		scanstate;
    IndexScanState 		indexstate;
    ScanDirection  		direction;
    int		   		indexPtr;
    IndexScanDescPtr 		scanDescs;
    IndexScanDesc  		scandesc;
    Relation	   		heapRelation;
    GeneralRetrieveIndexResult  result;
    ItemPointer	   		iptr;
    HeapTuple	   		tuple;
    TupleTableSlot		returnTuple;
    TupleTableSlot		slot;
    Buffer			buffer;
    
    /* ----------------
     *	extract necessary information from index scan node
     * ----------------
     */
    estate =     	(EState) get_state(node);
    direction =  	get_es_direction(estate);
    
    scanstate =  	get_scanstate(node);
    indexstate = 	get_indxstate(node);
    indexPtr =   	get_iss_IndexPtr(indexstate);
    scanDescs = 	get_iss_ScanDescs(indexstate);
    scandesc =  	scanDescs[ indexPtr ];
    heapRelation =	get_css_currentRelation(scanstate);
    
    slot = (TupleTableSlot)
	get_css_ScanTupleSlot(scanstate);
    
    /* ----------------
     *	ok, now that we have what we need, fetch an index tuple.
     * ----------------
     */
    
    for(;;) {
	result = AMgettuple(scandesc, direction);
	/* ----------------
	 *  if scanning this index succeeded then return the
	 *  appropriate heap tuple.. else return LispNil.
	 * ----------------
	 */
	if (GeneralRetrieveIndexResultIsValid(result)) {
	    iptr =  GeneralRetrieveIndexResultGetHeapItemPointer(result);
	    tuple = heap_fetch(heapRelation,
			       NowTimeQual,
			       iptr,
			       &buffer);
	    /* be tidy */
	    pfree(result);

	    if (tuple == NULL) {
		/* ----------------
		 *   we found a deleted tuple, so keep on scanning..
		 * ----------------
		 */
		continue;
	    }

	    /* ----------------
	     *	store the scanned tuple in the scan tuple slot of
	     *  the scan state.  Eventually we will only do this and not
	     *  return a tuple.  Note: we pass 'false' because tuples
	     *  returned by amgetnext are pointers onto disk pages and
	     *  were not created with palloc() and so should not be pfree()'d.
	     * ----------------
	     */
	    ExecStoreTuple(tuple,	  /* tuple to store */
			   slot, 	  /* slot to store in */
			   buffer, 	  /* buffer associated with tuple  */
			   false);   	  /* don't pfree */
	    
	    return slot;
	}
	
	/* ----------------
	 *  if we get here it means the index scan failed so we
	 *  are at the end of the scan..
	 * ----------------
	 */
	return (TupleTableSlot)
	    ExecClearTuple(slot);
    }
}

/* ----------------------------------------------------------------
 *	ExecIndexScan(node)
 *
 * old comments:
 *   	Scans the relation using primary or secondary indices and returns 
 *         the next qualifying tuple in the direction specified.
 *   	It calls ExecScan() and passes it the access methods which returns
 *   	the next tuple using the indices.
 *   	
 *   	Conditions:
 *   	  -- the "cursor" maintained by the AMI is positioned at the tuple
 *   	     returned previously.
 *   
 *   	Initial States:
 *   	  -- the relation indicated is opened for scanning so that the 
 *   	     "cursor" is positioned before the first qualifying tuple.
 *   	  -- all index realtions are opened for scanning.
 *   	  -- indexPtr points to the first index.
 *   	  -- state variable ruleFlag = nil.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot
ExecIndexScan(node)
    IndexScan node;
{
    TupleTableSlot returnTuple;
    
    /* ----------------
     *	use IndexNext as access method
     * ----------------
     */
    returnTuple = ExecScan((Plan) node, IndexNext);
    return
	returnTuple;
}	
 
/* ----------------------------------------------------------------
 *    	ExecIndexReScan(node)
 *
 * old comments
 *    	Rescans the indexed relation.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecReScan
 ****/
List
ExecIndexReScan(node)
    IndexScan node;
{
    EState	    estate;
    IndexScanState  indexstate;
    ScanDirection   direction;
    
    IndexScanDescPtr scanDescs;
    ScanKeyPtr	    scanKeys;
    IndexScanDesc   sdesc;
    ScanKey	    skey;
    int		    numIndices;
    int 	    i;
    
    /* ----------------
     *	get information from the node..
     * ----------------
     */
    estate =     (EState) get_state(node);
    direction =  get_es_direction(estate);
    
    indexstate = get_indxstate(node);
    numIndices = get_iss_NumIndices(indexstate);
    scanDescs =  get_iss_ScanDescs(indexstate);
    scanKeys =   get_iss_ScanKeys(indexstate);
    
    /* ----------------
     * rescans all indices
     *
     * note: AMrescan assumes only one scan key.  This may have
     *	     to change if we ever decide to support multiple keys.
     *	     (actually I doubt the thing is even used -cim 9/11/89)
     *
     *	  this is actually necessary because the index scan keys are
     *    copied into the scan desc in AMrescan() and when we call
     *    ExecUpdateIndexScanKeys() we create a new set of keys which
     *    have to be copied into the scan desc.
     * ----------------
     */
    for (i = 0; i < numIndices; i++) {
	sdesc = scanDescs[ i ];
	skey =  scanKeys[ i ];
	AMrescan(sdesc, direction, skey);
    }

    /* ----------------
     *	perhaps return something meaningful
     * ----------------
     */
    return LispNil;
}
 
/* ----------------------------------------------------------------
 *    	ExecEndIndexScan
 *
 * old comments
 *    	Releases any storage allocated through C routines.
 *    	Returns nothing.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndIndexScan(node)
    IndexScan node;
{
    ScanState	    	scanstate;
    IndexScanState  	indexstate;
    ScanKeyPtr	    	scanKeys;
    ScanKey	    	skey;
    int		    	numIndices;
    int 	        i;
    AttributeNumberPtr	scanAtts;
    RelationRuleInfo	ruleInfo;

    scanstate =  get_scanstate(node);
    indexstate = get_indxstate(node);
    
    /* ----------------
     *	extract information from the node
     * ----------------
     */
    numIndices = get_iss_NumIndices(indexstate);
    scanKeys =   get_iss_ScanKeys(indexstate);
    scanAtts =	 get_cs_ScanAttributes(scanstate);
    
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
     *	close the heap and index relations
     * ----------------
     */
    ExecCloseR((Plan) node);
    
    /* ----------------
     *	free the scan keys used in scanning the indices
     * ----------------
     */
    for (i=0; i<numIndices; i++) { 
	skey = scanKeys[ i ];
	ExecFreeSkeys(skey);
    }

    /* ----------------
     *	clear out tuple table slots
     * ----------------
     */
    ExecClearTuple(get_cs_ResultTupleSlot(scanstate));    
    ExecClearTuple(get_css_ScanTupleSlot(scanstate));    
    ExecClearTuple(get_css_RawTupleSlot(scanstate));    
}
 
/* ----------------------------------------------------------------
 *    	ExecIndexMarkPos
 *
 * old comments
 *    	Marks scan position by marking the current index.
 *    	Returns nothing.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecMarkPos
 ****/
List
ExecIndexMarkPos(node)
    IndexScan node;
{
    IndexScanState	indexstate;
    IndexScanDescPtr	indexScanDescs;
    IndexScanDesc	scanDesc;
    int			indexPtr;
    
    indexstate =     get_indxstate(node);
    indexPtr =       get_iss_IndexPtr(indexstate);
    indexScanDescs = get_iss_ScanDescs(indexstate);
    scanDesc = indexScanDescs[ indexPtr ];
    
    /* ----------------
     *	XXX access methods don't return marked positions so
     *	    we return LispNil.
     * ----------------
     */
    IndexScanMarkPosition( scanDesc );
    return LispNil;
    
 /* was:   
  * IndexScanMarkPosition( SGetIScanDesc( SGetIndexID( SGetIndexPtr(node),
  *						       node)));
  */
}
 
/* ----------------------------------------------------------------
 *    	ExecIndexRestrPos
 *
 * old comments
 *    	Restores scan position by restoring the current index.
 *    	Returns nothing.
 *    
 *    	XXX Assumes previously marked scan position belongs to current index
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecRestrPos
 ****/
void
ExecIndexRestrPos(node)
    IndexScan node;
{
    IndexScanState	indexstate;
    IndexScanDescPtr	indexScanDescs;
    IndexScanDesc	scanDesc;
    int			indexPtr;
    
    indexstate =     get_indxstate(node);
    indexPtr =       get_iss_IndexPtr(indexstate);
    indexScanDescs = get_iss_ScanDescs(indexstate);
    scanDesc = indexScanDescs[ indexPtr ];
    
    IndexScanRestorePosition( scanDesc );
    
 /* was:
  * IndexScanRestorePosition( SGetIScanDesc( SGetIndexID( SGetIndexPtr(node),
  *							 node)));
  */
}
 
/* ----------------------------------------------------------------
 *	ExecUpdateIndexScanKeys
 *
 *	This recalculates the value of the scan keys whose
 *	value depends on information known at runtime.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecIndexReScan
 *           ExecNestLoop
 ****/
void
ExecUpdateIndexScanKeys(node, econtext)
    IndexScan	node;
    ExprContext econtext;
{
    IndexScanState	indexstate;
    Pointer		*runtimeKeyInfo;
    
    int		    	indexPtr;
    ScanKeyPtr	    	scanKeys;
    IntPtr	    	numScanKeys;
    
    List	    	indxqual;
    List		qual;
    int 		n_keys;
    ScanKey		scan_keys;
    int			*run_keys;
    int			j;
    
    List		clause;
    Node		scanexpr;
    Const		scanconst;
    Datum		scanvalue;
    Boolean		isNull;
    
    /* ----------------
     *	get info from the node
     * ----------------
     */
    indexstate = 	get_indxstate(node);
    runtimeKeyInfo = 	(Pointer *) get_iss_RuntimeKeyInfo(indexstate);
    
    /* ----------------
     *	if runtimeKeyInfo is null, then it means all our
     *  scan keys are constant so we don't have to recompute
     *  anything.
     * ----------------
     */
    if (runtimeKeyInfo == NULL)
	return;
    
    /* ----------------
     *	otherwise get the index qualifications and
     *  recalculate the appropriate values.
     * ----------------
     */
    indexPtr =		get_iss_IndexPtr(indexstate);
    
    indxqual = 		get_indxqual(node);
    qual =		nth(indexPtr, indxqual);
    
    scanKeys =  	get_iss_ScanKeys(indexstate);
    numScanKeys =	get_iss_NumScanKeys(indexstate);
    
    n_keys = 		numScanKeys[ indexPtr ];
    run_keys = 		(int *)   runtimeKeyInfo[ indexPtr ];
    scan_keys = 	(ScanKey) scanKeys[ indexPtr ];
    
    for (j=0; j < n_keys; j++) {
	/* ----------------
	 *   if we have a run-time key, then extract the run-time
	 *   expression and evautate it with respect to the current
	 *   outer tuple.  We then stick the result into the scan
	 *   key.
	 * ----------------
	 */
	if (run_keys[j] != NO_OP) {
	    clause =    nth(j, qual);
	    
	    scanexpr =  (run_keys[j] == RIGHT_OP) ?
		(Node) get_rightop(clause) : (Node) get_leftop(clause) ;
	    
	    scanvalue = (Datum)
		ExecEvalExpr(scanexpr, econtext, &isNull);
	    
	    ExecSetSkeysValue(j,	  /* index into scan_keys array */
			      scan_keys,  /* array of scan keys */
			      scanvalue); /* const computed from outerTuple */
	}
    }
}
 
/* ----------------------------------------------------------------
 *    	ExecInitIndexScan
 *
 *	Initializes the index scan's state information, creates
 *	scan keys, and opens the base and index relations.
 *
 *	Note: index scans have 2 sets of state information because
 *	      we have to keep track of the base relation and the
 *	      index relations.
 *	      
 * old comments
 *    	Creates the run-time state information for the node and 
 *    	sets the relation id to contain relevant decriptors.
 *    
 *    	Parameters: 
 *    	  node: IndexNode node produced by the planner.
 *	  estate: the execution state initialized in InitPlan.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitIndexScan(node, estate, parent)
    IndexScan 	node;
    EState 	estate;
    Plan	parent;
{
    IndexScanState  	indexstate;
    ScanState		scanstate;
    List	    	indxqual;
    List	    	indxid;
    int			i;
    int		    	numIndices;
    int		    	indexPtr;
    ScanKeyPtr	    	scanKeys;
    IntPtr	    	numScanKeys;
    RelationPtr	    	relationDescs;
    IndexScanDescPtr	scanDescs;
    Pointer		*runtimeKeyInfo;
    bool		have_runtime_keys;
    List	   	rangeTable;
    List	    	rtentry;
    Index	    	relid;
    ObjectId	   	reloid;
    TimeQual		timeQual;
    
    Relation	    	currentRelation;
    HeapScanDesc    	currentScanDesc;
    ScanDirection   	direction;
    ParamListInfo       paraminfo;
    ExprContext	        econtext;
    int			baseid;
    void                partition_indexscan();
    
    /* ----------------
     *	assign execution state to node
     * ----------------
     */
    set_state(node, estate);
    
    /* --------------------------------
     *  Part 1)  initialize scan state
     *
     *	create new ScanState for node
     * --------------------------------
     */
    scanstate = MakeScanState();
    set_scanstate(node, scanstate);
    
    /* ----------------
     *	assign node's base_id .. we don't use AssignNodeBaseid() because
     *  the increment is done later on after we assign the index scan's
     *  scanstate.  see below. 
     * ----------------
     */
    baseid = get_es_BaseId(estate);
    set_base_id(scanstate, baseid);
    
    /* ----------------
     *  create expression context for node
     * ----------------
     */
    ExecAssignExprContext(estate, scanstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, scanstate);
    ExecInitScanTupleSlot(estate, scanstate);
    ExecInitRawTupleSlot(estate, scanstate);
  
    /* ----------------
     * 	initialize projection info.  result type comes from scan desc
     *  below..
     * ----------------
     */
    ExecAssignProjectionInfo(node, scanstate);

    /* ----------------
     *	initialize scanAttributes (used by rule manager)
     * ----------------
     */
    ExecInitScanAttributes(node);
    
    /* --------------------------------
     *  Part 2)  initialize index scan state
     *
     *	create new IndexScanState for node
     * --------------------------------
     */
    indexstate = MakeIndexScanState();
    set_indxstate(node, indexstate);
    
    /* ----------------
     *	assign base id to index scan state also
     * ----------------
     */
    set_base_id(indexstate, baseid);
    baseid++;
    set_es_BaseId(estate, baseid);
    
    /* ----------------
     *	assign debugging hooks
     * ----------------
     */
    ExecAssignDebugHooks(node, indexstate);
            
    /* ----------------
     *	get the index node information
     * ----------------
     */
    indxid = 	get_indxid(node);
    indxqual = 	get_indxqual(node);
    numIndices = length(indxid);
    indexPtr = 	 0;
    
    CXT1_printf("ExecInitIndexScan: context is %d\n", CurrentMemoryContext);
    
    numScanKeys = (IntPtr)	palloc(numIndices * sizeof(int));
    scanKeys = (ScanKeyPtr)	palloc(numIndices * sizeof(ScanKey));
    relationDescs = (RelationPtr) palloc(numIndices * sizeof(Relation));
    scanDescs =  (IndexScanDescPtr) palloc(numIndices * sizeof(IndexScanDesc));
    
    /* ----------------
     *	initialize runtime key info.
     * ----------------
     */
    have_runtime_keys = false;
    runtimeKeyInfo = (Pointer *)
	palloc(numIndices * sizeof(Pointer));
    
    /* ----------------
     *	build the index scan keys from the index qualification
     * ----------------
     */
    for (i=0; i < numIndices; i++) {
	int 		j;
	List		qual;
	int 		n_keys;
	ScanKey		scan_keys;
	int		*run_keys;
	
	qual =		nth(i, indxqual);
	n_keys = 	length(qual);
	scan_keys =	(ScanKey) ExecMakeSkeys(n_keys);
	
	CXT1_printf("ExecInitIndexScan: context is %d\n",
		    CurrentMemoryContext);
	
	if (n_keys > 0) {
	    run_keys = (int *)	palloc(n_keys * sizeof(int));
	}
	
	/* ----------------
	 *  for each opclause in the given qual,
	 *  convert each qual's opclause into a single scan key
	 * ----------------
	 */
	for (j=0; j < n_keys; j++) {
	    List	clause;		/* one part of index qual */
	    Oper	op;		/* operator used in scan.. */
	    Node	leftop;		/* expr on lhs of operator */
	    Node	rightop; 	/* expr on rhs ... */
	    
	    int		scanvar; 	/* which var identifies varattno */
	    int		varattno; 	/* att number used in scan */
	    ObjectId	opid;		/* operator id used in scan */
	    Datum	scanvalue; 	/* value used in scan (if const) */
	    
	    /* ----------------
	     *	extract clause information from the qualification
	     * ----------------
	     */
	    clause = 	nth(j, qual);
	    
	    op = (Oper)  get_op(clause);
	    if (! ExactNodeType(op,Oper))
		elog(WARN, "ExecInitIndexScan: op not an Oper!");
	    
	    opid = get_opid(op);

	    /* ----------------
	     *  Here we figure out the contents of the index qual.
	     *  The usual case is (op var const) or (op const var)
	     *  which means we form a scan key for the attribute
	     *  listed in the var node and use the value of the const.
	     *
	     *  If we don't have a const node, then it means that
	     *  one of the var nodes refers to the "scan" tuple and
	     *  is used to determine which attribute to scan, and the
	     *  other expression is used to calculate the value used in
	     *  scanning the index.
	     *
	     *  This means our index scan's scan key is a function of
	     *  information obtained during the execution of the plan
	     *  in which case we need to recalculate the index scan key
	     *  at run time.
	     *  
	     *  Hence, we set have_runtime_keys to true and then set
	     *  the appropriate flag in run_keys to LEFT_OP or RIGHT_OP.
	     *  The corresponding scan keys are recomputed at run time.
	     * ----------------
	     */

	    scanvar = NO_OP;
	    
	    /* ----------------
	     *	determine information in leftop
	     * ----------------
	     */
	    leftop = 	(Node) get_leftop(clause);
	    
	    if (ExactNodeType(leftop,Var) && var_is_rel(leftop)) {
		/* ----------------
		 *  if the leftop is a "rel-var", then it means
		 *  that it is a var node which tells us which
		 *  attribute to use for our scan key.
		 * ----------------
		 */
		varattno = 	get_varattno(leftop);
		scanvar = 	LEFT_OP;
	    } else if (ExactNodeType(leftop,Const)) {
		/* ----------------
		 *  if the leftop is a const node then it means
		 *  it identifies the value to place in our scan key.
		 * ----------------
		 */
		run_keys[ j ] = NO_OP;
		scanvalue = get_constvalue(leftop);
	    } else if (consp(leftop) && 
		       ExactNodeType(CAR((List)leftop),Func) &&
		       var_is_rel(CADR((List)leftop))) {
		/* ----------------
		 *  if the leftop is a func node then it means
		 *  it identifies the value to place in our scan key.
		 *  Since functional indices have only one attribute
		 *  the attno must always be set to 1.
		 * ----------------
		 */
		varattno = 	1;
		scanvar = 	LEFT_OP;

	    } else {
		/* ----------------
		 *  otherwise, the leftop contains information usable
		 *  at runtime to figure out the value to place in our
		 *  scan key.
		 * ----------------
		 */
		have_runtime_keys = true;
		run_keys[ j ] = LEFT_OP;
		scanvalue = get_constvalue(ConstTrue);
	    }

	    /* ----------------
	     *	now determine information in rightop
	     * ----------------
	     */
	    rightop = 	(Node) get_rightop(clause);
	    
	    if (ExactNodeType(rightop,Var) && var_is_rel(rightop)) {
		/* ----------------
		 *  here we make sure only one op identifies the
		 *  scan-attribute...
		 * ----------------
		 */
		if (scanvar == LEFT_OP)
		    elog(WARN, "ExecInitIndexScan: %s",
			 "both left and right op's are rel-vars");
		
		/* ----------------
		 *  if the rightop is a "rel-var", then it means
		 *  that it is a var node which tells us which
		 *  attribute to use for our scan key.
		 * ----------------
		 */
		varattno = 	get_varattno(rightop);
		scanvar = 	RIGHT_OP;
		
	    } else if (ExactNodeType(rightop,Const)) {
		/* ----------------
		 *  if the leftop is a const node then it means
		 *  it identifies the value to place in our scan key.
		 * ----------------
		 */
		run_keys[ j ] = NO_OP;
		scanvalue = get_constvalue(rightop);

	    } else if (consp(rightop) &&
		       ExactNodeType(CAR((List)rightop),Func) &&
		       var_is_rel(CADR((List)rightop))) {
		/* ----------------
		 *  if the rightop is a func node then it means
		 *  it identifies the value to place in our scan key.
		 *  Since functional indices have only one attribute
		 *  the attno must always be set to 1.
		 * ----------------
		 */
		if (scanvar == LEFT_OP)
		    elog(WARN, "ExecInitIndexScan: %s",
			 "both left and right ops are rel-vars");

		varattno = 	1;
		scanvar = 	RIGHT_OP;

	    } else {
		/* ----------------
		 *  otherwise, the leftop contains information usable
		 *  at runtime to figure out the value to place in our
		 *  scan key.
		 * ----------------
		 */
		have_runtime_keys = true;
		run_keys[ j ] = RIGHT_OP;
		scanvalue = get_constvalue(ConstTrue);
	    }

	    /* ----------------
	     *	now check that at least one op tells us the scan
	     *  attribute...
	     * ----------------
	     */
	    if (scanvar == NO_OP) 
		elog(WARN, "ExecInitIndexScan: %s",
		     "neither leftop nor rightop refer to scan relation");
	    
	    /* ----------------
	     *	initialize the scan key's fields appropriately
	     * ----------------
	     */
	    ExecSetSkeys(j,		/* index into scan_keys array */
			 scan_keys,	/* array in which to plug scan key */
			 varattno,	/* attribute number to scan */
			 opid,		/* reg proc to use */
			 scanvalue);	/* constant */
	}
	
	/* ----------------
	 *  store the key information into our array.
	 * ----------------
	 */
	numScanKeys[ i ] = 	n_keys;
	scanKeys[ i ] = 	scan_keys;
	runtimeKeyInfo[ i ] =   (Pointer) run_keys;
    }
    
    set_iss_NumIndices(indexstate, numIndices);
    set_iss_IndexPtr(indexstate, indexPtr);
    set_iss_ScanKeys(indexstate, scanKeys);
    set_iss_NumScanKeys(indexstate, numScanKeys);
    
    /* ----------------
     *	If all of our keys have the form (op var const) , then we have no
     *  runtime keys so we store NULL in the runtime key info.
     *  Otherwise runtime key info contains an array of pointers
     *  (one for each index) to arrays of flags (one for each key)
     *  which indicate that the qual needs to be evaluated at runtime.
     *  -cim 10/24/89
     * ----------------
     */
    if (have_runtime_keys)
	set_iss_RuntimeKeyInfo(indexstate, runtimeKeyInfo);
    else {
	set_iss_RuntimeKeyInfo(indexstate, NULL);
	for (i=0; i < numIndices; i++) {
	    List qual;
	    int n_keys;
	    qual = nth(i, indxqual);
	    n_keys = length(qual);
	    if (n_keys > 0)
	        pfree(runtimeKeyInfo[i]);
	  }
	pfree(runtimeKeyInfo);
    }
    
    /* ----------------
     *	get the range table and direction information
     *  from the execution state (these are needed to
     *  open the relations).
     * ----------------
     */
    rangeTable = get_es_range_table(estate);
    direction =  get_es_direction(estate);
    
    /* ----------------
     *	open the base relation
     * ----------------
     */
    relid =   get_scanrelid(node);
    rtentry = rt_fetch(relid, rangeTable);
    reloid =  CInteger(rt_relid(rtentry));
    timeQual = (TimeQual) CInteger(rt_time(rtentry));
    
    ExecOpenScanR(reloid,	      /* relation */
		  0,		      /* nkeys */
		  NULL,		      /* scan key */
		  0,		      /* is index */
		  direction,          /* scan direction */
		  timeQual,	      /* time qual */
		  &currentRelation,   /* return: rel desc */
		  &currentScanDesc);  /* return: scan desc */
    
    set_css_currentRelation(scanstate, currentRelation);
    set_css_currentScanDesc(scanstate, currentScanDesc);
    /*
     * intialize scan state rule info
     */
    set_css_ruleInfo(scanstate,
		prs2MakeRelationRuleInfo(currentRelation, RETRIEVE));


    /* ----------------
     *	XXX temporary hack to prevent tuple level locks
     *      from exhausting our semaphores.   Remove this
     *	    when the tuple level lock stuff is fixed.
     * ----------------
     */
    RelationSetLockForRead(currentRelation);
    
    /* ----------------
     *	get the scan type from the relation descriptor.
     * ----------------
     */
    ExecAssignScanType(scanstate, &currentRelation->rd_att);
    ExecAssignResultTypeFromTL(node, scanstate);
	
    /* ----------------
     *	index scans don't have subtrees..
     * ----------------
     */
    set_ss_ProcOuterFlag(scanstate, false);
    
    /* ----------------
     *	open the index relations and initialize
     *  relation and scan descriptors.
     * ----------------
     */
    for (i=0; i < numIndices; i++) {
	List	 index;
	ObjectId indexOid;
	
	index =     nth(i, indxid);
	indexOid =  CInteger(index);
	
	if (indexOid != 0) {
	    ExecOpenScanR(indexOid, 		  /* relation */
			  numScanKeys[ i ],	  /* nkeys */
			  scanKeys[ i ],	  /* scan key */
			  true,			  /* is index */
			  direction,		  /* scan direction */
			  timeQual, 		  /* time qual */
			  &(relationDescs[ i ]),  /* return: rel desc */
			  &(scanDescs[ i ]));	  /* return: scan desc */
	}
    }
    
    set_iss_RelationDescs(indexstate, relationDescs);
    if (ParallelExecutorEnabled() && SlaveLocalInfoD.nparallel > 1)
       partition_indexscan(numIndices, scanDescs, SlaveLocalInfoD.nparallel);
    set_iss_ScanDescs(indexstate, scanDescs);
    
    /* ----------------
     *	all done.
     * ----------------
     */
    return
	LispTrue;
}

/* ---------------------------------------------------------
 *	partition_indexscan
 *
 *	XXXXXX this function now only works for indexes on
 *	int2 and int4 and does handle skewed data distribution.
 *	more elaborate schemes will be implemented later.
 *
 * --------------------------------------------------------
 */
 
#define INT4LT  66
#define INT4LE  149
#define INT4GT  147
#define INT4GE  150
#define INT2LT  64
#define INT2LE  148
#define INT2GT  146
#define INT2GE  151

void
partition_indexscan(numIndices, scanDescs, parallel)
    int                     numIndices;
    IndexScanDescPtr        scanDescs;
    int                     parallel;
{
    int 		i;
    IndexScanDesc 	scanDesc;
    ScanKeyEntryData 	lowKeyEntry;
    ScanKeyEntryData 	highKeyEntry;
    int32 		lowKey, newlowkey;
    int32 		highKey, newhighkey;
    int 		delt;

    for (i=0; i<numIndices; i++) {
        scanDesc = scanDescs[i];
        if (scanDesc->numberOfKeys < 2) {
	   /* -------------------------------------
	    *  don't know how to partition, just return
	    * -------------------------------------
	    */
           return;
	   }
        lowKeyEntry = scanDesc->keyData.data[0];
	
        switch (lowKeyEntry.procedure) {
        case INT4GT:
        case INT4GE:
           highKeyEntry = scanDesc->keyData.data[1];
           break;
	   
        case INT4LT:
        case INT4LE:
           highKeyEntry = lowKeyEntry;
           lowKeyEntry = scanDesc->keyData.data[1];
           break;
	   
        defaults:
	   /* -------------------------------------
	    *  don't know how to partition, just return
	    * -------------------------------------
	    */
           return;
        }
	
        lowKey = DatumGetInt32(lowKeyEntry.argument);
        highKey = DatumGetInt32(highKeyEntry.argument);
        delt = highKey - lowKey;
	
        Assert(delt > 0);
	
        newlowkey = lowKey + SlaveInfoP[MyPid].groupPid * delt / parallel;
        newhighkey = lowKey + (SlaveInfoP[MyPid].groupPid + 1) * delt/parallel;
	
        lowKeyEntry.argument = Int32GetDatum(newlowkey);
	
        if (SlaveInfoP[MyPid].groupPid > 0) {
           lowKeyEntry.procedure = INT4GE;
	   fmgr_info(INT4GE, &lowKeyEntry.func, &lowKeyEntry.nargs);
	  }
	
        highKeyEntry.argument = Int32GetDatum(newhighkey);
	
        if (SlaveInfoP[MyPid].groupPid < parallel - 1) {
           highKeyEntry.procedure = INT4LT;
	   fmgr_info(INT4LT, &highKeyEntry.func, &highKeyEntry.nargs);
	  }
	
        scanDesc->keyData.data[0] = lowKeyEntry;
        scanDesc->keyData.data[1] = highKeyEntry;
      }
}
