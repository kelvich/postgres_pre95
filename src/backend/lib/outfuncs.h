/* ------------------------------------------
 *   FILE
 *	outfuncs.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/outfuncs.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef OUTFUNCS_H
#define OUTFUNC_H
void _outPlanInfo ARGS((StringInfo str , Plan node ));
void _outPlan ARGS((StringInfo str , Plan node ));
void _outFragment ARGS((StringInfo str , Fragment node ));
void _outResult ARGS((StringInfo str , Result node ));
void _outExistential ARGS((StringInfo str , Existential node ));
int _outAppend ARGS((StringInfo str , Append node ));
void _outJoinRuleInfo ARGS((StringInfo str , JoinRuleInfo node ));
void _outJoin ARGS((StringInfo str , Join node ));
void _outNestLoop ARGS((StringInfo str , NestLoop node ));
void _outMergeJoin ARGS((StringInfo str , MergeJoin node ));
void _outHashJoin ARGS((StringInfo str , HashJoin node ));
void _outScan ARGS((StringInfo str , Scan node ));
void _outScanTemps ARGS((StringInfo str , ScanTemps node ));
void _outSeqScan ARGS((StringInfo str , SeqScan node ));
void _outIndexScan ARGS((StringInfo str , IndexScan node ));
void _outTemp ARGS((StringInfo str , Temp node ));
void _outSort ARGS((StringInfo str , Sort node ));
void _outAgg ARGS((StringInfo str , Agg node ));
void _outUnique ARGS((StringInfo str , Unique node ));
void _outHash ARGS((StringInfo str , Hash node ));
void _outResdom ARGS((StringInfo str , Resdom node ));
void _outExpr ARGS((StringInfo str , Expr node ));
void _outVar ARGS((StringInfo str , Var node ));
void _outConst ARGS((StringInfo str , Const node ));
void _outArray ARGS((StringInfo str , Array node ));
void _outFunc ARGS((StringInfo str , Func node ));
void _outOper ARGS((StringInfo str , Oper node ));
void _outParam ARGS((StringInfo str , Param node ));
void _outEState ARGS((StringInfo str , EState node ));
void _outRel ARGS((StringInfo str , Rel node ));
void _outSortKey ARGS((StringInfo str , SortKey node ));
void _outPath ARGS((StringInfo str , Path node ));
void _outIndexPath ARGS((StringInfo str , IndexPath node ));
void _outJoinPath ARGS((StringInfo str , JoinPath node ));
void _outMergePath ARGS((StringInfo str , MergePath node ));
void _outHashPath ARGS((StringInfo str , HashPath node ));
void _outOrderKey ARGS((StringInfo str , OrderKey node ));
void _outJoinKey ARGS((StringInfo str , JoinKey node ));
void _outMergeOrder ARGS((StringInfo str , MergeOrder node ));
void _outCInfo ARGS((StringInfo str , CInfo node ));
void _outJoinMethod ARGS((StringInfo str , JoinMethod node ));
void _outHInfo ARGS((StringInfo str , HInfo node ));
void _outJInfo ARGS((StringInfo str , JInfo node ));
void _outDatum ARGS((StringInfo str , Datum value , ObjectId type ));

#endif
