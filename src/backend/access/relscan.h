/*
 * relscan.h --
 *	POSTGRES internal relation scan descriptor definitions.
 */

#ifndef	RelScanIncluded		/* Include this file only once */
#define RelScanIncluded	1

/*
 * Identification:
 */
#define RELSCAN_H	"$Header$"

#include "tmp/c.h"

#include "access/skey.h"
#include "storage/buf.h"
#include "access/htup.h"
#include "storage/itemptr.h"
#include "access/tqual.h"
#include "utils/rel.h"

typedef ItemPointerData	MarkData;

typedef struct HeapScanDescData {
	Relation	rs_rd;		/* pointer to relation descriptor */
	HeapTuple	rs_ptup;	/* previous tuple in scan */
	HeapTuple	rs_ctup;	/* current tuple in scan */
	HeapTuple	rs_ntup;	/* next tuple in scan */
	Buffer		rs_pbuf;	/* previous buffer in scan */
	Buffer		rs_cbuf;	/* current buffer in scan */
	Buffer		rs_nbuf;	/* next buffer in scan */
	struct	dchain	*rs_dc;		/* cached expanded delta chain */
	ItemPointerData	rs_mptid;	/* marked previous tid */
	ItemPointerData	rs_mctid;	/* marked current tid */
	ItemPointerData	rs_mntid;	/* marked next tid */
	ItemPointerData	rs_mcd;		/* marked current delta XXX ??? */
	Boolean		rs_atend;	/* restart scan at end? */
	TimeQual	rs_tr;		/* time qualification */
	uint16		rs_cdelta;	/* current delta in chain */
	bool		rs_parallel_ok; /* ok to parallelize? */
	uint16		rs_nkeys;	/* number of attributes in keys */
	ScanKeyData	rs_key;		/* key descriptors */
	/* VARIABLE LENGTH ARRAY AT END OF STRUCT */
} HeapScanDescData;

typedef HeapScanDescData *HeapScanDesc;

typedef struct IndexScanDescData {
	Relation	relation;		/* relation descriptor */
	Pointer		opaque;			/* am-specific slot */
	ItemPointerData	previousItemData;	/* previous index pointer */
	ItemPointerData	currentItemData;	/* current index pointer */
	ItemPointerData	nextItemData;		/* next index pointer */
	MarkData	previousMarkData;	/* marked previous pointer */
	MarkData	currentMarkData;	/* marked current  pointer */
	MarkData	nextMarkData;		/* marked next pointer */
	uint8		flags;			/* scan position flags */
	Boolean		scanFromEnd;		/* restart scan at end? */
	uint16		numberOfKeys;		/* number of key attributes */
	ScanKeyData	keyData;			/* key descriptor */
	/* VARIABLE LENGTH ARRAY AT END OF STRUCT */
} IndexScanDescData;

typedef IndexScanDescData	*IndexScanDesc;

/* ----------------
 *	IndexScanDescPtr is used in the executor where we have to
 *	keep track of several index scans when using several indices
 *	- cim 9/10/89
 * ----------------
 */
typedef IndexScanDesc		*IndexScanDescPtr;

/*
 * HeapScanIsValid --
 *	True iff the heap scan is valid.
 */
#define	HeapScanIsValid(scan) PointerIsValid(scan)

/*
 * IndexScanIsValid --
 *	True iff the index scan is valid.
 */
#define IndexScanIsValid(scan) PointerIsValid(scan)

#endif	/* !defined(RelScanIncluded) */
