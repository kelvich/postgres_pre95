/* ----------------------------------------------------------------
 *   FILE
 * 	genam.c
 *	
 *   DESCRIPTION
 *	general index access method routines
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 * old comments:
 * Scans are implemented as follows:
 *
 * `0' represents an invalid item pointer.
 * `-' represents an unknown item pointer.
 * `X' represents a known item pointers.
 * `+' represents known or invalid item pointers.
 * `*' represents any item pointers.
 *
 * State is represented by a triple of these symbols in the order of
 * previous, current, next.  Note that the case of reverse scans works
 * identically.
 *
 *	State	Result
 * (1)	+ + -	+ 0 0		(if the next item pointer is invalid)
 * (2)		+ X -		(otherwise)
 * (3)	* 0 0	* 0 0		(no change)
 * (4)	+ X 0	X 0 0		(shift)
 * (5)	* + X	+ X -		(shift, add unknown)
 *
 * All other states cannot occur.
 *
 * Note:
 *	It would be possible to cache the status of the previous and
 *	next item pointer using the flags.
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

/* ----------------------------------------------------------------
 *	general access method routines
 *
 *	All indexed access methods use an identical scan structure.
 *	We don't know how the various AMs do locking, however, so we don't
 *	do anything about that here.
 *
 *	The intent is that an AM implementor will define a front-end routine
 *	that calls this one, to fill in the scan, and then does whatever kind
 *	of locking he wants.
 * ----------------------------------------------------------------
 */

/* ----------------
 *  RelationGetIndexScan -- Create and fill an IndexScanDesc.
 *
 *	This routine creates an index scan structure and sets its contents
 *	up correctly. This routine calls AMrescan to set up the scan with
 *	the passed key.
 *
 *	Parameters:
 *		relation -- index relation for scan.
 *		scanFromEnd -- if true, begin scan at one of the index's
 *			       endpoints.
 *		numberOfKeys -- count of scan keys (more than one won't
 *				necessarily do anything useful, yet).
 *		key -- the ScanKey for the starting position of the scan.
 *
 *	Returns:
 *		An initialized IndexScanDesc.
 *
 *	Side Effects:
 *		Bumps the ref count on the relation to keep it in the cache.
 *	
 * ----------------
 */
IndexScanDesc
RelationGetIndexScan(relation, scanFromEnd, numberOfKeys, key)
    Relation	relation;
    Boolean	scanFromEnd;
    uint16	numberOfKeys;
    ScanKey	key;
{
    IndexScanDesc	scan;

    if (! RelationIsValid(relation))
	elog(WARN, "RelationGetIndexScan: relation invalid");

    RelationIncrementReferenceCount(relation);

    scan = (IndexScanDesc)
	palloc(sizeof *scan + (numberOfKeys - 1) * sizeof key->data[0]);

    scan->relation = relation;
    scan->opaque = (Pointer) NULL;
    scan->numberOfKeys = numberOfKeys;

    ItemPointerSetInvalid(&scan->previousItemData);
    ItemPointerSetInvalid(&scan->currentItemData);
    ItemPointerSetInvalid(&scan->nextItemData);
    ItemPointerSetInvalid(&scan->previousMarkData);
    ItemPointerSetInvalid(&scan->currentMarkData);
    ItemPointerSetInvalid(&scan->nextMarkData);

    index_rescan(scan, scanFromEnd, key);

    return (scan);
}

/* ----------------
 *  IndexScanRestart -- Restart an index scan.
 *
 *	This routine isn't used by any existing access method.  It's
 *	appropriate if relation level locks are what you want.
 *
 *  Returns:
 *	None.
 *
 *  Side Effects:
 *	None.
 * ----------------
 */

void
IndexScanRestart(scan, scanFromEnd, key)
    IndexScanDesc	scan;
    bool		scanFromEnd;
    ScanKey		key;
{
    if (! IndexScanIsValid(scan))
	elog(WARN, "IndexScanRestart: invalid scan");

    ItemPointerSetInvalid(&scan->previousItemData);
    ItemPointerSetInvalid(&scan->currentItemData);
    ItemPointerSetInvalid(&scan->nextItemData);

    if (RelationGetNumberOfBlocks(scan->relation) == 0) 
	scan->flags = ScanUnmarked;
    else if (scanFromEnd)
	scan->flags = ScanUnmarked | ScanUncheckedPrevious;
    else
	scan->flags = ScanUnmarked | ScanUncheckedNext;

    scan->scanFromEnd = (Boolean) scanFromEnd;

    if (scan->numberOfKeys > 0)
	bcopy((Pointer)& key->data[0],
	      (Pointer)& scan->keyData.data[0],
	      scan->numberOfKeys * sizeof key->data[0]);
}

/* ----------------
 *  IndexScanEnd -- End and index scan.
 *
 *	This routine is not used by any existing access method, but is
 *	suitable for use if you don't want to do sophisticated locking.
 *
 *  Returns:
 *	None.
 *
 *  Side Effects:
 *	None.
 * ----------------
 */
void
IndexScanEnd(scan)
    IndexScanDesc	scan;
{
    if (! IndexScanIsValid(scan))
	elog(WARN, "IndexScanEnd: invalid scan");

    RelationDecrementReferenceCount(scan->relation);
    pfree((Pointer) scan);
}

/* ----------------
 *  IndexScanMarkPosition -- Mark current position in a scan.
 *
 *	This routine isn't used by any existing access method, but is the
 *	one that AM implementors should use, if they don't want to do any
 *	special locking.  If relation-level locking is sufficient, this is
 *	the routine for you.
 *
 *  Returns:
 *	None.
 *
 *  Side Effects:
 *	None.
 * ----------------
 */
void
IndexScanMarkPosition(scan)
    IndexScanDesc	scan;
{
    RetrieveIndexResult	result;

    if (scan->flags & ScanUncheckedPrevious) {
	result = (RetrieveIndexResult)
	    index_getnext(scan, BackwardScanDirection);

	if (RetrieveIndexResultIsValid(result)) {
	    scan->previousItemData =
		*RetrieveIndexResultGetIndexItemPointer(result);
	} else {
	    ItemPointerSetInvalid(&scan->previousItemData);
	}

    } else if (scan->flags & ScanUncheckedNext) {
	result = (RetrieveIndexResult)
	    index_getnext(scan, ForwardScanDirection);

	if (RetrieveIndexResultIsValid(result)) {
	    scan->nextItemData =
		*RetrieveIndexResultGetIndexItemPointer(result);
	} else {
	    ItemPointerSetInvalid(&scan->nextItemData);
	}
    }

    scan->previousMarkData = scan->previousItemData;
    scan->currentMarkData = scan->currentItemData;
    scan->nextMarkData = scan->nextItemData;

    scan->flags = 0x0;	/* XXX should have a symbolic name */
}

/* ----------------
 *  IndexScanRestorePosition -- Restore position on a marked scan.
 *
 *	This routine isn't used by any existing access method, but is the
 *	one that AM implementors should use if they don't want to do any
 *	special locking.  If relation-level locking is sufficient, then
 *	this is the one you want.
 *
 *  Returns:
 *	None.
 *
 *  Side Effects:
 *	None.
 * ----------------
 */
void
IndexScanRestorePosition(scan)
    IndexScanDesc	scan;
{	
    RetrieveIndexResult	result;

    if (scan->flags & ScanUnmarked) 
	elog(WARN, "IndexScanRestorePosition: no mark to restore");

    scan->previousItemData = scan->previousMarkData;
    scan->currentItemData = scan->currentMarkData;
    scan->nextItemData = scan->nextMarkData;

    scan->flags = 0x0;	/* XXX should have a symbolic name */
}

/* ----------------
 *  IndexScanGetRetrieveIndexResult 
 *
 *  Return a RetrieveIndexResult for the next tuple in the index scan.
 *  A RetrieveIndexResult (see itup.h) is a index tuple/heap tuple pair.
 *  This routine is used primarily by the vacuum daemon, so that it can
 *  delete index tuples associated with zapped heap tuples.
 *
 *  Parameters:
 *		scan -- Index scan to use.
 *		direction -- Forward, backward, or no movement.
 *
 *  Returns:
 *		RetrieveIndexResult for the tuple in the scan that's next
 *		in the specified direction.
 *
 *  Side Effects: None.
 * ----------------
 */
RetrieveIndexResult
IndexScanGetRetrieveIndexResult(scan, direction)
    IndexScanDesc	scan;
    ScanDirection	direction;
{
    return (RetrieveIndexResult)
	index_getnext(scan, direction);
}

/* ----------------
 *	IndexScanGetGeneralRetrieveIndexResult
 *
 *  	A GeneralRetrieveIndexResult (see itup.h) is a index tuple
 *	rule lock pair.  But currently we don't support index rule
 *	lock results so this routine is pretty bogus.
 * ----------------
 */
GeneralRetrieveIndexResult
IndexScanGetGeneralRetrieveIndexResult(scan, direction)
    IndexScanDesc	scan;
    ScanDirection	direction;
{
    RetrieveIndexResult		result;
    ItemPointer 		heapItemPointer;
    GeneralRetrieveIndexResult	generalResult;
    
    /* ----------------
     *	use index_getnext to get the "RetrieveIndexResult"
     * ----------------
     */
    result = (RetrieveIndexResult)
	index_getnext(scan, direction);

    if (! RetrieveIndexResultIsValid(result))
	return (GeneralRetrieveIndexResult) NULL;

    /* ----------------
     *	silly crap to convert a "RetrieveIndexResult" to a
     *  "GeneralRetrieveIndexResult".  
     * ----------------
     */
    heapItemPointer = RetrieveIndexResultGetHeapItemPointer(result);
    
    generalResult = (GeneralRetrieveIndexResult)
	palloc(sizeof *result);

    generalResult->heapItemData = *heapItemPointer;
    
    return generalResult;
}

/* ----------------------------------------------------------------
 *	old interface routines - see access/index/indexam.c for
 *	new index_ interface functions.
 * ----------------------------------------------------------------
 */
/* ----------------
 *	old create / destroy interface
 *
 * 	Note: index_create and index_destroy are in lib/catalog/index.c
 * ----------------
 */
void
DestroyIndexRelationById(indexId)
    ObjectId	indexId;
{
    index_destroy(indexId);
}

void
AMdestroy(indexId)
    ObjectId	indexId;
{
    index_destroy(indexId);
}

/* ----------------
 *	old open / close interface
 *
 * 	Note: index_open, index_openr, and index_close are all
 *	in access/index/indexam.c
 * ----------------
 */
Relation
ObjectIdOpenIndexRelation(relationObjectId)
    ObjectId	relationObjectId;
{
    return (Relation)
	index_open(relationObjectId);
}

Relation
AMopen(relationObjectId)
    ObjectId	relationObjectId;
{
    return (Relation)
	index_open(relationObjectId);
}

Relation
AMopenr(relationName)
    Name	relationName;
{
    return (Relation)
	index_openr(relationName);
}

void
RelationCloseIndexRelation(relation)
    Relation	relation;
{
    (void) index_close(relation);
}

void
AMclose(relation)
    Relation	relation;
{
    (void) index_close(relation);
}

/* ----------------
 *	old insert / delete routines
 * ----------------
 */
GeneralInsertIndexResult
RelationInsertIndexTuple(relation, indexTuple, scan, offsetOutP)
    Relation	relation;
    IndexTuple	indexTuple;
    char	*scan;
    double	*offsetOutP;
{
    return (GeneralInsertIndexResult)
	index_insert(relation, indexTuple, offsetOutP);
}

GeneralInsertIndexResult
AMinsert(relation, indexTuple, scan, offsetOutP)
    Relation	relation;
    IndexTuple	indexTuple;
    char	*scan;
    double	*offsetOutP;
{
    return (GeneralInsertIndexResult)
	index_insert(relation, indexTuple, offsetOutP);
}

void
RelationDeleteIndexTuple(relation, indexItem)
    Relation	relation;
    ItemPointer	indexItem;
{
    (void) index_delete(relation, indexItem);
}

void
AMdelete(relation, indexItem)
    Relation	relation;
    ItemPointer	indexItem;
{
    (void) index_delete(relation, indexItem);
}

/* ----------------
 *	old index scan support
 * ----------------
 */
IndexScanDesc
AMbeginscan(relation, scanFromEnd, numberOfKeys, key)
    Relation	relation;
    Boolean	scanFromEnd;
    uint16	numberOfKeys;
    ScanKey	key;
{
    return (IndexScanDesc)
	index_beginscan(relation, scanFromEnd, numberOfKeys, key);
}

void
AMrescan(scan, scanFromEnd, key)
    IndexScanDesc	scan;
    bool		scanFromEnd;
    ScanKey		key;
{
    (void) index_rescan(scan, scanFromEnd, key);
}

void
AMendscan(scan) 
    IndexScanDesc scan;
{
    (void) index_endscan(scan);
}


void
AMmarkpos(scan)
    IndexScanDesc scan;
{
    (void) index_markpos(scan);
}

void
AMrestrpos(scan)
    IndexScanDesc scan;
{
    (void) index_restrpos(scan);
}

GeneralRetrieveIndexResult
AMgettuple(scan, direction)
    IndexScanDesc	scan;		/* the scan on the index reln */
    ScanDirection	direction;	/* -1 back, 0 no move, 1 fwd */
{
    return (GeneralRetrieveIndexResult)
	IndexScanGetGeneralRetrieveIndexResult(scan, direction);
}

/* ----------------------------------------------------------------
 *			unimplemented crap
 * ----------------------------------------------------------------
 */
void
RelationSetIndexRuleLock(relation, indexItem, lock)
    Relation	relation;
    ItemPointer	indexItem;
    RuleLock	lock;
{
    elog(DEBUG, "RelationSetIndexRuleLock; unimplemented");
}

void
AMsetlock(relation, indexItem, lock)
    Relation	relation;
    ItemPointer	indexItem;
    RuleLock	lock;
{
    elog(DEBUG, "AMsetlock; unimplemented");
}

void
RelationSetIndexItemPointer(relation, indexItem, heapItem)
    Relation	relation;
    ItemPointer	indexItem;
    ItemPointer	heapItem;
{
    elog(DEBUG, "RelationSetIndexItemPointer: unimplemented");
}

void
AMsettid(relation, indexItem, heapItem)
    Relation	relation;
    ItemPointer	indexItem;
    ItemPointer	heapItem;
{
    elog(DEBUG, "AMsettid: unimplemented");
}

extern
IndexTupleFree()
{
    elog(DEBUG, "IndexTupleFree: unimplemented");
}

extern
AMfreetuple()
{
    elog(DEBUG, "AMfreetuple: unimplemented");
}
