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
#include "executor/execdesc.h"

/* ----------------------------------------------------------------
 *     #define macros
 * ----------------------------------------------------------------
 */

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
 * Routines dealing with the structure 'skey' which contains the
 * information on keys used to restrict scanning. This is also the
 * structure expected by the sort utility.
 * XXX I think we can get rid of these  -- aoki
 *
 *	ExecMakeSkeys(noKeys) --
 *		returns pointer to an array containing 'noKeys' strucutre skey.
 *
 *	ExecSetSkeys(index, skeys, attNo, opr, value) --
 *		sets the index'th  element of skeys with the values.
 *
 *	ExecSetSkeysValue(index, skeys, value) -
 *		sets the index'th element of skeys with 'value'.
 *		this is needed because 'value' field changes all the time.
 *
 *	ExecFreeSkeys(skeys) --
 *		frees the array of structure skeys.
 * ----------------
 */
#define ExecMakeSkeys(noSkeys) \
    (noSkeys <= 0 ? \
     ((Pointer) NULL) : \
     ((Pointer) palloc(noSkeys * sizeof(struct skey))))
 
#define ExecSetSkeys(index, skeys, attNo, opr, value) \
    ScanKeyEntryInitialize(&((skeys)[index]), NULL, attNo, opr, value)

#define ExecSetSkeysValue(index, skeys, value) \
    ((ScanKey) skeys)->data[index].argument = ((Datum) value)
 
#define ExecFreeSkeys(skeys) \
    if (skeys != NULL) pfree((Pointer) skeys)


/* --------------------------------
 * Routines dealing with the structure 'TLValues' which contains the
 * values from evaluating a target list. 
 *
 *	ExecMakeTLValues(noDomains) --
 *		returns pointer to an array containing 'noDomains' values.
 *
 *	ExecSetTLValues(index, TLValues, value)
 *		sets the index'th  element of "TLValues" with the value.
 *
 *	ExecFreeTLValues(TLValues) --
 *		frees the array of structure cmplist.
 * --------------------------------
 */
#define ExecMakeTLValues(noDomains) \
    (noDomains <= 0 ? \
    ((Pointer) NULL) : \
    ((Pointer) palloc((noDomains) * sizeof(Datum))))

#define ExecSetTLValues(index, TLValues, value) \
    ((Datum *) TLValues)[index] = value

#define ExecFreeTLValues(TLValues) \
    if (TLValues != NULL) pfree(TLValues)

/* ----------------
 *	get_innerPlan()
 *	get_outerPlan()
 *
 *	these are are defined to avoid confusion problems with "left"
 *	and "right" and "inner" and "outer".  The convention is that
 *      the "left" plan is the "outer" plan and the "right" plan is
 *      the inner plan, but these make the code more readable. 
 * ----------------
 */
#define get_innerPlan(node) \
    ((Plan) get_righttree(node))

#define get_outerPlan(node) \
    ((Plan) get_lefttree(node))

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
#define ExecIsScanTemps(node)   IsA(node,ScanTemps)

/* ----------------------------------------------------------------
 *	debugging structures
 * ----------------------------------------------------------------
 */

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

extern bool		ExecIsInitialized;
extern Const		ConstTrue;
extern Const		ConstFalse;

extern int		_debug_hook_id_;
extern HookNode		_debug_hooks_[];

/* ----------------------------------------------------------------
 *	the end
 * ----------------------------------------------------------------
 */

#endif  ExecmiscHIncluded
