/*
 * palloc.c --
 *	POSTGRES memory allocator code.
 */

#include "c.h"

RcsId("$Header$");

#include "mcxt.h"
#include "mnodes.h"

#include "palloc.h"

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
