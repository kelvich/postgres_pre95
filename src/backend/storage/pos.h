
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
 * pos.h --
 *	POSTGRES "position" definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PosIncluded	/* Include this file only once. */
#define PosIncluded	1

#include "c.h"

#include "part.h"
#include "pagenum.h"

typedef bits16	PositionIdData;	/* internal position identifier */
typedef PositionIdData	*PositionId;	/* position identifier */

/*
 * PositionIdIsValid --
 *	True iff the position identifier is valid.
 */
extern
bool
PositionIdIsValid ARGS((
	PositionId	positionId,
	PagePartition	partition
));

/*
 * PositionIdSet --
 *	Sets a position identifier to the specified value.
 */
extern
void
PositionIdSet ARGS((
	PositionId	positionId,
	PagePartition	partition,
	PageNumber	pageNumber,
	OffsetNumber	offsetNumber
));

/*
 * PositionIdGetPageNumber --
 *	Retrieve the page number from a position identifier.
 */
extern
PageNumber
PositionIdGetPageNumber ARGS((
	PositionId	positionId,
	PagePartition	partition
));

/*
 * PositionIdGetOffsetNumber --
 *	Retrieve the offset number from a position identifier.
 */
extern
OffsetNumber
PositionIdGetOffsetNumber ARGS((
	PositionId	positionId,
	PagePartition	partition
));

#endif	/* !defined(PosIncluded) */
