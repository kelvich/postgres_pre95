
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
 * htup.h --
 *	POSTGRES heap tuple definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	HTupIncluded	/* Include this file only once. */
#define HTupIncluded	1

#include "postgres.h"

#include "c.h"

#include "attnum.h"
#include "form.h"
#include "itemptr.h"
#include "oid.h"
#include "tupsiz.h"
#include "rlock.h"

#define MinHeapTupleBitmapSize	32		/* 8 * 4 */

/* check these, they are likely to be more severely limited by t_hoff */

#ifdef  vax
#define MaxHeapAttributeNumber	1584		/* 8 * 198 */
#else
#define MaxHeapAttributeNumber	1600		/* 8 * 200 */
#endif

typedef struct HeapTupleData {
	TupleSize	t_len;		/* length of entire tuple */
	ItemPointerData	t_ctid;		/* current TID of this tuple */
	union {
		ItemPointerData	l_ltid;	/* TID of the lock */
		RuleLock	l_lock;	/* internal lock format */
	}		t_lock;
	ObjectId	t_oid;		/* OID of this tuple */
	XID		t_xmin;		/* insert XID stamp */
	CID		t_cmin;		/* insert CID stamp */
	XID		t_xmax;		/* delete XID stamp */
	CID		t_cmax;		/* delete CID stamp */
	ItemPointerData	t_chain;	/* replaced tuple TID */
	ItemPointerData	t_anchor;	/* anchor point TID */
	ABSTIME		t_tmin, t_tmax;	/* time stamps */
	AttributeNumber	t_natts;	/* number of attributes */
	uint8		t_hoff;		/* sizeof tuple header */
	char		t_vtype;	/* `d', `i', `r' */
	char		t_bits[MinHeapTupleBitmapSize / 8];
					/* bit map of domains */
} HeapTupleData;	/* MORE DATA FOLLOWS AT END OF STRUCT */

typedef HeapTupleData	*HeapTuple;

/* three files? */

#ifndef	RuleLockAttributeNumber
#define SelfItemPointerAttributeNumber		(-1)
#define RuleLockAttributeNumber			(-2)
#define ObjectIdAttributeNumber			(-3)
#define MinTransactionIdAttributeNumber		(-4)
#define MinCommandIdAttributeNumber		(-5)
#define MaxTransactionIdAttributeNumber		(-6)
#define MaxCommandIdAttributeNumber		(-7)
#define ChainItemPointerAttributeNumber		(-8)
#define AnchorItemPointerAttributeNumber	(-9)
#define MinAbsoluteTimeAttributeNumber		(-10)
#define MaxAbsoluteTimeAttributeNumber		(-11)
#define VersionTypeAttributeNumber		(-12)
#define FirstLowInvalidHeapAttributeNumber	(-13)
#endif	/* !defined(RuleLockAttributeNumber) */

#ifndef	GETSTRUCT
#define GETSTRUCT(TUP)	(((char *)(TUP)) + ((HeapTuple)(TUP))->t_hoff)

/*
 *	BITMAPLEN(NATTS)	- compute size of aligned bitmap
 *	u_int2	NATTS;
 *
 *	Computes minimum size of bitmap given number of domains.
 */
#define BITMAPLEN(NATTS)\
	((((((int)NATTS - 1) >> 3) + 4 - (MinHeapTupleBitmapSize >> 3)) & ~03) + (MinHeapTupleBitmapSize >> 3))

/*
 *	USEDBITMAPLEN(TUP)	- compute length of bitmap used
 *	struct	tuple	*TUP;
 */
#define USEDBITMAPLEN(TUP)\
	(1 + (((TUP)->t_natts - 1) >> 3))
#endif

/*
 * HeapTupleIsValid
 *	True iff the heap tuple is valid.
 */
extern
bool
HeapTupleIsValid ARGS((
	HeapTuple	tuple
));

/*
 * HeapTupleGetForm
 *	Returns the formated user attribute values of a heap tuple.
 */
extern
Form
HeapTupleGetForm ARGS((
	HeapTuple	tuple
));

#endif	/* !defined(HTupIncluded) */
