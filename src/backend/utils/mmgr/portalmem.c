/* ----------------------------------------------------------------
 *   FILE
 *	portalmem.c
 *	
 *   DESCRIPTION
 *	backend portal memory context management stuff
 *
 *   INTERNAL UTILITY ROUTINES
 *	CreateNewBlankPortal, ComputePortalNameHashIndex,
 *	ComputePortalHashIndex, PortalHasPortalName, PortalDump,
 *	DumpPortals, PortalVariableMemoryAlloc,
 *	PortalVariableMemoryFree, PortalVariableMemoryRealloc,
 *	PortalVariableMemoryGetName, PortalVariableMemoryDump,
 *	PortalHeapMemoryAlloc, PortalHeapMemoryFree,
 *	PortalHeapMemoryRealloc, PortalHeapMemoryGetName,
 *	PortalHeapMemoryDump
 *	
 *   INTERFACE ROUTINES
 *	EnablePortalManager
 *	GetPortalByName
 *	BlankPortalAssignName
 *	PortalSetQuery
 *	PortalGetQueryDesc
 *	PortalGetState
 *	CreatePortal
 *	PortalDestroy
 *	PortalResetHeapMemory
 *	StartPortalAllocMode
 *	EndPortalAllocMode
 *	PortalGetVariableMemory
 *	PortalGetHeapMemory
 *	PortalVariableMemoryGetPortal
 *	PortalHeapMemoryGetPortal
 *	PortalVariableMemoryGetHeapMemory
 *	PortalHeapMemoryGetVariableMemory
 *
 *   NOTES
 *	Do not confuse "Portal" with "PortalEntry" (or "PortalBuffer").
 *	When a PQexec() routine is run, the resulting tuples
 *	find their way into a "PortalEntry".  The contents of the resulting
 *	"PortalEntry" can then be inspected by other PQxxx functions.
 *
 *	A "Portal" is a structure used to keep track of queries of the
 *	form:
 *		retrieve portal FOO ( blah... ) where blah...
 *
 *	When the backend sees a "retrieve portal" query, it allocates
 *	a "PortalD" structure, plans the query and then stores the query
 *	in the portal without executing it.  Later, when the backend
 *	sees a
 *		fetch 1 into FOO
 *
 *	the system looks up the portal named "FOO" in the portal table,
 *	gets the planned query and then calls the executor with a feature of
 *	'(EXEC_FOR 1).  The executor then runs the query and returns a single
 *	tuple.  The problem is that we have to hold onto the state of the
 *	portal query until we see a "close p".  This means we have to be
 *	careful about memory management.
 *
 *	Having said all that, here is what a PortalD currently looks like:
 *
 * struct PortalD {
 *	String				name;
 *	classObj(PortalVariableMemory)	variable;
 *	classObj(PortalHeapMemory)	heap;
 *	List				queryDesc;
 *	EState				state;
 *	void				(*cleanup) ARGS((Portal portal));
 * };
 *
 *	I hope this makes things clearer to whoever reads this -cim 2/22/91
 *
 *	Here is an old comment taken from nodes/mnodes.h
 *
 * MemoryContext --
 *	A logical context in which memory allocations occur.
 *
 * The types of memory contexts can be thought of as members of the
 * following inheritance hierarchy with properties summarized below.
 *
 *			Node
 *			|
 *		MemoryContext___
 *		/		\
 *	GlobalMemory	PortalMemoryContext
 *			/		\
 *	PortalVariableMemory	PortalHeapMemory
 *
 *			Flushed at	Flushed at	Checkpoints
 *			Transaction	Portal
 *			Commit		Close
 *
 * GlobalMemory			n		n		n
 * PortalVariableMemory		n		y		n
 * PortalHeapMemory		y		y		y *	
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"

RcsId("$Header$");

#include <strings.h>	/* for strlen, strncpy */

#include "tmp/hasht.h"
#include "utils/module.h"
#include "utils/excid.h"	/* for Unimplemented */
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/hsearch.h"

#include "nodes/mnodes.h"
#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"	/* for EState */
#include "tags.h"

#include "tmp/portal.h"

/* ----------------
 *   	ALLOCFREE_ERROR_ABORT
 *   	define this if you want a core dump when you try to
 *   	free memory already freed -cim 2/9/91
 * ----------------
 */
#undef ALLOCFREE_ERROR_ABORT

/* ----------------
 *   	Global state
 * ----------------
 */

static Count PortalManagerEnableCount = 0;
#define MAX_PORTALNAME_LEN	50
typedef struct portalhashent {
    char portalname[MAX_PORTALNAME_LEN];
    Portal portal;
} PortalHashEnt;

#define PortalManagerEnabled	(PortalManagerEnableCount >= 1)

static HTAB		*PortalHashTable = NULL;
#define PortalHashTableLookup(NAME, PORTAL) \
    {   PortalHashEnt *hentry; Boolean found; char key[MAX_PORTALNAME_LEN]; \
	bzero(key, MAX_PORTALNAME_LEN); \
	sprintf(key, "%s", NAME); \
	hentry = (PortalHashEnt*)hash_search(PortalHashTable, \
					     key, HASH_FIND, &found); \
	if (hentry == NULL) \
	    elog(FATAL, "error in PortalHashTable"); \
	if (found) \
	    PORTAL = hentry->portal; \
	else \
	    PORTAL = NULL; \
    }
#define PortalHashTableInsert(PORTAL) \
    {   PortalHashEnt *hentry; Boolean found; char key[MAX_PORTALNAME_LEN]; \
	bzero(key, MAX_PORTALNAME_LEN); \
	sprintf(key, "%s", PORTAL->name); \
	hentry = (PortalHashEnt*)hash_search(PortalHashTable, \
					     key, HASH_ENTER, &found); \
	if (hentry == NULL) \
	    elog(FATAL, "error in PortalHashTable"); \
	if (found) \
	    elog(NOTICE, "trying to insert a portal name that exists."); \
	hentry->portal = PORTAL; \
    }
#define PortalHashTableDelete(PORTAL) \
    {   PortalHashEnt *hentry; Boolean found; char key[MAX_PORTALNAME_LEN]; \
	bzero(key, MAX_PORTALNAME_LEN); \
	sprintf(key, "%s", PORTAL->name); \
	hentry = (PortalHashEnt*)hash_search(PortalHashTable, \
					     key, HASH_REMOVE, &found); \
	if (hentry == NULL) \
	    elog(FATAL, "error in PortalHashTable"); \
	if (!found) \
	    elog(NOTICE, "trying to delete portal name that does not exist."); \
    }

static GlobalMemory	PortalMemory = NULL;
static char		PortalMemoryName[] = "Portal";

static Portal		BlankPortal = NULL;

/* ----------------
 * 	Internal class definitions
 * ----------------
 */
typedef struct HeapMemoryBlockData {
    AllocSetData	setData;
    FixedItemData	itemData;
} HeapMemoryBlockData;

typedef HeapMemoryBlockData	*HeapMemoryBlock;

#define HEAPMEMBLOCK(context) \
    ((HeapMemoryBlock)(context)->block)

/* ----------------------------------------------------------------
 *		  Variable and heap memory methods
 * ----------------------------------------------------------------
 */
/* ----------------
 *	PortalVariableMemoryAlloc
 * ----------------
 */
static Pointer
PortalVariableMemoryAlloc(this, size)
    PortalVariableMemory	this;
    Size			size;
{
    return (AllocSetAlloc(&this->setData, size));
}

/* ----------------
 *	PortalVariableMemoryFree
 * ----------------
 */
static void
PortalVariableMemoryFree(this, pointer)
    PortalVariableMemory	this;
    Pointer			pointer;
{
    AllocSetFree(&this->setData, pointer);
}

/* ----------------
 *	PortalVariableMemoryRealloc
 * ----------------
 */
static Pointer
PortalVariableMemoryRealloc(this, pointer, size)
    PortalVariableMemory	this;
    Pointer			pointer;
    Size			size;
{
    return (AllocSetRealloc(&this->setData, pointer, size));
}

/* ----------------
 *	PortalVariableMemoryGetName
 * ----------------
 */
static String
PortalVariableMemoryGetName(this)
    PortalVariableMemory	this;
{
    return (form((int)"%s-var", PortalVariableMemoryGetPortal(this)->name));
}

/* ----------------
 *	PortalVariableMemoryDump
 * ----------------
 */
static void
PortalVariableMemoryDump(this)
    PortalVariableMemory	this;
{
    printf("--\n%s:\n", PortalVariableMemoryGetName(this));
    
    AllocSetDump(&this->setData);	/* XXX is this the right interface */
}

/* ----------------
 *	PortalHeapMemoryAlloc
 * ----------------
 */
static Pointer
PortalHeapMemoryAlloc(this, size)
    PortalHeapMemory	this;
    Size			size;
{
    HeapMemoryBlock	block = HEAPMEMBLOCK(this);
    
    AssertState(PointerIsValid(block));
    
    return (AllocSetAlloc(&block->setData, size));
}

/* ----------------
 *	PortalHeapMemoryFree
 * ----------------
 */
static void
PortalHeapMemoryFree(this, pointer)
    PortalHeapMemory	this;
    Pointer			pointer;
{
    HeapMemoryBlock	block = HEAPMEMBLOCK(this);
    
    AssertState(PointerIsValid(block));
    
    if (AllocSetContains(&block->setData, pointer))
	AllocSetFree(&block->setData, pointer);
    else {
	elog(NOTICE,
	     "PortalHeapMemoryFree: 0x%x not in alloc set!",
	     pointer);
#ifdef ALLOCFREE_ERROR_ABORT
	Assert(AllocSetContains(&block->setData, pointer));
#endif ALLOCFREE_ERROR_ABORT
    }
}

/* ----------------
 *	PortalHeapMemoryRealloc
 * ----------------
 */
static Pointer
PortalHeapMemoryRealloc(this, pointer, size)
    PortalHeapMemory	this;
    Pointer			pointer;
    Size			size;
{
    HeapMemoryBlock	block = HEAPMEMBLOCK(this);
    
    AssertState(PointerIsValid(block));
    
    return (AllocSetRealloc(&block->setData, pointer, size));
}

/* ----------------
 *	PortalHeapMemoryGetName
 * ----------------
 */
static String
PortalHeapMemoryGetName(this)
    PortalHeapMemory	this;
{
    return (form((int)"%s-heap", PortalHeapMemoryGetPortal(this)->name));
}

/* ----------------
 *	PortalHeapMemoryDump
 * ----------------
 */
static void
PortalHeapMemoryDump(this)
    PortalHeapMemory	this;
{
    HeapMemoryBlock	block;
    
    printf("--\n%s:\n", PortalHeapMemoryGetName(this));
    
    /* XXX is this the right interface */
    if (PointerIsValid(this->block))
	AllocSetDump(&HEAPMEMBLOCK(this)->setData);
    
    /* dump the stack too */
    for (block = (HeapMemoryBlock)FixedStackGetTop(&this->stackData);
	 PointerIsValid(block);
	 block = (HeapMemoryBlock)
	 FixedStackGetNext(&this->stackData, (Pointer)block)) {
	
	printf("--\n");
	AllocSetDump(&block->setData);
    }
}

/* ----------------------------------------------------------------
 *	 	variable / heap context method tables
 * ----------------------------------------------------------------
 */
static MemoryContextMethodsData	PortalVariableContextMethodsData = {
    PortalVariableMemoryAlloc,	/* Pointer (*)(this, uint32)	palloc */
    PortalVariableMemoryFree,	/* void (*)(this, Pointer)	pfree */
    PortalVariableMemoryRealloc,/* Pointer (*)(this, Pointer)	repalloc */
    PortalVariableMemoryGetName,/* String (*)(this)		getName */
    PortalVariableMemoryDump	/* void (*)(this)		dump */
    };
    
static MemoryContextMethodsData	PortalHeapContextMethodsData = {
    PortalHeapMemoryAlloc,	/* Pointer (*)(this, uint32)	palloc */
    PortalHeapMemoryFree,	/* void (*)(this, Pointer)	pfree */
    PortalHeapMemoryRealloc,    /* Pointer (*)(this, Pointer)	repalloc */
    PortalHeapMemoryGetName,    /* String (*)(this)		getName */
    PortalHeapMemoryDump	/* void (*)(this)		dump */
    };
    

/* ----------------------------------------------------------------
 *		  private internal support routines
 * ----------------------------------------------------------------
 */
/* ----------------
 *	CreateNewBlankPortal
 * ----------------
 */
static void
CreateNewBlankPortal()
{
    Portal	portal;
    uint16	length;
    
    AssertState(!PortalIsValid(BlankPortal));
    
    /*
     * make new portal structure
     */
    portal = (Portal)
	MemoryContextAlloc((MemoryContext)PortalMemory, sizeof *portal);
    
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
    
    /* initialize portal query */
    portal->queryDesc = LispNil;
    portal->state = NULL;
    portal->cleanup = NULL;
    
    /*
     * install blank portal
     */
    BlankPortal = portal;
}

/* ----------------
 *	PortalDump
 * ----------------
 */
static void
PortalDump(this)
    Portal	this;
{
    /* XXX state/argument checking here */
    
    PortalVariableMemoryDump(PortalGetVariableMemory(this));
    PortalHeapMemoryDump(PortalGetHeapMemory(this));
}

/* ----------------
 *	DumpPortals
 * ----------------
 */
static void
DumpPortals()
{
    /* XXX state checking here */
    
    HashTableWalk(PortalHashTable, PortalDump, NULL);
}

/* ----------------------------------------------------------------
 *		   public portal interface functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	EnablePortalManager
 * ----------------
 */
void
EnablePortalManager(on)
    bool	on;
{
    static bool	processing = false;
    HASHCTL ctl;
    
    AssertState(!processing);
    AssertArg(BoolIsValid(on));
    
    if (BypassEnable(&PortalManagerEnableCount, on)) 
	return;
    
    processing = true;
    
    if (on) {	/* initialize */
	EnableMemoryContext(true);
	
	PortalMemory = CreateGlobalMemory(PortalMemoryName);
	
	ctl.keysize = MAX_PORTALNAME_LEN;
	ctl.datasize = sizeof(Portal);
	/* (Size) 21 est:  7 open portals or less per user */;
	PortalHashTable = hash_create(21, &ctl, HASH_ELEM);
	
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
	HashTableWalk(PortalHashTable, PortalDestroy, NULL);
	hash_destroy(PortalHashTable);
	PortalHashTable = NULL;
	
	GlobalMemoryDestroy(PortalMemory);
	PortalMemory = NULL;
	
	EnableMemoryContext(true);
    }
    
    processing = false;
}

/* ----------------
 *	GetPortalByName
 * ----------------
 */
Portal
GetPortalByName(name)
    String	name;
{
    Portal	portal;
    
    AssertState(PortalManagerEnabled);
    
    if (PointerIsValid(name)) {
	PortalHashTableLookup(name, portal);
      }
    else {
	if (!PortalIsValid(BlankPortal))
	    CreateNewBlankPortal();
	portal = BlankPortal;
    }
    
    return (portal);
}

/* ----------------
 *	BlankPortalAssignName
 * ----------------
 */
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
	elog(NOTICE, "BlankPortalAssignName: portal %s already exists", name);
	return (portal);
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
    portal->name = (String)
	MemoryContextAlloc((MemoryContext)&portal->variable, length);
    
    strncpy(portal->name, name, length);
    
    /*
     * put portal in table
     */
    PortalHashTableInsert(portal);
    
    return (portal);
}

/* ----------------
 *	PortalSetQuery
 * ----------------
 */
void
PortalSetQuery(portal, queryDesc, state, cleanup)
    Portal	portal;
    List	queryDesc;
    EState	state;
    void	(*cleanup) ARGS((Portal portal));
{
    AssertState(PortalManagerEnabled);
    AssertArg(PortalIsValid(portal));
    AssertArg(NodeIsType((Node)queryDesc, classTag(LispList)));
    AssertArg(NodeIsType((Node)state, classTag(EState)));
    
    portal->queryDesc = queryDesc;
    portal->state = state;
    portal->cleanup = cleanup;
}

/* ----------------
 *	PortalGetQueryDesc
 * ----------------
 */
List	/* QueryDesc */
PortalGetQueryDesc(portal)
    Portal		portal;
{
    AssertState(PortalManagerEnabled);
    AssertArg(PortalIsValid(portal));
    
    return (portal->queryDesc);
}

/* ----------------
 *	PortalGetState
 * ----------------
 */
EState
PortalGetState(portal)
    Portal		portal;
{
    AssertState(PortalManagerEnabled);
    AssertArg(PortalIsValid(portal));
    
    return (portal->state);
}

/* ----------------
 *	CreatePortal
 * ----------------
 */
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
	elog(NOTICE, "CreatePortal: portal %s already exists", name);
	return (portal);
    }
    
    /* make new portal structure */
    portal = (Portal)
	MemoryContextAlloc((MemoryContext)PortalMemory, sizeof *portal);
    
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
    portal->name = (String)
	MemoryContextAlloc((MemoryContext)&portal->variable, length);
    strncpy(portal->name, name, length);
    
    /* initialize portal query */
    portal->queryDesc = LispNil;
    portal->state = NULL;
    portal->cleanup = NULL;
    
    /* put portal in table */
    PortalHashTableInsert(portal);
    
    /* Trap(PointerIsValid(name), Unimplemented); */
    return (portal);
}

/* ----------------
 *	PortalDestroy
 * ----------------
 */
void
PortalDestroy(portal)
    Portal	portal;
{
    AssertState(PortalManagerEnabled);
    AssertArg(PortalIsValid(portal));
    
    /* remove portal from table if not blank portal */
    if (portal != BlankPortal)
	PortalHashTableDelete(portal);
    
    /* reset portal */
    if (PointerIsValid(portal->cleanup))
	(*portal->cleanup)(portal);
	
    PortalResetHeapMemory(portal);
    MemoryContextFree((MemoryContext)&portal->variable,
		      (Pointer)portal->name);
    AllocSetReset(&portal->variable.setData);	/* XXX log */
    
    if (portal != BlankPortal)
	MemoryContextFree((MemoryContext)PortalMemory, (Pointer)portal);
}

/* ----------------
 *	PortalResetHeapMemory
 *
 * Someday, Reset, Start, and End can be optimized by keeping a global
 * portal module stack of free HeapMemoryBlock's.  This will make Start
 * and End be fast.
 * ----------------
 */
void
PortalResetHeapMemory(portal)
    Portal	portal;
{
    PortalHeapMemory	context;
    MemoryContext	currentContext;
    
    context = PortalGetHeapMemory(portal);
    
    if (PointerIsValid(context->block)) {
	/* save present context */
	currentContext = MemoryContextSwitchTo((MemoryContext)context);
	
	do {
	    EndPortalAllocMode();
	} while (PointerIsValid(context->block));
	
	/* restore context */
	(void) MemoryContextSwitchTo(currentContext);
    }
}

/* ----------------
 *	StartPortalAllocMode
 * ----------------
 */
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
    if (PointerIsValid(context->block))
	FixedStackPush(&context->stackData, context->block);
    
    /* allocate and initialize new block */
    context->block =
	MemoryContextAlloc(
	    (MemoryContext)PortalHeapMemoryGetVariableMemory(context),
	    sizeof (HeapMemoryBlockData) );
    
    /* XXX careful, context->block has never been stacked => bad state */
    
    AllocSetInit(&HEAPMEMBLOCK(context)->setData, mode, limit);
}

/* ----------------
 *	EndPortalAllocMode
 * ----------------
 */
void
EndPortalAllocMode()
{
    PortalHeapMemory	context;
    
    AssertState(PortalManagerEnabled);
    AssertState(IsA(CurrentMemoryContext,PortalHeapMemory));
    
    context = (PortalHeapMemory)CurrentMemoryContext;
    AssertState(PointerIsValid(context->block));	/* XXX Trap(...) */
    
    /* free current mode */
    AllocSetReset(&HEAPMEMBLOCK(context)->setData);
    MemoryContextFree((MemoryContext)PortalHeapMemoryGetVariableMemory(context),
		      context->block);
    
    /* restore previous mode */
    context->block = FixedStackPop(&context->stackData);
}

/* ----------------
 *	PortalGetVariableMemory
 * ----------------
 */
PortalVariableMemory
PortalGetVariableMemory(portal)
    Portal	portal;
{
    return (&portal->variable);
}

/* ----------------
 *	PortalGetHeapMemory
 * ----------------
 */
PortalHeapMemory
PortalGetHeapMemory(portal)
    Portal	portal;
{
    return (&portal->heap);
}

/* ----------------
 *	PortalVariableMemoryGetPortal
 * ----------------
 */
Portal
PortalVariableMemoryGetPortal(context)
    PortalVariableMemory	context;
{
    return ((Portal)((char *)context - offsetof (PortalD, variable)));
}

/* ----------------
 *	PortalHeapMemoryGetPortal
 * ----------------
 */
Portal
PortalHeapMemoryGetPortal(context)
    PortalHeapMemory	context;
{
    return ((Portal)((char *)context - offsetof (PortalD, heap)));
}

/* ----------------
 *	PortalVariableMemoryGetHeapMemory
 * ----------------
 */
PortalHeapMemory
PortalVariableMemoryGetHeapMemory(context)
    PortalVariableMemory	context;
{
    return ((PortalHeapMemory)((char *)context
			       - offsetof (PortalD, variable)
			       + offsetof (PortalD, heap)));
}

/* ----------------
 *	PortalHeapMemoryGetVariableMemory
 * ----------------
 */
PortalVariableMemory
PortalHeapMemoryGetVariableMemory(context)
    PortalHeapMemory	context;
{
    return ((PortalVariableMemory)((char *)context
				   - offsetof (PortalD, heap)
				   + offsetof (PortalD, variable)));
}

