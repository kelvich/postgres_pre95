/*
 * mcxt.h --
 *	POSTGRES memory context definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	MCxtIncluded
#define MCxtIncluded	1	/* Include this only once */

#include "c.h"

#include "mnodes.h"
#include "tnodes.h"

/*
 * CurrentMemoryContext --
 *	Memory context for general global allocations.
 */
extern MemoryContext	CurrentMemoryContext;

/*
 * EnableMemoryContext --
 *	Enables/disables memory context module.
 *
 * Note:
 *	...
 *
 * Exceptions:
 *	...
 */
extern
void
EnableMemoryContext ARGS((
	bool	on
));

/*
 * CreateGlobalMemory --
 *	Returns new global memory context.
 *
 * Note:
 *	Assumes name is static.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called outside TopGlobalMemory.
 *	BadArg if name is invalid.
 */
extern
GlobalMemory
CreateGlobalMemory ARGS((
	String	name	/* XXX MemoryContextName */
));

/*
 * DestroyGlobalMemory --
 *	Destroys given global memory context.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called outside TopGlobalMemory.
 *	BadArg if context is invalid GlobalMemory.
 *	BadArg if context is TopGlobalMemory.
 */
extern
void
GlobalMemoryDestroy ARGS((
	GlobalMemory	context
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
 * TopMemoryContext --
 *	Memory context for general global allocations.
 *
 * Note:
 *	Don't use this memory context for random allocations.  If you
 *	allocate something here, you are expected to clean it up when
 *	appropriate.
 */
extern MemoryContext	TopMemoryContext;

#endif /* !defined(MCxtIncluded) */
