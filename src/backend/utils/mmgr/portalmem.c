/*
 * portal.c --
 *	POSTGRES portal code.
 */

#include "c.h"

RcsId("$Header$");

#include <strings.h>	/* for strlen, strncpy */

#include "enbl.h"
#include "excid.h"	/* for Unimplemented */
#include "execnodes.h"	/* for EState */
#include "hasht.h"
#include "log.h"
#include "mcxt.h"
#include "mnodes.h"
#include "nodes.h"
#include "pg_lisp.h"
#include "plannodes.h"	/* for Plan */

#include "portal.h"

/*
 * Global state
 */

static Count PortalManagerEnableCount = 0;
#define PortalManagerEnabled	(PortalManagerEnableCount >= 1)

static HashTable	PortalHashTable = NULL;
static GlobalMemory	PortalMemory = NULL;
static char		PortalMemoryName[] = "Portal";

static Portal		BlankPortal = NULL;

/*
Portal	CurrentPortal = NULL;
*/

/*
 * Internal class definitions
 */

typedef struct HeapMemoryBlockData {
	AllocSetData	setData;
	FixedItemData	itemData;
} HeapMemoryBlockData;

typedef HeapMemoryBlockData	*HeapMemoryBlock;

#define PortalHeapMemoryGetBlock(context) \
	((HeapMemoryBlock)(context)->block)

/*
 * CreateNewBlankPortal --
 *	Creates a new "blank portal."
 */
static void CreateNewBlankPortal ARGS((void));

/*
 * ComputePortalNameHashIndex --
 *	Returns hash index given collisions, table size, and portal name.
 */
static
Index
ComputePortalNameHashIndex ARGS((
	uint32	collisions,
	Size	size,
	String	name	/* XXX PortalName */
));

/*
 * ComputePortalHashIndex --
 *	Returns hash index given collisions, table size, and portal.
 */
static
Index
ComputePortalHashIndex ARGS((
	uint32	collisions,
	Size	size,
	Portal	portal
));

/*
 * PortalHasPortalName --
 *	True iff name of portal is given name.
 */
static
int	/* XXX should be bool, but expects -1, 0, 1 !!! */
PortalHasPortalName ARGS((
	Portal	portal,
	String	name	/* XXX PortalName */
));

/*
 * PortalDump --
 *	Dumps portal for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
PortalDump ARGS((
	Portal	this
));

/*
 * DumpPortals --
 *	Dumps all portals for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
DumpPortals ARGS((
	void
));


/*
 * Private definitions
 */

/*
 * PortalVariableMemory
 */

/*
 * PortalVariableMemoryAlloc --
 *	Returns pointer to aligned space in the variable context.
 *
 * Note:
 *	NoMoreMemoryErr if allocation fails.
 */
static
Pointer
PortalVariableMemoryAlloc ARGS((
	PortalVariableMemory	this,
	Size			size
));

/*
 * PortalVariableMemoryFree --
 *	Frees allocated memory in the variable context.
 *
 * Exceptions:
 *	BadContextErr if current context is not the variable context.
 *	BadArgumentsErr if pointer is invalid.
 */
static
void
PortalVariableMemoryFree ARGS((
	PortalVariableMemory	this,
	Pointer			pointer
));

/*
 * PortalVariableMemoryRealloc --
 *	Returns pointer to aligned space in the variable context.
 *
 * Note:
 *	Memory associated with the pointer is freed before return.
 *
 * Exceptions:
 *	BadContextErr if current context is not the variable context.
 *	BadArgumentsErr if pointer is invalid.
 *	NoMoreMemoryErr if allocation fails.
 */
static
Pointer
PortalVariableMemoryRealloc ARGS((
	PortalVariableMemory	this,
	Pointer			pointer,
	Size			size
));

/*
 * PortalVariableMemoryGetName --
 *	Returns name string for context.
 *
 * Exceptions:
 *	???
 */
static
String
PortalVariableMemoryGetName ARGS((
	PortalVariableMemory	this
));

/*
 * PortalVariableMemoryDump --
 *	Dumps PortalVariableMemory context for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
PortalVariableMemoryDump ARGS((
	PortalVariableMemory	this
));

/*
 * PortalHeapMemory
 */

/*
 * PortalHeapMemoryAlloc --
 *	Returns pointer to aligned space in the heap context.
 *
 * Note:
 *	NoMoreMemoryErr if allocation fails.
 */
static
Pointer
PortalHeapMemoryAlloc ARGS((
	PortalHeapMemory	this,
	Size			size
));

/*
 * PortalHeapMemoryFree --
 *	Frees allocated memory in the heap context.
 *
 * Exceptions:
 *	BadContextErr if current context is not the heap context.
 *	BadArgumentsErr if pointer is invalid.
 */
static
void
PortalHeapMemoryFree ARGS((
	PortalHeapMemory	this,
	Pointer			pointer
));

/*
 * PortalHeapMemoryRealloc --
 *	Returns pointer to aligned space in the heap context.
 *
 * Note:
 *	Memory associated with the pointer is freed before return.
 *
 * Exceptions:
 *	BadContextErr if current context is not the heap context.
 *	BadArgumentsErr if pointer is invalid.
 *	NoMoreMemoryErr if allocation fails.
 */
static
Pointer
PortalHeapMemoryRealloc ARGS((
	PortalHeapMemory	this,
	Pointer			pointer,
	Size			size
));

/*
 * PortalHeapMemoryGetName --
 *	Returns name string for context.
 *
 * Exceptions:
 *	???
 */
static
String
PortalHeapMemoryGetName ARGS((
	PortalHeapMemory	this
));

/*
 * PortalHeapMemoryDump --
 *	Dumps heap context for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
PortalHeapMemoryDump ARGS((
	PortalHeapMemory	this
));


/*
 * Variable and heap memory methods
 */

static MemoryContextMethodsData	PortalVariableContextMethodsData = {
	PortalVariableMemoryAlloc,	/* Pointer (*)(this, uint32)	palloc */
	PortalVariableMemoryFree,	/* void (*)(this, Pointer)	pfree */
	PortalVariableMemoryRealloc,	/* Pointer (*)(this, Pointer)	repalloc */
	PortalVariableMemoryGetName,	/* String (*)(this)		getName */
	PortalVariableMemoryDump	/* void (*)(this)		dump */
};

static MemoryContextMethodsData	PortalHeapContextMethodsData = {
	PortalHeapMemoryAlloc,	/* Pointer (*)(this, uint32)	palloc */
	PortalHeapMemoryFree,	/* void (*)(this, Pointer)	pfree */
	PortalHeapMemoryRealloc, /* Pointer (*)(this, Pointer)	repalloc */
	PortalHeapMemoryGetName, /* String (*)(this)		getName */
	PortalHeapMemoryDump	/* void (*)(this)		dump */
};

/*
 * External routines
 */

void
EnablePortalManager(on)
	bool	on;
{
	static bool	processing = false;

	AssertState(!processing);
	AssertArg(BoolIsValid(on));

	if (BypassEnable(&PortalManagerEnableCount, on)) {
		return;
	}

	processing = true;

	if (on) {	/* initialize */
		EnableMemoryContext(true);
		EnableHashTable(true);

		PortalMemory = CreateGlobalMemory(PortalMemoryName);
		PortalHashTable = CreateHashTable(
			ComputePortalNameHashIndex,
			ComputePortalHashIndex,
			PortalHasPortalName,
			NULL,
			(Size)21 /* est:  7 open portals or less per user */);
		CreateNewBlankPortal();

	} else {	/* cleanup */
		if (PortalIsValid(BlankPortal)) {
			PortalDestroy(BlankPortal);
			MemoryContextFree((MemoryContext)PortalMemory,
				(Pointer)BlankPortal);
			BlankPortal = NULL;
		}
		/*
		 * Each portal must free its non-memory resources specially.
		 */
		HashTableWalk(PortalHashTable, PortalDestroy);
		HashTableDestroy(PortalHashTable);
		PortalHashTable = NULL;

		GlobalMemoryDestroy(PortalMemory);
		PortalMemory = NULL;

		EnableHashTable(true);
		EnableMemoryContext(true);
	}

	processing = false;
}

bool
PortalIsValid(portal)
	Portal	portal;
{
	return (PointerIsValid(portal));
}

Portal
GetPortalByName(name)
	String	name;	/* XXX PortalName */
{
	Portal	portal;

	AssertState(PortalManagerEnabled);

	if (PointerIsValid(name)) {	/* XXX PortalNameIsValid */
		portal = (Portal)KeyHashTableLookup(PortalHashTable, name);
	} else {
		if (!PortalIsValid(BlankPortal)) {
			CreateNewBlankPortal();
		}
		portal = BlankPortal;
	}

	return (portal);
}

Portal
BlankPortalAssignName(name)
	String	name;	/* XXX PortalName */
{
	Portal	portal;
	uint16	length;

	AssertState(PortalManagerEnabled);
	AssertState(PortalIsValid(BlankPortal));
	AssertArg(PointerIsValid(name));	/* XXX PortalName */

	portal = GetPortalByName(name);
	if (PortalIsValid(portal)) {
		elog(WARN, "BlankPortalAssignName: portal %s existant", name);
	}

	/*
	 * remove blank portal
	 */
	portal = BlankPortal;
	BlankPortal = NULL;

	/*
	 * initialize portal name
	 */
	length = 1 + strlen(name);
	portal->name = (String)MemoryContextAlloc(
		(MemoryContext)&portal->variable, length);
	strncpy(portal->name, name, length);

	/*
	 * put portal in table
	 */
	HashTableInsert(PortalHashTable, (Pointer)portal);

	return (portal);
}

void
PortalSetQuery(portal, parse, plan, state)
	Portal		portal;
	LispValue	parse;
	Plan		plan;
	EState		state;
{
	AssertState(PortalManagerEnabled);
	AssertArg(PortalIsValid(portal));
	AssertArg(NodeIsType((Node)parse, classTag(LispValue)));
	AssertArg(NodeIsType((Node)plan, classTag(Plan)));
	AssertArg(NodeIsType((Node)state, classTag(EState)));

	portal->parse = parse;
	portal->plan = plan;
	portal->state = state;
}

LispValue	/* Parse */
PortalGetParse(portal)
	Portal		portal;
{
	AssertState(PortalManagerEnabled);
	AssertArg(PortalIsValid(portal));

	return (portal->parse);
}

Plan
PortalGetPlan(portal)
	Portal		portal;
{
	AssertState(PortalManagerEnabled);
	AssertArg(PortalIsValid(portal));

	return (portal->plan);
}

EState
PortalGetState(portal)
	Portal		portal;
{
	AssertState(PortalManagerEnabled);
	AssertArg(PortalIsValid(portal));

	return (portal->state);
}

Portal
CreatePortal(name)
	String	name;	/* XXX PortalName */
{
	Portal	portal;
	uint16	length;

	AssertState(PortalManagerEnabled);
	AssertArg(PointerIsValid(name));	/* XXX PortalName */

	portal = GetPortalByName(name);
	if (PortalIsValid(portal)) {
		elog(WARN, "CreatePortal: portal %s existant", name);
	}

	/* make new portal structure */
	portal = (Portal)MemoryContextAlloc((MemoryContext)PortalMemory,
		sizeof *portal);

	/* initialize portal variable context */
	NodeSetTag((Node)&portal->variable, classTag(PortalVariableMemory));
	AllocSetInit(&portal->variable.setData, DynamicAllocMode, (Size)0);
	portal->variable.method = &PortalVariableContextMethodsData;

	/* initialize portal heap context */
	NodeSetTag((Node)&portal->heap, classTag(PortalHeapMemory));
	portal->heap.block = NULL;
	FixedStackInit(&portal->heap.stackData,
		offsetof (HeapMemoryBlockData, itemData));
	portal->heap.method = &PortalHeapContextMethodsData;

	/* initialize portal name */
	length = 1 + strlen(name);
	portal->name = (String)MemoryContextAlloc(
		(MemoryContext)&portal->variable, length);
	strncpy(portal->name, name, length);

	/* put portal in table */
	HashTableInsert(PortalHashTable, (Pointer)portal);

	/* Trap(PointerIsValid(name), Unimplemented); */
	return (portal);
}

void
PortalDestroy(portal)
	Portal	portal;
{
	AssertState(PortalManagerEnabled);
	AssertArg(PortalIsValid(portal));

	if (portal != BlankPortal) {
		/*
		 * remove portal from table
		 */
		HashTableDelete(PortalHashTable, (Pointer)portal);
	}
	/*
	 * reset portal
	 */
	PortalResetHeapMemory(portal);
	MemoryContextFree((MemoryContext)&portal->variable,
		(Pointer)portal->name);
	AllocSetReset(&portal->variable.setData);	/* XXX log */

	if (portal != BlankPortal) {
		MemoryContextFree((MemoryContext)PortalMemory, (Pointer)portal);
	}
}

/*
 * Someday, Reset, Start, and End can be optimized by keeping a global
 * portal module stack of free HeapMemoryBlock's.  This will make Start
 * and End be fast.
 */
void
PortalResetHeapMemory(portal)
	Portal	portal;
{
	PortalHeapMemory	context;
	MemoryContext		currentContext;

	context = PortalGetHeapMemory(portal);

	if (PointerIsValid(context->block)) {
		/* save present context */
		currentContext = MemoryContextSwitchTo((MemoryContext)context);

		do {
			EndPortalAllocMode();
		} while (PointerIsValid(context->block));

		/* restore context */
		(void)MemoryContextSwitchTo(currentContext);
	}
}

void
StartPortalAllocMode(mode, limit)
	AllocMode	mode;
	Size		limit;
{
	PortalHeapMemory	context;

	AssertState(PortalManagerEnabled);
	AssertState(IsA(CurrentMemoryContext,PortalHeapMemory));
	/* AssertArg(AllocModeIsValid); */

	context = (PortalHeapMemory)CurrentMemoryContext;

	/* stack current mode */
	if (PointerIsValid(context->block)) {
		FixedStackPush(&context->stackData, context->block);
	}

	/* allocate and initialize new block */
	context->block = MemoryContextAlloc(
		(MemoryContext)PortalHeapMemoryGetVariableMemory(context),
		sizeof (HeapMemoryBlockData));
	/* XXX careful, context->block has never been stacked => bad state */

	AllocSetInit(&PortalHeapMemoryGetBlock(context)->setData, mode, limit);
}

void
EndPortalAllocMode()
{
	PortalHeapMemory	context;

	AssertState(PortalManagerEnabled);
	AssertState(IsA(CurrentMemoryContext,PortalHeapMemory));

	context = (PortalHeapMemory)CurrentMemoryContext;
	AssertState(PointerIsValid(context->block));	/* XXX Trap(...) */

	/* free current mode */
	AllocSetReset(&PortalHeapMemoryGetBlock(context)->setData);
	MemoryContextFree(
		(MemoryContext)PortalHeapMemoryGetVariableMemory(context),
		context->block);

	/* restore previous mode */
	context->block = FixedStackPop(&context->stackData);
}

/*
 * START HERE
 *	Add functions to move memory between contexts or blocks.
 */

PortalVariableMemory
PortalGetVariableMemory(portal)
	Portal	portal;
{
	return (&portal->variable);
}

PortalHeapMemory
PortalGetHeapMemory(portal)
	Portal	portal;
{
	return (&portal->heap);
}

Portal
PortalVariableMemoryGetPortal(context)
	PortalVariableMemory	context;
{
	return ((Portal)((char *)context - offsetof (PortalData, variable)));
}

Portal
PortalHeapMemoryGetPortal(context)
	PortalHeapMemory	context;
{
	return ((Portal)((char *)context - offsetof (PortalData, heap)));
}

PortalHeapMemory
PortalVariableMemoryGetHeapMemory(context)
	PortalVariableMemory	context;
{
	return ((PortalHeapMemory)((char *)context
		- offsetof (PortalData, variable)
		+ offsetof (PortalData, heap)));
}

PortalVariableMemory
PortalHeapMemoryGetVariableMemory(context)
	PortalHeapMemory	context;
{
	return ((PortalVariableMemory)((char *)context
		- offsetof (PortalData, heap)
		+ offsetof (PortalData, variable)));
}


/*
 * Private
 */

static
void
CreateNewBlankPortal()
{
	Portal	portal;
	uint16	length;

	AssertState(!PortalIsValid(BlankPortal));

	/*
	 * make new portal structure
	 */
	portal = (Portal)MemoryContextAlloc((MemoryContext)PortalMemory,
		sizeof *portal);

	/*
	 * initialize portal variable context
	 */
	NodeSetTag((Node)&portal->variable, classTag(PortalVariableMemory));
	AllocSetInit(&portal->variable.setData, DynamicAllocMode, (Size)0);
	portal->variable.method = &PortalVariableContextMethodsData;

	/*
	 * initialize portal heap context
	 */
	NodeSetTag((Node)&portal->heap, classTag(PortalHeapMemory));
	portal->heap.block = NULL;
	FixedStackInit(&portal->heap.stackData,
		offsetof (HeapMemoryBlockData, itemData));
	portal->heap.method = &PortalHeapContextMethodsData;

	/*
	 * set bogus portal name
	 */
	portal->name = "** Blank Portal **";

	/*
	 * install blank portal
	 */
	BlankPortal = portal;
}

static
Index
ComputePortalNameHashIndex(collisions, size, name)
	uint32	collisions;
	Size	size;
	String	name;	/* XXX PortalName */
{
	return (StringHashFunction(collisions, size, name));
}

static
Index
ComputePortalHashIndex(collisions, size, portal)
	uint32	collisions;
	Size	size;
	Portal	portal;
{
	return (ComputePortalNameHashIndex(collisions, size, portal->name));
}

static
int		/* XXX should be bool, but expects -1, 0, 1 !!! */
PortalHasPortalName(portal, name)
	Portal	portal;
	String	name;	/* XXX PortalName */
{
	return (strcmp(portal->name, name));
}


static
void
PortalDump(this)
	Portal	this;
{
	/* XXX state/argument checking here */

	PortalVariableMemoryDump(PortalGetVariableMemory(this));
	PortalHeapMemoryDump(PortalGetHeapMemory(this));
}

static
void
DumpPortals()
{
	/* XXX state checking here */

	HashTableWalk(PortalHashTable, PortalDump);
/*
	VariableMemory	context;

	context = (VariableMemory)OrderedSetGetHead(&ActiveVariableMemorySetData);

	while (PointerIsValid(context)) {
		VariableMemoryDump(context);

		context = (VariableMemory)OrderedElemGetSuccessor(
			&context->elemData);
	}
*/
}



/*
 * Portal variable functions
 */

static
Pointer
PortalVariableMemoryAlloc(this, size)
	PortalVariableMemory	this;
	Size			size;
{
	return (AllocSetAlloc(&this->setData, size));
}

static
void
PortalVariableMemoryFree(this, pointer)
	PortalVariableMemory	this;
	Pointer			pointer;
{
	AllocSetFree(&this->setData, pointer);
}

static
Pointer
PortalVariableMemoryRealloc(this, pointer, size)
	PortalVariableMemory	this;
	Pointer			pointer;
	Size			size;
{
	return (AllocSetRealloc(&this->setData, pointer, size));
}

static
String
PortalVariableMemoryGetName(this)
	PortalVariableMemory	this;
{
	return (form("%s-var", PortalVariableMemoryGetPortal(this)->name));
}

static
void
PortalVariableMemoryDump(this)
	PortalVariableMemory	this;
{
	printf("--\n%s:\n", PortalVariableMemoryGetName(this));

	AllocSetDump(&this->setData);	/* XXX is this the right interface */
}

/*
 * Portal heap functions
 */

static
Pointer
PortalHeapMemoryAlloc(this, size)
	PortalHeapMemory	this;
	Size			size;
{
	HeapMemoryBlock	block = PortalHeapMemoryGetBlock(this);

	AssertState(PointerIsValid(block));

	return (AllocSetAlloc(&block->setData, size));
}

static
void
PortalHeapMemoryFree(this, pointer)
	PortalHeapMemory	this;
	Pointer			pointer;
{
	HeapMemoryBlock	block = PortalHeapMemoryGetBlock(this);

	AssertState(PointerIsValid(block));
	AssertState(AllocSetContains(&block->setData, pointer));
	/* XXX Trap(..., IllegalFreeRequest) or some such */

	AllocSetFree(&block->setData, pointer);
}

static
Pointer
PortalHeapMemoryRealloc(this, pointer, size)
	PortalHeapMemory	this;
	Pointer			pointer;
	Size			size;
{
	HeapMemoryBlock	block = PortalHeapMemoryGetBlock(this);

	AssertState(PointerIsValid(block));

	return (AllocSetRealloc(&block->setData, pointer, size));
}

static
String
PortalHeapMemoryGetName(this)
	PortalHeapMemory	this;
{
	return (form("%s-heap", PortalHeapMemoryGetPortal(this)->name));
}

static
void
PortalHeapMemoryDump(this)
	PortalHeapMemory	this;
{
	HeapMemoryBlock	block;

	printf("--\n%s:\n", PortalHeapMemoryGetName(this));

	/* XXX is this the right interface */
	if (PointerIsValid(this->block)) {
		AllocSetDump(&PortalHeapMemoryGetBlock(this)->setData);
	}

	/* dump the stack too */
	for (block = (HeapMemoryBlock)FixedStackGetTop(&this->stackData);
			PointerIsValid(block);
			block = (HeapMemoryBlock)
			FixedStackGetNext(&this->stackData, (Pointer)block)) {

		printf("--\n");
		AllocSetDump(&block->setData);
	}
}
