/* ------------------------------------------
 *   FILE
 *	readfuncs.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/readfuncs.h
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef READFUNCS_H
#define READFUNCS_H
void _getPlan ARGS((Plan node ));
Plan _readPlan ARGS((void ));
Result _readResult ARGS((void ));
Existential _readExistential ARGS((void ));
Append _readAppend ARGS((void ));
JoinRuleInfo _readJoinRuleInfo ARGS((void ));
void _getJoin ARGS((Join node ));
Join _readJoin ARGS((void ));
NestLoop _readNestLoop ARGS((void ));
MergeJoin _readMergeJoin ARGS((void ));
HashJoin _readHashJoin ARGS((void ));
void _getScan ARGS((Scan node ));
Scan _readScan ARGS((void ));
SeqScan _readSeqScan ARGS((void ));
IndexScan _readIndexScan ARGS((void ));
Temp _readTemp ARGS((void ));
Sort _readSort ARGS((void ));
Agg _readAgg ARGS((void ));
Unique _readUnique ARGS((void ));
Hash _readHash ARGS((void ));
Resdom _readResdom ARGS((void ));
Expr _readExpr ARGS((void ));
Var _readVar ARGS((void ));
Array _readArray ARGS((void ));
Const _readConst ARGS((void ));
Func _readFunc ARGS((void ));
Oper _readOper ARGS((void ));
Param _readParam ARGS((void ));
EState _readEState ARGS((void ));
Rel _readRel ARGS((void ));
SortKey _readSortKey ARGS((void ));
Path _readPath ARGS((void ));
IndexPath _readIndexPath ARGS((void ));
JoinPath _readJoinPath ARGS((void ));
MergePath _readMergePath ARGS((void ));
HashPath _readHashPath ARGS((void ));
OrderKey _readOrderKey ARGS((void ));
JoinKey _readJoinKey ARGS((void ));
MergeOrder _readMergeOrder ARGS((void ));
CInfo _readCInfo ARGS((void ));
JoinMethod _readJoinMethod ARGS((void ));
HInfo _readHInfo ARGS((void ));
JInfo _readJInfo ARGS((void ));
LispValue parsePlanString ARGS((void ));
Datum readValue ARGS((ObjectId type ));

#endif
