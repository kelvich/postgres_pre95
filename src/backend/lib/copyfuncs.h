/* ------------------------------------------
 *   FILE
 *	copyfuncs.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/copyfuncs.h
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef COPYFUNCSINCLUDED
#define COPYFUNCSINCLUDED
/* copyfuncs.c */
LispValue lispCopy ARGS((LispValue lispObject ));
bool NodeCopy ARGS((Node from , Node *toP , char *(*alloc )()));
Node CopyObject ARGS((Node from ));
Node CopyObjectUsing ARGS((Node from , char *(*alloc )()));
bool CopyNodeFields ARGS((Node from , Node newnode , char *(*alloc )()));
bool _copyNode ARGS((Node from , Node *to , char *(*alloc )()));
bool CopyPlanFields ARGS((Plan from , Plan newnode , char *(*alloc )()));
bool _copyPlan ARGS((Plan from , Plan *to , char *(*alloc )()));
bool _copyExistential ARGS((Existential from , Existential *to , char *(*alloc )()));
bool _copyResult ARGS((Result from , Result *to , char *(*alloc )()));
bool _copyAppend ARGS((Append from , Append *to , char *(*alloc )()));
bool CopyScanFields ARGS((Scan from , Scan newnode , char *(*alloc )()));
Relation CopyRelDescUsing ARGS((Relation reldesc , char *(*alloc )()));
List copyRelDescsUsing ARGS((List relDescs , char *(*alloc )()));
bool CopyScanTempFields ARGS((ScanTemps from , ScanTemps newnode , char *(*alloc )()));
bool _copyScan ARGS((Scan from , Scan *to , char *(*alloc )()));
bool _copyScanTemps ARGS((ScanTemps from , ScanTemps *to , char *(*alloc )()));
bool _copySeqScan ARGS((SeqScan from , SeqScan *to , char *(*alloc )()));
bool _copyIndexScan ARGS((IndexScan from , IndexScan *to , char *(*alloc )()));
bool _copyJoinRuleInfo ARGS((JoinRuleInfo from , JoinRuleInfo *to , char *(*alloc )()));
bool CopyJoinFields ARGS((Join from , Join newnode , char *(*alloc )()));
bool _copyJoin ARGS((Join from , Join *to , char *(*alloc )()));
bool _copyNestLoop ARGS((NestLoop from , NestLoop *to , char *(*alloc )()));
bool _copyMergeJoin ARGS((MergeJoin from , MergeJoin *to , char *(*alloc )()));
bool _copyHashJoin ARGS((HashJoin from , HashJoin *to , char *(*alloc )()));
bool CopyTempFields ARGS((Temp from , Temp newnode , char *(*alloc )()));
bool _copyTemp ARGS((Temp from , Temp *to , char *(*alloc )()));
bool _copyMaterial ARGS((Material from , Material *to , char *(*alloc )()));
bool _copySort ARGS((Sort from , Sort *to , char *(*alloc )()));
bool _copyAgg ARGS((Agg from , Agg *to , char *(*alloc )()));
bool _copyUnique ARGS((Unique from , Unique *to , char *(*alloc )()));
bool _copyHash ARGS((Hash from , Hash *to , char *(*alloc )()));
bool _copyResdom ARGS((Resdom from , Resdom *to , char *(*alloc )()));
bool CopyExprFields ARGS((Expr from , Expr newnode , char *(*alloc )()));
bool _copyExpr ARGS((Expr from , Expr *to , char *(*alloc )()));
bool _copyVar ARGS((Var from , Var *to , char *(*alloc )()));
bool _copyArray ARGS((Array from , Array *to , char *(*alloc )()));
bool _copyOper ARGS((Oper from , Oper *to , char *(*alloc )()));
bool _copyConst ARGS((Const from , Const *to , char *(*alloc )()));
bool _copyParam ARGS((Param from , Param *to , char *(*alloc )()));
bool _copyFunc ARGS((Func from , Func *to , char *(*alloc )()));
bool _copyRel ARGS((Rel from , Rel *to , char *(*alloc )()));
bool _copySortKey ARGS((SortKey from , SortKey *to , char *(*alloc )()));
bool CopyPathFields ARGS((Path from , Path newnode , char *(*alloc )()));
bool _copyPath ARGS((Path from , Path *to , char *(*alloc )()));
bool _copyIndexPath ARGS((IndexPath from , IndexPath *to , char *(*alloc )()));
bool CopyJoinPathFields ARGS((JoinPath from , JoinPath newnode , char *(*alloc )()));
bool _copyJoinPath ARGS((JoinPath from , JoinPath *to , char *(*alloc )()));
bool _copyMergePath ARGS((MergePath from , MergePath *to , char *(*alloc )()));
bool _copyHashPath ARGS((HashPath from , HashPath *to , char *(*alloc )()));
bool _copyOrderKey ARGS((OrderKey from , OrderKey *to , char *(*alloc )()));
bool _copyJoinKey ARGS((JoinKey from , JoinKey *to , char *(*alloc )()));
bool _copyMergeOrder ARGS((MergeOrder from , MergeOrder *to , char *(*alloc )()));
bool _copyCInfo ARGS((CInfo from , CInfo *to , char *(*alloc )()));
bool CopyJoinMethodFields ARGS((JoinMethod from , JoinMethod newnode , char *(*alloc )()));
bool _copyJoinMethod ARGS((JoinMethod from , JoinMethod *to , char *(*alloc )()));
bool _copyHInfo ARGS((HInfo from , HInfo *to , char *(*alloc )()));
bool _copyMInfo ARGS((MInfo from , MInfo *to , char *(*alloc )()));
bool _copyJInfo ARGS((JInfo from , JInfo *to , char *(*alloc )()));
bool _copyLispValue ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispSymbol ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispStr ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispInt ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispFloat ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispVector ARGS((LispValue from , LispValue *to , char *(*alloc )()));
bool _copyLispList ARGS((LispValue from , LispValue *to , char *(*alloc )()));
#endif
