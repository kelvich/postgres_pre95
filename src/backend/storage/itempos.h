/*
 * itempos.h --
 *	Standard POSTGRES buffer page long item subposition definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ItemPosIncluded	/* Include this file only once. */
#define ItemPosIncluded	1

#include "tmp/c.h"
#include "storage/buf.h"
#include "storage/itemid.h"

typedef struct ItemSubpositionData {
	Buffer		op_db;
	ItemId		op_lpp;
	char		*op_cp;		/* XXX */
	uint32		op_len;
} ItemSubpositionData;

typedef ItemSubpositionData	*ItemSubposition;

/*
 *	PNOBREAK(OBJP, LEN)
 *	struct	objpos	*OBJP;
 *	unsigned	LEN;
 */
#define PNOBREAK(OBJP, LEN)	((OBJP)->op_len >= LEN)

/*
 *	PSKIP(OBJP, LEN)
 *	struct	objpos	*OBJP;
 *	unsigned	LEN;
 */
#define PSKIP(OBJP, LEN)\
	{ (OBJP)->op_cp += (LEN); (OBJP)->op_len -= (LEN); }

/*
 * ItemPositionIsValid --
 *	True iff disk item identifier is valid.
 */
extern
bool
ItemPositionIsValid ARGS((
	ItemPosition	position
));

#endif	/* !defined(ItemPosIncluded) */
