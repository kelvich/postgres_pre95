/* ----------------------------------------------------------------
 *   FILE
 *	memutils.h
 *
 *   DESCRIPTION
 *	this file contains general memory alignment, allocation
 *	and manipulation stuff that used to be spread out
 *	between the following files:
 *
 *	align.h				alignment macros
 *	aset.h				memory allocation set stuff
 *	oset.h				  (used by aset.h)
 *	bit.h				bit array type / extern
 *	clib.h				mem routines
 *	limit.h				max bits/byte, etc.
 *
 *   NOTES
 *	some of the information in this file will be moved to
 *	other files, (like MaxHeapTupleSize and MaxAttributeSize).
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef MemutilsHIncluded
#define MemutilsHIncluded 1

#include "tmp/c.h"

/* ----------------
 *	align.h
 * ----------------
 */
#ifndef	_ALIGN_H_
#define	_ALIGN_H_	"$Header$"

/*
 *	align.h		- alignment macros
 */

/*
 *	SHORTALIGN(LEN)	- length (or address) aligned for shorts
 */
#define	SHORTALIGN(LEN)\
	(((long)(LEN) + 1) & ~01)

/*
 *	LONGALIGN(LEN)	- length (or address) aligned for longs
 */
#if defined(sun) && ! defined(sparc)
#define	LONGALIGN(LEN)	SHORTALIGN(LEN)
#else 
#define	LONGALIGN(LEN)\
	(((long)(LEN) + 3) & ~03)
#endif

#endif _ALIGN_H_

/* ----------------
 *	bit.h
 * ----------------
 */
/*
 * bit.h --
 *	Standard bit array definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	BitIncluded	/* Include this file only once. */
#define BitIncluded	1

/* #include "tmp/c.h" (now at the top of memutils.h) */

typedef bits8	*BitArray;
typedef uint32	BitIndex;

#define BitsPerByte	8

/*
 * NumberOfBitsPerByte --
 *	Returns the number of bits per byte.
 */
int
NumberOfBitsPerByte ARGS((
	void
));

/*
 * BitArraySetBit --
 *	Sets (to 1) the value of a bit in a bit array.
 */
void
BitArraySetBit ARGS((
	BitArray	bitArray,
	BitIndex	bitIndex
));

/*
 * BitArrayClearBit --
 *	Clears (to 0) the value of a bit in a bit array.
 */
void
BitArrayClearBit ARGS((
	BitArray	bitArray,
	BitIndex	bitIndex
));

/*
 * BitArrayBitIsSet --
 *	True iff the bit is set (1) in a bit array.
 */
bool
BitArrayBitIsSet ARGS((
	BitArray	bitArray,
	BitIndex	bitIndex
));

/*
 * BitArrayCopy --
 *	Copys the contents of one bit array into another.
 */
void
BitArrayCopy ARGS((
	BitArray	fromBitArray,
	BitArray	toBitArray,
	BitIndex	fromBitIndex,
	BitIndex	toBitIndex,
	BitIndex	numberOfBits
));

/*
 * BitArrayZero --
 *	Zeros the contents of a bit array.
 */
void
BitArrayZero ARGS((
	BitArray	bitArray,
	BitIndex	bitIndex,
	BitIndex	numberOfBits
));

#endif	/* !defined(BitIncluded) */

/* ----------------
 *	oset.h
 * ----------------
 */
/*
 * oset.h --
 *	Fixed format ordered set definitions.
 *
 * Note:
 *	Fixed format ordered sets are <EXPLAIN>.
 *	XXX This is a preliminary version.  Work is needed to explain
 *	XXX semantics of the external definitions.  Otherwise, the
 *	XXX functional interface should not change.
 *
 * Identification:
 *	$Header$
 */

#ifndef	OSetIncluded		/* Include this only once */
#define OSetIncluded	1

/* #include "tmp/c.h" 	(now at the top of memutils.h) */

typedef struct OrderedElemData OrderedElemData;
typedef OrderedElemData* OrderedElem;

typedef struct OrderedSetData OrderedSetData;
typedef OrderedSetData* OrderedSet;

struct OrderedElemData {
	OrderedElem	next;	/* Next elem or &this->set->dummy	*/
	OrderedElem	prev;	/* Previous elem or &this->set->head	*/
	OrderedSet	set;	/* Parent set				*/
};

struct OrderedSetData {
	OrderedElem	head;	/* First elem or &this->dummy		*/
	OrderedElem	dummy;	/* (hack) Terminator == NULL		*/
	OrderedElem	tail;	/* Last elem or &this->head		*/
	Offset		offset;	/* Offset from struct base to elem	*/
	/* this could be signed short int! */
};

/*
 * OrderedSetInit --
 */
extern
void
OrderedSetInit ARGS((
	OrderedSet	set,
	Offset		offset
));

/*
 * OrderedElemInit --
 */
extern
void
OrderedElemInit ARGS((
	OrderedElem	elem,
	OrderedSet	set
));

/*
 * OrderedSetContains --
 *	True iff ordered set contains given element.
 */
extern
bool
OrderedSetContains ARGS((
	OrderedSet	set,
	OrderedElem	elem
));

/*
 * OrderedSetGetHead --
 */
extern
Pointer
OrderedSetGetHead ARGS((
	OrderedSet	set
));

/*
 * OrderedSetGetTail --
 */
extern
Pointer
OrderedSetGetTail ARGS((
	OrderedSet	set
));

/*
 * OrderedElemGetPredecessor --
 */
extern
Pointer
OrderedElemGetPredecessor ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemGetSuccessor --
 */
extern
Pointer
OrderedElemGetSuccessor ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemPop --
 */
extern
void
OrderedElemPop ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemPushInto --
 */
extern
void
OrderedElemPushInto ARGS((
	OrderedElem	elem,
	OrderedSet	set
));

/*
 * OrderedElemPush --
 */
extern
void
OrderedElemPush ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemPushHead --
 */
extern
void
OrderedElemPushHead ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemPushTail --
 */
extern
void
OrderedElemPushTail ARGS((
	OrderedElem	elem
));

/*
 * OrderedElemPushAfter --
 */
extern
void
OrderedElemPushAfter ARGS((
	OrderedElem	elem,
	OrderedElem	oldElem
));

/*
 * OrderedElemPushBefore --
 */
extern
void
OrderedElemPushBefore ARGS((
	OrderedElem	elem,
	OrderedElem	oldElem
));

/*
 * OrderedSetPop --
 */
extern
Pointer
OrderedSetPop ARGS((
	OrderedSet	set
));

/*
 * OrderedSetPopHead --
 */
extern
Pointer
OrderedSetPopHead ARGS((
	OrderedSet	set
));

/*
 * OrderedSetPopTail --
 */
extern
Pointer
OrderedSetPopTail ARGS((
	OrderedSet	set
));

#endif	/* !defined(OSetIncluded) */

/* ----------------
 *	aset.h
 * ----------------
 */
/*
 * aset.h --
 *	Allocation set definitions.
 *
 * Identification:
 *	$Header$
 *
 * Description:
 *	An allocation set is a set containing allocated elements.  When
 *	an allocation is requested for a set, memory is allocated and a
 *	pointer is returned.  Subsequently, this memory may be freed or
 *	reallocated.  In addition, an allocation set may be reset which
 *	will cause all allocated memory to be freed.
 *
 *	Allocations may occur in four different modes.  The mode of
 *	allocation does not affect the behavior of allocations except in
 *	terms of performance.  The allocation mode is set at the time of
 *	set initialization.  Once the mode is chosen, it cannot be changed
 *	unless the set is reinitialized.
 *
 *	"Dynamic" mode forces all allocations to occur in a heap.  This
 *	is a good mode to use when small memory segments are allocated
 *	and freed very frequently.  This is a good choice when allocation
 *	characteristics are unknown.  This is the default mode.
 *
 *	"Static" mode attemts to allocate space as efficiently as possible
 *	without regard to freeing memory.  This mode should be chosen only
 *	when it is known that many allocations will occur but that very
 *	little of the allocated memory will be explicitly freed.
 *
 *	"Tunable" mode is a hybrid of dynamic and static modes.  The
 *	tunable mode will use static mode allocation except when the
 *	allocation request exceeds a size limit supplied at the time of set
 *	initialization.  "Big" objects are allocated using dynamic mode.
 *
 *	"Bounded" mode attempts to allocate space efficiently given a limit
 *	on space consumed by the allocation set.  This restriction can be
 *	considered a "soft" restriction, because memory segments will
 *	continue to be returned after the limit is exceeded.  The limit is
 *	specified at the time of set initialization like for tunable mode.
 *
 * Note:
 *	Allocation sets are not automatically reset on a system reset.
 *	Higher level code is responsible for cleaning up.
 *
 *	There may other modes in the future.
 */

#ifndef	ASetIncluded		/* Include this file only once */
#define ASetIncluded	1

/* #include "tmp/c.h" (now at the top of memutils.h) */
/* #include "tmp/oset.h" (now just above this stuff in memutils.h) */

/*
 * AllocPointer --
 *	Aligned pointer which may be a member of an allocation set.
 */
typedef Pointer AllocPointer;

/*
 * AllocMode --
 *	Mode of allocation for an allocation set.
 *
 * Note:
 *	See above for a description of the various nodes.
 */
typedef enum AllocMode {
	DynamicAllocMode,	/* always dynamically allocate */
	StaticAllocMode,	/* always "statically" allocate */
	TunableAllocMode,	/* allocations are "tuned" */
	BoundedAllocMode	/* allocations bounded to fixed usage */

#define DefaultAllocMode	DynamicAllocMode
} AllocMode;

/*
 * AllocSet --
 *	Allocation set.
 */

typedef struct AllocSetData {
	OrderedSetData	setData;
	/* Note: this will change in the future to support other modes */
} AllocSetData;

typedef AllocSetData *AllocSet;

/*
 * AllocPointerIsValid --
 *	True iff pointer is valid allocation pointer.
 */
extern
bool
AllocPointerIsValid ARGS((
	AllocPointer	pointer
));

/*
 * AllocSetIsValid --
 *	True iff set is valid allocation set.
 */
extern
bool
AllocSetIsValid ARGS((
	AllocSet	set
));

/*
 * AllocSetInit --
 *	Initializes given allocation set.
 *
 * Note:
 *	The semantics of the mode are explained above.  Limit is ignored
 *	for dynamic and static modes.
 *
 * Exceptions:
 *	BadArg if set is invalid pointer.
 *	BadArg if mode is invalid.
 */
extern
void
AllocSetInit ARGS((
	AllocSet	set,
	AllocMode	mode,
	Size		limit
));

/*
 * AllocSetReset --
 *	Frees memory which is allocated in the given set.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 */
extern
void
AllocSetReset ARGS((
	AllocSet	set
));

/*
 * AllocSetContains --
 *	True iff allocation set contains given allocation element.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 *	BadArg if pointer is invalid.
 */
extern
bool
AllocSetContains ARGS((
	AllocSet	set,
	AllocPointer	pointer
));

/*
 * AllocSetAlloc --
 *	Returns pointer to allocated memory of given size; memory is added
 *	to the set.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 *	MemoryExhausted if allocation fails.
 */
extern
AllocPointer
AllocSetAlloc ARGS((
	AllocSet	set,
	Size		size
));

/*
 * AllocSetFree --
 *	Frees allocated memory; memory is removed from the set.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 *	BadArg if pointer is invalid.
 *	BadArg if pointer is not member of set.
 */
extern
void
AllocSetFree ARGS((
	AllocSet	set,
	AllocPointer	pointer
));

/*
 * AllocSetRealloc --
 *	Returns new pointer to allocated memory of given size; this memory
 *	is added to the set.  Memory associated with given pointer is copied
 *	into the new memory, and the old memory is freed.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 *	BadArg if pointer is invalid.
 *	BadArg if pointer is not member of set.
 *	MemoryExhausted if allocation fails.
 */
extern
AllocPointer
AllocSetRealloc ARGS((
	AllocSet	set,
	AllocPointer	pointer,
	Size		size
));


/*
 * AllocSetIterate --
 *	Returns size of set.  Iterates through set elements calling function
 *	(if valid) on each.
 *
 * Note:
 *	This was written as an aid to debugging.  It is intended for
 *	debugging use only.
 *
 * Exceptions:
 *	BadArg if set is invalid.
 */
extern
Count
AllocSetStep ARGS((
	AllocSet	set,
	void		(*function) ARGS((AllocPointer pointer))
));

#endif	/* !defined(ASetIncluded) */

/* ----------------
 *	clib.h
 * ----------------
 */
/*
 * clib.h --
 *	Standard C library definitions.
 *
 * Note:
 *	This file is OPERATING SYSTEM dependent!!!
 *
 * Identification:
 *	$Header$
 */

#ifndef	CLibIncluded	/* Include this file only once. */
#define CLibIncluded	1

/* #include "tmp/c.h" (now at the top of memutils.h) */
#include <memory.h>

/* 
 *	LibCCopyLength is only used within this file. -cim 6/12/90
 * 
 */
typedef int	LibCCopyLength;

typedef 	CLibCopyLength;

/*
 * MemoryCopy --
 *	Copies fixed length block of memory to another.
 */
#define MemoryCopy(toBuffer, fromBuffer, length)\
    memcpy(toBuffer, fromBuffer, length)

#endif	/* !defined(CLibIncluded) */

/* ----------------
 *	limit.h
 * ----------------
 */
/*
 * limit.h --
 *	POSTGRES limit definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	LimitIncluded	/* Include this file only once. */
#define LimitIncluded	1

/* #include "tmp/c.h" (now at the top of memutils.h) */

#define MaxBitsPerByte	8

typedef uint32	AttributeSize;	/* XXX should be defined elsewhere */

#define MaxHeapTupleSize	0x7fffffff
#define MaxAttributeSize	0x7fffffff

#define MaxIndexAttributeNumber	7

/*
 * AttributeSizeIsValid --
 *	True iff the attribute size is valid.
 */
extern
bool
AttributeSizeIsValid ARGS((
	AttributeSize	size
));

/*
 * TupleSizeIsValid --
 *	True iff the tuple size is valid.
 */
extern
bool
TupleSizeIsValid ARGS((
	TupleSize	size
));

#endif	/* !defined(LimitIncluded) */

#endif MemutilsHIncluded
