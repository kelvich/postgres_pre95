/* $Header$ */
extern void set_cost ARGS((Plan node, Cost value));
extern Cost get_cost ARGS((Plan node));
extern void set_fragment ARGS((Plan node, Index value));
extern Index get_fragment ARGS((Plan node));
extern void set_state ARGS((Plan node, struct EState *value));
extern struct EState *get_state ARGS((Plan node));
extern void set_qptargetlist ARGS((Plan node, List value));
extern List get_qptargetlist ARGS((Plan node));
extern void set_qpqual ARGS((Plan node, List value));
extern List get_qpqual ARGS((Plan node));
extern void set_lefttree ARGS((Plan node, struct Plan *value));
extern struct Plan *get_lefttree ARGS((Plan node));
extern void set_righttree ARGS((Plan node, struct Plan *value));
extern struct Plan *get_righttree ARGS((Plan node));
extern void set_exstate ARGS((Existential node, ExistentialState value));
extern ExistentialState get_exstate ARGS((Existential node));
extern void set_resrellevelqual ARGS((Result node, List value));
extern List get_resrellevelqual ARGS((Result node));
extern void set_resconstantqual ARGS((Result node, List value));
extern List get_resconstantqual ARGS((Result node));
extern void set_resstate ARGS((Result node, ResultState value));
extern ResultState get_resstate ARGS((Result node));
extern void set_unionplans ARGS((Append node, List value));
extern List get_unionplans ARGS((Append node));
extern void set_unionrelid ARGS((Append node, Index value));
extern Index get_unionrelid ARGS((Append node));
extern void set_unionrtentries ARGS((Append node, List value));
extern List get_unionrtentries ARGS((Append node));
extern void set_unionstate ARGS((Append node, AppendState value));
extern AppendState get_unionstate ARGS((Append node));
extern void set_parstate ARGS((Parallel node, ParallelState value));
extern ParallelState get_parstate ARGS((Parallel node));
extern void set_recurMethod ARGS((Recursive node, RecursiveMethod value));
extern RecursiveMethod get_recurMethod ARGS((Recursive node));
extern void set_recurCommand ARGS((Recursive node, LispValue value));
extern LispValue get_recurCommand ARGS((Recursive node));
extern void set_recurInitPlans ARGS((Recursive node, List value));
extern List get_recurInitPlans ARGS((Recursive node));
extern void set_recurLoopPlans ARGS((Recursive node, List value));
extern List get_recurLoopPlans ARGS((Recursive node));
extern void set_recurCleanupPlans ARGS((Recursive node, List value));
extern List get_recurCleanupPlans ARGS((Recursive node));
extern void set_recurCheckpoints ARGS((Recursive node, List value));
extern List get_recurCheckpoints ARGS((Recursive node));
extern void set_recurResultRelationName ARGS((Recursive node, LispValue value));
extern LispValue get_recurResultRelationName ARGS((Recursive node));
extern void set_recurState ARGS((Recursive node, RecursiveState value));
extern RecursiveState get_recurState ARGS((Recursive node));
extern void set_scanrelid ARGS((Scan node, Index value));
extern Index get_scanrelid ARGS((Scan node));
extern void set_scanstate ARGS((Scan node, ScanState value));
extern ScanState get_scanstate ARGS((Scan node));
extern void set_indxid ARGS((IndexScan node, List value));
extern List get_indxid ARGS((IndexScan node));
extern void set_indxqual ARGS((IndexScan node, List value));
extern List get_indxqual ARGS((IndexScan node));
extern void set_indxstate ARGS((IndexScan node, IndexScanState value));
extern IndexScanState get_indxstate ARGS((IndexScan node));
extern void set_nlstate ARGS((NestLoop node, NestLoopState value));
extern NestLoopState get_nlstate ARGS((NestLoop node));
extern void set_mergeclauses ARGS((MergeJoin node, List value));
extern List get_mergeclauses ARGS((MergeJoin node));
extern void set_mergesortop ARGS((MergeJoin node, ObjectId value));
extern ObjectId get_mergesortop ARGS((MergeJoin node));
extern void set_mergerightorder ARGS((MergeJoin node, List value));
extern List get_mergerightorder ARGS((MergeJoin node));
extern void set_mergeleftorder ARGS((MergeJoin node, List value));
extern List get_mergeleftorder ARGS((MergeJoin node));
extern void set_mergestate ARGS((MergeJoin node, MergeJoinState value));
extern MergeJoinState get_mergestate ARGS((MergeJoin node));
extern void set_hashclauses ARGS((HashJoin node, List value));
extern List get_hashclauses ARGS((HashJoin node));
extern void set_hashjoinop ARGS((HashJoin node, ObjectId value));
extern ObjectId get_hashjoinop ARGS((HashJoin node));
extern void set_hashjoinstate ARGS((HashJoin node, HashJoinState value));
extern HashJoinState get_hashjoinstate ARGS((HashJoin node));
extern void set_tempid ARGS((Temp node, ObjectId value));
extern ObjectId get_tempid ARGS((Temp node));
extern void set_keycount ARGS((Temp node, Count value));
extern Count get_keycount ARGS((Temp node));
extern void set_matstate ARGS((Material node, MaterialState value));
extern MaterialState get_matstate ARGS((Material node));
extern void set_sortstate ARGS((Sort node, SortState value));
extern SortState get_sortstate ARGS((Sort node));
extern void set_uniquestate ARGS((Unique node, UniqueState value));
extern UniqueState get_uniquestate ARGS((Unique node));
extern void set_hashstate ARGS((Hash node, HashState value));
extern HashState get_hashstate ARGS((Hash node));
extern Plan MakePlan ARGS((Cost cost, Index fragment, struct EState *state, List qptargetlist, List qpqual, struct Plan *lefttree, struct Plan *righttree));
extern void PrintPlan ARGS((FILE *fp, Plan node));
extern bool EqualPlan ARGS((Plan a, Plan b));
extern Existential MakeExistential ARGS((ExistentialState exstate));
extern void PrintExistential ARGS((FILE *fp, Existential node));
extern bool EqualExistential ARGS((Existential a, Existential b));
extern Result MakeResult ARGS((List resrellevelqual, List resconstantqual, ResultState resstate));
extern void PrintResult ARGS((FILE *fp, Result node));
extern bool EqualResult ARGS((Result a, Result b));
extern Append MakeAppend ARGS((List unionplans, Index unionrelid, List unionrtentries, AppendState unionstate));
extern void PrintAppend ARGS((FILE *fp, Append node));
extern bool EqualAppend ARGS((Append a, Append b));
extern Parallel MakeParallel ARGS((ParallelState parstate));
extern void PrintParallel ARGS((FILE *fp, Parallel node));
extern bool EqualParallel ARGS((Parallel a, Parallel b));
extern Recursive MakeRecursive ARGS((RecursiveMethod recurMethod, LispValue recurCommand, List recurInitPlans, List recurLoopPlans, List recurCleanupPlans, List recurCheckpoints, LispValue recurResultRelationName, RecursiveState recurState));
extern void PrintRecursive ARGS((FILE *fp, Recursive node));
extern bool EqualRecursive ARGS((Recursive a, Recursive b));
extern Scan MakeScan ARGS((Index scanrelid, ScanState scanstate));
extern void PrintScan ARGS((FILE *fp, Scan node));
extern bool EqualScan ARGS((Scan a, Scan b));
extern SeqScan MakeSeqScan ARGS((int scanrelid));
extern void PrintSeqScan ARGS((FILE *fp, SeqScan node));
extern bool EqualSeqScan ARGS((SeqScan a, SeqScan b));
extern IndexScan MakeIndexScan ARGS((List indxid, List indxqual, IndexScanState indxstate));
extern void PrintIndexScan ARGS((FILE *fp, IndexScan node));
extern bool EqualIndexScan ARGS((IndexScan a, IndexScan b));
extern Join MakeJoin ARGS((int indxid));
extern void PrintJoin ARGS((FILE *fp, Join node));
extern bool EqualJoin ARGS((Join a, Join b));
extern NestLoop MakeNestLoop ARGS((NestLoopState nlstate));
extern void PrintNestLoop ARGS((FILE *fp, NestLoop node));
extern bool EqualNestLoop ARGS((NestLoop a, NestLoop b));
extern MergeJoin MakeMergeJoin ARGS((List mergeclauses, ObjectId mergesortop, List mergerightorder, List mergeleftorder, MergeJoinState mergestate));
extern void PrintMergeJoin ARGS((FILE *fp, MergeJoin node));
extern bool EqualMergeJoin ARGS((MergeJoin a, MergeJoin b));
extern HashJoin MakeHashJoin ARGS((List hashclauses, ObjectId hashjoinop, HashJoinState hashjoinstate));
extern void PrintHashJoin ARGS((FILE *fp, HashJoin node));
extern bool EqualHashJoin ARGS((HashJoin a, HashJoin b));
extern Temp MakeTemp ARGS((ObjectId tempid, Count keycount));
extern void PrintTemp ARGS((FILE *fp, Temp node));
extern bool EqualTemp ARGS((Temp a, Temp b));
extern Material MakeMaterial ARGS((MaterialState matstate));
extern void PrintMaterial ARGS((FILE *fp, Material node));
extern bool EqualMaterial ARGS((Material a, Material b));
extern Sort MakeSort ARGS((SortState sortstate));
extern void PrintSort ARGS((FILE *fp, Sort node));
extern bool EqualSort ARGS((Sort a, Sort b));
extern Unique MakeUnique ARGS((UniqueState uniquestate));
extern void PrintUnique ARGS((FILE *fp, Unique node));
extern bool EqualUnique ARGS((Unique a, Unique b));
extern Hash MakeHash ARGS((HashState hashstate));
extern void PrintHash ARGS((FILE *fp, Hash node));
extern bool EqualHash ARGS((Hash a, Hash b));
