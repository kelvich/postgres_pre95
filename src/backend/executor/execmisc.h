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
 *
 *	XXX if one of these types become a super-type (something inherits
 *	    from it), its call may have to become IsA instead of 
 *	    ExactNodeType.
 * ----------------
 */
#define ExecIsRoot(node) 	LispNil
#define ExecIsResult(node)	ExactNodeType(node,Result)
#define ExecIsExistential(node)	ExactNodeType(node,Existential)
#define ExecIsAppend(node)	ExactNodeType(node,Append)
#define ExecIsParallel(node)	ExactNodeType(node,Parallel)
#define ExecIsRecursive(node)	ExactNodeType(node,Recursive)
#define ExecIsNestLoop(node)	ExactNodeType(node,NestLoop)
#define ExecIsMergeJoin(node)	ExactNodeType(node,MergeJoin)
#define ExecIsHashJoin(node)	ExactNodeType(node,HashJoin)
#define ExecIsSeqScan(node)	ExactNodeType(node,SeqScan)
#define ExecIsIndexScan(node)	ExactNodeType(node,IndexScan)
#define ExecIsMaterial(node)	ExactNodeType(node,Material)
#define ExecIsSort(node)	ExactNodeType(node,Sort)
#define ExecIsUnique(node)	ExactNodeType(node,Unique)
#define ExecIsHash(node)	ExactNodeType(node,Hash)
#define ExecIsScanTemps(node)   ExactNodeType(node,ScanTemps)

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
