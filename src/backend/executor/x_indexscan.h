/* $Header$ */
extern void ExecGetIndexKeyInfo ARGS((IndexTupleForm indexTuple, int *numAttsOutP, AttributeNumberPtr *attsOutP));
extern void ExecOpenIndices ARGS((ObjectId resultRelationOid, RelationInfo resultRelationInfo));
extern void ExecCloseIndices ARGS((RelationInfo resultRelationInfo));
extern IndexTuple ExecFormIndexTuple ARGS((HeapTuple heapTuple, Relation heapRelation, Relation indexRelation, IndexInfo indexInfo));
extern RuleLock ExecInsertIndexTuples ARGS((HeapTuple heapTuple, ItemPointer tupleid, EState estate));
extern TupleTableSlot IndexNext ARGS((IndexScan node));
extern TupleTableSlot ExecIndexScan ARGS((IndexScan node));
extern List ExecIndexReScan ARGS((IndexScan node));
extern void ExecEndIndexScan ARGS((IndexScan node));
extern List ExecIndexMarkPos ARGS((IndexScan node));
extern void ExecIndexRestrPos ARGS((IndexScan node));
extern void ExecUpdateIndexScanKeys ARGS((IndexScan node, ExprContext econtext));
extern List ExecInitIndexScan ARGS((IndexScan node, EState estate, int level, Plan parent));
extern void partition_indexscan ARGS((int numIndices, IndexScanDescPtr scanDescs, int parallel));
