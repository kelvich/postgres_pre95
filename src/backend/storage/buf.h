/*
 * buf.h --
 *	Basic buffer manager data types.
 */

#ifndef	BufIncluded		/* Include this file only once */
#define BufIncluded	1

/*
 * Identification:
 */
#define BUF_H	"$Header$"

#include "tmp/c.h"
#include "storage/block.h"

#define InvalidBuffer	(-1)
#define UnknownBuffer	(-2)

typedef int	Buffer;

/*
 * RelationGetNumberOfBlocks --
 *	Returns the buffer descriptor associated with a page in a relation.
 */
extern
BlockNumber
RelationGetNumberOfBlocks ARGS((
	const Relation	relation,
));

/*
 * BufferIsValid --
 *	True iff the buffer is valid.
 * Note:
 *	BufferIsValid(InvalidBuffer) is False.
 *	BufferIsValid(UnknownBuffer) is False.
 */
extern
bool
BufferIsValid ARGS ((
	Buffer	buffer
));

/*
 * BufferIsInvalid --
 *	True iff the buffer is invalid.
 *
 * Note:
 *	BufferIsInvalid(UnknownBuffer) is False.
 */
extern
bool
BufferIsInvalid ARGS ((
	Buffer	buffer
));

/*
 * BufferIsUnknown --
 *	True iff the buffer is unknown.
 *
 * Note:
 *	BufferIsUnknown(InvalidBuffer) is False.
 */
extern
bool
BufferIsUnknown ARGS ((
	Buffer	buffer
));

#endif	/* !defined(BufIncluded) */


/*
 * If NO_BUFFERISVALID is defined, all error checking using BufferIsValid()
 * are suppressed.  Decision-making using BufferIsValid is not affected.
 * This should be set only if one is sure there will be no errors.
 * - plai 9/10/90
 */

#undef NO_BUFFERISVALID
