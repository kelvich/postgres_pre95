/*
 * htup.h --
 *	POSTGRES heap tuple definitions.
 */

#ifndef	HTupIncluded		/* Include this file only once */
#define HTupIncluded	1

/*
 * Identification:
 */
#define HTUP_H	"$Header$"

#include "tmp/postgres.h"	/* XXX obsolete, for XID, etc. */

#include "access/attnum.h"
#include "storage/form.h"
#include "storage/page.h"  		/* just to reduce levels of #include */
#include "storage/part.h"  		/* just to reduce levels of #include */
#include "storage/itemptr.h"
#include "rules/rlock.h"

#define MinHeapTupleBitmapSize	32		/* 8 * 4 */

/* check these, they are likely to be more severely limited by t_hoff */

#ifdef  vax
#define MaxHeapAttributeNumber	1584		/* 8 * 198 */
#else
#define MaxHeapAttributeNumber	1600		/* 8 * 200 */
#endif

/*-----------------------------------------------------------
 * NOTE:
 * A rule lock has 2 different representations:
 *    The disk representation (t_lock.l_ltid) is an ItemPointer
 * to the actual rule lock data (stored somewhere else in the disk page).
 * In this case `t_locktype' has the value DISK_RULE_LOCK.
 *    The main memory representation (t_lock.l_lock) is a pointer
 * (RuleLock) to a (palloced) structure. In this case `t_locktype' has
 * the value MEM_RULE_LOCK.
 */

#define DISK_RULE_LOCK	'd'
#define MEM_RULE_LOCK	'm'

/*
 * to avoid wasting space, the attributes should be layed out in such a
 * way to reduce structure padding.
 */

typedef struct HeapTupleData {

    /* size is unsigned int */

	Size		t_len;		/* length of entire tuple */

	/* this is six bytes long */

	/* ItemPointerData	t_anchor;*/	/* anchor point TID */

	/* keep these chars here as padding */


	ItemPointerData	t_ctid;		/* current TID of this tuple */

	/* keep these here as padding */


	ItemPointerData	t_chain;	/* replaced tuple TID */

	/* keep these here as padding */

	union {
		ItemPointerData	l_ltid;	/* TID of the lock */
		RuleLock	l_lock;	/* internal lock format */
	}		t_lock;

	/* four bytes long */

	ObjectId	t_oid;		/* OID of this tuple */

	/* char[5] each */

	XID		t_xmin;		/* insert XID stamp */
	XID		t_xmax;		/* delete XID stamp */

	/* one byte each */

	CID		t_cmin;		/* insert CID stamp */
	CID		t_cmax;		/* delete CID stamp */

	/* four bytes each */

	ABSTIME		t_tmin, t_tmax;	/* time stamps */

	AttributeNumber	t_natts;	/* number of attributes */

    /* one byte each */

	char		t_locktype;	/* type of rule lock representation*/
	uint8		t_hoff;		/* sizeof tuple header */
	char		t_vtype;	/* `d', `i', `r' */
	char		t_infomask; /* used for various stuff in getattr() */

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

/* ----------------
 *	support macros
 * ----------------
 */
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
#define	HeapTupleIsValid(tuple)	PointerIsValid(tuple)

/*
 * HeapTupleGetForm
 *	Returns the formated user attribute values of a heap tuple.
 */
#define HeapTupleGetForm(tuple) \
    ((Form) ((char *)(tuple) + (tuple)->t_hoff))

/* Various information about the tuple used by getattr() */

#define HeapTupleNoNulls(tuple) (!(((HeapTuple) (tuple))->t_infomask & 0x01))
#define HeapTupleAllFixed(tuple) (!(((HeapTuple) (tuple))->t_infomask & 0x02))

#define HeapTupleSetNull(tuple) (((HeapTuple) (tuple))->t_infomask |= 0x01)
#define HeapTupleSetVarlena(tuple) (((HeapTuple) (tuple))->t_infomask |= 0x02)

#endif	/* !defined(HTupIncluded) */
