/*
 * palloc.c --
 *	POSTGRES memory allocator code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/mcxt.h"
#include "nodes/mnodes.h"

#include "utils/palloc.h"

/*
 * User library functions
 */

Pointer
palloc(size)
	Size	size;
{
	return (MemoryContextAlloc(CurrentMemoryContext, size));
}

void
pfree(pointer)
	Pointer	pointer;
{
	MemoryContextFree(CurrentMemoryContext, pointer);
}

Size
psize(pointer)
	Pointer	pointer;
{
	return (PointerGetAllocSize(pointer));
}

Pointer
repalloc(pointer, size)
	Pointer	pointer;
	Size	size;
{
	return (MemoryContextRealloc(CurrentMemoryContext, pointer, size));
}
