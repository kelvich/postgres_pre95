/*
 * item.h --
 *	POSTGRES disk item definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ItemIncluded	/* Include this file only once. */
#define ItemIncluded	1

#include "tmp/c.h"
#include "access/htup.h"

typedef Pointer	Item;

typedef uint16	ItemIndex;
typedef uint16	ItemNumber;

/*
 * ItemIsValid
 *	True iff the block number is valid.
 */
#define	ItemIsValid(item) PointerIsValid(item)

#endif	/* !defined(ItemIncluded) */
