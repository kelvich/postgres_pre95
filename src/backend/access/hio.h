
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
 * hio.h --
 *	POSTGRES heap access method input/output definitions.
 *
 * Note:
 *	XXX This file should be moved to heap/.
 *
 * Identification:
 *	$Header$
 */

#ifndef	HIOIncluded	/* Include this file only once */
#define HIOIncluded	1

#include "c.h"

#include "block.h"
#include "htup.h"
#include "rel.h"

/*
 * RelationPutHeapTuple --
 *	Places a heap tuple in a specified disk block.
 *
 * Note:
 *	Assumes relation is valid.
 *	Assumes tuple is valid.
 *	Assumes block number is valid.
 *	Assumes tuple will fit in the disk block.
 */
extern
void
RelationPutHeapTuple ARGS((
	Relation	relation,
	BlockNumber	blockIndex,
	HeapTuple	tuple
));

/*
 * RelationPutLongHeapTuple --
 *	Places a long heap tuple in a relation.
 *
 * Note:
 *	Assumes relation is valid.
 *	Assumes tuple is valid.
 *	Assumes block number is valid.
 *	Assumes tuple will not fit into a disk block.
 */
extern
void
RelationPutLongHeapTuple ARGS((
	Relation	relation,
	HeapTuple	tuple
));

#endif	/* !defined(HIOIncluded) */
