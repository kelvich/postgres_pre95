/*
 * aset.c --
 *	Allocation set definitions.
 *
 * Note:
 *	XXX This is a preliminary implementation which lacks fail-fast
 *	XXX validity checking of arguments.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/excid.h"	/* for ExhaustedMemory */
#include "utils/memutils.h"
#include "utils/log.h"

#undef AllocSetReset
#undef malloc
#undef free

/*
 * Internal type definitions
 */

/*
 * AllocElem --
 *	Allocation element.
 */
typedef struct AllocElemData {
	OrderedElemData	elemData;	/* elem in AllocSet */
	Size		size;
} AllocElemData;

typedef AllocElemData *AllocElem;


/*
 * Private method definitions
 */

/*
 * AllocPointerGetAllocElem --
 *	Returns allocation (internal) elem given (external) pointer.
 */
#define AllocPointerGetAllocElem(pointer)	(&((AllocElem)(pointer))[-1])

/*
 * AllocElemGetAllocPointer --
 *	Returns allocation (external) pointer given (internal) elem.
 */
#define AllocElemGetAllocPointer(alloc)	((AllocPointer)&(alloc)[1])

/*
 * AllocElemIsValid --
 *	True iff alloc is valid.
 */
#define AllocElemIsValid(alloc)	PointerIsValid(alloc)

/*
 * AllocSetGetFirst --
 *	Returns "first" allocation pointer in a set.
 *
 * Note:
 *	Assumes set is valid.
 */
static
AllocPointer
AllocSetGetFirst ARGS((
	AllocSet	set
));

/*
 * AllocPointerGetNext --
 *	Returns "successor" allocation pointer.
 *
 * Note:
 *	Assumes pointer is valid.
 */
static
AllocPointer
AllocPointerGetNext ARGS((
	AllocPointer	pointer
));

/*
 * Public routines
 */

/* 
 *	AllocPointerIsValid(pointer)
 * 	AllocSetIsValid(set)    
 *
 *		.. are now macros in aset.h -cim 4/27/91
 */

void
AllocSetInit(set, mode, limit)
	AllocSet	set;
	AllocMode	mode;
	Size		limit;
{
	AssertArg(PointerIsValid(set));
	AssertArg((int)DynamicAllocMode <= (int)mode);
	AssertArg((int)mode <= (int)BoundedAllocMode);

	/*
	 * XXX mode is currently ignored and treated as DynamicAllocMode.
	 * XXX limit is also ignored.  This affects this whole file.
	 */

	OrderedSetInit(&set->setData, offsetof(AllocElemData, elemData));
}

void
AllocSetReset(set)
	AllocSet	set;
{
	AllocPointer	pointer;

	AssertArg(AllocSetIsValid(set));

	while (AllocPointerIsValid(pointer = AllocSetGetFirst(set))) {
	    AllocSetFree(set, pointer);
	}
}

void
AllocSetReset_debug(file, line, set)
    String      file;
    int	        line;
    AllocSet	set;
{
    AllocPointer	pointer;

    AssertArg(AllocSetIsValid(set));

    while (AllocPointerIsValid(pointer = AllocSetGetFirst(set))) {
	alloc_set_message(file, line, pointer, set);
	AllocSetFree(set, pointer);
    }
}

bool
AllocSetContains(set, pointer)
	AllocSet	set;
	AllocPointer	pointer;
{
	AssertArg(AllocSetIsValid(set));
	AssertArg(AllocPointerIsValid(pointer));

	return (OrderedSetContains(&set->setData,
		&AllocPointerGetAllocElem(pointer)->elemData));
}

AllocPointer
AllocSetAlloc(set, size)
	AllocSet	set;
	Size		size;
{
	AllocElem	alloc;

	AssertArg(AllocSetIsValid(set));

	/* allocate */
	alloc = (AllocElem)malloc(sizeof (*alloc) + size);

	if (!PointerIsValid(alloc)) {
		elog (FATAL, "palloc failure: memory exhausted");
	}

	/* add to allocation list */
	OrderedElemPushInto(&alloc->elemData, &set->setData);

	/* set size */
	alloc->size = size;

	return (AllocElemGetAllocPointer(alloc));
}

void
AllocSetFree(set, pointer)
	AllocSet	set;
	AllocPointer	pointer;
{
	AllocElem	alloc;

	/* AssertArg(AllocSetIsValid(set)); */
	/* AssertArg(AllocPointerIsValid(pointer)); */
	AssertArg(AllocSetContains(set, pointer));

	alloc = AllocPointerGetAllocElem(pointer);

	/* remove from allocation set */
	OrderedElemPop(&alloc->elemData);

	/* free storage */
	delete(alloc);
}

AllocPointer
AllocSetRealloc(set, pointer, size)
	AllocSet	set;
	AllocPointer	pointer;
	Size		size;
{
	AllocPointer	newPointer;
	AllocElem	alloc;

	/* AssertArg(AllocSetIsValid(set)); */
	/* AssertArg(AllocPointerIsValid(pointer)); */
	AssertArg(AllocSetContains(set, pointer));

	/*
	 * Calling realloc(3) directly is not be possible (unless we use
	 * our own hacked version of malloc) since we must keep the
	 * allocations in the allocation set.
	 */

	alloc = AllocPointerGetAllocElem(pointer);

	/* allocate new pointer */
	newPointer = AllocSetAlloc(set, size);

	/* fill new memory */
	bcopy(pointer, newPointer, (alloc->size < size) ? alloc->size : size);

	/* free old pointer */
	AllocSetFree(set, pointer);

	return (newPointer);
}

Count
AllocSetIterate(set, function)
	AllocSet	set;
	void		(*function) ARGS((AllocPointer pointer));
{
	Count		count = 0;
	AllocPointer	pointer;

	AssertArg(AllocSetIsValid(set));

	for (pointer = AllocSetGetFirst(set);
			AllocPointerIsValid(pointer);
			pointer = AllocPointerGetNext(pointer)) {

		if (PointerIsValid(function)) {
			(*function)(pointer);
		}
		count += 1;
	}

	return (count);
}

Count
AllocSetCount(set)
AllocSet set;
{
	Count		count = 0;
	AllocPointer	pointer;

	AssertArg(AllocSetIsValid(set));

	for (pointer = AllocSetGetFirst(set);
			AllocPointerIsValid(pointer);
			pointer = AllocPointerGetNext(pointer)) {
		count++;
	}
	return count;
}

/*
 * Private routines
 */

static
AllocPointer
AllocSetGetFirst(set)
	AllocSet	set;
{
	AllocElem	alloc;

	alloc = (AllocElem)OrderedSetGetHead(&set->setData);

	if (!AllocElemIsValid(alloc)) {
		return (NULL);
	}

	return (AllocElemGetAllocPointer(alloc));
}

static
AllocPointer
AllocPointerGetNext(pointer)
	AllocPointer	pointer;
{
	AllocElem	alloc;

	alloc = (AllocElem)OrderedElemGetSuccessor(
		&AllocPointerGetAllocElem(pointer)->elemData);

	if (!AllocElemIsValid(alloc)) {
		return (NULL);
	}

	return (AllocElemGetAllocPointer(alloc));
}


/*
 * Debugging routines
 */

/*
 * XXX AllocPointerDump --
 *	Displays allocated pointer.
 */
void
AllocPointerDump(pointer)
	AllocPointer	pointer;
{
	printf("\t%-10d@ %0#x\n", ((long*)pointer)[-1], pointer); /* XXX */
}

/*
 * AllocSetDump --
 *	Displays allocated set.
 */
void
AllocSetDump(set)
	AllocSet	set;
{
	int count;
	count = AllocSetIterate(set, AllocPointerDump);
	printf("\ttotal %d allocations\n", count);
}
