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

typedef struct HeapTupleData {
	Size		t_len;		/* length of entire tuple */
	ItemPointerData	t_ctid;		/* current TID of this tuple */
	char		t_locktype;	/* type of rule lock representation*/
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

/* ----------------------------------------------------------------
 *			old stuff from tuple.h
 * ----------------------------------------------------------------
 */

/* check these, they are likely to be more severely limited by t_hoff */
#define	MAXIATTS 8
#define	MINATTS	32		/* 8 * 4 */
#ifdef	vax
#define	MAXATTS	1584		/* 8 * 198 */
#else
#define	MAXATTS	1600		/* 8 * 200 */
#endif

struct	tuple	{
	uint32		t_len;		/* length of entire tuple */
	ItemPointerData	t_ctid;		/* current TID of this tuple */
	char		t_locktype;	/* type of rule lock representation*/
	union {
		ItemPointerData	l_ltid;		/* TID of the lock */
		RuleLock	l_lock;		/* internal lock format */
	}		t_lock;
	OID		t_oid;		/* OID of this tuple */
	XID		t_xmin;		/* insert XID stamp */
	CID		t_cmin;		/* insert CID stamp */
	XID		t_xmax;		/* delete XID stamp */
	CID		t_cmax;		/* delete CID stamp */
	ItemPointerData	t_chain;	/* replaced tuple TID */
	ItemPointerData	t_anchor;	/* anchor point TID */
	ABSTIME		t_tmin, t_tmax;	/* time stamps */
	uint16		t_natts;	/* number of attributes */
	uint8		t_hoff;		/* sizeof tuple header */
	char		t_vtype;	/* `d', `i', `r' */
	char		t_bits[MINATTS / 8];
					/* bit map of domains */
};	/* MORE DATA FOLLOWS AT END OF STRUCT */

struct	ituple {
	ItemPointerData	t_ctid;		/* current TID of this tuple */
	union {
		ItemPointerData	l_ltid;		/* TID of the lock */
		RuleLock	l_lock;		/* internal lock format */
	}		t_lock;
	ItemPointerData	t_tid;		/* reference TID to base tuple */
	char		t_bits[MAXIATTS / 8];	/* bitmap of domains */
};	/* MORE DATA FOLLOWS AT END OF STRUCT */

typedef	union {
	struct	tuple	tp_t;
	struct	ituple	tp_it;
	char		tp_ot[1];
} TUPLE;

/*	attnum codes for the internal tuple fields	*/

#ifndef T_CTID
#define	T_CTID	(-1)
#define	T_LOCK	(-2)
#define	T_TID	(-3)
#endif
#define	T_OID	(-3)
#define	T_XMIN	(-4)
#define	T_CMIN	(-5)
#define	T_XMAX	(-6)
#define	T_CMAX	(-7)
#define	T_CHAIN	(-8)
#define	T_ANCHOR	(-9)
#define	T_TMIN	(-10)
#define	T_TMAX	(-11)
#define	T_VTYPE	(-12)
#define	T_HLOW	(T_VTYPE - 1)

#endif	/* !defined(HTupIncluded) */
