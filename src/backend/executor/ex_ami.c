/* ----------------------------------------------------------------
 *   FILE
 *	ex_ami.c
 *	
 *   DESCRIPTION
 *	miscellanious executor access method routines
 *
 *   INTERFACE ROUTINES
 *
 *   	ExecOpenScanR	\			      /	amopen
 *   	ExecBeginScan	 \			     /	ambeginscan
 *   	ExecCloseR	  \			    /	amclose
 *    	ExecInsert	   \  executor interface   /	aminsert
 *   	ExecReScanNode	   /  to access methods    \	amrescan
 *   	ExecReScanR	  /			    \	amrescan
 *   	ExecMarkPos	 /			     \	ammarkpos
 *   	ExecRestrPos    /			      \	amrestpos
 *
 *	ExecCreatR	function to create temporary relations
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "executor/executor.h"
#include "storage/smgr.h"

 RcsId("$Header$");

/* ----------------------------------------------------------------
 *   	ExecOpenScanR
 *
 * old comments:
 *   	Parameters:
 *   	  relation -- relation to be opened and scanned.
 *   	  nkeys	   -- number of keys
 *   	  skeys    -- keys to restrict scanning
 *           isindex  -- if this is true, the relation is the relid of
 *                       an index relation, else it is an index into the
 *                       range table.
 *   	Returns the relation as(relDesc scanDesc)
 *         If this structure is changed, need to modify the access macros
 *   	defined in execInt.h.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecInitIndexScan
 *           ExecInitSeqScan
 ****/
void
ExecOpenScanR(relOid, nkeys, skeys, isindex, dir, timeRange,
	      returnRelation, returnScanDesc)
    ObjectId 	  relOid;
    int 	  nkeys;
    ScanKey 	  skeys;
    bool 	  isindex;
    ScanDirection dir;
    TimeQual 	  timeRange;
    Relation 	  *returnRelation;		/* return */
    Pointer	  *returnScanDesc; 		/* return */
{
    Relation relation;
    Pointer  scanDesc;
    
    /* ----------------
     *	note: scanDesc returned by ExecBeginScan can be either
     *	      a HeapScanDesc or an IndexScanDesc so for now we
     *	      make it a Pointer.  There should be a better scan
     *	      abstraction someday -cim 9/9/89
     * ----------------
     */
    relation = ExecOpenR(relOid, isindex);
    scanDesc = ExecBeginScan(relation,
			     nkeys,
			     skeys,
			     isindex,
			     dir,
			     timeRange);
    
    if (returnRelation != NULL)
	*returnRelation = relation;
    if (scanDesc != NULL)
	*returnScanDesc = scanDesc;
}
 
/* ----------------------------------------------------------------
 *	ExecOpenR
 *
 *	returns a relation descriptor given an object id.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecOpenScanR
 ****/
Relation
ExecOpenR(relationOid, isindex)
    ObjectId 	relationOid;
    bool	isindex;
{
    Relation relation;
    relation = (Relation) NULL;

    /* ----------------
     *	open the relation with the correct call depending
     *  on whether this is a heap relation or an index relation.
     *  note: I am not certain that AMopen supports index scans
     *        correctly yet! -cim 9/9/89
     * ----------------
     */
    if (isindex) {
	relation = AMopen(relationOid);
    } else
	relation = amopen(relationOid);
	
    if (relation == NULL)
	elog(DEBUG, "ExecOpenR: relation == NULL, amopen failed.");

    return relation;
}
 
/* ----------------------------------------------------------------
 *   	ExecBeginScan
 *
 *   	beginscans a relation in current direction.
 *
 *	XXX fix parameters to AMbeginscan (and btbeginscan)
 *		currently we need to pass a flag stating whether
 *		or not the scan should begin at an endpoint of
 *		the relation.. Right now we always pass false
 *		-cim 9/14/89
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecOpenScanR
 ****/
Pointer
ExecBeginScan(relation, nkeys, skeys, isindex, dir, time_range)
    Relation 	  relation;
    int 	  nkeys;
    ScanKey	  skeys;
    bool	  isindex;
    ScanDirection dir;
    TimeQual	  time_range;
{
    Pointer	 scanDesc;
    
    scanDesc = 	 NULL;

    /* ----------------
     *	open the appropriate type of scan.
     *	
     *  Note: ambeginscan()'s second arg is a boolean indicating
     *	      that the scan should be done in reverse..  That is,
     *	      if you pass it true, then the scan is backward.
     * ----------------
     */
    if (isindex) {
	scanDesc = (Pointer) AMbeginscan(relation,
					 false,	/* see above comment */
					 nkeys,
					 skeys);
    } else {
	scanDesc = (Pointer) ambeginscan(relation,
					 (dir == EXEC_BKWD),
					 time_range,
					 nkeys,
					 skeys);
    }
   
    if (scanDesc == NULL)
	elog(DEBUG, "ExecBeginScan: scanDesc = NULL, ambeginscan failed.");
    
    
    return scanDesc;
}
 
/* ----------------------------------------------------------------
 *   	ExecCloseR
 *
 *	closes the relation and scan descriptor for a scan or sort
 *	node.  Also closes index relations and scans for index scans.
 *
 * old comments
 *   	closes the relation indicated in 'relID'
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndHashJoin
 *           ExecEndIndexScan
 *           ExecEndMaterial
 *           ExecEndSeqScan
 *           ExecEndSort
 ****/
void
ExecCloseR(node)
    Plan	node;
{
    Pointer	 state;
    Relation	 relation;
    HeapScanDesc scanDesc;
    
    /* ----------------
     *  shut down the heap scan and close the heap relation
     * ----------------
     */
    switch (NodeType(node)) {
	
    case classTag(SeqScan):
    case classTag(IndexScan):
	state =  (Pointer) get_scanstate((Scan) node);
	break;
	
    case classTag(Material):
	state =  (Pointer) get_matstate((Material) node);
	break;
	
    case classTag(Sort):
	state =  (Pointer) get_sortstate((Sort) node);
	break;

    case classTag(Agg):
	state = (Pointer) get_aggstate((Agg) node);
	break;

    default:
	elog(DEBUG, "ExecCloseR: not a scan, material, or sort node!");
	return;
    }
    
    relation = get_css_currentRelation((CommonScanState) state);
    scanDesc = get_css_currentScanDesc((CommonScanState) state);
    
    if (scanDesc != NULL)
	amendscan(scanDesc);
    
    if (relation != NULL)
	amclose(relation);
    
    /* ----------------
     *	if this is an index scan then we have to take care
     *  of the index relations as well..
     * ----------------
     */
    if (ExecIsIndexScan(node)) {
	IndexScanState	 indexstate;
	int		 numIndices;
	RelationPtr	 indexRelationDescs;
	IndexScanDescPtr indexScanDescs;
	int		 i;
	
	indexstate = 	     get_indxstate((IndexScan) node);
	numIndices =         get_iss_NumIndices(indexstate);
	indexRelationDescs = get_iss_RelationDescs(indexstate);
	indexScanDescs =     get_iss_ScanDescs(indexstate);
	
	for (i = 0; i<numIndices; i++) {
	    /* ----------------
	     *	shut down each of the scans and
	     *  close each of the index relations
	     *
	     *  note: should this be using AMendscan and AMclose??
	     *        -cim 9/10/89
	     *  YES!! -mer 2/26/92
	     * ----------------
	     */
	    if (indexScanDescs[i] != NULL)
		AMendscan(indexScanDescs[i]);
	    
	    if (indexRelationDescs[i] != NULL)
		AMclose(indexRelationDescs[i]);
	}
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecReScan
 *
 *	XXX this should be extended to cope with all the node types..
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecNestLoop
 *           ExecSeqReScan
 ****/
void
ExecReScan(node)
    Plan node;
{
    switch(NodeType(node)) {
    case classTag(SeqScan):
	ExecSeqReScan((Plan)  node);
	return;
    
    case classTag(IndexScan):
	ExecIndexReScan((IndexScan) node);
	return;

    case classTag(Material):
	/* the first call to ExecReScan should have no effect because
	 * everything is initialized properly already.  the following
	 * calls will be handled by ExecSeqReScan() because the nodes
	 * below the Material node have already been materialized into
	 * a temp relation.
	 */
	return;
    default:
	elog(DEBUG, "ExecReScan: not a seqscan or indexscan node.");
	return;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecReScanR
 *
 *	XXX this does not do the right thing with indices yet.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecSeqReScan
 ****/
HeapScanDesc
ExecReScanR(relDesc, scanDesc, direction, nkeys, skeys)
    Relation      relDesc;	/* LLL relDesc unused  */
    HeapScanDesc  scanDesc;
    ScanDirection direction;
    int		  nkeys;	/* LLL nkeys unused  */
    ScanKey	  skeys;
{
    if (scanDesc != NULL)
	amrescan(scanDesc,			/* scan desc */
		 (direction == EXEC_BKWD),      /* backward flag */
		 skeys);			/* scan keys */
    
    return scanDesc;
}
 
/* ----------------------------------------------------------------
 *   	ExecMarkPos
 *
 *	Marks the current scan position.  Someday mabey the scan
 *	position will be returned but currently the routines 
 *	just return LispNil.
 *
 *	XXX Needs to be extended to include all the node types.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecMergeJoin
 *           ExecNestLoop
 *           ExecSeqMarkPos
 ****/
List
ExecMarkPos(node)
    Plan node;
{
    switch(NodeType(node)) {
    case classTag(SeqScan):
	return ExecSeqMarkPos((Plan) node);
    
    case classTag(IndexScan):
	return ExecIndexMarkPos((IndexScan) node);
    
    case classTag(Sort):
	return ExecSortMarkPos((Plan) node);

    default:
	/* elog(DEBUG, "ExecMarkPos: unsupported node type"); */
	return LispNil;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecRestrPos
 *
 *   	restores the scan position previously saved with ExecMarkPos()
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecMergeJoin
 *           ExecNestLoop
 *           ExecSeqRestrPos
 ****/
void
ExecRestrPos(node)
    Plan node;
{
    switch(NodeType(node)) {
    case classTag(SeqScan):
	ExecSeqRestrPos((Plan) node);
	return;
    
    case classTag(IndexScan):
	ExecIndexRestrPos((IndexScan) node);
	return;
    
    case classTag(Sort):
	ExecSortRestrPos((Plan) node);
	return;

    default:
	/* elog(DEBUG, "ExecRestrPos: node type not supported"); */
	return;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecCreatR
 *
 * old comments
 *   	Creates a relation.
 *
 *   	Parameters:
 *   	  noAttr    -- number of attributes in the created relation.
 *   	  attrType  -- type information on the attributes.
 *   	  accessMtd -- access methods used to access the created relation.
 *   	  relation  -- optional. Either an index to the range table or
 *   		       negative number indicating a temporary relation.
 *   		       A temporary relation is assume is this field is absent.
 * ----------------------------------------------------------------
 */
int tmpcnt = 0;			/* used to generate unique temp names */

/**** xxref:
 *           ExecInitHashJoin
 *           ExecInitMaterial
 *           ExecInitSort
 ****/
Relation
ExecCreatR(numberAttributes, tupType, relationOid)
    int  		numberAttributes;
    TupleDescriptor	tupType;
    int			relationOid;
{
    char 	tempname[40];
    char 	archiveMode;
    Relation 	relDesc;
    
    EU4_printf("ExecCreatR: %s numatts=%d type=%d oid=%d\n",
	       "entering: ", numberAttributes, tupType, relationOid);
    CXT1_printf("ExecCreatR: context is %d\n", CurrentMemoryContext);
    
    relDesc = NULL;
    bzero(tempname, 40);
    
    if (relationOid < 0) {
	/* ----------------
	 *   create a temporary relation
	 *   (currently the planner always puts a -1 in the relation
	 *    argument so we expect this to be the case although
	 *    it's possible that someday we'll get the name from
	 *    from the range table.. -cim 10/12/89)
	 * ----------------
	 */
	sprintf(tempname, "temp_%d.%d", getpid(), tmpcnt++);
	EU1_printf("ExecCreatR: attempting to create %s\n", tempname);
	relDesc = amcreatr(tempname,
			   numberAttributes,
			   DEFAULT_SMGR,
			   (struct attribute **)tupType);
    } else {
	/* ----------------
	 *	use a relation from the range table
	 * ----------------
	 */
	elog(DEBUG, "ExecCreatR: %s",
	     "stuff using range table id's is not functional");
    }
    
    if (relDesc == NULL)
	elog(DEBUG, "ExecCreatR: failed to create relation.");
    
    EU1_printf("ExecCreatR: returning relDesc=%d\n", relDesc);
    
    return relDesc;
}
 
