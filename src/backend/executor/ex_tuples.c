/* ----------------------------------------------------------------
 *   FILE
 *	ex_tuples.c 
 *	
 *   DESCRIPTION
 *	Routines dealing with the executor tuple tables.  These
 *	are used to ensure that the executor frees copies of tuples
 *	(made by ExecTargetList) properly.
 *
 *   	Routines dealing with the type inforamtion for tuples.
 *   	Currently, the type information for a tuple is an array of
 *   	struct attribute. This information is needed by routines
 *   	manipulating tuples (getattribute, formtuple, etc.).
 *	
 *   INTERFACE ROUTINES
 *
 *   TABLE CREATE/DELETE
 *	ExecCreateTupleTable	- create a new tuple table
 *	ExecDestroyTupleTable	- destroy a table
 *
 *   SLOT RESERVERATION
 *	ExecAllocTableSlot	- find an available slot in the table
 *	ExecGetTableSlot	- get a slot corresponding to a table index
 *
 *   SLOT ACCESSORS
 *	ExecStoreTuple		- store a tuple in the table
 *	ExecFetchTuple		- fetch a tuple from the table
 *	ExecClearTuple		- clear contents of a table slot
 *	ExecSlotPolicy		- return slot's tuple pfree policy
 *	ExecSetSlotPolicy	- diddle the slot policy
 *	ExecSlotDescriptor	- type of tuple in a slot
 *	ExecSetSlotDescriptor	- set a slot's tuple descriptor
 *	ExecSetSlotDescriptorIsNew - diddle the slot-desc-is-new flag
 *	ExecSetNewSlotDescriptor - set a desc and the is-new-flag all at once
 *	ExecSlotBuffer		- return buffer of tuple in slot
 *	ExecSetSlotBuffer	- set the buffer for tuple in slot
 *	ExecIncrSlotBufferRefcnt - bump the refcnt of the slot buffer
 *
 *   SLOT STATUS PREDICATES
 *	ExecNullSlot		- true when slot contains no tuple
 *	ExecSlotDescriptorIsNew	- true if we're now storing a different
 *				  type of tuple in a slot
 *
 *   CONVENIENCE INITIALIZATION ROUTINES
 *	ExecInitResultTupleSlot    \	convience routines to initialize
 *	ExecInitScanTupleSlot       \  	the various tuple slots for nodes
 *	ExecInitRawTupleSlot        \ 	which store copies of tuples.
 *	ExecInitMarkedTupleSlot      /  	
 *	ExecInitOuterTupleSlot      /  	
 *	ExecInitHashTupleSlot /  	
 *
 *   old routines:
 *   	ExecGetTupType	 	- get type of tuple returned by this node
 *   	ExecTypeFromTL   	- form a TupleDescriptor from a target list
 *
 *   EXAMPLE OF HOW TABLE ROUTINES WORK
 *	Suppose we have a query such as retrieve (EMP.name) and we have
 *	a single SeqScan node in the query plan.
 *
 *	At ExecMain(EXEC_START)
 * 	----------------
 *	- InitPlan() calls ExecCreateTupleTable() to create the tuple
 *	  table which will hold tuples processed by the executor.
 *
 *	- ExecInitSeqScan() calls ExecInitScanTupleSlot() and
 *	  ExecInitResultTupleSlot() to reserve places in the tuple
 *	  table for the tuples returned by the access methods and the
 *	  tuples resulting from preforming target list projections.
 *
 *	During ExecMain(EXEC_RUN)
 * 	----------------
 *	- SeqNext() calls ExecStoreTuple() to place the tuple returned
 *	  by the access methods into the scan tuple slot.
 *
 *	- ExecProcSeqScan() calls ExecStoreTuple() to take the result
 *	  tuple from ExecTargetList() and place it into the result tuple
 *	  slot.
 *
 *	- ExecutePlan() calls ExecRetrieve() which gets the tuple out of
 *	  the slot passed to it by calling ExecFetchTuple().  this tuple
 *	  is then returned.
 *
 *	At ExecMain(EXEC_END)
 * 	----------------
 *	- EndPlan() calls ExecDestroyTupleTable() to clean up any remaining
 *	  tuples left over from executing the query.
 *
 *	The important thing to watch in the executor code is how pointers
 *	to the slots containing tuples are passed instead of the tuples
 *	themselves.  This facilitates the communication of related information
 *	(such as whether or not a tuple should be pfreed, what buffer contains
 *	this tuple, the tuple's tuple descriptor, etc).   Note that much of
 *	this information is also kept in the ExprContext of each node.
 *	Soon the executor will be redesigned and ExprContext's will contain
 *	only slot pointers.  -cim 3/14/91
 *
 *   NOTES
 *	The tuple table stuff is relatively new, put here to alleviate
 *	the process growth problems in the executor.  The other routines
 *	are old (from the original lisp system) and may someday become
 *	obsolete.  -cim 6/23/90
 *
 *	In the implementation of nested-dot queries such as
 *	"retrieve (EMP.hobbies.all)", a single scan may return tuples
 *	of many types, so now we return pointers to tuple descriptors
 *	along with tuples returned via the tuple table.  This means
 *	we now have a bunch of routines to diddle the slot descriptors
 *	too.  -cim 1/18/90
 *
 *	The tuple table stuff depends on the lib/H/executor/tuptable.h macros,
 *	and the TupleTableSlot node in execnodes.h.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
 
#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) execdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h
 *	6) externs.h comes last
 * ----------------
 */
#include "executor/execdebug.h"

#include "parser/parsetree.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/externs.h"

#undef ExecStoreTuple

/* ----------------------------------------------------------------
 *		  tuple table create/delete functions
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	ExecCreateTupleTable
 *
 *	This creates a new tuple table of the specified initial
 *	size.  If the size is insufficient, ExecAllocTableSlot()
 *	will grow the table as necessary.
 *
 *	This should be used by InitPlan() to allocate the table.
 *	The table's address would then be stored somewhere
 *	in the EState structure.
 * --------------------------------
 */
TupleTable			/* return: address of table */
ExecCreateTupleTable(initialSize)
    int		initialSize;	/* initial number of slots in table */
{
    TupleTable	newtable;	/* newly allocated table */
    Pointer	array;		/* newly allocated table array */

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(initialSize >= 1);
    
    /* ----------------
     *	Now allocate our new table along with space for the pointers
     *	to the tuples.  Note: our array is actually an array of
     *  TupleTableCells (which is a subclass of LispValue).
     *  This is for 2 reasons:  1) there's lots of code that expects
     *  tuples to be returned "inside" lispCons cells and 2) the cdr
     *  field may one day be useful.
     *  -cim 6/23/90
     * ----------------
     */
    newtable = (TupleTable) palloc(sizeof(TupleTableData));
    array    = (Pointer)    palloc(initialSize * TableSlotSize);

    /* ----------------
     *	clean out the slots we just allocated
     * ----------------
     */
    bzero(array, initialSize * TableSlotSize);
    
    /* ----------------
     *	initialize the new table and return it to the caller.
     * ----------------
     */
    newtable->size =    initialSize;
    newtable->next =    0;
    newtable->array = 	array;

    return newtable;
}

/* --------------------------------
 *	ExecDestroyTupleTable
 *
 *	This pfrees the storage assigned to the tuple table and
 *	optionally pfrees the contents of the table also.  
 *	It is expected that this routine be called by EndPlan().
 * --------------------------------
 */
void
ExecDestroyTupleTable(table, shouldFree)
    TupleTable	table;		/* tuple table */
    bool	shouldFree;	/* true if we should free slot contents */
{
    int		next;		/* next avaliable slot */
    Pointer	array;		/* start of table array */
    int		i;		/* counter */

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(table != NULL);
    
    /* ----------------
     *	get information from the table
     * ----------------
     */
    array = table->array;
    next =  table->next;
		      
    /* ----------------
     *	first free all the valid pointers in the tuple array
     *  if that's what the caller wants..
     *
     *	Note: we do nothing about the Buffer and Tuple Descriptor's
     *  we store in the slots.  This may have to change (ex: we should
     *  probably worry about pfreeing tuple descs too) -cim 3/14/91
     * ----------------
     */
    if (shouldFree)
	for (i = 0; i < next; i++) {
	    Pointer	slot;
	    Pointer	tuple;

	    slot =  TableSlot(array, i);
	    tuple = SlotContents(slot);
	
	    if (tuple != NULL) {
		SetSlotContents(slot, NULL);
		if (SlotShouldFree(slot)) {
		    /* ----------------
		     *	since a tuple may contain a pointer to
		     *  lock information allocated along with the
		     *  tuple, we have to be careful to free any
		     *  rule locks also -cim 1/17/90
		     * ----------------
		     */
		    HeapTupleFreeRuleLock(tuple);
		    pfree(tuple);
		}
	    }
	}

    /* ----------------
     *	finally free the tuple array and the table itself.
     * ----------------
     */
    pfree(array);
    pfree(table);
    
}


/* ----------------------------------------------------------------
 *		  tuple table slot reservation functions
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	ExecAllocTableSlot
 *
 *	This routine is used to reserve slots in the table for
 *	use by the various plan nodes.  It is expected to be
 *	called by the node init routines (ex: ExecInitNestLoop).
 *	once per slot needed by the node.  Not all nodes need
 *	slots (some just pass tuples around).
 * --------------------------------
 */
int 				/* return: index into tuple table */
ExecAllocTableSlot(table)
    TupleTable	table;		/* tuple table */
{
    int	slotnum;		/* new slot number (returned) */
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(table != NULL);

    /* ----------------
     *	if our table is full we have to allocate a larger
     *	size table.  Since ExecAllocTableSlot() is only called
     *  before the table is ever used to store tuples, we don't
     *  have to worry about the contents of the old table.
     *  If this changes, then we will have to preserve the contents.
     *  -cim 6/23/90
     * ----------------
     */
    if (table->next >= table->size) {
	int newsize = NewTableSize(table->size);
	
	pfree(table->array);
	table->array = (Pointer) palloc(newsize * TableSlotSize);
	bzero(table->array, newsize * TableSlotSize);
	table->size =  newsize;
    }

    /* ----------------
     *	at this point, space in the table is guaranteed so we
     *  reserve the next slot, initialize and return it.
     * ----------------
     */
    slotnum = table->next;
    table->next++;

    InitSlot(ExecGetTableSlot(table, slotnum));
    return slotnum;
}

/* --------------------------------
 *	ExecGetTableSlot
 *
 *	This routine is used to get the slot (an address) corresponding
 *	to a slot number (an index) for the specified tuple table.
 * --------------------------------
 */
Pointer 			/* return: address of slot containing tuple */
ExecGetTableSlot(table, slotnum)
    TupleTable	table;		/* table */
    int		slotnum;	/* number of slot */
{
    Pointer	slot;		/* slot corresponding to slotnum in table */
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(table != NULL);
    Assert(slotnum >= 0 && slotnum < table->next);

    /* ----------------
     *	get information from the tuple table and return it.
     * ----------------
     */
    slot = TableSlot(table->array, slotnum);    

    return slot;
}

/* ----------------------------------------------------------------
 *		  tuple table slot accessor functions
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	ExecStoreTuple
 *
 *	This function is used to store a tuple into a specified
 *	slot in the tuple table.  Note: the only slots which should
 *	be called with shouldFree == false are those slots used to
 *	store tuples not allocated with pfree().  Currently the
 *	seqscan and indexscan nodes use this for the tuples returned
 *	by amgetattr, which are actually pointers onto disk pages.
 * --------------------------------
 */
Pointer 			/* return: slot passed */
ExecStoreTuple(tuple, slot, buffer, shouldFree)
    Pointer 	tuple;		/* tuple to store */
    Pointer	slot;		/* slot in which to store tuple */
    Buffer	buffer;		/* buffer associated with tuple */
    bool	shouldFree;	/* true if we call pfree() when we gc. */
{
    Pointer 	oldtuple;	/* prior contents of slot */
	
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(slot != NULL);
    
    /* ----------------
     *	get information from the tuple table
     * ----------------
     */
    oldtuple = 	SlotContents(slot);

    /* ----------------
     *	free the old contents of the specified slot if necessary.
     * ----------------
     */
    if (SlotShouldFree(slot) && oldtuple != NULL) {
	/* ----------------
	 *  since a tuple may contain a pointer to
	 *  lock information allocated along with the
	 *  tuple, we have to be careful to free any
	 *  rule locks also -cim 1/17/90
	 * ----------------
	 */
	HeapTupleFreeRuleLock(oldtuple);
	pfree(oldtuple);
    }

    /* ----------------
     *  if we have a buffer pinned, release it before stomping on it.
     * ----------------
     */
    if (BufferIsValid(SlotBuffer(slot)))
	ReleaseBuffer(SlotBuffer(slot));

    /* ----------------
     *	store the new tuple into the specified slot and
     *  return the slot into which we stored the tuple.
     * ----------------
     */
    SetSlotContents(slot, tuple);
    SetSlotBuffer(slot, buffer);
    SetSlotShouldFree(slot, shouldFree);

    return slot;
}

Pointer 			/* return: slot passed */
ExecStoreTupleDebug(file, line, tuple, slot, buffer, shouldFree)
    String	file;		/* filename */
    int		line;		/* line number */
    Pointer 	tuple;		/* tuple to store */
    Pointer	slot;		/* slot in which to store tuple */
    Buffer	buffer;		/* buffer associated with tuple */
    bool	shouldFree;	/* true if we call pfree() when we gc. */
{
    printf(":EST f %s l %d t 0x%x s 0x%x b %d sf %d\n",
	   file, line, tuple, slot, buffer, shouldFree);
    return 
	ExecStoreTuple(tuple, slot, buffer, shouldFree);
}

/* --------------------------------
 *	ExecFetchTuple
 *
 *	This function is used to get the heap tuple out of
 *	a slot in the tuple table.
 * --------------------------------
 */
Pointer				/* return: address of tuple */
ExecFetchTuple(slot)
    Pointer	slot;		/* slot from which fetch store tuple */
{
    Pointer 	tuple;		/* contents of slot (returned) */

    /* ----------------
     *	if the slot itself is null then we return NULL.  we do not
     *  differentiate between a NULL slot pointer and a pointer to
     *  a slot containing NULL.
     * ----------------
     */
    if (slot == NULL)
	return NULL;
    
    /* ----------------
     *	get information from the slot and return it
     * ----------------
     */
    tuple = 	SlotContents(slot);

    return tuple;
}

/* --------------------------------
 *	ExecClearTuple
 *
 *	This function is used to clear out a slot in the tuple table.
 * --------------------------------
 */
Pointer 			/* return: slot passed */
ExecClearTuple(slot)
    Pointer	slot;		/* slot in which to store tuple */
{
    Pointer 	oldtuple;	/* prior contents of slot */
	
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(slot != NULL);

    /* ----------------
     *	get information from the tuple table
     * ----------------
     */
    oldtuple = 	SlotContents(slot);

    /* ----------------
     *	free the old contents of the specified slot if necessary.
     * ----------------
     */
    if (SlotShouldFree(slot) && oldtuple != NULL) {
	/* ----------------
	 *  since a tuple may contain a pointer to
	 *  lock information allocated along with the
	 *  tuple, we have to be careful to free any
	 *  rule locks also -cim 1/17/90
	 * ----------------
	 */
	HeapTupleFreeRuleLock(oldtuple);
	pfree(oldtuple);
    }

    /* ----------------
     *	store NULL into the specified slot and return the slot.
     *  - also set buffer to InvalidBuffer -cim 3/14/91
     * ----------------
     */
    SetSlotContents(slot, NULL);

    if (BufferIsValid(SlotBuffer(slot)))
	ReleaseBuffer(SlotBuffer(slot));
    
    SetSlotBuffer(slot, InvalidBuffer);
    SetSlotShouldFree(slot, true);

    return slot;
}


/* --------------------------------
 *	ExecSlotPolicy
 *
 *	This function is used to get the call/don't call pfree
 *	setting of a slot.  Most executor routines don't need this.
 *	It's only when you do tricky things like marking tuples for
 *	merge joins that you need to diddle the slot policy.
 * --------------------------------
 */
bool				/* return: slot policy */
ExecSlotPolicy(slot)
    Pointer	slot;		/* slot to inspect */
{
    bool shouldFree = SlotShouldFree(slot);
    return shouldFree;
}

/* --------------------------------
 *	ExecSetSlotPolicy
 *
 *	This function is used to change the call/don't call pfree
 *	setting of a slot.  Most executor routines don't need this.
 *	It's only when you do tricky things like marking tuples for
 *	merge joins that you need to diddle the slot policy.
 * --------------------------------
 */
bool				/* return: old slot policy */
ExecSetSlotPolicy(slot, shouldFree)
    Pointer	slot;		/* slot to change */
    bool	shouldFree;	/* true if we call pfree() when we gc. */
{
    bool old_shouldFree = SlotShouldFree(slot);
    SetSlotShouldFree(slot, shouldFree);

    return old_shouldFree;
}

/* --------------------------------
 *	ExecSlotDescriptor
 *
 *	This function is used to get the tuple descriptor associated
 *	with the slot's tuple.
 * --------------------------------
 */
TupleDescriptor			/* return: tuple descriptor */
ExecSlotDescriptor(slot)
    Pointer	slot;		/* slot to inspect */
{
    TupleDescriptor tupdesc = SlotTupleDescriptor(slot);
    return tupdesc;
}

/* --------------------------------
 *	ExecSetSlotDescriptor
 *
 *	This function is used to set the tuple descriptor associated
 *	with the slot's tuple.
 * --------------------------------
 */
TupleDescriptor			/* return: old slot tuple descriptor */
ExecSetSlotDescriptor(slot, tupdesc)
    Pointer	     slot;	/* slot to change */
    TupleDescriptor  tupdesc;	/* tuple descriptor */
{
    TupleDescriptor  old_tupdesc = SlotTupleDescriptor(slot);
    SetSlotTupleDescriptor(slot, tupdesc);
    
    return old_tupdesc;
}

/* --------------------------------
 *	ExecSetSlotDescriptorIsNew
 *
 *	This function is used to change the setting of the "isNew" flag
 * --------------------------------
 */
void
ExecSetSlotDescriptorIsNew(slot, isNew)
    Pointer	slot;		/* slot to change */
    bool	isNew;		/* "isNew" setting */
{
    SetSlotTupleDescriptorIsNew(slot, isNew);
}

/* --------------------------------
 *	ExecSetNewSlotDescriptor
 *
 *	This function is used to set the tuple descriptor associated
 *	with the slot's tuple, and set the "isNew" flag at the same time.
 * --------------------------------
 */
TupleDescriptor			/* return: old slot tuple descriptor */
ExecSetNewSlotDescriptor(slot, tupdesc)
    Pointer	     slot;	/* slot to change */
    TupleDescriptor  tupdesc;	/* tuple descriptor */
{
    TupleDescriptor old_tupdesc = SlotTupleDescriptor(slot);
    SetSlotTupleDescriptor(slot, tupdesc);
    SetSlotTupleDescriptorIsNew(slot, true);
    
    return old_tupdesc;
}

/* --------------------------------
 *	ExecSlotBuffer
 *
 *	This function is used to get the tuple descriptor associated
 *	with the slot's tuple.  Be very careful with this as it does not
 *	balance the reference counts.  If the buffer returned is stored
 *	someplace else, then also use ExecIncrSlotBufferRefcnt().
 * --------------------------------
 */
Buffer				/* return: tuple descriptor */
ExecSlotBuffer(slot)
    Pointer	slot;		/* slot to inspect */
{
    Buffer b = SlotBuffer(slot);
    return b;
}

/* --------------------------------
 *	ExecSetSlotBuffer
 *
 *	This function is used to set the tuple descriptor associated
 *	with the slot's tuple.   Be very careful with this as it does not
 *	balance the reference counts.  If we're using this then we should
 *	also use ExecIncrSlotBufferRefcnt().
 * --------------------------------
 */
Buffer				/* return: old slot buffer */
ExecSetSlotBuffer(slot, b)
    Pointer slot;		/* slot to change */
    Buffer  b;			/* tuple descriptor */
{
    Buffer oldb = SlotBuffer(slot);
    SetSlotBuffer(slot, b);
    
    return oldb;
}

/* --------------------------------
 *	ExecIncrSlotBufferRefcnt
 *
 *	When we pass around buffers in the tuple table, we have to
 *	be careful to increment reference counts appropriately.
 *	This is used mainly in the mergejoin code.
 * --------------------------------
 */
void
ExecIncrSlotBufferRefcnt(slot)    
    Pointer slot;		/* slot to bump refcnt */
{
    Buffer b = SlotBuffer(slot);
    if (BufferIsValid(b))
	IncrBufferRefCount(b);
}

/* ----------------------------------------------------------------
 *		  tuple table slot status predicates
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecNullSlot
 *
 *	This is used mainly to detect when there are no more
 *	tuples to process.  The TupIsNull() macro calls this.
 * ----------------
 */
bool				/* return: true if tuple in slot is NULL */
ExecNullSlot(slot)
    Pointer	slot;		/* slot to check */
{
    Pointer 	tuple;		/* contents of slot (returned) */

    /* ----------------
     *	if the slot itself is null then we return true
     * ----------------
     */
    if (slot == NULL)
	return true;
    
    /* ----------------
     *	get information from the slot and return true or
     *  false depending on the contents of the slot.
     * ----------------
     */
    tuple = 	SlotContents(slot);

    return
	(tuple == NULL ? true : false);
}

/* --------------------------------
 *	ExecSlotDescriptorIsNew
 *
 *	This function is used to check if the tuple descriptor
 *	associated with this slot has just changed.  ie: we are
 *	now storing a new type of tuple in this slot
 * --------------------------------
 */
bool				/* return: descriptor "is new" */
ExecSlotDescriptorIsNew(slot)
    Pointer	slot;		/* slot to inspect */
{
    bool isNew = SlotTupleDescriptorIsNew(slot);
    return isNew;
}

/* ----------------------------------------------------------------
 *		convenience initialization routines 
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	ExecInit{Result,Scan,Raw,Marked,Outer,Hash}TupleSlot
 *
 *	These are convience routines to initialize the specfied slot
 *	in nodes inheriting the appropriate state.
 * --------------------------------
 */
#define INIT_SLOT_DEFS \
    TupleTable     tupleTable; \
    int 	   slotnum; \
    Pointer	   slot

#define INIT_SLOT_ALLOC \
    tupleTable = (TupleTable) get_es_tupleTable(estate); \
    slotnum =    ExecAllocTableSlot(tupleTable); \
    slot =       ExecGetTableSlot(tupleTable, slotnum); \
    SetSlotContents(slot, NULL); \
    SetSlotShouldFree(slot, true); \
    SetSlotTupleDescriptor(slot, (TupleDescriptor) NULL); \
    SetSlotTupleDescriptorIsNew(slot, true)
    
/* ----------------
 *	ExecInitResultTupleSlot
 * ----------------
 */
void
ExecInitResultTupleSlot(estate, commonstate)
    EState 	estate;
    CommonState	commonstate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_cs_ResultTupleSlot(commonstate, slot);
}

/* ----------------
 *	ExecInitScanTupleSlot
 * ----------------
 */
void
ExecInitScanTupleSlot(estate, commonscanstate)
    EState 		estate;
    CommonScanState	commonscanstate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_css_ScanTupleSlot(commonscanstate, slot);
}

/* ----------------
 *	ExecInitRawTupleSlot
 * ----------------
 */
void
ExecInitRawTupleSlot(estate, commonscanstate)
    EState 		estate;
    CommonScanState	commonscanstate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_css_RawTupleSlot(commonscanstate, slot);
}

/* ----------------
 *	ExecInitMarkedTupleSlot
 * ----------------
 */
void
ExecInitMarkedTupleSlot(estate, mergestate)
    EState 		estate;
    MergeJoinState	mergestate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_mj_MarkedTupleSlot(mergestate, slot);
}

/* ----------------
 *	ExecInitOuterTupleSlot
 * ----------------
 */
void
ExecInitOuterTupleSlot(estate, hashstate)
    EState 		estate;
    HashJoinState	hashstate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_hj_OuterTupleSlot(hashstate, slot);
}

/* ----------------
 *	ExecInitHashTupleSlot
 * ----------------
 */
void
ExecInitHashTupleSlot(estate, hashstate)
    EState 		estate;
    HashJoinState	hashstate;
{
    INIT_SLOT_DEFS;
    INIT_SLOT_ALLOC;
    set_hj_HashTupleSlot(hashstate, slot);
}

/* ----------------------------------------------------------------
 *		      old lisp support routines
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *   	ExecGetTupType
 *
 *	this gives you the tuple descriptor for tuples returned
 *	by this node.  I really wish I could ditch this routine,
 *	but since not all nodes store their type info in the same
 *	place, we have to do something special for each node type.
 *
 *	Soon, the system will have to adapt to deal with changing
 *	tuple descriptors as we deal with dynamic tuple types
 *	being returned from procedure nodes.  Perhaps then this
 *	routine can be retired.  -cim 6/3/91
 *
 * old comments
 *	This routine just gets the type information out of the
 *	node's state.  If you already have a node's state, you
 *	can get this information directly, but this is a useful
 *	routine if you want to get the type information from
 *	the node's inner or outer subplan easily without having
 *	to inspect the subplan.. -cim 10/16/89
 *
 *   	Assume that for existential nodes, we get the targetlist out
 *   	of the right node's targetlist
 * ----------------------------------------------------------------
 */

TupleDescriptor
ExecGetTupType(node) 
    Plan node;
{
    TupleTableSlot    slot;
    TupleDescriptor   tupType;
    union {
	ResultState    	resstate;
	ScanState      	scanstate;
	NestLoopState  	nlstate;
	MaterialState	matstate;
	SortState	sortstate;
	AggState	aggstate;
	HashState	hashstate;
	UniqueState	uniquestate;
	MergeJoinState 	mergestate;
	HashJoinState 	hashjoinstate;
	ScanTempState	scantempstate;
    } s;
    
    if (node == NULL)
	return NULL;

    switch(NodeType(node)) {
	
    case classTag(Result):
	s.resstate = 		get_resstate(node);
	slot = 			get_cs_ResultTupleSlot(s.resstate);
	tupType =		ExecSlotDescriptor(slot);
	return tupType;
    
    case classTag(SeqScan):
	s.scanstate = 		get_scanstate(node);
	slot = 			get_cs_ResultTupleSlot(s.scanstate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
    
    case classTag(ScanTemps):
	s.scantempstate = 	get_scantempState(node);
	slot = 			get_cs_ResultTupleSlot(s.scantempstate);
	tupType = 		ExecSlotDescriptor(slot);
	return tupType;

    case classTag(NestLoop):
	s.nlstate =  		get_nlstate(node);
	slot = 			get_cs_ResultTupleSlot(s.nlstate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
       
    case classTag(Append):
	{
	    AppendState 	unionstate;
	    List 		unionplans;
	    int  		whichplan;
	    Plan 		subplan;
	    
	    unionstate = 	get_unionstate(node);
	    unionplans = 	get_unionplans(node);
	    whichplan = 	get_as_whichplan(unionstate);
      
	    subplan = (Plan) nth(whichplan, unionplans);
	    return
		ExecGetTupType(subplan);
	}
    
    case classTag(IndexScan):
	s.scanstate = 		get_scanstate(node);
	slot =  		get_cs_ResultTupleSlot(s.scanstate);
	tupType  =  		ExecSlotDescriptor(slot);
	return tupType;
    
    case classTag(Material):
	s.matstate = 		get_matstate(node);
	slot = 			get_css_ScanTupleSlot(s.matstate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
	
    case classTag(Sort):
	s.sortstate = 		get_sortstate(node);
	slot = 			get_css_ScanTupleSlot(s.sortstate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
        
    case classTag(Agg):
	s.aggstate = 		get_aggstate(node);
	slot = 			get_css_ScanTupleSlot(s.aggstate);
	tupType = 		ExecSlotDescriptor(slot);
	return tupType;

    case classTag(Hash):
	s.hashstate = 		get_hashstate(node);
	slot =			get_cs_ResultTupleSlot(s.hashstate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
        
    case classTag(Unique):
	s.uniquestate = 	get_uniquestate(node);
	slot =			get_cs_ResultTupleSlot(s.uniquestate);
	tupType =  		ExecSlotDescriptor(slot);
	return tupType;
    
    case classTag(MergeJoin):
	s.mergestate = 		get_mergestate(node);
	slot =			get_cs_ResultTupleSlot(s.mergestate);
	tupType =    		ExecSlotDescriptor(slot);
	return tupType;
    
    case classTag(HashJoin):
	s.hashjoinstate = 	get_hashjoinstate(node);
	slot =			get_cs_ResultTupleSlot(s.hashjoinstate);
	tupType =    		ExecSlotDescriptor(slot);
	return tupType;

    default:
	/* ----------------
	 *    should never get here
	 * ----------------
	 */
	elog(DEBUG, "ExecGetTupType: node not yet supported: %d ",
	     NodeGetTag(node));
    
	return NULL;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecTypeFromTL
 *   	
 *	Currently there are about 4 different places where we create
 *	TupleDescriptors.  They should all be merged, or perhaps
 *	be rewritten to call BuildDesc().
 *   	
 *  old comments
 *   	Forms attribute type info from the target list in the node.
 *   	It assumes all domains are individually specified in the target list.
 *   	It fails if the target list contains something like Emp.all
 *   	which represents all the attributes from EMP relation.
 *   
 *   	Conditions:
 *   	    The inner and outer subtrees should be initialized because it
 *   	    might be necessary to know the type infos of the subtrees.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitIndexScan
 *           ExecInitMergeJoin
 *           ExecInitNestLoop
 *           ExecInitResult
 *           ExecInitSeqScan
 ****/
TupleDescriptor
ExecTypeFromTL(targetList)
    List 	 	targetList;
{
    List 	 	tlcdr;
    TupleDescriptor 	typeInfo;
    Resdom   	 	resdom;
    ObjectId     	restype;
    int 	 	len;
   
   /* ----------------
    *  examine targetlist - if empty then return NULL
    * ----------------
    */
   len = length(targetList);
   if (len == 0)
      return NULL;
   
   /* ----------------
    *  allocate a new typeInfo
    * ----------------
    */
    typeInfo = ExecMakeTypeInfo(len);
    if (typeInfo == NULL)
	elog(DEBUG, "ExecTypeFromTL: ExecMakeTypeInfo returns null.");

   /* ----------------
    * notes: get resdom from (resdom expr)
    *        get_typbyval comes from src/lib/l-lisp/lsyscache.c
    * ----------------
    */
    tlcdr = targetList;
    while (! lispNullp(tlcdr)) {
	resdom =  (Resdom) tl_resdom(CAR(tlcdr));
	restype = get_restype(resdom);
      
	ExecSetTypeInfo(get_resno(resdom) - 1,	/* index */
			typeInfo,		/* addr of type info */
			restype,		/* type id */
			get_resno(resdom),	/* att num */
			get_reslen(resdom),	/* att len */
			get_resname(resdom),	/* att name */
			get_typbyval(restype));	/* att by val */

	tlcdr = CDR(tlcdr);
    }
   
    return typeInfo;
}
