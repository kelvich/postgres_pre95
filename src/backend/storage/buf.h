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

#ifndef C_H
#include "c.h"
#endif

#ifndef	BLOCK_H
# include "block.h"
#endif

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

#define BUF_SYMBOLS \
	SymbolDecl(RelationGetNumberOfBlocks, "_RelationGetNumberOfBlocks"), \
	SymbolDecl(BufferIsValid, "_BufferIsValid"), \
	SymbolDecl(BufferIsInvalid, "_BufferIsInvalid"), \
	SymbolDecl(BufferIsUnknown, "_BufferIsUnknown")

#endif	/* !defined(BufIncluded) */
