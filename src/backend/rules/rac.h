
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
 * rac.h --
 *	POSTGRES rule lock access definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RAcIncluded	/* Include this file only once. */
#define RAcIncluded	1

#include "c.h"

#include "block.h"
#include "buf.h"
#include "htup.h"
#include "rel.h"
#include "rlock.h"

/*
 * HeapTupleGetRuleLock --
 *	Returns the rule lock for a heap tuple or InvalidRuleLock if
 *	the rule lock is NULL.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes buffer is invalid or associated with the heap tuple.
 */
extern
RuleLock
HeapTupleGetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer
));

/*
 * HeapTupleSetRuleLock --
 *	Sets the rule lock for a heap tuple.
 *
 * Note:
 *	Assumes heap tuple is valid.
 */
extern
HeapTuple
HeapTupleSetRuleLock ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	RuleLock	lock
));

/*
 * HeapTupleStoreRuleLock --
 *	Stores the rule lock of a heap tuple into a heap relation.  If
 *	the block number is valid, the lock will be placed on the buffer
 *	frame with this number if possible.
 *
 * Note:
 *	Assumes heap tuple is valid.
 *	Assumes buffer is valid.
 */
extern
void
HeapTupleStoreRuleLock ARGS((
	HeapTuple	tuple;
	Buffer		buffer;
	RuleLock	lock;
));

#endif	/* !defined(RAcIncluded) */
