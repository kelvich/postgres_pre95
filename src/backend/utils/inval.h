
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
 * inval.h --
 *	POSTGRES cache invalidation dispatcher definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	InvalIncluded	/* Include this file only once */
#define InvalIncluded	1

#include "c.h"

#include "htup.h"
#include "oid.h"
#include "rel.h"

/*
 * DiscardInvalid --
 *	Causes the invalidated cache state to be discarded.
 *
 * Note:
 *	This should be called as the first step in processing a transaction.
 *	This should be called while waiting for a query from the front end
 *	when other backends are active.
 */
extern
void
DiscardInvalid ARGS((
	void
));

/*
 * RegisterInvalid --
 *	Causes registration of invalidated state with other backends iff true.
 *
 * Note:
 *	This should be called as the last step in processing a transaction.
 */
extern
void
RegisterInvalid ARGS((
	bool	send
));

/*
 * SetRefreshWhenInvalidate --
 *	Causes the local caches to be immediately refreshed iff true.
 */
void
SetRefreshWhenInvalidate ARGS((
	bool	on
));

/*
 * RelationIdInvalidateHeapTuple --
 *	Causes the given tuple in a relation to be invalidated.
 *
 * Note:
 *	Assumes object id is valid.
 *	Assumes tuple is valid.
 */
extern
void
RelationIdInvalidateHeapTuple ARGS((
	ObjectId	relationId,
	HeapTuple	tuple
));

#endif	/* !defined(InvalIncluded) */
