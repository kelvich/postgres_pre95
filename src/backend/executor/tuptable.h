/* ----------------------------------------------------------------
 *   FILE
 *     	tuptable.h
 *
 *   DESCRIPTION
 *     	tuple table support stuff
 *
 *   NOTES
 *	The tuple table interface is getting pretty ugly.
 *	It should be redesigned soon.
 *
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef ExecTupTableHIncluded
#define ExecTupTableHIncluded 1

/* ----------------
 *	tuple table data structure
 * ----------------
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
 *	SlotShouldFree		- flag: T if should call pfree() on contents
 *	SetSlotShouldFree	- sets "should call pfree()" flag
 *	SlotTupleDescriptor	- gets type info for contents of a slot
 *	SetSlotTupleDescriptor	- assigns type info for contents of a slot
 *	SlotTupleDescriptorIsNew -  flag: T when type info changes
 *	SetSlotTupleDescriptorIsNew - sets "type info has changed" flag
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
    
#define SlotTupleDescriptor(slot) \
    get_ttc_tupleDescriptor(slot)

#define SlotExecTupDescriptor(slot) \
    get_ttc_execTupDescriptor(slot)

#define SetSlotTupleDescriptor(slot, desc) \
    set_ttc_tupleDescriptor(slot, desc)
        
#define SetSlotExecTupDescriptor(slot, desc) \
    set_ttc_execTupDescriptor(slot, desc)
        
#define SlotTupleDescriptorIsNew(slot) \
    get_ttc_descIsNew(slot)

#define SetSlotTupleDescriptorIsNew(slot, isnew) \
    set_ttc_descIsNew(slot, isnew)

#define SetSlotWhichPlan(slot, plannum) \
    set_ttc_whichplan(slot, plannum)

#define SlotBuffer(slot) \
    get_ttc_buffer(slot)

#define SetSlotBuffer(slot, b) \
    set_ttc_buffer(slot, b)
    
#define SlotSpecialInfo(slot) \
    (Pointer) CDR((List) slot)

#define SetSlotSpecialInfo(slot, ptr) \
    CDR((List) slot) = (List) ptr

#define NewTableSize(oldsize) \
    (oldsize * 2 + 20)

#define TupIsNull(x)	ExecNullSlot(x)

#define ExecFetchTuple(slot) ((Pointer)((slot) ? SlotContents(slot) : NULL))

#define ExecSlotDescriptor(slot) \
	((TupleDescriptor)SlotTupleDescriptor((TupleTableSlot) slot))

#define ExecSlotExecDescriptor(slot) \
	((ExecTupDescriptor)SlotExecTupDescriptor((TupleTableSlot) slot))

#define ExecSetSlotExecDescriptor(slot, exectupdesc) \
	SetSlotExecTupDescriptor((TupleTableSlot)slot, \
				 (ExecTupDescriptor)exectupdesc)
#define ExecSlotBuffer(slot) ((Buffer)SlotBuffer((TupleTableSlot) slot))

#endif ExecTupTableHIncluded
