/* ----------------------------------------------------------------
 *   FILE
 *	itup.h
 *
 *   DESCRIPTION
 *	POSTGRES index tuple definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef ITUP_H
#define ITUP_H

#include "tmp/c.h"
#include "storage/form.h"
#include "access/ibit.h"
#include "storage/itemptr.h"
#include "rules/rlock.h"

#define MaxIndexAttributeNumber	7

/* ----------------
 * NOTE:
 * A rule lock has 2 different representations:
 *    The disk representation (t_lock.l_ltid) is an ItemPointer
 * to the actual rule lock data (stored somewher else in the disk page).
 * In this case `t_locktype' has the value DISK_INDX_RULE_LOCK.
 *    The main memory representation (t_lock.l_lock) is a pointer
 * (RuleLock) to a (palloced) structure. In this case `t_locktype'
 * has the value MEM_INDX_RULE_LOCK.
 * ----------------
 */

#define DISK_INDX_RULE_LOCK	'd'
#define MEM_INDX_RULE_LOCK	'm'

typedef union 
{
		ItemPointerData	l_ltid;	/* TID of the lock */
		RuleLock	l_lock;		/* internal lock format */
}
IndexTupleRuleLock;

typedef struct IndexTupleData {
	ItemPointerData			t_tid; /* reference TID to base tuple */

	/*
	 * t_info is layed out in the following fashion:
	 *
	 * first (leftmost) bit: "has nulls" bit
	 * second bit: "has varlenas" bit
	 * third bit: "has rules" bit
	 * fourth-16th bit: size of tuple.
	 */

	unsigned short			t_info; /* various info about tuple */

#ifdef NOTDEF
        char            t_locktype;     /* type of rule lock representation*/
	IndexAttributeBitMapData	bits;	/* bitmap of domains */
#endif
} IndexTupleData;	/* MORE DATA FOLLOWS AT END OF STRUCT */

/*
 *  Warning: T_* defined also in tuple.h
 */

/* ----------------
 * "Special" attributes of index tuples...
 * NOTE: I used these big values so that there is no overlapping
 * with the HeapTuple system attributes.
 * ----------------
 */
#define IndxBaseTupleIdAttributeNumber	 	(-101)
#define IndxRuleLockAttributeNumber		(-102)

/* ----------------
 *	{,general,general retrieve} index insert result crap
 * ----------------
 */
typedef IndexTupleData	*IndexTuple;

typedef struct GeneralInsertIndexResultData {
	ItemPointerData	pointerData;
	RuleLock	lock;
} GeneralInsertIndexResultData;

typedef GeneralInsertIndexResultData
	*GeneralInsertIndexResult;	/* from AMinsert() */

typedef struct InsertIndexResultData {
	ItemPointerData	pointerData;
	RuleLock	lock;
	double		offset;
} InsertIndexResultData;

typedef InsertIndexResultData
	*InsertIndexResult;	/* from newinsert() */

typedef struct GeneralRetrieveIndexResultData {
	ItemPointerData	heapItemData;
} GeneralRetrieveIndexResultData;

typedef GeneralRetrieveIndexResultData	*GeneralRetrieveIndexResult;
				/* from AMgettuple() */

typedef struct RetrieveIndexResultData {
	ItemPointerData	indexItemData;
	ItemPointerData	heapItemData;
} RetrieveIndexResultData;

typedef RetrieveIndexResultData	*RetrieveIndexResult;
				/* from newgettuple() */

/* ----------------
 *	support macros
 * ----------------
 */
/*
 * IndexTupleIsValid --
 *	True iff index tuple is valid.
 */
#define	IndexTupleIsValid(tuple)			PointerIsValid(tuple)

/*
 * IndexTupleGetRuleLockItemPointer --
 *	Returns rule lock item pointer for an index tuple.
 *
 * Note:
 *	Assumes index tuple is a valid internal index tuple.
 */
#define IndexTupleGetRuleLockItemPointer(tuple) \
    (AssertMacro(IndexTupleIsValid(tuple)) ? \
     ((ItemPointer) (&(tuple)->t_lock.l_ltid)) : (ItemPointer) 0)

/*
 * IndexTupleGetRuleLock --
 *	Returns rule lock for an index tuple.
 *
 * Note:
 *	Assumes index tuple is a valid external index tuple.
 */
#define IndexTupleGetRuleLock(tuple) \
    (AssertMacro(IndexTupleIsValid(tuple)) ? \
     ((RuleLock) ((tuple)->t_lock.l_lock)) : (RuleLock) 0)

/*
 * IndexTupleGetIndexAttributeBitMap --
 *	Returns attribute bit map for an index tuple.
 *
 * Note:
 *	Assumes index tuple is valid.
 */
#define IndexTupleGetIndexAttributeBitMap(tuple) \
    (AssertMacro(IndexTupleIsValid(tuple)) ? \
     ((IndexAttributeBitMap) (&(tuple)->bits)) : (IndexAttributeBitMap) 0)

/*
 * IndexTupleGetForm --
 *	Returns formated data for an index tuple.
 *
 * Note:
 *	Assumes index tuple is valid.
 */
#define IndexTupleGetForm(tuple) \
    (AssertMacro(IndexTupleIsValid(tuple)) ? \
     ((Form) &(tuple)[1]) : (Form) 0)

/* ----------------
 *	soon to be obsolete index result stuff
 * ----------------
 */
/*
 * GeneralInsertIndexResultIsValid --
 *	True iff general index insertion result is valid.
 */
#define	GeneralInsertIndexResultIsValid(result)		PointerIsValid(result)

/*
 * InsertIndexResultIsValid --
 *	True iff (specific) index insertion result is valid.
 */
#define	InsertIndexResultIsValid(result)		PointerIsValid(result)

/*
 * GeneralRetrieveIndexResultIsValid --
 *	True iff general index retrieval result is valid.
 */
#define	GeneralRetrieveIndexResultIsValid(result)	PointerIsValid(result)

/*
 * RetrieveIndexResultIsValid --
 *	True iff (specific) index retrieval result is valid.
 */
#define	RetrieveIndexResultIsValid(result)		PointerIsValid(result)

/*
 * GeneralInsertIndexResultGetItemPointer --
 *	Returns heap tuple item pointer associated with a general index
 *	insertion result.
 *
 * Note:
 *	Assumes general index insertion result is valid.
 */
#define GeneralInsertIndexResultGetItemPointer(result) \
    (AssertMacro(GeneralInsertIndexResultIsValid(result)) ? \
     ((ItemPointer) (&(result)->pointerData)) : (ItemPointer) 0)

/*
 * GeneralInsertIndexResultGetRuleLock --
 *	Returns rule lock associated with a general index insertion result.
 *
 * Note:
 *	Assumes general index insertion result is valid.
 */
#define GeneralInsertIndexResultGetRuleLock(result) \
    (AssertMacro(GeneralInsertIndexResultIsValid(result)) ? \
     ((RuleLock) ((result)->lock)) : (RuleLock) 0)

/*
 * InsertIndexResultGetItemPointer --
 *	Returns heap tuple item pointer associated with a (specific) index
 *	insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
#define InsertIndexResultGetItemPointer(result) \
    (AssertMacro(InsertIndexResultIsValid(result)) ? \
     ((ItemPointer) (&(result)->pointerData)) | (ItemPointer) 0)

/*
 * InsertIndexResultGetRuleLock --
 *	Returns rule lock associated with a (specific) index insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
#define InsertIndexResultGetRuleLock(result) \
    (AssertMacro(InsertIndexResultIsValid(result)) ? \
     ((RuleLock) ((result)->lock)) : (RuleLock) 0)

/*
 * InsertIndexResultGetInsertOffset --
 *	Returns insertion offset for a (specific) index insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
#define InsertIndexResultGetInsertOffset(result) \
    (AssertMacro(InsertIndexResultIsValid(result)) ? \
     ((double) ((result)->offset)) : (double) 0)

/*
 * GeneralRetrieveIndexResultGetHeapItemPointer --
 *	Returns heap item pointer associated with a general index retrieval.
 *
 * Note:
 *	Assumes general index retrieval result is valid.
 */
#define GeneralRetrieveIndexResultGetHeapItemPointer(result) \
    (AssertMacro(GeneralRetrieveIndexResultIsValid(result)) ? \
     ((ItemPointer) (&(result)->heapItemData)) : (ItemPointer) 0)

/*
 * RetrieveIndexResultGetIndexItemPointer --
 *	Returns index item pointer associated with a (specific) index retrieval
 *
 * Note:
 *	Assumes (specific) index retrieval result is valid.
 */
#define RetrieveIndexResultGetIndexItemPointer(result) \
    (AssertMacro(RetrieveIndexResultIsValid(result)) ? \
     ((ItemPointer) (&(result)->indexItemData)) : (ItemPointer) 0)

/*
 * RetrieveIndexResultGetHeapItemPointer --
 *	Returns heap item pointer associated with a (specific) index retrieval.
 *
 * Note:
 *	Assumes (specific) index retrieval result is valid.
 */
#define RetrieveIndexResultGetHeapItemPointer(result) \
    (AssertMacro(RetrieveIndexResultIsValid(result)) ? \
     ((ItemPointer) (&(result)->heapItemData)) : (ItemPointer) 0)

/* ----------------
 *	externs 
 * ----------------
 */

/*
 * IndexTupleGetHeapTupleItemPointer --
 *	Returns heap tuple item pointer for an index tuple.
 *
 * Note:
 *	Assumes index tuple is valid.
 */
extern
ItemPointer
IndexTupleGetHeapTupleItemPointer ARGS((
	IndexTuple	tuple
));

/*
 * ItemPointerFormGeneralInsertIndexResult --
 *	Returns a general index insertion result.
 *
 * Note:
 *	Assumes item pointer is valid.
 *	Assumes rule lock is valid.
 */
extern
GeneralInsertIndexResult
ItemPointerFormGeneralInsertIndexResult ARGS((
	ItemPointer	itemPointer,
	RuleLock	lock,
));

/*
 * ItemPointerFormInsertIndexResult --
 *	Returns a (specific) index insertion result.
 *
 * Note:
 *	Assumes item pointer is valid.
 *	Assumes rule lock is valid.
 *	Assumes insertion offset is valid.
 */
extern
InsertIndexResult
ItemPointerFormInsertIndexResult ARGS((
	ItemPointer	itemPointer,
	RuleLock	lock,
	double		offset
));

/*
 * ItemPointerFormGeneralRetrieveIndexResult --
 *	Returns a (specific) index retrieval result.
 *
 * Note:
 *	Assumes item pointer is valid.
 */
extern
GeneralRetrieveIndexResult
ItemPointerFormGeneralRetrieveIndexResult ARGS((
	ItemPointer	heapItemPointer
));

/*
 * ItemPointerFormRetrieveIndexResult --
 *	Returns a general index retrieval result.
 *
 * Note:
 *	Assumes item pointers are valid.
 */
extern
RetrieveIndexResult
ItemPointerFormRetrieveIndexResult ARGS((
	ItemPointer	indexItemPointer,
	ItemPointer	heapItemPointer
));


#define INDEX_SIZE_MASK 0x1FFF
#define INDEX_NULL_MASK 0x8000
#define INDEX_VAR_MASK  0x4000
#define INDEX_RULE_MASK 0x2000

#define IndexTupleSize(itup)       (((IndexTuple) (itup))->t_info & 0x1FFF)
#define IndexTupleDSize(itup)                      ((itup).t_info & 0x1FFF)
#define IndexTupleNoNulls(itup)  (!(((IndexTuple) (itup))->t_info & 0x8000))
#define IndexTupleAllFixed(itup) (!(((IndexTuple) (itup))->t_info & 0x4000))
#define IndexTupleNoRule(itup)   (!(((IndexTuple) (itup))->t_info & 0x2000))
#define IndexTupleHasMinHeader(itup) (IndexTupleNoNulls(itup) \
								   && IndexTupleNoRule(itup))

extern Size IndexInfoFindDataOffset ARGS((
	 unsigned short t_info,
	 Attribute att
));

#endif
