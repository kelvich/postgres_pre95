/* ----------------------------------------------------------------
 *   FILE
 *	indexvalid.c
 *	
 *   DESCRIPTION
 *	index tuple qualification validity checking code
 *
 *   INTERFACE ROUTINES
 *	index_keytest
 *  	index_satisifies
 *
 *   OLD INTERFACE FUNCTIONS
 *	ikeytest_tupdesc, ikeytest
 *	ItemIdSatisfiesScanKey
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "executor/execdebug.h"
#include "access/genam.h"
#include "access/iqual.h"
#include "access/itup.h"
#include "access/skey.h"

#include "storage/buf.h"
#include "storage/bufpage.h"
#include "storage/itemid.h"
#include "storage/page.h"
#include "utils/rel.h"

/* ----------------------------------------------------------------
 *		  index scan key qualification code
 * ----------------------------------------------------------------
 */
int	NIndexTupleProcessed;

/* ----------------
 *	index_keytest
 *
 * old comments
 *	May eventually combine with other tests (like timeranges)?
 *	Should have Buffer buffer; as an argument and pass it to amgetattr.
 * ----------------
 */
bool
index_keytest(tuple, tupdesc, scanKeySize, key)
    IndexTuple	    tuple;
    TupleDescriptor tupdesc;
    ScanKeySize	    scanKeySize;
    ScanKey	    key;
{
    Boolean	    isNull;
    Datum	    datum;
    int		    test;

    IncrIndexProcessed();
    
    while (scanKeySize > 0) {
	datum = IndexTupleGetAttributeValue(tuple,
					    1,
					    tupdesc,
					    &isNull);
					    
	if (isNull) {
	    /* XXX eventually should check if SK_ISNULL */
	    return (false);
	}
	
	if (key->data[0].flags & CommuteArguments) {
	    test = (int) (*(key->data[0].func))
			     (DatumGetPointer(key->data[0].argument),
			      datum);
	} else {
	    test = (int) (*(key->data[0].func))
			     (datum,
			      DatumGetPointer(key->data[0].argument));
	}

	if (!test == !(key->data[0].flags & NegateResult)) {
	    return (false);
	}

	scanKeySize -= 1;
	key = (ScanKey)&key->data[1];
    }

    return (true);
}

/* ----------------
 *  	index_satisifies
 * ----------------
 */
bool
index_satisifies(itemId, page, relation, scanKeySize, key)
    ItemId	itemId;
    Page	page;
    Relation	relation;
    ScanKeySize	scanKeySize;
    ScanKey	key;    
{
    IndexTuple	tuple;
    TupleDescriptor tupdesc;

    Assert(ItemIdIsValid(itemId));
    Assert(PageIsValid(page));	/* XXX needs better checking */
    Assert(RelationIsValid(relation));
    Assert(ScanKeyIsValid(scanKeySize, key));

    if (!ItemIdIsUsed(itemId) || ItemIdIsContinuation(itemId)) 
	return (false);

    tuple = (IndexTuple) PageGetItem(page, itemId);
    tupdesc = RelationGetTupleDescriptor(relation);
    
    if (! index_keytest(tuple, tupdesc, scanKeySize, key))
	return (false);
    
    return (true);
}
