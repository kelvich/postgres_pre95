
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
 * sdir.h --
 *	POSTGRES scan direction definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SDirIncluded	/* Include this file only once. */
#define SDirIncluded	1

#include "c.h"

typedef int8	ScanDirection;

#define BackwardScanDirection	(-1)
#define NoMovementScanDirection	0
#define ForwardScanDirection	1

/*
 * ScanDirectionIsValid --
 *	True iff scan direciton is valid.
 */
extern
bool
ScanDirectionIsValid ARGS((
	ScanDirection	direction
));

/*
 * ScanDirectionIsBackward --
 *	True iff scan direciton is backward.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
extern
bool
ScanDirectionIsBackward ARGS((
	ScanDirection	direction
));

/*
 * ScanDirectionIsNoMovement --
 *	True iff scan direciton indicates no movement.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
extern
bool
ScanDirectionIsNoMovement ARGS((
	ScanDirection	direction
));

/*
 * ScanDirectionIsForward --
 *	True iff scan direciton is forward.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
extern
bool
ScanDirectionIsForward ARGS((
	ScanDirection	direction
));

#endif	/* !defined(SDirIncluded) */
