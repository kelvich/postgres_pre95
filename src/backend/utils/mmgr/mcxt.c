/*
 * mcxt.c --
 *	POSTGRES memory context code.
 */

#include <stdio.h>	/* XXX for printf debugging */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/memutils.h"
#include "utils/module.h"
#include "utils/excid.h"

#include "nodes/mnodes.h"
#include "nodes/nodes.h"
#include "tags.h"	/* for classTag */

#include "utils/mcxt.h"
#include "utils/log.h"

#include "utils/palloc.h"

extern void bcopy();	/* XXX use header */

#undef MemoryContextAlloc
#undef MemoryContextFree
#undef malloc
#undef free

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
static bool
MemoryContextIsValid ARGS((
        MemoryContext context;
));
*/

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
extern void PrintGlobalMemory();
/* extern bool EqualGlobalMemory(); */

static classObj(GlobalMemory)	TopGlobalMemoryData   = {
	classTag(GlobalMemory),		/* NodeTag		tag 	  */
	PrintGlobalMemory,		/* (print function) 		  */
	NULL,				/* (equal function) 		  */
	NULL,				/* copy function 		  */
	&GlobalContextMethodsData,	/* ContextMethods	method    */
	{ 0 },	/* uninitialized */	/* OrderedSetData	allocSetD */
	"TopGlobal",			/* String		name      */
	{ 0 }	/* uninitialized */	/* OrderedElemData	elemD     */
};


MemoryContext	TopMemoryContext =  (MemoryContext)&TopGlobalMemoryData;


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

	context->method->free_p(context, pointer);
}

extern SLList PallocDebugList;
extern SLList *PallocList;
extern SLList *PfreeList;
extern int PallocDiffTag;
extern bool PallocRecord;
extern bool PallocNoisy;

Pointer
MemoryContextAlloc_Debug(file, line, context, size)
String file;
int line;
MemoryContext context;
Size size;
{
	Pointer p;
	PallocDebugData *d, *d1;

	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));
	AssertArg(SizeIsValid(size));

	LogTrap(!AllocSizeIsValid(size), BadAllocSize,
		("size=%d [0x%x]", size, size));

	p = context->method->alloc(context, size);
	if (PallocRecord) {
	    d = (PallocDebugData*)malloc(sizeof(PallocDebugData));
	    d->pointer = p;
	    d->size = size;
	    d->line = line;
	    d->file = file;
	    d->context = MemoryContextGetName(context);

	    SLNewNode(&(d->Link));
	    SLAddTail(&PallocDebugList, &(d->Link));
	    if (PallocDiffTag) {
		d1 = (PallocDebugData*)malloc(sizeof(PallocDebugData));
		d1->pointer = p;
		d1->size = size;
		d1->line = line;
		d1->file = file;
		d1->context = d->context;
		SLNewNode(&(d1->Link));
		SLAddTail(PallocList, &(d1->Link));
	      }
	}
	if (PallocNoisy)
	    printf("!+ f: %s l: %d p: 0x%x s: %d %s\n",
		   file, line, p, size, MemoryContextGetName(context));
	return (p);
}

void
MemoryContextFree_Debug(file, line, context, pointer)
String file;
int line;
MemoryContext	context;
Pointer		pointer;
{
	PallocDebugData *d;

	AssertState(MemoryContextEnabled);
	AssertArg(MemoryContextIsValid(context));
	AssertArg(PointerIsValid(pointer));

	if (PallocRecord) {
	    d = (PallocDebugData*)SLGetHead(&PallocDebugList);
	    while (d != NULL) {
		if (d->pointer == pointer) {
		    SLRemove(&(d->Link));
		    break;
		  }
		d = (PallocDebugData*)SLGetSucc(&(d->Link));
	      }
	    if (d != NULL) {
		if (PallocDiffTag)
		    SLAddTail(PfreeList, &(d->Link));
		else
		    free(d);
	      }
	    else
		elog(NOTICE, "pfree_remove l:%d f:%s p:0x%x %s",
		     line, file, pointer, "** not on list **");
	   }
	if (PallocNoisy)
	    printf("!- f: %s l: %d p: 0x%x %s\n",
		   file, line, pointer, context);

	context->method->free_p(context, pointer);
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
	MemoryContext	savecxt;

	AssertState(MemoryContextEnabled);
	AssertArg(StringIsValid(name));	/* XXX MemoryContextName */

	savecxt = MemoryContextSwitchTo(TopMemoryContext);
	
	context = CreateNode(GlobalMemory);
	context->method = &GlobalContextMethodsData;
	context->name = name;		/* assumes name is static */
	AllocSetInit(&context->setData, DynamicAllocMode, (Size)0);

	/* link the context */
	OrderedElemPushInto(&context->elemData, ActiveGlobalMemorySet);

	MemoryContextSwitchTo(savecxt);
	return (context);
}

void
GlobalMemoryDestroy(context)
	GlobalMemory	context;
{
	AssertState(MemoryContextEnabled);
	AssertArg(IsA(context,GlobalMemory));
	AssertArg(context != &TopGlobalMemoryData);

	AllocSetReset(&context->setData);

	/* unlink and delete the context */
	OrderedElemPop(&context->elemData);
#ifdef PALLOC_DEBUG
	MemoryContextFree_Debug(__FILE__, __LINE__, 
				TopMemoryContext, (Pointer)context);
#else
	MemoryContextFree(TopMemoryContext, (Pointer)context);
#endif
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
bool
MemoryContextIsValid(context)
	MemoryContext	context;
{
	return((bool)IsA(context,MemoryContext));
}

void
PrintGlobalMemory(foo)
     GlobalMemory foo;
{
}
