/*
 * item.h --
 *	POSTGRES disk item definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ItemIncluded	/* Include this file only once. */
#define ItemIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "htup.h"

/*
typedef union ItemData {
	HeapTupleData	tuple;
} ItemData;
*/

/*
typedef ItemData	*Item;
*/

typedef Pointer	Item;

typedef uint16	ItemIndex;
typedef uint16	ItemNumber;

/*
 * ItemIsValid
 *	True iff the block number is valid.
 */
extern
bool
ItemIsValid ARGS((
	Item	item
));

#endif	/* !defined(ItemIncluded) */
