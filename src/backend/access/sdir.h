/*
 * sdir.h --
 *	POSTGRES scan direction definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SDirIncluded	/* Include this file only once. */
#define SDirIncluded	1

#ifndef C_H
#include "c.h"
#endif

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
