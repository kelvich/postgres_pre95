/*
 * palloc.h --
 *	POSTGRES memory allocator definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PAllocIncluded
#define PAllocIncluded	1	/* Include this only once */

#include "c.h"

/*
 * palloc --
 *	Returns pointer to aligned memory of specified size.
 *
 * Exceptions:
 *	BadArgument if size < 1 or size >= MaxAllocSize.
 *	ExhaustedMemory if allocation fails.
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been freed already.
 */
extern
Pointer
palloc ARGS((
	Size	size
));

/*
 * pfree --
 *	Frees memory associated with pointer returned from palloc or repalloc.
 *
 * Exceptions:
 *	BadArgument if pointer is invalid.
 *	FreeInWrongContext if pointer was allocated in a different "context."
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been subsequently freed.
 */
extern
void
pfree ARGS((
	Pointer	pointer
));

/*
 * psize --
 *	Returns size of memory returned from palloc or repalloc.
 *
 * Note:
 *	Psize may be called on objects allocated in other memory contexts.
 *
 * Exceptions:
 *	BadArgument if pointer is invalid.
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been freed already.
 */
extern
Size
psize ARGS((
	Pointer	pointer
));

/*
 * repalloc --
 *	Returns pointer to aligned memory of specified size.
 *
 * Side effects:
 *	The returned memory is first filled with the contents of *pointer
 *	up to the minimum of size and psize(pointer).  Pointer is freed.
 *
 * Exceptions:
 *	BadArgument if pointer is invalid or size < 1 or size >= MaxAllocSize.
 *	ExhaustedMemory if allocation fails.
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been freed already.
 */
extern
Pointer
repalloc ARGS((
	Pointer	pointer,
	Size	size
));

#endif /* !defined(PAllocIncluded) */
