/* ----------------------------------------------------------------
 *   FILE
 *	n_indexscan.h
 *
 *   DESCRIPTION
 *	prototypes for n_indexscan.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_indexscanIncluded		/* include this file only once */
#define n_indexscanIncluded	1

extern void ExecGetIndexKeyInfo ARGS((IndexTupleForm indexTuple, int *numAttsOutP, AttributeNumberPtr *attsOutP, FuncIndexInfoPtr fInfoP));
extern void ExecOpenIndices ARGS((ObjectId resultRelationOid, RelationInfo resultRelationInfo));
extern void ExecCloseIndices ARGS((RelationInfo resultRelationInfo));
extern IndexTuple ExecFormIndexTuple ARGS((HeapTuple heapTuple, Relation heapRelation, Relation indexRelation, IndexInfo indexInfo));
extern RuleLock ExecInsertIndexTuples ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate));
extern TupleTableSlot IndexNext ARGS((IndexScan node));
extern TupleTableSlot ExecIndexScan ARGS((IndexScan node));
extern List ExecIndexReScan ARGS((IndexScan node));
extern void ExecEndIndexScan ARGS((IndexScan node));
extern List ExecIndexMarkPos ARGS((IndexScan node));
extern void ExecIndexRestrPos ARGS((IndexScan node));
extern void ExecUpdateIndexScanKeys ARGS((IndexScan node, ExprContext econtext));
extern List ExecInitIndexScan ARGS((IndexScan node, EState estate, Plan parent));
extern int ExecCountSlotsIndexScan ARGS((Plan node));
extern void partition_indexscan ARGS((int numIndices, IndexScanDescPtr scanDescs, int parallel));

#endif /* n_indexscanIncluded */
