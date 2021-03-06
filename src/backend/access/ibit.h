/*
 * ibit.h --
 *	POSTGRES index valid attribute bit map definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IBitIncluded	/* Include this file only once. */
#define IBitIncluded	1

#include "tmp/c.h"
#include "utils/memutils.h"

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
#define	IndexAttributeBitMapIsValid(bits) PointerIsValid(bits)

#endif	/* !defined(IBitIncluded) */
