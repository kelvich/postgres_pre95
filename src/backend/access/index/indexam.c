/* ----------------------------------------------------------------
 *   FILE
 *	indexam.c
 *	
 *   DESCRIPTION
 *	general index access method routines
 *
 *   INTERFACE ROUTINES
 *	index_open 	- open an index relation by relationId
 *	index_openr 	- open a index relation by name
 *	index_close 	- close a index relation
 *	index_beginscan - start a scan of an index
 *	index_rescan  	- restart a scan of an index
 *	index_endscan 	- end a scan
 *	index_insert 	- insert an index tuple into a relation
 *	index_delete 	- delete an item from an index relation
 *	index_markpos  	- mark a scan position
 *	index_restrpos  - restore a scan position
 *	index_getnext 	- get the next tuple from a scan
 * **	index_fetch	- retrieve tuple with tid
 * **	index_replace	- replace a tuple
 * **	index_getattr	- get an attribute from an index tuple
 *	index_getprocid - get a support procedure id from the rel tuple
 *	
 *	IndexScanIsValid - check index scan
 *
 *   NOTES
 *	This file contains the index_ routines which used
 *	to be a scattered collection of stuff in access/genam.
 *
 *	The ** routines: index_fetch, index_replace, and index_getattr
 *	have not yet been implemented.  They may not be needed.
 *
 * old comments
 * 	Scans are implemented as follows:
 *
 * 	`0' represents an invalid item pointer.
 * 	`-' represents an unknown item pointer.
 * 	`X' represents a known item pointers.
 * 	`+' represents known or invalid item pointers.
 * 	`*' represents any item pointers.
 *
 * 	State is represented by a triple of these symbols in the order of
 * 	previous, current, next.  Note that the case of reverse scans works
 * 	identically.
 *
 *		State	Result
 * 	(1)	+ + -	+ 0 0		(if the next item pointer is invalid)
 * 	(2)		+ X -		(otherwise)
 * 	(3)	* 0 0	* 0 0		(no change)
 * 	(4)	+ X 0	X 0 0		(shift)
 * 	(5)	* + X	+ X -		(shift, add unknown)
 *
 * 	All other states cannot occur.
 *
 * 	Note: It would be possible to cache the status of the previous and
 *	      next item pointer using the flags.
 *
 *  IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

#include "access/attnum.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/itup.h"
#include "access/newam.h"
#include "access/relscan.h"
#include "access/sdir.h"
#include "access/skey.h"

#include "storage/form.h"
#include "utils/log.h"
#include "utils/rel.h"

#include "catalog/catname.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"

RcsId("$Header$");

/* ----------------
 *   delete is used as the argument to a macro in this file so
 *   we don't use the "delete" macro defined in c.h
 * ----------------
 */
#undef delete

/* ----------------
 *	IndexScanIsValid
 * ----------------
 */
bool
IndexScanIsValid(scan)
    IndexScanDesc	scan;
{
    return ((bool) PointerIsValid(scan));
}

/* ----------------------------------------------------------------
 *		    macros used in index_ routines
 * ----------------------------------------------------------------
 */
#define RELATION_CHECKS \
    Assert(RelationIsValid(relation)); \
    Assert(FormIsValid((Form) relation->rd_am))

#define SCAN_CHECKS \
    Assert(IndexScanIsValid(scan)); \
    Assert(RelationIsValid(scan->relation)); \
    Assert(FormIsValid((Form) scan->relation->rd_am))

#define GET_REL_PROCEDURE(x,y) \
    CppConcat(procedure = relation->rd_am->,y); \
    if (! RegProcedureIsValid(procedure)) \
        elog(WARN, "index_%s: invalid %s regproc", \
	     CppAsString(x), CppAsString(y))

#define GET_SCAN_PROCEDURE(x,y) \
    CppConcat(procedure = scan->relation->rd_am->,y); \
    if (! RegProcedureIsValid(procedure)) \
        elog(WARN, "index_%s: invalid %s regproc", \
	     CppAsString(x), CppAsString(y))


/* ----------------------------------------------------------------
 *		   index_ interface functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	index_open - open an index relation by relationId
 *
 *	presently the relcache routines do all the work we need
 *	to open/close index relations.
 * ----------------
 */
Relation
index_open(relationId)
    ObjectId	relationId;
{
    return (Relation)
	RelationIdGetRelation(relationId);
}

/* ----------------
 *	index_openr - open a index relation by name
 *
 *	presently the relcache routines do all the work we need
 *	to open/close index relations.
 * ----------------
 */
Relation
index_openr(relationName)
    Name relationName;
{
    return (Relation)
	RelationNameGetRelation(relationName);
}

/* ----------------
 *	index_close - close a index relation
 *
 *	presently the relcache routines do all the work we need
 *	to open/close index relations.
 * ----------------
 */
void
index_close(relation)
    Relation relation;
{
    (void) RelationClose(relation);
}

/* ----------------
 *	index_insert - insert an index tuple into a relation
 * ----------------
 */
GeneralInsertIndexResult
index_insert(relation, indexTuple, offsetOutP)
    Relation	relation;
    IndexTuple	indexTuple;
    double	*offsetOutP;
{
    RegProcedure		procedure;
    InsertIndexResult		specificResult;
    GeneralInsertIndexResult	returnResult;

    RELATION_CHECKS;
    GET_REL_PROCEDURE(insert,aminsert);
    
    /* ----------------
     *	have the am's insert proc do all the work.  
     * ----------------
     */
    specificResult = (InsertIndexResult)
	fmgr(procedure, relation, indexTuple, NULL);

    /* ----------------
     *	the insert proc is supposed to return a "specific result" and
     *  this routine has to return a "general result" so after we get
     *  something back from the insert proc, we allocate a
     *  "general result" and copy some crap between the two.
     *
     *  As far as I'm concerned all this result shit is needlessly c
     *  omplicated and should be eliminated.  -cim 1/19/91
     * ----------------
     */
    returnResult = (GeneralInsertIndexResult) palloc(sizeof *returnResult);
    returnResult->pointerData = specificResult->pointerData;
    
    if (PointerIsValid(offsetOutP))
	*offsetOutP = specificResult->offset;
    
    return (returnResult);
}

/* ----------------
 *	index_delete - delete an item from an index relation
 * ----------------
 */
void
index_delete(relation, indexItem)
    Relation	relation;
    ItemPointer	indexItem;
{
    RegProcedure	procedure;
    
    RELATION_CHECKS;
    GET_REL_PROCEDURE(delete,amdelete);
    
    (void) fmgr(procedure, relation, indexItem);    
}

/* ----------------
 *	index_beginscan - start a scan of an index
 * ----------------
 */
IndexScanDesc
index_beginscan(relation, scanFromEnd, numberOfKeys, key)
    Relation	relation;
    Boolean	scanFromEnd;
    uint16	numberOfKeys;
    ScanKey	key;
{
    IndexScanDesc	scandesc;
    RegProcedure	procedure;
    
    RELATION_CHECKS;
    GET_REL_PROCEDURE(beginscan,ambeginscan);
    
    scandesc = (IndexScanDesc)
	fmgr(procedure, relation, scanFromEnd, numberOfKeys, key);
    
    return scandesc;
}

/* ----------------
 *	index_rescan  - restart a scan of an index
 * ----------------
 */
void
index_rescan(scan, scanFromEnd, key)
    IndexScanDesc	scan;
    bool		scanFromEnd;
    ScanKey		key;
{
    RegProcedure	procedure;
    
    SCAN_CHECKS;
    GET_SCAN_PROCEDURE(rescan,amrescan);

    (void) fmgr(procedure, scan, scanFromEnd, key);
}

/* ----------------
 *	index_endscan - end a scan
 * ----------------
 */
void
index_endscan(scan)
    IndexScanDesc	scan;
{
    RegProcedure	procedure;
    
    SCAN_CHECKS;
    GET_SCAN_PROCEDURE(endscan,amendscan);

    (void) fmgr(procedure, scan);
}

/* ----------------
 *	index_markpos  - mark a scan position
 * ----------------
 */
void
index_markpos(scan)
    IndexScanDesc	scan;
{
    RegProcedure	procedure;
    
    SCAN_CHECKS;
    GET_SCAN_PROCEDURE(markpos,ammarkpos);

    (void) fmgr(procedure, scan);
}

/* ----------------
 *	index_restrpos  - restore a scan position
 * ----------------
 */
void
index_restrpos(scan)
    IndexScanDesc	scan;
{
    RegProcedure	procedure;
    
    SCAN_CHECKS;
    GET_SCAN_PROCEDURE(restrpos,amrestrpos);

    (void) fmgr(procedure, scan);
}

/* ----------------
 *	index_getnext - get the next tuple from a scan
 *
 *  	A RetrieveIndexResult is a index tuple/heap tuple pair
 * ----------------
 */
RetrieveIndexResult
index_getnext(scan, direction)
    IndexScanDesc	scan;
    ScanDirection	direction;
{
    RegProcedure		procedure;
    RetrieveIndexResult		result;

    SCAN_CHECKS;
    GET_SCAN_PROCEDURE(getnext,amgettuple);

    /* ----------------
     *	have the am's gettuple proc do all the work.  
     * ----------------
     */
    result = (RetrieveIndexResult)
	fmgr(procedure, scan, direction);

    if (! RetrieveIndexResultIsValid(result))
	return NULL;
    
    return result;
}
/* ----------------
 *	index_getprocid
 *
 *	Some indexed access methods may require support routines that are
 *	not in the operator class/operator model imposed by pg_am.  These
 *	access methods may store the OIDs of registered procedures they
 *	need in pg_amproc.  These registered procedure OIDs are ordered in
 *	a way that makes sense to the access method, and used only by the
 *	access method.  The general index code doesn't know anything about
 *	the routines involved; it just builds an ordered list of them for
 *	each attribute on which an index is defined.
 *
 *	This routine returns the requested procedure OID for a particular
 *	indexed attribute.
 * ----------------
 */

RegProcedure
index_getprocid(irel, attnum, procnum)
    Relation irel;
    AttributeNumber attnum;
    uint16 procnum;
{
    RegProcedure *loc;
    AttributeNumber natts;

    natts = irel->rd_rel->relnatts;

    loc = (RegProcedure *)
       (((char *) &(irel->rd_att.data[natts])) + sizeof(IndexStrategy));

    Assert(loc != NULL);

    return (loc[(natts * (attnum - 1)) + (procnum - 1)]);
}

/* ----------------
 *	InsertIndexTuple
 * ----------------
 */
void
InsertIndexTuple(heapRelation, indexRelation, numberOfAttributes, 
		 attributeNumber, heapTuple, 
		 indexStrategy, parameterCount, parameter)
    Relation		heapRelation;
    Relation		indexRelation;
    AttributeNumber	numberOfAttributes;
    AttributeNumber	attributeNumber[];
    HeapTuple          	heapTuple;
    IndexStrategy	indexStrategy;
    uint16		parameterCount;
    Datum		parameter[];
{
    HeapScanDesc	scan;
    Buffer		buffer;
    AttributeNumber	attributeIndex;
    IndexTuple		indexTuple;
    TupleDescriptor	heapDescriptor;
    TupleDescriptor	indexDescriptor;
    Datum		*datum;
    Boolean		*null;

    char		*dom[2];
    extern		HasCached;
    extern char		CachedClass[];
    extern		fillCached();
    GeneralInsertIndexResult	insertResult;

    /* more & better checking is needed */
    Assert(ObjectIdIsValid(indexRelation->rd_rel->relam));	/* XXX */

    datum = (Datum *)   palloc(numberOfAttributes * sizeof *datum);
    null =  (Boolean *) palloc(numberOfAttributes * sizeof *null);
    
    heapDescriptor =  RelationGetTupleDescriptor(heapRelation);
    indexDescriptor = RelationGetTupleDescriptor(indexRelation);

    Assert(HeapTupleIsValid(heapTuple));
    for (attributeIndex = 1; attributeIndex <= numberOfAttributes;
	 attributeIndex += 1) {
	  
	AttributeOffset	attributeOffset;
	Boolean		attributeIsNull;
	  
	attributeOffset = AttributeNumberGetAttributeOffset(attributeIndex);

	datum[attributeOffset] =
	    PointerGetDatum( heap_getattr(heapTuple, buffer,
					  attributeNumber[attributeOffset],
					  heapDescriptor, &attributeIsNull) );
	
	  null[attributeOffset] = (attributeIsNull) ? 'n' : ' ';
    }

    indexTuple = (IndexTuple)
	index_formtuple(numberOfAttributes,
			indexDescriptor,
			datum,
			null);

    indexTuple->t_tid = heapTuple->t_ctid;

    insertResult = index_insert(indexRelation,
				indexTuple, 
				(Pointer)NULL,
				(double *)NULL);
	
    pfree(indexTuple);
    pfree(null);
    pfree(datum);
}

/* ----------------
 *	GetHeapTuple
 * ----------------
 */
HeapTuple
GetHeapTuple(result, heaprel, buffer)
    GeneralRetrieveIndexResult      result;
    Relation                        heaprel;
    Buffer                          buffer;
{
    ItemPointer     pointer;
    HeapTuple       tuple;

    Assert(GeneralRetrieveIndexResultIsValid(result));

    pointer = GeneralRetrieveIndexResultGetHeapItemPointer(result);

    if (! ItemPointerIsValid(pointer))
	return NULL;

    tuple = heap_fetch(heaprel, NowTimeQual, pointer, &buffer);

    if (! HeapTupleIsValid(tuple)) 
	return(NULL);
    else 
	return(tuple);
}
