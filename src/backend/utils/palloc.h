/*
 * palloc.h --
 *	POSTGRES memory allocator definitions.
 */

#ifndef	PAllocIncluded		/* Include this file only once */
#define PAllocIncluded	1

/*
 * Identification:
 */
#define PALLOC_H	"$Header$"

#include "tmp/c.h"
#include "tmp/simplelists.h"

/*
 * palloc --
 *	Returns pointer to aligned memory of specified size.
 *
 * Exceptions:
 *	BadArgument if size < 1 or size >= MaxAllocSize.
 *	ExhaustedMemory if allocation fails.
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been freed already.
 *
 * pfree --
 *	Frees memory associated with pointer returned from palloc or repalloc.
 *
 * Exceptions:
 *	BadArgument if pointer is invalid.
 *	FreeInWrongContext if pointer was allocated in a different "context."
 *	NonallocatedPointer if pointer was not returned by palloc or repalloc
 *		or may have been subsequently freed.
 */

#ifndef PALLOC_DEBUG
extern Pointer palloc ARGS((Size size));
extern void    pfree  ARGS((Pointer pointer)); 
#else
extern Pointer palloc_debug ARGS((Size	size));
extern void    pfree_debug  ARGS((Pointer pointer));
#endif PALLOC_DEBUG

typedef struct PallocDebugData {
    Pointer     pointer;
    Size        size;
    String      file;
    int         line;
    String      context;
    SLNode      Link;
} PallocDebugData;

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
