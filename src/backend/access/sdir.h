/*
 * sdir.h --
 *	POSTGRES scan direction definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SDirIncluded	/* Include this file only once. */
#define SDirIncluded	1

#include "tmp/c.h"

typedef int8	ScanDirection;

#define BackwardScanDirection	(-1)
#define NoMovementScanDirection	0
#define ForwardScanDirection	1

/*
 * ScanDirectionIsValid --
 *	True iff scan direciton is valid.
 */
#define ScanDirectionIsValid(direction) \
    ((bool) (AsInt8(BackwardScanDirection) <= AsInt8(direction) && \
	     AsInt8(direction) <= AsInt8(ForwardScanDirection)))

/*
 * ScanDirectionIsBackward --
 *	True iff scan direciton is backward.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
#define ScanDirectionIsBackward(direction) \
    ((bool) (AsInt8(direction) == AsInt8(BackwardScanDirection)))

/*
 * ScanDirectionIsNoMovement --
 *	True iff scan direciton indicates no movement.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
#define ScanDirectionIsNoMovement(direction) \
    ((bool) (AsInt8(direction) == AsInt8(NoMovementScanDirection)))

/*
 * ScanDirectionIsForward --
 *	True iff scan direciton is forward.
 *
 * Note:
 *	Assumes scan direction is valid.
 */
#define ScanDirectionIsForward(direction) \
    ((bool) (AsInt8(direction) == AsInt8(ForwardScanDirection)))

#endif	/* !defined(SDirIncluded) */
