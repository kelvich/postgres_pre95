/* ----------------------------------------------------------------
 *   FILE
 *	itemid.h
 *
 *   DESCRIPTION
 *	Standard POSTGRES buffer page item identifier definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	ItemIdIncluded		/* Include this file only once */
#define ItemIdIncluded	1

#define ITEMID_H	"$Header$"

#include "tmp/c.h"

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

/* ----------------
 *	support macros
 * ----------------
 */
/* 
 *	ItemIdGetLength
 */
#define ItemIdGetLength(itemId) \
   ((itemId)->lp_len)

/* 
 *	ItemIdGetOffset
 */
#define ItemIdGetOffset(itemId) \
   ((itemId)->lp_off)

/* 
 *	ItemIdGetFlags
 */
#define ItemIdGetFlags(itemId) \
   ((itemId)->lp_flags)

/*
 * ItemIdIsValid --
 *	True iff disk item identifier is valid.
 */
#define	ItemIdIsValid(itemId)	PointerIsValid(itemId)

/*
 * ItemIdIsUsed --
 *	True iff disk item identifier is in use.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
#define ItemIdIsUsed(itemId) \
    (AssertMacro(ItemIdIsValid(itemId)) ? \
     (bool) (((itemId)->lp_flags & LP_USED) != 0) : false)

/*
 * ItemIdIsContinuation --
 *	True iff disk item identifier is a continuation.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
#define ItemIdIsContinuation(itemId) \
    (AssertMacro(ItemIdIsValid(itemId)) ? \
     ((bool) (((itemId)->lp_flags & LP_CTUP) != 0)) : false)

/*
 * ItemIdIsContinuing --
 *	True iff disk item identifier continues.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
#define ItemIdIsContinuing(itemId) \
    (AssertMacro(ItemIdIsValid(itemId)) ? \
     ((bool) (((itemId)->lp_flags & LP_DOCNT) != 0)) : false)

/*
 * ItemIdIsLock --
 *	True iff disk item identifier is a lock.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
#define ItemIdIsLock(itemId) \
    (AssertMacro(ItemIdIsValid(itemId)) ? \
     ((bool) (((itemId)->lp_flags & LP_LOCK) != 0)) : false)

/*
 * ItemIdIsInternal --
 *	True iff disk item identifier is an internal index tuple.
 *
 * Note:
 *	Assumes disk item identifier is valid.
 */
#define ItemIdIsInternal(itemId) \
    (AssertMacro(ItemIdIsValid(itemId)) ? \
     ((bool) (((itemId)->lp_flags & LP_ISINDEX) != 0)) : false)


#endif	/* !defined(ItemIdIncluded) */
