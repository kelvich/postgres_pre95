/*
 * mcxt.h --
 *	POSTGRES memory context definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	MCxtIncluded
#define MCxtIncluded	1	/* Include this only once */

#include "tmp/c.h"

#include "nodes/mnodes.h"
#include "nodes/nodes.h"

/*
 * CurrentMemoryContext --
 *	Memory context for general global allocations.
 */
extern MemoryContext	CurrentMemoryContext;


/*
 * EnableMemoryContext --
 *	Enables/disables memory management and global contexts.
 *
 * Note:
 *	This must be called before creating contexts or allocating memory.
 *	This must be called before other contexts are created.
 *
 * Exceptions:
 *	BadArg if on is invalid.
 *	BadState if on is false when disabled.
 */
extern
void
EnableMemoryContext ARGS((
	bool	on
));

/*
 * EnterMemoryManagement --
 *	Initializes the memory management module and the global context.
 *
 * need several classes of these?  and how does this relate to portals?
 *
 * ...
 */

/*
 * MemoryContextAlloc --
 *	Returns pointer to aligned allocated memory in the given context.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	BadState if called before InitMemoryManager.
 *	BadArg if context is invalid or if size is 0.
 *	BadAllocSize if size is larger than MaxAllocSize.
 */
#ifndef PALLOC_DEBUG
extern
Pointer
MemoryContextAlloc ARGS((
	MemoryContext	context,
	Size		size
));
#else
extern
Pointer
MemoryContextAlloc_Debug ARGS((
	String		file,
	int		line,
        MemoryContext   context,
        Size            size
));
#endif

/*
 * MemoryContextFree --
 *	Frees allocated memory referenced by pointer in the given context.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	???
 *	BadArgumentsErr if firstTime is true for subsequent calls.
 */
#ifndef PALLOC_DEBUG
extern
void
MemoryContextFree ARGS((
	MemoryContext	context,
	Pointer		pointer
));
#else
extern
void
MemoryContextFree_Debug ARGS((
	String		file,
	int		line,
        MemoryContext   context,
        Pointer         pointer
));
#endif
/*
 * MemoryContextRelloc --
 *	Returns pointer to aligned allocated memory in the given context.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	???
 *	BadArgumentsErr if firstTime is true for subsequent calls.
 */
extern
Pointer
MemoryContextRealloc ARGS((
	MemoryContext	context,
	Pointer		pointer,
	Size		size
));

/*
 * MemoryContextGetName --
 *	Returns pointer to aligned allocated memory in the given context.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	???
 *	BadArgumentsErr if firstTime is true for subsequent calls.
 */
extern
String
MemoryContextGetName ARGS((
	MemoryContext	context
));

/*
 * PointerGetAllocSize --
 *	Returns size of aligned allocated memory given pointer to it.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	???
 *	BadArgumentsErr if firstTime is true for subsequent calls.
 */
extern
Size
PointerGetAllocSize ARGS((
	Pointer	pointer
));

/*
 * MemoryContextSwitchTo --
 *	Returns the current context; installs the given context.
 *
 * Note:
 *	none
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if context is invalid.
 */
extern
MemoryContext
MemoryContextSwitchTo ARGS((
	MemoryContext	context
));

/*
 * START HERE
 *	Add routines to move memory between contexts.
 */

/*
 * CreateGlobalMemory --
 *	Returns new global memory context.
 *
 * Note:
 *	Assumes name is static.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called outside TopMemoryContext (TopGlobalMemory).
 *	BadArg if name is invalid.
 */
extern
GlobalMemory
CreateGlobalMemory ARGS((
	String	name	/* XXX MemoryContextName */
));

/*
 * GlobalMemoryDestroy --
 *	Destroys given global memory context.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called outside TopMemoryContext (TopGlobalMemory).
 *	BadArg if context is invalid GlobalMemory.
 *	BadArg if context is TopMemoryContext (TopGlobalMemory).
 */
extern
void
GlobalMemoryDestroy ARGS((
	GlobalMemory	context
));

/*
 * TopMemoryContext --
 *	Memory context for general global allocations.
 *
 * Note:
 *	Don't use this memory context for random allocations.  If you
 *	allocate something here, you are expected to clean it up when
 *	appropriate.
 */
extern MemoryContext	TopMemoryContext;

/*
 * MaxAllocSize --
 *	Arbitrary limit on size of allocations.
 *
 * Note:
 *	There is no guarantee that allocations smaller than MaxAllocSize
 *	will succeed.  Allocation requests larger than MaxAllocSize will
 *	be summarily denied.
 *
 *	This value should not be referenced except in one place in the code.
 *
 * XXX This should be defined in a file of tunable constants.
 */
#define MaxAllocSize	(0xfffffff)	/* 16G - 1 */

/* mcxt.c */
void MemoryContextDump ARGS((MemoryContext context ));
void DumpMemoryContexts ARGS((void ));
void PrintGlobalMemory ARGS((GlobalMemory foo ));

#endif /* !defined(MCxtIncluded) */
