
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * itup.h --
 *	POSTGRES index tuple definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ITupIncluded	/* Include this file only once. */
#define ITupIncluded	1

#include "c.h"

#include "form.h"
#include "ibit.h"
#include "itemptr.h"
#include "rlock.h"

#define MaxIndexAttributeNumber	7

typedef struct IndexTupleData {
	union {
		ItemPointerData	l_ltid;	/* TID of the lock */
		RuleLock	l_lock;		/* internal lock format */
	}	t_lock;
	ItemPointerData			t_tid;	/* reference TID to base tuple */
	IndexAttributeBitMapData	bits;	/* bitmap of domains */
} IndexTupleData;	/* MORE DATA FOLLOWS AT END OF STRUCT */

#ifndef	T_CTID
#define T_CTID	(-1)	/* remove me */
#define T_LOCK	(-2)	/* -1 */
#define T_TID	(-3)	/* -2 */
#endif	/* !defined(T_CTID) */

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

/*
 * IndexTupleIsValid --
 *	True iff index tuple is valid.
 */
extern
bool
IndexTupleIsValid ARGS((
	IndexTuple	tuple
));

/*
 * GeneralInsertIndexResultIsValid --
 *	True iff general index insertion result is valid.
 */
extern
bool
GeneralInsertIndexResultIsValid ARGS((
	GeneralInsertIndexResult	result
));

/*
 * InsertIndexResultIsValid --
 *	True iff (specific) index insertion result is valid.
 */
extern
bool
InsertIndexResultIsValid ARGS((
	InsertIndexResult	result
));

/*
 * GeneralRetrieveIndexResultIsValid --
 *	True iff general index retrieval result is valid.
 */
extern
bool
GeneralRetrieveIndexResultIsValid ARGS((
	GeneralRetrieveIndexResult	result
));

/*
 * RetrieveIndexResultIsValid --
 *	True iff (specific) index retrieval result is valid.
 */
extern
bool
RetrieveIndexResultIsValid ARGS((
	RetrieveIndexResult	result
));

/*
 * IndexTupleGetRuleLockItemPointer --
 *	Returns rule lock item pointer for an index tuple.
 *
 * Note:
 *	Assumes index tuple is a valid internal index tuple.
 */
extern
ItemPointer
IndexTupleGetRuleLockItemPointer ARGS((
	IndexTuple	tuple
));

/*
 * IndexTupleGetRuleLock --
 *	Returns rule lock for an index tuple.
 *
 * Note:
 *	Assumes index tuple is a valid external index tuple.
 */
extern
RuleLock
IndexTupleGetRuleLock ARGS((
	IndexTuple	tuple
));

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
 * IndexTupleGetIndexAttributeBitMap --
 *	Returns attribute bit map for an index tuple.
 *
 * Note:
 *	Assumes index tuple is valid.
 */
extern
IndexAttributeBitMap
IndexTupleGetIndexAttributeBitMap ARGS((
	IndexTuple	tuple
));

/*
 * IndexTupleGetForm --
 *	Returns formated data for an index tuple.
 *
 * Note:
 *	Assumes index tuple is valid.
 */
extern
Form
IndexTupleGetForm ARGS((
	IndexTuple	tuple
));

/*
 * GeneralInsertIndexResultGetItemPointer --
 *	Returns heap tuple item pointer associated with a general index
 *	insertion result.
 *
 * Note:
 *	Assumes general index insertion result is valid.
 */
extern
ItemPointer
GeneralInsertIndexResultGetItemPointer ARGS((
	GeneralInsertIndexResult	result
));

/*
 * GeneralInsertIndexResultGetRuleLock --
 *	Returns rule lock associated with a general index insertion result.
 *
 * Note:
 *	Assumes general index insertion result is valid.
 */
extern
RuleLock
GeneralInsertIndexResultGetRuleLock ARGS((
	GeneralInsertIndexResult	result
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
 * InsertIndexResultGetItemPointer --
 *	Returns heap tuple item pointer associated with a (specific) index
 *	insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
extern
ItemPointer
InsertIndexResultGetItemPointer ARGS((
	InsertIndexResult	result
));

/*
 * InsertIndexResultGetItemPointer --
 *	Returns rule lock associated with a (specific) index insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
extern
RuleLock
InsertIndexResultGetRuleLock ARGS((
	InsertIndexResult	result
));

/*
 * InsertIndexResultGetInsertOffset --
 *	Returns insertion offset for a (specific) index insertion result.
 *
 * Note:
 *	Assumes (specific) index insertion result is valid.
 */
extern
double	/* XXX invalid POSTGRES ADT type */
InsertIndexResultGetInsertOffset ARGS((
	InsertIndexResult	result
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
 * GeneralRetrieveIndexResultGetHeapItemPointer --
 *	Returns heap item pointer associated with a general index retrieval.
 *
 * Note:
 *	Assumes general index retrieval result is valid.
 */
extern
ItemPointer
GeneralRetrieveIndexResultGetHeapItemPointer ARGS((
	GeneralRetrieveIndexResult	result
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
 * RetrieveIndexResultGetIndexItemPointer --
 *	Returns index item pointer associated with a (specific) index retrieval.
 *
 * Note:
 *	Assumes (specific) index retrieval result is valid.
 */
extern
ItemPointer
RetrieveIndexResultGetIndexItemPointer ARGS((
	RetrieveIndexResult	result
));

/*
 * RetrieveIndexResultGetHeapItemPointer --
 *	Returns heap item pointer associated with a (specific) index retrieval.
 *
 * Note:
 *	Assumes (specific) index retrieval result is valid.
 */
extern
ItemPointer
RetrieveIndexResultGetHeapItemPointer ARGS((
	RetrieveIndexResult	result
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

#endif	/* !defined(ITupIncluded) */
