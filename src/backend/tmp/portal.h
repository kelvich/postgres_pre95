/*
 * portal.h --
 *	POSTGRES portal definitions.
 *
 * Note:
 *	A portal is an abstraction which represents the execution state of
 * a running query (or a fixed sequence of queries).  The "blank portal" is
 * a portal with an InvalidName.  This blank portal is in existance except
 * between calls to BlankPortalAssignName and GetPortalByName(NULL).
 *
 * Identification:
 *	$Header$
 */

#ifndef	PortalIncluded		/* Include this file only once */
#define PortalIncluded	1

#ifndef	C_H
#include "c.h"
#endif

#include "mnodes.h"
#include "nodes.h"

typedef struct PortalBlockData {
	AllocSetData	setData;
	FixedItemData	itemData;
} PortalBlockData;

typedef PortalBlockData	*PortalBlock;

typedef struct PortalData {
	String				name;	/* XXX PortalName */
	classObj(PortalVariableMemory)	variable;
	classObj(PortalHeapMemory)	heap;
	/* ... */
} PortalData;

typedef PortalData	*Portal;

/*
 * EnablePortalManager --
 *	Enables/disables the portal management module.
 *
 * Note:
 *	...
 *
 * Exceptions:
 *	...
 */
extern
void
EnablePortalManager ARGS((
	bool	on
));

/*
 * PortalIsValid --
 *	True iff portal is valid.
 */
extern
bool
PortalIsValid ARGS((
	Portal	portal
));

/*
 * GetPortalByName --
 *	Returns a portal given a portal name; returns blank portal given
 * NULL; returns invalid portal if portal not found.
 *
 * Exceptions:
 *	BadState if called when disabled.
 */
extern
Portal
GetPortalByName ARGS((
	String	name	/* XXX PortalName */
));

/*
 * BlankPortalAssignName --
 *	Returns former blank portal as portal with given name.
 *
 * Side effect:
 *	All references to the former blank portal become incorrect.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called without an intervening call to GetPortalByName(NULL).
 *	BadArg if portal name is invalid.
 *	"WARN" if portal name is in use.
 */
extern
Portal
BlankPortalAssignName ARGS((
	String	name	/* XXX PortalName */
));

/*
 * CreatePortal --
 *	Returns a new portal given a name.
 *
 * Note:
 *	This is expected to be of very limited usability.  See instead,
 * BlankPortalAssignName.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if portal name is invalid.
 *	"WARN" if portal name is in use.
 */
extern
Portal
CreatePortal ARGS((
	String	name	/* XXX PortalName */
));

/*
 * PortalDestroy --
 *	Destroys portal.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if portal is invalid.
 */
extern
void
PortalDestroy ARGS((
	Portal	portal
));

/*
 * PortalResetHeapMemory --
 *	Resets portal's heap memory context.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called when not in PortalHeapMemory context.
 *	BadArg if mode is invalid.
 */
extern
void
PortalResetHeapMemory ARGS((
	Portal	portal
));

/*
 * StartPortalAllocMode --
 *	Starts a new block of portal heap allocation using mode and limit;
 *	the current block is disabled until EndPortalAllocMode is called.
 *
 * Note:
 *	Note blocks may be stacked and restored arbitarily.
 *	The semantics of mode and limit are described in aset.h.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called when not in PortalHeapMemory context.
 *	BadArg if mode is invalid.
 */
extern
void
StartPortalAllocMode ARGS((
	AllocMode	mode,
	Size		limit
));

/*
 * EndPortalAllocMode --
 *	Ends current block of portal heap allocation; previous block is
 *	reenabled.
 *
 * Note:
 *	Note blocks may be stacked and restored arbitarily.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadState if called when not in PortalHeapMemory context.
 */
extern
void
EndPortalAllocMode ARGS((
	void
));

/*
 * PortalGetVariableMemory --
 *	Returns variable memory context for a given portal.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if portal is invalid.
 */
extern
PortalVariableMemory
PortalGetVariableMemory ARGS((
	Portal	portal
));

/*
 * PortalGetHeapMemory --
 *	Returns heap memory context for a given portal.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if portal is invalid.
 */
extern
PortalHeapMemory
PortalGetHeapMemory ARGS((
	Portal	portal
));

/*
 * PortalVariableMemoryGetPortal --
 *	Returns portal containing given variable memory context.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if context is invalid.
 */
extern
Portal
PortalVariableMemoryGetPortal ARGS((
	PortalVariableMemory	context
));

/*
 * PortalHeapMemoryGetPortal --
 *	Returns portal containing given heap memory context.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if context is invalid.
 */
extern
Portal
PortalHeapMemoryGetPortal ARGS((
	PortalHeapMemory	context
));

/*
 * PortalVariableMemoryGetHeapMemory --
 *	Returns heap memory context associated with given variable memory.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if context is invalid.
 */
extern
PortalHeapMemory
PortalVariableMemoryGetHeapMemory ARGS((
	PortalVariableMemory	context
));

/*
 * PortalHeapMemoryGetVariableMemory --
 *	Returns variable memory context associated with given heap memory.
 *
 * Exceptions:
 *	BadState if called when disabled.
 *	BadArg if context is invalid.
 */
extern
PortalVariableMemory
PortalHeapMemoryGetVariableMemory ARGS((
	PortalHeapMemory	context
));

#endif	/* !defined(PortalIncluded) */
