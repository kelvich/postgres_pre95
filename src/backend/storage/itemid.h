/*
 * itemid.h --
 *	Standard POSTGRES buffer page item identifier definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ItemIdIncluded	/* Include this file only once. */
#define ItemIdIncluded	1

#ifndef C_H
#include "c.h"
#endif

typedef uint16	ItemOffset;
typedef uint16	ItemLength;

typedef bits16	ItemIdFlags;

typedef struct ItemIdData {		/* line pointers */
	unsigned	lp_off:13,	/* offset to find tup */
					/* can be reduced by 2 if necc. */
			lp_flags:6,	/* flags on tuple */
			lp_len:13;	/* length of tuple */
} ItemIdData;

typedef struct ItemIdData	*ItemId;

#ifndef	LP_USED
#define LP_USED		0x01	/* this line pointer is being used */
#define LP_IVALID	0x02	/* this tuple is known to be insert valid */
#define LP_DOCNT	0x04	/* this tuple continues on another page */
#define LP_CTUP		0x08	/* this is a continuation tuple */
#define LP_LOCK		0x10	/* this is a lock */
#define LP_ISINDEX	0x20	/* this is an internal index tuple */
#endif

/* ----------------------------------------------------------------
 *	ItemIdGetLength
 * ----------------------------------------------------------------
 */

#define ItemIdGetLength(itemId) \
   ((itemId)->lp_len)

/* ----------------------------------------------------------------
 *	ItemIdGetOffset
 * ----------------------------------------------------------------
 */

#define ItemIdGetOffset(itemId) \
   ((itemId)->lp_off)

/* ----------------------------------------------------------------
 *	ItemIdGetLength
 * ----------------------------------------------------------------
 */

#define ItemIdGetFlags(itemId) \
   ((itemId)->lp_flags)

/* ----------------------------------------------------------------
 *	entern declarations
 * ----------------------------------------------------------------
 */

/*
 * ItemIdIsValid --
 *	True iff disk item identifier is valid.
 */
extern
bool
ItemIdIsValid ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsUsed --
 *	True iff disk item identifier is in use.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsUsed ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsInsertValid --
 *	True iff disk item identifier is inserted validly.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsInsertValid ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsContinuing --
 *	True iff disk item identifier continues.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsContinuing ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsContinuation --
 *	True iff disk item identifier is a continuation.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsContinuation ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsLock --
 *	True iff disk item identifier is a lock.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsLock ARGS((
	ItemId	itemId;
));

/*
 * ItemIdIsInternal --
 *	True iff disk item identifier is an internal index tuple.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
extern
bool
ItemIdIsInternal ARGS((
	ItemId	itemId;
));

#endif	/* !defined(ItemIdIncluded) */
