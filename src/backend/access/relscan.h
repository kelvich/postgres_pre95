
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
 * relscan.h --
 *	POSTGRES internal relation scan descriptor definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RelScanIncluded	/* Include this file only once. */
#define RelScanIncluded	1

#include "c.h"

#include "buf.h"
#include "htup.h"
#include "itemptr.h"
#include "skey.h"
#include "trange.h"
#include "rel.h"

typedef ItemPointerData	MarkData;

typedef struct HeapScanData {
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
	TimeRange	rs_tr;		/* time range */
	uint16		rs_cdelta;	/* current delta in chain */
	uint16		rs_nkeys;	/* number of attributes in keys */
	ScanKeyData	rs_key;		/* key descriptors */
	/* VARIABLE LENGTH ARRAY AT END OF STRUCT */
} HeapScanData;

typedef HeapScanData		*HeapScan;

typedef struct IndexScanData {
	Relation	relation;		/* relation descriptor */
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
} IndexScanData;

typedef IndexScanData	*IndexScan;

/*
 * HeapScanIsValid --
 *	True iff the heap scan is valid.
 */
extern
bool
HeapScanIsValid ARGS ((
	HeapScan	scan
));

/*
 * IndexScanIsValid --
 *	True iff the index scan is valid.
 */
extern
bool
IndexScanIsValid ARGS ((
	IndexScan	scan
));

#endif	/* !defined(RelScanIncluded) */
