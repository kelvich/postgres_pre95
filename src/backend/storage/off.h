
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
 * off.h --
 *	POSTGRES disk "offset" definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	OffIncluded	/* Include this file only once. */
#define OffIncluded	1

#include "c.h"

typedef uint16	OffsetNumber;	/* offset number */
typedef uint16	OffsetIndex;	/* offset index */

typedef int16		OffsetIdData;	/* internal offset identifier */
typedef OffsetIdData	*OffsetId;	/* offset identifier */

#define InvalidOffsetNumber	0
#define FirstOffsetIndex	0

/*
 * OffsetIndexIsValid --
 *	True iff the offset index is valid.
 */
extern
bool
OffsetIndexIsValid ARGS((
	OffsetIndex	offsetIndex
));

/*
 * OffsetNumberIsValid --
 *	True iff the offset number is valid.
 */
extern
bool
OffsetNumberIsValid ARGS((
	OffsetNumber	offsetNumber
));

/*
 * OffsetIdIsValid --
 *	True iff the offset identifier is valid.
 */
extern
bool
OffsetIdIsValid ARGS((
	OffsetId	offsetId
));

/*
 * OffsetIdSet --
 *	Sets a offset identifier to the specified value.
 */
extern
void
OffsetIdSet ARGS((
	OffsetId	offsetId,
	OffsetNumber	offsetNumber
));

/*
 * OffsetIdGetOffsetNumber --
 *	Retrieve the offset number from a offset identifier.
 */
extern
OffsetNumber
OffsetIdGetOffsetNumber ARGS((
	OffsetId	offsetId
));

#endif	/* !defined(OffIncluded) */
