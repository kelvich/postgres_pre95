/*
 * ibit.h --
 *	POSTGRES index valid attribute bit map definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IBitIncluded	/* Include this file only once. */
#define IBitIncluded	1

#include "c.h"

#include "limit.h"

typedef struct IndexAttributeBitMapData {
	char	bits[(MaxIndexAttributeNumber + MaxBitsPerByte - 1)
		/ MaxBitsPerByte];
} IndexAttributeBitMapData;

typedef IndexAttributeBitMapData	*IndexAttributeBitMap;

#define IndexAttributeBitMapSize	sizeof(IndexAttributeBitMapData)

/*
 * IndexAttributeBitMapIsValid --
 *	True iff attribute bit map is valid.
 */
extern
bool
IndexAttributeBitMapIsValid ARGS((
	IndexAttributeBitMap	bits
));

#endif	/* !defined(IBitIncluded) */
