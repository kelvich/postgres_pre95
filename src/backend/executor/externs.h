/* this file generated by genexterns.sh -- do not edit */

/* $Header$ */

/* bad extern:PrintParallelDebugInfoARGS((void)); */
/* bad extern:ResetParallelDebugInfoARGS((void)); */
/* bad extern:void DisplayTupleCountARGS((void)); */
/* bad extern:void ExecXHideTreeARGS((void)); */
/* bad extern:void ExecXInitializeARGS((void)); */
/* bad extern:void ExecXNewTreeARGS((void)); */
/* bad extern:void ExecXShowTreeARGS((void)); */
/* bad extern:void ExecXTerminateARGS((void)); */
/* bad extern:void InitializeExecutorARGS((void)); */
/* bad extern:void ResetTupleCountARGS((void)); */
/* bad extern:void noopARGS((void)); */
/* bad extern:void say_at_initARGS((void)); */
/* bad extern:void say_post_endARGS((void)); */
/* bad extern:void say_post_initARGS((void)); */
/* bad extern:void say_post_procARGS((void)); */
/* bad extern:void say_pre_endARGS((void)); */
/* bad extern:void say_pre_procARGS((void)); */
/* bad extern:void say_yowARGS((void)); */
extern char *ExecHashJoinSaveTuple ARGS((HeapTuple heapTuple, char *buffer, File file, char *position));
extern print_buckets ARGS((HashJoinTable hashtable));
extern Plan AddMaterialNode ARGS((Plan plan));
extern Plan AddSortNode ARGS((Plan plan, int op));
extern Plan AddUniqueNode ARGS((Plan plan));
extern bool ArgumentIsRelation ARGS((List arg));
extern void DebugVariableFileSet ARGS((String filename));
extern bool DebugVariableProcessCommand ARGS((char *buf));
/* bad extern:extern static DebugVariablePtr SearchDebugVariables ARGS((String debug_variable)); */
extern bool DebugVariableSet ARGS((String debug_variable, int debug_value));
extern List EndPlan ARGS((Plan plan, EState estate));
extern int ExecAllocTableSlot ARGS((TupleTable table));
extern TupleTableSlot ExecAppend ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate, RuleLock newlocks));
extern void ExecAssignDebugHooks ARGS((BaseNode base node, int basenode));
extern void ExecAssignExprContext ARGS((EState estate, CommonState commonstate));
extern void ExecAssignNodeBaseInfo ARGS((EState estate, BaseNode basenode, Plan parent));
extern void ExecAssignProjectionInfo ARGS((Plan node, CommonState commonstate));
extern void ExecAssignResultType ARGS((CommonState commonstate, TupleDescriptor tupType));
extern void ExecAssignResultTypeFromOuterPlan ARGS((Plan node, CommonState commonstate));
extern void ExecAssignResultTypeFromTL ARGS((Plan node, CommonState commonstate));
extern void ExecAssignScanType ARGS((CommonState commonstate, TupleDescriptor tupType));
extern void ExecAssignScanTypeFromOuterPlan ARGS((Plan node, CommonState commonstate));
extern Pointer ExecBeginScan ARGS((Relation relation, int nkeys, ScanKey skeys, bool isindex, ScanDirection dir, TimeQual time_range));
extern Pointer ExecClearTuple ARGS((Pointer slot));
extern void ExecCloseIndices ARGS((RelationInfo resultRelationInfo));
extern void ExecCloseR ARGS((Plan node));
extern List ExecCollect ARGS((List l, int applyFunction, int collectFunction, List applyParameters));
extern Relation ExecCreatR ARGS((int numberAttributes, TupleDescriptor tupType, int relationOid));
extern TupleTable ExecCreateTupleTable ARGS((int initialSize));
extern TupleTableSlot ExecDelete ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate));
extern void ExecDestroyTupleTable ARGS((TupleTable table, bool shouldFree));
extern void ExecEndAppend ARGS((Append node));
extern void ExecEndHash ARGS((Hash node));
extern void ExecEndHashJoin ARGS((HashJoin node));
extern void ExecEndIndexScan ARGS((IndexScan node));
extern void ExecEndMaterial ARGS((Material node));
extern void ExecEndMergeJoin ARGS((MergeJoin node));
extern List ExecEndNestLoop ARGS((NestLoop node));
extern void ExecEndNode ARGS((Plan node));
extern void ExecEndResult ARGS((Result node));
extern void ExecEndScanTemps ARGS((ScanTemps node));
extern void ExecEndSeqScan ARGS((SeqScan node));
extern void ExecEndSort ARGS((Sort node));
extern void ExecEndUnique ARGS((Unique node));
extern Datum ExecEvalArrayRef ARGS((Datum object, int32 indirection, int32 array_len, int32 element_len, bool byval, Boolean *isNull));
extern Datum ExecEvalExpr ARGS((Node expression, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalFunc ARGS((Func funcClause, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalNot ARGS((List notclause, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalOper ARGS((List opClause, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalOr ARGS((List orExpr, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalParam ARGS((Param expression, ExprContext econtext));
extern Datum ExecEvalVar ARGS((Var variable, ExprContext econtext, Boolean *isNull));
extern Pointer ExecFetchTuple ARGS((Pointer slot));
extern IndexTuple ExecFormIndexTuple ARGS((HeapTuple heapTuple, Relation heapRelation, Relation indexRelation, IndexInfo indexInfo));
extern void ExecFreeProjectionInfo ARGS((CommonState commonstate));
extern void ExecFreeResultType ARGS((CommonState commonstate));
extern void ExecFreeScanAttributes ARGS((AttributeNumberPtr ptr));
extern void ExecFreeScanType ARGS((CommonState commonstate));
extern void ExecFreeTypeInfo ARGS((struct attribute **typeInfo));
extern void ExecGetIndexKeyInfo ARGS((IndexTupleForm indexTuple, int *numAttsOutP, AttributeNumberPtr *attsOutP));
extern bool ExecGetJunkAttribute ARGS((JunkFilter junkfilter, TupleTableSlot slot, Name attrName, Datum *value, Boolean *isNull));
extern BaseNode ExecGetPrivateState ARGS((Plan node));
extern TupleDescriptor ExecGetResultType ARGS((CommonState commonstate));
extern TupleDescriptor ExecGetScanType ARGS((CommonState commonstate));
extern Pointer ExecGetTableSlot ARGS((TupleTable table, int slotnum));
extern TupleDescriptor ExecGetTupType ARGS((Plan node));
extern Attribute ExecGetTypeInfo ARGS((Relation relDesc));
extern List ExecGetVarAttlistFromExpr ARGS((Node expr, List relationNum));
extern List ExecGetVarAttlistFromTLE ARGS((List tle, List relationNum));
extern TupleTableSlot ExecHash ARGS((Hash node));
extern int ExecHashGetBucket ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey));
extern TupleTableSlot ExecHashJoin ARGS((HashJoin node));
extern int ExecHashJoinGetBatch ARGS((int bucketno, HashJoinTable hashtable, int nbatch));
extern TupleTableSlot ExecHashJoinGetSavedTuple ARGS((HashJoinState hjstate, char *buffer, File file, int *block, char **position));
extern int ExecHashJoinNewBatch ARGS((HashJoinState hjstate));
extern TupleTableSlot ExecHashJoinOuterGetTuple ARGS((Plan node, HashJoinState hjstate));
extern void ExecHashOverflowInsert ARGS((HashJoinTable hashtable, HashBucket bucket, HeapTuple heapTuple));
extern int ExecHashPartition ARGS((Hash node));
extern HashJoinTable ExecHashTableCreate ARGS((Plan node));
extern void ExecHashTableDestroy ARGS((HashJoinTable hashtable));
extern void ExecHashTableInsert ARGS((HashJoinTable hashtable, ExprContext econtext, Var hashkey, File *batches));
extern void ExecHashTableReset ARGS((HashJoinTable hashtable, int ntuples));
extern bool ExecIdenticalTuples ARGS((List t1, List t2));
extern void ExecIncrSlotBufferRefcnt ARGS((Pointer slot));
extern List ExecIndexMarkPos ARGS((IndexScan node));
extern List ExecIndexReScan ARGS((IndexScan node));
extern void ExecIndexRestrPos ARGS((IndexScan node));
extern TupleTableSlot ExecIndexScan ARGS((IndexScan node));
extern List ExecInitAppend ARGS((Append node, EState estate, Plan parent));
extern List ExecInitHash ARGS((Hash node, EState estate, Plan parent));
extern List ExecInitHashJoin ARGS((HashJoin node, EState estate, Plan parent));
extern List ExecInitIndexScan ARGS((IndexScan node, EState estate, Plan parent));
extern JunkFilter ExecInitJunkFilter ARGS((List targetList));
extern void ExecInitMarkedTupleSlot ARGS((MergeJoinState merg estate, int mergestate));
extern List ExecInitMaterial ARGS((Sort node, EState estate, Plan parent));
extern List ExecInitMergeJoin ARGS((MergeJoin node, EState estate, Plan parent));
extern List ExecInitNestLoop ARGS((NestLoop node, EState estate, Plan parent));
extern List ExecInitNode ARGS((Plan node, EState estate, Plan parent));
extern void ExecInitRawTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern List ExecInitResult ARGS((Plan node, EState estate, Plan parent));
extern void ExecInitResultTupleSlot ARGS((EState estate, CommonState commonstate));
extern void ExecInitSavedTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern void ExecInitScanAttributes ARGS((Plan node));
extern List ExecInitScanTemps ARGS((ScanTemps node, EState estate, Plan parent));
extern void ExecInitScanTupleSlot ARGS((EState estate, CommonScanState commonscanstate));
extern List ExecInitSeqScan ARGS((Plan node, EState estate, Plan parent));
extern List ExecInitSort ARGS((Sort node, EState estate, Plan parent));
extern void ExecInitTemporaryTupleSlot ARGS((EState estate, HashJoinState hashstate));
extern List ExecInitUnique ARGS((Unique node, EState estate, Plan parent));
extern RuleLock ExecInsertIndexTuples ARGS((HeapTuple heapTuple, ItemPointer tupleid, EState estate));
extern List ExecMain ARGS((List queryDesc, EState estate, List feature));
extern AttributeNumberPtr ExecMakeAttsFromList ARGS((List attlist, int *numAttsPtr));
extern AttributeNumberPtr ExecMakeBogusScanAttributes ARGS((int natts));
extern Datum ExecMakeFunctionResult ARGS((FunctionCachePtr fcache, List arguments, ExprContext econtext));
extern TupleDescriptor ExecMakeTypeInfo ARGS((int nelts));
extern List ExecMarkPos ARGS((Plan node));
extern TupleTableSlot ExecMaterial ARGS((Material node));
extern List ExecMaterialMarkPos ARGS((Material node));
extern void ExecMaterialRestrPos ARGS((Material node));
extern TupleTableSlot ExecMergeJoin ARGS((MergeJoin node));
extern void ExecMergeTupleDump ARGS((ExprContext econtext, MergeJoinState mergestate));
extern void ExecMergeTupleDumpInner ARGS((ExprContext econtext));
extern void ExecMergeTupleDumpMarked ARGS((ExprContext econtext, MergeJoinState mergestate));
extern void ExecMergeTupleDumpOuter ARGS((ExprContext econtext));
extern TupleTableSlot ExecNestLoop ARGS((NestLoop node));
extern bool ExecNullSlot ARGS((Pointer slot));
extern void ExecOpenIndices ARGS((ObjectId resultRelationOid, RelationInfo resultRelationInfo));
extern Relation ExecOpenR ARGS((ObjectId relationOid, bool isindex));
extern void ExecOpenScanR ARGS((ObjectId relOid, int nkeys, ScanKey skeys, bool isindex, ScanDirection dir, TimeQual timeRange, Relation *returnRelation, Pointer *returnScanDesc));
extern TupleTableSlot ExecProcAppend ARGS((Append node));
extern TupleTableSlot ExecProcNode ARGS((Plan node));
extern TupleTableSlot ExecProject ARGS((ProjectionInfo projInfo));
extern bool ExecQual ARGS((List qual, ExprContext econtext));
extern bool ExecQualClause ARGS((List clause, ExprContext econtext));
extern void ExecReScan ARGS((Plan node));
extern HeapScanDesc ExecReScanR ARGS((Relation relDesc, HeapScanDesc scanDesc, ScanDirection direction, int nkeys, ScanKey skeys));
extern HeapTuple ExecRemoveJunk ARGS((JunkFilter junkfilter, TupleTableSlot slot));
extern TupleTableSlot ExecReplace ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate, LispValue parseTree, RuleLock newlocks));
extern void ExecRestrPos ARGS((Plan node));
extern TupleTableSlot ExecResult ARGS((Result node));
extern TupleTableSlot ExecRetrieve ARGS((TupleTableSlot slot, int printfunc, Relation intoRelationDesc));
extern TupleTableSlot ExecScan ARGS((Plan node, int accessMtd));
extern HeapTuple ExecScanHashBucket ARGS((HashJoinState hjstate, HashBucket bucket, HeapTuple curtuple, List hjclauses, ExprContext econtext));
extern TupleTableSlot ExecScanTemps ARGS((ScanTemps node));
extern List ExecSeqMarkPos ARGS((Plan node));
extern void ExecSeqReScan ARGS((Plan node));
extern void ExecSeqRestrPos ARGS((Plan node));
extern TupleTableSlot ExecSeqScan ARGS((SeqScan node));
extern TupleDescriptor ExecSetNewSlotDescriptor ARGS((Pointer slot, TupleDescriptor tupdesc));
extern Buffer ExecSetSlotBuffer ARGS((Pointer slot, Buffer b));
extern TupleDescriptor ExecSetSlotDescriptor ARGS((Pointer slot, TupleDescriptor tupdesc));
extern void ExecSetSlotDescriptorIsNew ARGS((Pointer slot, bool isNew));
extern bool ExecSetSlotPolicy ARGS((Pointer slot, bool shouldFree));
extern void ExecSetTypeInfo ARGS((int index, struct attribute **typeInfo, int typeID, int attNum, int attLen, char *attName, Boolean attbyVal));
extern Buffer ExecSlotBuffer ARGS((Pointer slot));
extern TupleDescriptor ExecSlotDescriptor ARGS((Pointer slot));
extern bool ExecSlotDescriptorIsNew ARGS((Pointer slot));
extern bool ExecSlotPolicy ARGS((Pointer slot));
extern TupleTableSlot ExecSort ARGS((Sort node));
extern List ExecSortMarkPos ARGS((Plan node));
extern void ExecSortRestrPos ARGS((Plan node));
extern Pointer ExecStoreTuple ARGS((Pointer tuple, Pointer slot, Buffer buffer, bool shouldFree));
extern Pointer ExecStoreTupleDebug ARGS((String file, int line, Pointer tuple, Pointer slot, Buffer buffer, bool shouldFree));
extern HeapTuple ExecTargetList ARGS((List targetlist, int nodomains, TupleDescriptor targettype, Pointer values, ExprContext econtext));
extern TupleDescriptor ExecTypeFromTL ARGS((List targetList));
extern TupleTableSlot ExecUnique ARGS((Unique node));
extern List ExecUniqueCons ARGS((List list1, List list2));
extern void ExecUpdateIndexScanKeys ARGS((IndexScan node, ExprContext econtext));
extern void ExecXAddNode ARGS((int base_id, char *title, int supernode_id));
extern void ExecXSignalNode ARGS((int base_id, int signal));
extern TupleTableSlot ExecutePlan ARGS((EState estate, Plan plan, LispValue parseTree, int operation, int numberTuples, int direction, int printfunc));
extern Pointer FGetOperation ARGS((List queryDesc));
extern Pointer FQdGetCommand ARGS((List queryDesc));
extern Pointer FQdGetCount ARGS((List queryDesc));
extern Pointer FQdGetFeature ARGS((List queryDesc));
extern Pointer FQdGetParseTree ARGS((List queryDesc));
extern Pointer FQdGetPlan ARGS((List queryDesc));
extern Pointer FQdGetState ARGS((List queryDesc));
extern Pointer FormSortKeys ARGS((Sort sortnode));
extern Pointer Fparse_tree_range_table ARGS((List queryDesc));
extern Pointer Fparse_tree_result_relation ARGS((List queryDesc));
extern Datum GetAttribute ARGS((char *attname));
extern String GetNodeName ARGS((Node node));
extern TupleTableSlot IndexNext ARGS((IndexScan node));
extern void InitHook ARGS((HookNode hook));
extern List InitPlan ARGS((int operation, List parseTree, Plan plan, EState estate));
extern ObjectId InitScanRelation ARGS((Plan node, EState estate, ScanState scanstate, Plan outerPlan));
extern void InitXHook ARGS((HookNode hook));
extern List MJFormISortopO ARGS((List qualList, ObjectId sortOp));
extern List MJFormOSortopI ARGS((List qualList, ObjectId sortOp));
extern bool MergeCompare ARGS((List eqQual, List compareQual, ExprContext econtext));
extern void NoisyHook ARGS((HookNode hook));
extern List QueryDescGetTypeInfo ARGS((List queryDesc));
extern TupleTableSlot SeqNext ARGS((SeqScan node));
extern void SetCurrentTuple ARGS((ExprContext econtext));
extern Datum array_cast ARGS((char *value, bool byval, int len));
extern List exec_append_initialize_next ARGS((Append node));
extern int hashFunc ARGS((char *key, int len));
extern RelativeAddr hashTableAlloc ARGS((int size, HashJoinTable hashtable));
extern void mk_hj_temp ARGS((char *tempname));
extern void partition_indexscan ARGS((int numIndices, IndexScanDescPtr scanDescs, int parallel));
extern void x_at_init ARGS((Plan node, BaseNode state));
extern void x_post_end ARGS((Plan node, BaseNode state));
extern void x_post_init ARGS((Plan node, BaseNode state));
extern void x_post_proc ARGS((Plan node, BaseNode state));
extern void x_pre_end ARGS((Plan node, BaseNode state));
extern void x_pre_proc ARGS((Plan node, BaseNode state));
