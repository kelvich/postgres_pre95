/* ----------------------------------------------------------------
 *   FILE
 *	iqual.h
 *
 *   DESCRIPTION
 *	Index scan key qualification definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	IQualDefined	/* Include this file only once. */
#define IQualDefined	1

#include "tmp/c.h"

#include "storage/itemid.h"
#include "utils/rel.h"
#include "access/skey.h"

/* ----------------
 *	index tuple qualification support
 * ----------------
 */

/* 
 *	index_keytest
 *	index_satisifies
 */
extern
bool
index_keytest ARGS((
    IndexTuple	    tuple,
    TupleDescriptor tupdesc,
    ScanKeySize	    scanKeySize,
    ScanKey	    key
));		    

extern
bool
index_satisifies ARGS((
    ItemId	itemId,
    Page	page,
    Relation	relation,
    ScanKeySize	scanKeySize,
    ScanKey	key
));

/* ----------------
 *	old interface macros
 * ----------------
 */
/*
 * ikeytest_tupdesc
 * ikeytest
 */
#define ikeytest_tupdesc(tuple, tupdesc, scanKeySize, key) \
    index_keytest(tuple, tupdesc, scanKeySize, key)

#define ikeytest(tuple, relation, scanKeySize, key) \
    index_keytest(tuple, \
		  RelationGetTupleDescriptor(relation), scanKeySize, key)

/*
 * ItemIdSatisfiesScanKey --
 *	Returns true iff item associated with an item identifier satisifes
 *	the index scan key qualification.
 *
 * Note:
 *	Assumes item identifier is valid.
 *	Assumes standard page.
 *	Assumes relation is valid.
 *	Assumes scan key is valid.
 */
#define ItemIdSatisfiesScanKey(itemId, page, relation, scanKeySize, key) \
    index_satisifies(itemId, page, relation, scanKeySize, key)

#endif	/* !defined(IQualDefined) */
