/* $Header$ */


/* aggregate.c */
TupleTableSlot ExecAgg ARGS((Agg node ));
List ExecInitAgg ARGS((Agg node , EState estate , Plan parent ));
void ExecEndAgg ARGS((Agg node ));

/* ex_ami.c */
void ExecOpenScanR ARGS((ObjectId relOid , int nkeys , ScanKey skeys , bool isindex , ScanDirection dir , TimeQual timeRange , Relation *returnRelation , Pointer *returnScanDesc ));
Relation ExecOpenR ARGS((ObjectId relationOid , bool isindex ));
Pointer ExecBeginScan ARGS((Relation relation , int nkeys , ScanKey skeys , bool isindex , ScanDirection dir , TimeQual time_range ));
void ExecCloseR ARGS((Plan node ));
void ExecReScan ARGS((Plan node ));
HeapScanDesc ExecReScanR ARGS((Relation relDesc , HeapScanDesc scanDesc , ScanDirection direction , int nkeys , ScanKey skeys ));
List ExecMarkPos ARGS((Plan node ));
void ExecRestrPos ARGS((Plan node ));
Relation ExecCreatR ARGS((int numberAttributes , TupleDescriptor tupType , int relationOid ));

/* ex_debug.c */
bool DebugVariableSet ARGS((String debug_variable , int debug_value ));
bool DebugVariableProcessCommand ARGS((char *buf ));
void DebugVariableFileSet ARGS((String filename ));
int PrintParallelDebugInfo ARGS((FILE *statfp ));
int ResetParallelDebugInfo ARGS((void ));
Pointer FQdGetCommand ARGS((List queryDesc ));
Pointer FQdGetCount ARGS((List queryDesc ));
Pointer FGetOperation ARGS((List queryDesc ));
Pointer FQdGetParseTree ARGS((List queryDesc ));
Pointer FQdGetPlan ARGS((List queryDesc ));
Pointer FQdGetState ARGS((List queryDesc ));
Pointer FQdGetFeature ARGS((List queryDesc ));
Pointer Fparse_tree_range_table ARGS((List queryDesc ));
Pointer Fparse_tree_result_relation ARGS((List queryDesc ));
void say_at_init ARGS((void ));
void say_pre_proc ARGS((void ));
void say_pre_end ARGS((void ));
void say_post_init ARGS((void ));
void say_post_proc ARGS((void ));
void say_post_end ARGS((void ));
void say_yow ARGS((void ));
void noop ARGS((void ));
void InitHook ARGS((HookNode hook ));
void NoisyHook ARGS((HookNode hook ));
String GetNodeName ARGS((Node node ));
Plan AddMaterialNode ARGS((Plan plan ));
Plan AddSortNode ARGS((Plan plan , int op ));
Plan AddUniqueNode ARGS((Plan plan ));

/* ex_junk.c */
JunkFilter ExecInitJunkFilter ARGS((List targetList ));
bool ExecGetJunkAttribute ARGS((JunkFilter junkfilter , TupleTableSlot slot , Name attrName , Datum *value , Boolean *isNull ));
HeapTuple ExecRemoveJunk ARGS((JunkFilter junkfilter , TupleTableSlot slot ));

/* ex_main.c */
void InitializeExecutor ARGS((void ));
List ExecMain ARGS((List queryDesc , EState estate , List feature ));
List InitPlan ARGS((int operation , List parseTree , Plan plan , EState estate ));
List EndPlan ARGS((Plan plan , EState estate ));
TupleTableSlot ExecutePlan ARGS((EState estate , Plan plan , LispValue parseTree , int operation , int numberTuples , int direction , void (*printfunc )()));
TupleTableSlot ExecRetrieve ARGS((TupleTableSlot slot , void (*printfunc )(), Relation intoRelationDesc ));
TupleTableSlot ExecAppend ARGS((TupleTableSlot slot , ItemPointer tupleid , EState estate , RuleLock newlocks ));
TupleTableSlot ExecDelete ARGS((TupleTableSlot slot , ItemPointer tupleid , EState estate ));
TupleTableSlot ExecReplace ARGS((TupleTableSlot slot , ItemPointer tupleid , EState estate , LispValue parseTree , RuleLock newlocks ));

/* ex_procnode.c */
List ExecInitNode ARGS((Plan node , EState estate , Plan parent ));
TupleTableSlot ExecProcNode ARGS((Plan node ));
void ExecEndNode ARGS((Plan node ));

/* ex_qual.c */
Datum array_cast ARGS((char *value , bool byval , int len ));
Datum ExecEvalArrayRef ARGS((ArrayRef arrayRef, ExprContext econtext, Boolean *isNull, Boolean *isDone));
Datum ExecEvalVar ARGS((Var variable , ExprContext econtext , Boolean *isNull ));
Datum ExecEvalParam ARGS((Param expression , ExprContext econtext, Boolean *isDone ));
/*Datum GetAttribute ARGS((char *attname ));*/
Datum ExecMakeFunctionResult ARGS((Node node, List arguments , ExprContext econtext, Boolean *isNull, Boolean *isDone ));
Datum ExecEvalOper ARGS((List opClause , ExprContext econtext , Boolean *isNull ));
Datum ExecEvalFunc ARGS((Func funcClause , ExprContext econtext , Boolean *isNull, Boolean *isDone ));
Datum ExecEvalNot ARGS((List notclause , ExprContext econtext , Boolean *isNull ));
Datum ExecEvalOr ARGS((List orExpr , ExprContext econtext , Boolean *isNull ));
Datum ExecEvalExpr ARGS((Node expression , ExprContext econtext , Boolean *isNull, Boolean *isDone ));
bool ExecQualClause ARGS((List clause , ExprContext econtext ));
bool ExecQual ARGS((List qual , ExprContext econtext ));
HeapTuple ExecTargetList ARGS((List targetlist , int nodomains , TupleDescriptor targettype , Pointer values , ExprContext econtext, Boolean *isDone ));
TupleTableSlot ExecProject ARGS((ProjectionInfo projInfo, Boolean *isDone ));

/* ex_scan.c */
TupleTableSlot ExecScan ARGS((Scan node , Pointer (*accessMtd )()));

/* ex_tuples.c */
TupleTable ExecCreateTupleTable ARGS((int initialSize ));
void ExecDestroyTupleTable ARGS((TupleTable table , bool shouldFree ));
int ExecAllocTableSlot ARGS((TupleTable table ));
Pointer ExecGetTableSlot ARGS((TupleTable table , int slotnum ));
Pointer ExecStoreTuple ARGS((Pointer tuple , Pointer slot , Buffer buffer , bool shouldFree ));
Pointer ExecStoreTupleDebug ARGS((String file , int line , Pointer tuple , Pointer slot , Buffer buffer , bool shouldFree ));
Pointer ExecClearTuple ARGS((Pointer slot ));
bool ExecSlotPolicy ARGS((Pointer slot ));
bool ExecSetSlotPolicy ARGS((Pointer slot , bool shouldFree ));
TupleDescriptor ExecSetSlotDescriptor ARGS((Pointer slot , TupleDescriptor tupdesc ));
void ExecSetSlotDescriptorIsNew ARGS((Pointer slot , bool isNew ));
TupleDescriptor ExecSetNewSlotDescriptor ARGS((Pointer slot , TupleDescriptor tupdesc ));
Buffer ExecSetSlotBuffer ARGS((Pointer slot , Buffer b ));
void ExecIncrSlotBufferRefcnt ARGS((Pointer slot ));
bool ExecNullSlot ARGS((Pointer slot ));
bool ExecSlotDescriptorIsNew ARGS((Pointer slot ));
void ExecInitResultTupleSlot ARGS((EState estate , CommonState commonstate ));
void ExecInitScanTupleSlot ARGS((EState estate , CommonScanState commonscanstate ));
void ExecInitRawTupleSlot ARGS((EState estate , CommonScanState commonscanstate ));
void ExecInitMarkedTupleSlot ARGS((EState estate , MergeJoinState mergestate ));
void ExecInitOuterTupleSlot ARGS((EState estate , HashJoinState hashstate ));
void ExecInitHashTupleSlot ARGS((EState estate , HashJoinState hashstate ));
TupleDescriptor ExecGetTupType ARGS((Plan node ));
TupleDescriptor ExecTypeFromTL ARGS((List targetList ));
TupleTableSlot NodeGetResultTupleSlot ARGS((Plan node));
ExecTupDescriptor ExecGetExecTupDesc ARGS((Plan node));
TupleDescriptor ExecTupDescToTupDesc ARGS((ExecTupDescriptor execTupDesc, int len));
ExecTupDescriptor TupDescToExecTupDesc ARGS((TupleDescriptor tupDesc, int len));
TupleDescriptor  ExecCopyTupType ARGS((TupleDescriptor td, int natts));

/* ex_utils.c */
void ResetTupleCount ARGS((void ));
void DisplayTupleCount ARGS((FILE *statfp ));
BaseNode ExecGetPrivateState ARGS((Plan node ));
void ExecAssignNodeBaseInfo ARGS((EState estate , BaseNode basenode , Plan parent ));
void ExecAssignDebugHooks ARGS((Plan node , BaseNode basenode ));
void ExecAssignExprContext ARGS((EState estate , CommonState commonstate ));
void ExecAssignResultType ARGS((CommonState commonstate , ExecTupDescriptor execTupDesc, TupleDescriptor tupDesc ));
void ExecAssignResultTypeFromOuterPlan ARGS((Plan node , CommonState commonstate ));
void ExecAssignResultTypeFromTL ARGS((Plan node , CommonState commonstate ));
TupleDescriptor ExecGetResultType ARGS((CommonState commonstate ));
void ExecFreeResultType ARGS((CommonState commonstate ));
void ExecAssignProjectionInfo ARGS((Plan node , CommonState commonstate ));
void ExecFreeProjectionInfo ARGS((CommonState commonstate ));
TupleDescriptor ExecGetScanType ARGS((CommonScanState csstate ));
void ExecFreeScanType ARGS((CommonScanState csstate ));
void ExecAssignScanType ARGS((CommonScanState csstate , ExecTupDescriptor execTupDesc, TupleDescriptor tupDesc ));
void ExecAssignScanTypeFromOuterPlan ARGS((Plan node , CommonState commonstate ));
Attribute ExecGetTypeInfo ARGS((Relation relDesc ));
TupleDescriptor ExecMakeTypeInfo ARGS((int nelts ));
void ExecSetTypeInfo ARGS((int index , struct attribute **typeInfo , ObjectId typeID , int attNum , int attLen , char *attName , Boolean attbyVal ));
void ExecFreeTypeInfo ARGS((struct attribute **typeInfo ));
List QueryDescGetTypeInfo ARGS((List queryDesc ));
List ExecCollect ARGS((List l , List (*applyFunction )(), List (*collectFunction )(), List applyParameters ));
List ExecUniqueCons ARGS((List list1 , List list2 ));
List ExecGetVarAttlistFromExpr ARGS((Node expr , List relationNum ));
List ExecGetVarAttlistFromTLE ARGS((List tle , List relationNum ));
AttributeNumberPtr ExecMakeAttsFromList ARGS((List attlist , int *numAttsPtr ));
void ExecInitScanAttributes ARGS((Plan node ));
AttributeNumberPtr ExecMakeBogusScanAttributes ARGS((int natts ));
void ExecFreeScanAttributes ARGS((AttributeNumberPtr ptr ));
int ExecGetVarLen ARGS((Plan node, CommonState commonstate, Var var));
TupleDescriptor ExecGetVarTupDesc ARGS((Plan node, CommonState commonstate, Var var));
ExecTupDescriptor ExecMakeExecTupDesc ARGS((int len));
ExecAttDesc ExecMakeExecAttDesc ARGS((AttributeTag tag, int len));
ExecAttDesc MakeExecAttDesc ARGS((AttributeTag tag, int len, TupleDescriptor tupdesc));

/* ex_xdebug.c */
void ExecXInitialize ARGS((void ));
void ExecXNewTree ARGS((void ));
void ExecXAddNode ARGS((int base_id , char *title , int supernode_id ));
void ExecXSignalNode ARGS((int base_id , int signal ));
void ExecXShowTree ARGS((void ));
void ExecXHideTree ARGS((void ));
void ExecXTerminate ARGS((void ));
void x_at_init ARGS((Plan node , BaseNode state ));
void x_post_init ARGS((Plan node , BaseNode state ));
void x_pre_proc ARGS((Plan node , BaseNode state ));
void x_post_proc ARGS((Plan node , BaseNode state ));
void x_pre_end ARGS((Plan node , BaseNode state ));
void x_post_end ARGS((Plan node , BaseNode state ));
void InitXHook ARGS((HookNode hook ));

/* functions.c */
char *postquel_lang_func_call_array ARGS((ObjectId procedureId , int pronargs , char *args []));
char *postquel_lang_func_call ARGS((ObjectId procedureId , int pronargs , FmgrValues values ));
int postquel_lang_func_call_array_fcache ARGS((void ));
char *ExecCallFunction ARGS((FunctionCachePtr fcache , char *args []));

/* n_append.c */
List exec_append_initialize_next ARGS((Append node ));
List ExecInitAppend ARGS((Append node , EState estate , Plan parent ));
TupleTableSlot ExecProcAppend ARGS((Append node ));
void ExecEndAppend ARGS((Append node ));

/* n_hash.c */
TupleTableSlot ExecHash ARGS((Hash node ));
List ExecInitHash ARGS((Hash node , EState estate , Plan parent ));
void ExecEndHash ARGS((Hash node ));
RelativeAddr hashTableAlloc ARGS((int size , HashJoinTable hashtable ));
HashJoinTable ExecHashTableCreate ARGS((Plan node ));
void ExecHashTableInsert ARGS((HashJoinTable hashtable , ExprContext econtext , Var hashkey , File *batches ));
void ExecHashTableDestroy ARGS((HashJoinTable hashtable ));
int ExecHashGetBucket ARGS((HashJoinTable hashtable , ExprContext econtext , Var hashkey ));
void ExecHashOverflowInsert ARGS((HashJoinTable hashtable , HashBucket bucket , HeapTuple heapTuple ));
HeapTuple ExecScanHashBucket ARGS((HashJoinState hjstate , HashBucket bucket , HeapTuple curtuple , List hjclauses , ExprContext econtext ));
int hashFunc ARGS((char *key , int len ));
int ExecHashPartition ARGS((Hash node ));
void ExecHashTableReset ARGS((HashJoinTable hashtable , int ntuples ));
void mk_hj_temp ARGS((char *tempname ));
int print_buckets ARGS((HashJoinTable hashtable ));

/* n_hashjoin.c */
TupleTableSlot ExecHashJoin ARGS((HashJoin node ));
List ExecInitHashJoin ARGS((HashJoin node , EState estate , Plan parent ));
void ExecEndHashJoin ARGS((HashJoin node ));
TupleTableSlot ExecHashJoinOuterGetTuple ARGS((Plan node , HashJoinState hjstate ));
TupleTableSlot ExecHashJoinGetSavedTuple ARGS((HashJoinState hjstate , char *buffer , File file , Pointer tupleSlot , int *block , char **position ));
int ExecHashJoinNewBatch ARGS((HashJoinState hjstate ));
int ExecHashJoinGetBatch ARGS((int bucketno , HashJoinTable hashtable , int nbatch ));
char *ExecHashJoinSaveTuple ARGS((HeapTuple heapTuple , char *buffer , File file , char *position ));

/* n_indexscan.c */
void ExecGetIndexKeyInfo ARGS((IndexTupleForm indexTuple , int *numAttsOutP , AttributeNumberPtr *attsOutP , FuncIndexInfoPtr fInfoP ));
void ExecOpenIndices ARGS((ObjectId resultRelationOid , RelationInfo resultRelationInfo ));
void ExecCloseIndices ARGS((RelationInfo resultRelationInfo ));
IndexTuple ExecFormIndexTuple ARGS((HeapTuple heapTuple , Relation heapRelation , Relation indexRelation , IndexInfo indexInfo ));
RuleLock ExecInsertIndexTuples ARGS((TupleTableSlot slot , ItemPointer tupleid , EState estate ));
TupleTableSlot IndexNext ARGS((IndexScan node ));
TupleTableSlot ExecIndexScan ARGS((IndexScan node ));
List ExecIndexReScan ARGS((IndexScan node ));
void ExecEndIndexScan ARGS((IndexScan node ));
List ExecIndexMarkPos ARGS((IndexScan node ));
void ExecIndexRestrPos ARGS((IndexScan node ));
void ExecUpdateIndexScanKeys ARGS((IndexScan node , ExprContext econtext ));
List ExecInitIndexScan ARGS((IndexScan node , EState estate , Plan parent ));
void partition_indexscan ARGS((int numIndices , IndexScanDescPtr scanDescs , int parallel ));

/* n_material.c */
TupleTableSlot ExecMaterial ARGS((Material node ));
List ExecInitMaterial ARGS((Material node , EState estate , Plan parent ));
void ExecEndMaterial ARGS((Material node ));
List ExecMaterialMarkPos ARGS((Material node ));
void ExecMaterialRestrPos ARGS((Material node ));

/* n_mergejoin.c */
List MJFormOSortopI ARGS((List qualList , ObjectId sortOp ));
List MJFormISortopO ARGS((List qualList , ObjectId sortOp ));
bool MergeCompare ARGS((List eqQual , List compareQual , ExprContext econtext ));
void ExecMergeTupleDumpInner ARGS((ExprContext econtext ));
void ExecMergeTupleDumpOuter ARGS((ExprContext econtext ));
void ExecMergeTupleDumpMarked ARGS((ExprContext econtext , MergeJoinState mergestate ));
void ExecMergeTupleDump ARGS((ExprContext econtext , MergeJoinState mergestate ));
TupleTableSlot ExecMergeJoin ARGS((MergeJoin node ));
List ExecInitMergeJoin ARGS((MergeJoin node , EState estate , Plan parent ));
void ExecEndMergeJoin ARGS((MergeJoin node ));

/* n_nestloop.c */
TupleTableSlot ExecNestLoop ARGS((NestLoop node ));
List ExecInitNestLoop ARGS((NestLoop node , EState estate , Plan parent ));
List ExecEndNestLoop ARGS((NestLoop node ));

/* n_result.c */
TupleTableSlot ExecResult ARGS((Result node ));
List ExecInitResult ARGS((Plan node , EState estate , Plan parent ));
void ExecEndResult ARGS((Result node ));

/* n_scantemps.c */
TupleTableSlot ExecScanTemps ARGS((ScanTemps node ));
List ExecInitScanTemps ARGS((ScanTemps node , EState estate , Plan parent ));
void ExecEndScanTemps ARGS((ScanTemps node ));

/* n_seqscan.c */
TupleTableSlot SeqNext ARGS((SeqScan node ));
TupleTableSlot ExecSeqScan ARGS((SeqScan node ));
ObjectId InitScanRelation ARGS((Plan node , EState estate , ScanState scanstate , Plan outerPlan ));
List ExecInitSeqScan ARGS((Plan node , EState estate , Plan parent ));
void ExecEndSeqScan ARGS((SeqScan node ));
void ExecSeqReScan ARGS((Plan node ));
List ExecSeqMarkPos ARGS((Plan node ));
void ExecSeqRestrPos ARGS((Plan node ));

/* n_sort.c */
Pointer FormSortKeys ARGS((Sort sortnode ));
TupleTableSlot ExecSort ARGS((Sort node ));
List ExecInitSort ARGS((Sort node , EState estate , Plan parent ));
void ExecEndSort ARGS((Sort node ));
List ExecSortMarkPos ARGS((Plan node ));
void ExecSortRestrPos ARGS((Plan node ));

/* n_unique.c */
bool ExecIdenticalTuples ARGS((List t1 , List t2 ));
TupleTableSlot ExecUnique ARGS((Unique node ));
List ExecInitUnique ARGS((Unique node , EState estate , Plan parent ));
void ExecEndUnique ARGS((Unique node ));

/* pre_nestdots.c */
void ReplaceNestedDots ARGS((List parseTree , Plan *planP ));


