/* ----------------------------------------------------------------
 *   FILE
 *	ex_shmem.c
 *	
 *   DESCRIPTION
 *	parallel executor shared memory management code
 *
 *   INTERFACE ROUTINES
 *	ExecSMInit	- initializes the shared mem meta-data
 *	ExecSMAlloc	- allocates portions of shared memory
 *	ExecSMHighAlloc	- allocates clean-permsistant shared memory
 *	ExecSMClean	- "frees" entire shared memory segment
 *
 *   NOTES
 *	These routines provide a simple interface to the
 *	executor's shared memory segment.  During InitPostgres()
 *	the shared memory segment is allocated.  Later, during
 *	SlaveBackendInit(), the shared memory segment's header
 *	is initialized via ExecSMInit().  From then on the system
 *	can make use of the ExecSMAlloc(), ExecSMHighAlloc() and
 *	ExecSMClean() routines.
 *
 *	ExecSMAlloc() and ExecSMClean() provide a means of
 *	allocating and recycling shared memory.  When the executor
 *	needs some amount of shared memory, it calls ExecSMAlloc()
 *	and later everything so allocated is disgarded via ExecSMClean().
 *
 *	ExecSMHighAlloc() is used to obtain shared memory that is
 *	not affected by ExecSMClean().  This is used only for very
 *	special data structures which do not grow and persist for
 *	the lifetime of the entire process group.
 *
 *	The parallel optimizer uses these routines to place plans in
 *	memory shared by the master and slave parallel backends.
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

#include "tmp/align.h"
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

/* ----------------------------------------------------------------
 *	executor shared memory segment header definition
 *
 *	After the executor's shared memory segment is allocated
 *	the initial bytes are initialized to contain 2 pointers.
 *	the first pointer points to the "low" end of the available
 *	area and the second points to the "high" end.
 *
 *	When part of the shared memory is reserved by a call to
 *	ExecSMAlloc(), the "low" pointer is advanced to point
 *	beyond the newly allocated area to the available free
 *	area.  If an allocation would cause "low" to exceed "high",
 *	then the allocation fails and an error occurrs.
 *
 *	Note:  Since no heap management is done, there is no
 *	provision for freeing shared memory allocated via ExecSMAlloc()
 *	other than wipeing out the entire space with ExecSMClean().
 * ----------------------------------------------------------------
 */

typedef struct ExecSMHeaderData {
    Pointer	low;		/* pointer to low water mark */
    Pointer	high;		/* pointer to high water mark */
} ExecSMHeaderData;

typedef ExecSMHeaderData *ExecSMHeader;

Pointer ExecutorSharedMemoryStart;

/* --------------------------------
 *	ExecSMInit
 *
 *	This function locates the shared memory segment and
 *	initializes the low and high water marks.  This function
 *      should only be called once.
 * --------------------------------
 */
void
ExecSMInit()
{
    Pointer 	 start;	
    Pointer 	 low;	
    Pointer 	 high;	
    ExecSMHeader header;

    /* ----------------
     *	get the pointer to the start of the segment
     * ----------------
     */
    start = (Pointer) ExecGetExecutorSharedMemory();
    if (start == NULL)
	return;

    /* ----------------
     *	initialize the shared memory semaphore
     * ----------------
     */
    I_SharedMemoryMutex();
    
    /* ----------------
     *	store the pointer in our global variable
     * ----------------
     */
    ExecutorSharedMemoryStart = start;

    /* ----------------
     *	calculate low and high water marks
     * ----------------
     */
    low =  (Pointer) ((char *)start + sizeof(ExecSMHeaderData));
    low =  (Pointer) LONGALIGN(low);
    high = (Pointer) ((char *)start + ExecGetExecutorSharedMemorySize());

    /* ----------------
     *	initialize the shared memory header.
     * ----------------
     */
    header = (ExecSMHeader) start;
    header->low =  (Pointer) low;
    header->high = (Pointer) high;

}

/* --------------------------------
 *	ExecSMClean
 *
 *	This function resets the low water mark to its
 *	original value.
 * --------------------------------
 */
void
ExecSMClean()
{
    ExecSMHeader header;
    Pointer 	 start;	
    Pointer 	 low;	
    Pointer 	 high;	

    /* ----------------
     *	locate the shared memory header
     * ----------------
     */
    start =  ExecutorSharedMemoryStart;
    if (start == NULL)
	return;
    
    /* ----------------
     *	obtain exclusive access to the executor shared memory
     *  allocation structures
     * ----------------
     */
    P_SharedMemoryMutex();
    
    header = (ExecSMHeader) start;
    
    /* ----------------
     *	calculate low water mark -- the high water allocations
     *  are not affected by ExecSMClean().
     * ----------------
     */
    low =  (Pointer) ((char *)start + sizeof(ExecSMHeaderData));
    low =  (Pointer) LONGALIGN(low);

    /* ----------------
     *	initialize the shared memory header.
     * ----------------
     */
    header = (ExecSMHeader) start;
    header->low =  (Pointer) low;
    
    /* ----------------
     *	release our hold on the shared memory meta-data
     * ----------------
     */
    V_SharedMemoryMutex();
}

/* --------------------------------
 *	ExecSMAlloc
 *
 *	This function allocates a portion of the shared segment
 *	by returning a pointer to the present low water mark and
 *	updating the low pointer to the first long-aligned byte
 *	beyond the newly allocated object.
 * --------------------------------
 */
Pointer
ExecSMAlloc(size)
    int size;
{
    ExecSMHeader header;
    Pointer 	 low;	
    Pointer 	 high;	
    Pointer 	 newlow;	
    
    /* ----------------
     *	locate the shared memory header
     * ----------------
     */
    header = (ExecSMHeader) ExecutorSharedMemoryStart;
    if (header == NULL) {
	elog(FATAL, "ExecSMAlloc: ExecutorSharedMemoryStart is NULL!");
	return NULL;
      }

    /* ----------------
     *	obtain exclusive access to the executor shared memory
     *  allocation structures
     * ----------------
     */
    P_SharedMemoryMutex();
    
    low =  header->low;
    high = header->high;

    /* ----------------
     *	calculate the new low water mark
     * ----------------
     */
    newlow =  (Pointer) ((char *)low + size);
    newlow =  (Pointer) LONGALIGN(newlow);

    /* ----------------
     *	if not enough memory remains, release the semaphore
     *  and return null.
     * ----------------
     */
    if (newlow > high) {
	elog(WARN, "ExecSMAlloc: parallel executor out of shared memory.");
	V_SharedMemoryMutex();
	return NULL;
    }
    
    /* ----------------
     *	update the low water mark
     * ----------------
     */
    header->low = newlow;
    
    /* ----------------
     *	release our hold on the shared memory meta-data
     *  and return the allocated area
     * ----------------
     */
    V_SharedMemoryMutex();

    return low;
}

/* --------------------------------
 *	ExecSMHighAlloc
 *
 *	This function allocates a portion of the shared segment
 *	from the "top" end rather than the bottom.  Since the high
 *	water mark is never raised, these allocations are in some
 *	sence permanent.
 * --------------------------------
 */
Pointer
ExecSMHighAlloc(size)
    int size;
{
    ExecSMHeader header;
    Pointer 	 low;	
    Pointer 	 high;	
    Pointer 	 newhigh;	
    
    /* ----------------
     *	locate the shared memory header
     * ----------------
     */
    header = (ExecSMHeader) ExecutorSharedMemoryStart;
    if (header == NULL)
	return NULL;

    /* ----------------
     *	obtain exclusive access to the executor shared memory
     *  allocation structures
     * ----------------
     */
    P_SharedMemoryMutex();
    
    low =  header->low;
    high = header->high;

    /* ----------------
     *	calculate the new high water mark.  Note: we use REVERSELONGALIGN
     *  because we are allocating size bytes in the "top-down" direction.
     *  This means we want to align the pointer to the first long word
     *  boundry below the calculated (high - size) position.
     * ----------------
     */
    newhigh =  (Pointer) ((char *)high - size);
    newhigh =  (Pointer) REVERSELONGALIGN(newhigh);

    /* ----------------
     *	if not enough memory remains, release the semaphore
     *  and return null.
     * ----------------
     */
    if (newhigh < low) {
	V_SharedMemoryMutex();
	return NULL;
    }
    
    /* ----------------
     *	update the high water mark
     * ----------------
     */
    header->high = newhigh;
    
    /* ----------------
     *	release our hold on the shared memory meta-data
     *  and return the allocated area
     * ----------------
     */
    V_SharedMemoryMutex();
    
    return newhigh;
}
