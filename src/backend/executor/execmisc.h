/* ----------------------------------------------------------------
 *   FILE
 *     	execmisc.h
 *     
 *   DESCRIPTION
 *     	misc support for the POSTGRES executor module
 *
 *   IDENTIFICATION
 *	$Header$"
 * ----------------------------------------------------------------
 */

#ifndef ExecmiscHIncluded
#define ExecmiscHIncluded 1	/* include only once */

/* ----------------
 *	include files needed for types used in this file.
 *	KEEP THIS LIST SHORT!
 * ----------------
 */
#include "tmp/postgres.h"
#include "access/genam.h"
#include "tmp/simplelists.h"
#include "executor/execdesc.h"

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


/* ----------------------------------------------------------------
 *     #define macros
 * ----------------------------------------------------------------
 */

/* ----------------
 *	the reverse alignment macros are for use with the executor
 *	shared memory allocator's ExecSMHighAlloc routine which
 *	allocates memory from the high-area "down" to the low-area.
 *	In this case we need alignment in the "other" direction than
 *	provided by SHORTALIGN and LONGALIGN
 * ----------------
 */
#define	REVERSESHORTALIGN(LEN)\
    (((long)(LEN)) & ~01)

#ifndef	sun
#define	REVERSELONGALIGN(LEN)\
    (((long)(LEN)) & ~03)
#else
#define	REVERSELONGALIGN(LEN)	REVERSESHORTALIGN(LEN)
#endif

/* ----------------
 *	miscellany
 * ----------------
 */
#define ExecCNull(Cptr)		(Cptr == NULL)
#define ExecCTrue(bval)		(bval != 0)
#define ExecCFalse(bval)	(bval == 0)

#define ExecCNot(bval) \
    (ExecCFalse(bval) ? EXEC_C_TRUE : EXEC_C_FALSE)

#define ExecRevDir(direction)	(direction - 1) 

/* ----------------
 * 	macros to determine node types
 * ----------------
 */

#define ExecIsRoot(node) 	LispNil
#define ExecIsResult(node)	IsA(node,Result)
#define ExecIsExistential(node)	IsA(node,Existential)
#define ExecIsAppend(node)	IsA(node,Append)
#define ExecIsParallel(node)	IsA(node,Parallel)
#define ExecIsRecursive(node)	IsA(node,Recursive)
#define ExecIsNestLoop(node)	IsA(node,NestLoop)
#define ExecIsMergeJoin(node)	IsA(node,MergeJoin)
#define ExecIsHashJoin(node)	IsA(node,HashJoin)
#define ExecIsSeqScan(node)	IsA(node,SeqScan)
#define ExecIsIndexScan(node)	IsA(node,IndexScan)
#define ExecIsMaterial(node)	IsA(node,Material)
#define ExecIsSort(node)	IsA(node,Sort)
#define ExecIsUnique(node)	IsA(node,Unique)
#define ExecIsHash(node)	IsA(node,Hash)
#define ExecIsScanTemps(node)    IsA(node,ScanTemps)

/* ----------------------------------------------------------------
 *	debugging structures
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExecAllocDebugData is used by ExecAllocDebug() and ExecFreeDebug()
 *	to keep track of memory allocation.
 * ----------------
 */
typedef struct ExecAllocDebugData {
    Pointer	pointer;
    Size	size;
    String	routine;
    String	file;
    int		line;
    SLNode 	Link;
} ExecAllocDebugData;

/* ----------------
 *	DebugVariable is a structure holding a string
 *	specifying the name of a debugging variable and
 *	a pointer specifying the address of the object
 *	holding the variable's value.  This is used only
 *	in debug.c
 * ----------------
 */
typedef struct DebugVariable {
    String	debug_variable;
    Pointer	debug_address;
} DebugVariable;
 
typedef DebugVariable *DebugVariablePtr;
 

/* ----------------------------------------------------------------
 *	externs
 * ----------------------------------------------------------------
 */

/* 
 *	miscellany
 */
extern bool		ExecIsInitialized;
extern Const		ConstTrue;
extern Const		ConstFalse;

extern SLList		ExecAllocDebugList;

extern int		_debug_hook_id_;
extern HookNode		_debug_hooks_[];

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecmiscHIncluded
