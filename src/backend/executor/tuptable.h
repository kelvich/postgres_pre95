#ifndef ExecTupTableHIncluded
#define ExecTupTableHIncluded 1
/* ----------------------------------------------------------------
 *		      tuple table support stuff
 *	$Header$
 * ----------------------------------------------------------------
 */

typedef struct TupleTableData {
    int		size;		/* size of the table */
    int		next;		/* next available slot number */
    Pointer	array;		/* pointer to the table slots */
} TupleTableData;

typedef TupleTableData *TupleTable;

/* ----------------
 *	tuple table macros
 *
 *	TableSlotSize		- size of one slot in the table
 *	TableSlot		- gets address of a slot
 *	SlotContents		- gets contents of a slot
 *	SetSlotContents		- assigns slot's contents
 *	SlotSpecialInfo		- gets special information regarding slot
 *	SetSlotSpecialInfo	- assigns slot's special information
 *	NewTableSize		- calculates new size if old size was too small
 *
 *	Nothing currently uses the SpecialInfo, but someday something might.
 * ----------------
 */
#define TableSlotSize \
    sizeof(classObj(TupleTableSlot))

#define TableSlot(array, i) \
    (Pointer) &(((classObj(TupleTableSlot) *) array)[i])

#define InitSlot(slot) \
    RInitTupleTableSlot((Pointer) slot)

#define SlotContents(slot) \
    (Pointer) CAR((List) slot)

#define SetSlotContents(slot, ptr) \
    CAR((List) slot) = (List) ptr

#define SlotShouldFree(slot) \
    get_ttc_shouldFree(slot)

#define SetSlotShouldFree(slot, shouldFree) \
    set_ttc_shouldFree(slot, shouldFree)
    
#define SlotSpecialInfo(slot) \
    (Pointer) CDR((List) slot)

#define SetSlotSpecialInfo(slot, ptr) \
    CDR((List) slot) = (List) ptr

#define NewTableSize(oldsize) \
    (oldsize * 2 + 20)

/* ----------------------------------------------------------------
 *	TupIsNull is a #define to the correct thing to use
 *	while the tuple table stuff is integrated..  It will
 *	go away once the tuple table stuff is robust.
 * ----------------------------------------------------------------
 */
#define TupIsNull(x)	ExecNullSlot(x)

#endif ExecTupTableHIncluded
