/* ----------------------------------------------------------------
 *   FILE
 *	ex_main.h
 *
 *   DESCRIPTION
 *	prototypes for ex_main.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_mainIncluded		/* include this file only once */
#define ex_mainIncluded	1

extern void InitializeExecutor ARGS((void));
extern void ExecCheckPerms ARGS((int operation, LispValue resultRelation, List rangeTable, List parseTree));
extern List ExecMain ARGS((List queryDesc, EState estate, List feature));
extern List InitPlan ARGS((int operation, List parseTree, Plan plan, EState estate));
extern List EndPlan ARGS((Plan plan, EState estate));
extern TupleTableSlot ExecutePlan ARGS((EState estate, Plan plan, LispValue parseTree, int operation, int numberTuples, int direction, void (*printfunc)()));
extern TupleTableSlot ExecRetrieve ARGS((TupleTableSlot slot, void (*printfunc)(), Relation intoRelationDesc));
extern TupleTableSlot ExecAppend ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate, RuleLock newlocks));
extern TupleTableSlot ExecDelete ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate));
extern TupleTableSlot ExecReplace ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate, LispValue parseTree, RuleLock newlocks));

#endif /* ex_mainIncluded */
