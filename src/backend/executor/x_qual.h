/* ----------------------------------------------------------------
 *   FILE
 *	ex_qual.h
 *
 *   DESCRIPTION
 *	prototypes for ex_qual.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_qualIncluded		/* include this file only once */
#define ex_qualIncluded	1

extern Datum ExecEvalArrayRef ARGS((ArrayRef arrayRef, ExprContext econtext, Boolean *isNull, Boolean *isDone));
extern Datum ExecExtractResult ARGS((TupleTableSlot slot, AttributeNumber attnum, Boolean *isNull));
extern Datum ExecEvalVar ARGS((Var variable, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalParam ARGS((Param expression, ExprContext econtext, Boolean *isNull));
extern char *GetAttributeByNum ARGS((TupleTableSlot slot, AttributeNumber attrno, Boolean *isNull));
extern char *att_by_num ARGS((TupleTableSlot slot, AttributeNumber attrno, Boolean *isNull));
extern char *GetAttributeByName ARGS((TupleTableSlot slot, Name attname, Boolean *isNull));
extern char *att_by_name ARGS((TupleTableSlot slot, Name attname, Boolean *isNull));
extern void ExecEvalFuncArgs ARGS((FunctionCachePtr fcache, ExprContext econtext, List argList, Datum argV[], Boolean *argIsDone));
extern Datum ExecMakeFunctionResult ARGS((Node node, List arguments, ExprContext econtext, Boolean *isNull, Boolean *isDone));
extern Datum ExecEvalOper ARGS((List opClause, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalFunc ARGS((LispValue funcClause, ExprContext econtext, Boolean *isNull, Boolean *isDone));
extern Datum ExecEvalNot ARGS((List notclause, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalOr ARGS((List orExpr, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalAnd ARGS((List andExpr, ExprContext econtext, Boolean *isNull));
extern Datum ExecEvalExpr ARGS((Node expression, ExprContext econtext, Boolean *isNull, Boolean *isDone));
extern bool ExecQualClause ARGS((List clause, ExprContext econtext));
extern bool ExecQual ARGS((List qual, ExprContext econtext));
extern HeapTuple ExecFormComplexResult ARGS((List tlist, int natts, TupleDescriptor tdesc, Datum *values, char *nulls));
extern int ExecTargetListLength ARGS((List targetlist));
extern HeapTuple ExecTargetList ARGS((List targetlist, int nodomains, TupleDescriptor targettype, Pointer values, ExprContext econtext, bool *isDone));
extern TupleTableSlot ExecProject ARGS((ProjectionInfo projInfo, bool *isDone));

#endif /* ex_qualIncluded */
