/*
 * mcxt.c --
 *	POSTGRES memory context code.
 */

#include "c.h"

RcsId("$Header$");

#include <stdio.h>	/* XXX for printf debugging */

#include "aset.h"
#include "enbl.h"
#include "excid.h"
#include "mnodes.h"
#include "oset.h"
#include "tags.h"	/* for classTag */
#include "tnodes.h"

#include "mcxt.h"

extern void bcopy();	/* XXX use header */

/*
 * Global State
 */
static Count MemoryContextEnableCount = 0;
#define MemoryContextEnabled	(MemoryContextEnableCount > 0)

static OrderedSetData	ActiveGlobalMemorySetData;	/* uninitialized */
#define ActiveGlobalMemorySet	(&ActiveGlobalMemorySetData)

MemoryContext	CurrentMemoryContext = NULL;

/*
 * description of allocated memory representation goes here
 */

#define PSIZE(PTR)	(*((int32 *)(PTR) - 1))
#define PSIZEALL(PTR)	(*((int32 *)(PTR) - 1) + sizeof (int32))
#define PSIZESKIP(PTR)	((char *)((int32 *)(PTR) + 1))
#define PSIZEFIND(PTR)	((char *)((int32 *)(PTR) - 1))
#define PSIZESPACE(LEN)	((LEN) + sizeof (int32))

/*
 * AllocSizeIsValid --
 *	True iff 0 < size and size <= MaxAllocSize.
 */
#define	AllocSizeIsValid(size)	SizeIsInBounds(size, MaxAllocSize)

/*
 * PRIVATE DEFINITIONS
 */

/*
 * GlobalMemory
 */

/*
 * GlobalMemoryAlloc --
 *	Returns pointer to aligned space in the global context.
 *
 * Exceptions:
 *	ExhaustedMemory if allocation fails.
 */
static
Pointer
GlobalMemoryAlloc ARGS((
	GlobalMemory	this,
	Size		size
));

/*
 * GlobalMemoryFree --
 *	Frees allocated memory in the global context.
 *
 * Exceptions:
 *	BadContextErr if current context is not the global context.
 *	BadArgumentsErr if pointer is invalid.
 */
static
void
GlobalMemoryFree ARGS((
	GlobalMemory	this,
	Pointer		pointer
));

/*
 * GlobalMemoryRealloc --
 *	Returns pointer to aligned space in the global context.
 *
 * Note:
 *	Memory associated with the pointer is freed before return.
 *
 * Exceptions:
 *	BadContextErr if current context is not the global context.
 *	BadArgumentsErr if pointer is invalid.
 *	NoMoreMemoryErr if allocation fails.
 */
static
Pointer
GlobalMemoryRealloc ARGS((
	GlobalMemory	this,
	Pointer		pointer,
	Size		size
));

/*
 * GlobalMemoryGetName --
 *	Returns name string for context.
 *
 * Exceptions:
 *	???
 */
static
String
GlobalMemoryGetName ARGS((
	GlobalMemory	this
));

/*
 * GlobalMemoryDump --
 *	Dumps global memory context for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
GlobalMemoryDump ARGS((
	GlobalMemory	this
));

/*
 * DumpGlobalMemories --
 *	Dumps all global memory contexts for debugging.
 *
 * Exceptions:
 *	???
 */
static
void
DumpGlobalMemories ARGS((
	GlobalMemory	this
));


/*
 * Global Memory Methods
 */

static MemoryContextMethodsData	GlobalContextMethodsData = {
	GlobalMemoryAlloc,	/* Pointer (*)(this, uint32)	palloc */
	GlobalMemoryFree,	/* void (*)(this, Pointer)	pfree */
	GlobalMemoryRealloc,	/* Pointer (*)(this, Pointer)	repalloc */
	GlobalMemoryGetName,	/* String (*)(this)		getName */
	GlobalMemoryDump	/* void (*)(this)		dump */
};

/*
 * Note:
 *	TopGlobalMemory is handled specially because of bootstrapping.
 */
static classObj(GlobalMemory)	TopGlobalMemoryData = {
	classTag(GlobalMemory),		/* NodeTag		tag */
	&GlobalContextMethodsData,	/* ContextMethods	method */
	{ 0 },	/* uninitialized */	/* OrderedSetData	allocSetD */
	"TopGlobal",			/* String		name */
	{ 0 }	/* uninitialized */	/* OrderedElemData	elemD */
};

MemoryContext	TopMemoryContext = (MemoryContext)&TopGlobalMemoryData;

/*
 * Module State
 */

void
EnableMemoryContext(on)
	bool	on;
{
	static bool	processing = false;

	AssertState(!processing);
	AssertArg(BoolIsValid(on));

	if (BypassEnable(&MemoryContextEnableCount, on)) {
		return;
	}

	processing = true;

	if (on) {	/* initialize */
		/* initialize TopGlobalMemoryData.setData */
		AllocSetInit(&TopGlobalMemoryData.setData, DynamicAllocMode,
			(Size)0);

		/* make TopGlobalMemoryData member of ActiveGlobalMemorySet */
		OrderedSetInit(ActiveGlobalMemorySet,
			offsetof(classObj(GlobalMemory), elemData));
		OrderedElemPushInto(&TopGlobalMemoryData.elemData,
			ActiveGlobalMemorySet);

		/* initialize CurrentMemoryContext */
		CurrentMemoryContext = TopMemoryContext;

	} else {	/* cleanup */
		GlobalMemory	context;

		/* walk the list of allocations */
		while (PointerIsValid(context = (GlobalMemory)
				OrderedSetGetHead(ActiveGlobalMemorySet))) {

			if (context == &TopGlobalMemoryData) {
				/* don't free it and clean it last */
				OrderedElemPop(&TopGlobalMemoryData.elemData);
			} else {
				GlobalMemoryDestroy(context);
			}
			/* what is needed for the top? */
		}

		/*
		 * Freeing memory here should be safe as this is called
		 * only after all modules which allocate in TopMemoryContext
		 * have been disabled.
		 */

		/* step through remaining allocations and log */
		/* AllocSetStep(...); */

		/* deallocate whatever is left */
		AllocSetReset(&TopGlobalMemoryData.setData);
	}

	processing = false;
}

Pointer
MemoryContextAlloc(context, size)
	MemoryContext	context;
	Size		size;
{
	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));
	AssertArg(SizeIsValid(size));

	LogTrap(!AllocSizeIsValid(size), BadAllocSize,
		("size=%d [0x%x]", size, size));

	return (context->method->alloc(context, size));
}

void
MemoryContextFree(context, pointer)
	MemoryContext	context;
	Pointer		pointer;
{
	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));
	AssertArg(PointerIsValid(pointer));

	context->method->free(context, pointer);
}

Pointer
MemoryContextRealloc(context, pointer, size)
	MemoryContext	context;
	Pointer		pointer;
	Size		size;
{
	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));
	AssertArg(PointerIsValid(pointer));
	AssertArg(SizeIsValid(size));

	LogTrap(!AllocSizeIsValid(size), BadAllocSize,
		("size=%d [0x%x]", size, size));

	return (context->method->realloc(context, pointer, size));
}

String
MemoryContextGetName(context)
	MemoryContext	context;
{
	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));

	return (context->method->getName(context));
}

Size
PointerGetAllocSize(pointer)
	Pointer	pointer;
{
	AssertState(MemoryContextEnabled);
	AssertArg(PointerIsValid(pointer));

	return (PSIZE(pointer));
}

MemoryContext
MemoryContextSwitchTo(context)
	MemoryContext	context;
{
	MemoryContext	old;

	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));

	old = CurrentMemoryContext;
	CurrentMemoryContext = context;
	return (old);
}

/*
 * START HERE
 *	Add routines to move memory between contexts.
 */

/*
 * DEBUGGING
 */
void
DumpMemoryContexts()
{
	if (!MemoryContextEnabled) {
		printf("!MemoryContextEnabled\n");
		return;
	}

	DumpGlobalMemories();
/*
 *  There should also be a standard log interface (DEBUG).
 *
 *  Also, add file pcxt for the portal memory context (and gcxt?).
 *
 */


}

/*
 * External Functions
 */

GlobalMemory
CreateGlobalMemory(name)
	String	name;	/* XXX MemoryContextName */
{
	GlobalMemory	context;

	AssertState(MemoryContextEnabled);
	AssertState(CurrentMemoryContext == TopMemoryContext);
	AssertArg(StringIsValid(name));	/* XXX MemoryContextName */

	context = NewNode(GlobalMemory);
	context->method = &GlobalContextMethodsData;
	context->name = name;		/* assumes name is static */
	AllocSetInit(&context->setData, DynamicAllocMode, (Size)0);

	/* link the context */
	OrderedElemPushInto(&context->elemData, ActiveGlobalMemorySet);

	return (context);
}

void
GlobalMemoryDestroy(context)
	GlobalMemory	context;
{
	AssertState(MemoryContextEnabled);
	AssertState(CurrentMemoryContext == TopMemoryContext);
	AssertArg(IsA(context, classTag(GlobalMemory)));
	AssertArg(context != &TopGlobalMemoryData);

	/* step through remaining allocations and log */
	/* AllocSetItterate(...); */

	/* cleanup by deallocating whatever is left */
	AllocSetReset(&context->setData);

	/* unlink and delete the context */
	OrderedElemPop(&context->elemData);
	MemoryContextFree(TopMemoryContext, (Pointer)context);
}

/*
 * PRIVATE
 */

static
Pointer
GlobalMemoryAlloc(this, size)
	GlobalMemory	this;
	Size		size;
{
	return (AllocSetAlloc(&this->setData, size));
}

static
void
GlobalMemoryFree(this, pointer)
	GlobalMemory	this;
	Pointer		pointer;
{
	AllocSetFree(&this->setData, pointer);
}

static
Pointer
GlobalMemoryRealloc(this, pointer, size)
	GlobalMemory	this;
	Pointer		pointer;
	Size		size;
{
	return (AllocSetRealloc(&this->setData, pointer, size));
}

static
String
GlobalMemoryGetName(this)
	GlobalMemory	this;
{
	return (this->name);
}

static
void
GlobalMemoryDump(this)
	GlobalMemory	this;
{
	GlobalMemory	context;

	printf("--\n%s:\n", GlobalMemoryGetName(this));

	context = (GlobalMemory)OrderedElemGetPredecessor(&this->elemData);
	if (PointerIsValid(context)) {
		printf("\tpredecessor=%s\n", GlobalMemoryGetName(context));
	}

	context = (GlobalMemory)OrderedElemGetSuccessor(&this->elemData);
	if (PointerIsValid(context)) {
		printf("\tsucessor=%s\n", GlobalMemoryGetName(context));
	}

	AllocSetDump(&this->setData);	/* XXX is this right interface */
}

static
void
DumpGlobalMemories()
{
	GlobalMemory	context;

	context = (GlobalMemory)OrderedSetGetHead(&ActiveGlobalMemorySetData);

	while (PointerIsValid(context)) {
		GlobalMemoryDump(context);

		context = (GlobalMemory)OrderedElemGetSuccessor(
			&context->elemData);
	}
}
