/* ----------------------------------------------------------------
 *   FILE
 *	primnodes.h
 *
 *   DESCRIPTION
 *	prototypes for primnodes.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef primnodesIncluded		/* include this file only once */
#define primnodesIncluded	1

extern void RInitResdom ARGS((Pointer p));
extern Resdom MakeResdom ARGS((AttributeNumber resno, ObjectId restype, bool rescomplex, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop, int resjunk));
extern void OutResdom ARGS((StringInfo str, Resdom node));
extern bool EqualResdom ARGS((Resdom a, Resdom b));
extern bool CopyResdom ARGS((Resdom from, Resdom *to, char *(*alloc)()));
extern Resdom IMakeResdom ARGS((AttributeNumber resno, ObjectId restype, bool rescomplex, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop, int resjunk));
extern Resdom RMakeResdom ARGS((void));
extern void RInitFjoin ARGS((Pointer p));
extern Fjoin MakeFjoin ARGS((bool fj_initialized, int fj_nNodes, List fj_innerNode, DatumPtr fj_results, BoolPtr fj_alwaysDone));
extern void OutFjoin ARGS((StringInfo str, Fjoin node));
extern bool EqualFjoin ARGS((Fjoin a, Fjoin b));
extern bool CopyFjoin ARGS((Fjoin from, Fjoin *to, char *(*alloc)()));
extern Fjoin IMakeFjoin ARGS((bool fj_initialized, int fj_nNodes, List fj_innerNode, DatumPtr fj_results, BoolPtr fj_alwaysDone));
extern Fjoin RMakeFjoin ARGS((void));
extern void RInitExpr ARGS((Pointer p));
extern Expr MakeExpr ARGS((int fj_initialized));
extern void OutExpr ARGS((StringInfo str, Expr node));
extern bool EqualExpr ARGS((Expr a, Expr b));
extern bool CopyExpr ARGS((Expr from, Expr *to, char *(*alloc)()));
extern Expr IMakeExpr ARGS((void));
extern Expr RMakeExpr ARGS((void));
extern void RInitVar ARGS((Pointer p));
extern Var MakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List varid, Pointer varslot));
extern void OutVar ARGS((StringInfo str, Var node));
extern bool EqualVar ARGS((Var a, Var b));
extern bool CopyVar ARGS((Var from, Var *to, char *(*alloc)()));
extern Var IMakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List varid, Pointer varslot));
extern Var RMakeVar ARGS((void));
extern void RInitOper ARGS((Pointer p));
extern Oper MakeOper ARGS((ObjectId opno, ObjectId opid, bool oprelationlevel, ObjectId opresulttype, int opsize, FunctionCachePtr op_fcache));
extern void OutOper ARGS((StringInfo str, Oper node));
extern bool EqualOper ARGS((Oper a, Oper b));
extern bool CopyOper ARGS((Oper from, Oper *to, char *(*alloc)()));
extern Oper IMakeOper ARGS((ObjectId opno, ObjectId opid, bool oprelationlevel, ObjectId opresulttype, int opsize, FunctionCachePtr op_fcache));
extern Oper RMakeOper ARGS((void));
extern void RInitConst ARGS((Pointer p));
extern Const MakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull, bool constbyval, bool constisset));
extern void OutConst ARGS((StringInfo str, Const node));
extern bool EqualConst ARGS((Const a, Const b));
extern bool CopyConst ARGS((Const from, Const *to, char *(*alloc)()));
extern Const IMakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull, bool constbyval, bool constisset));
extern Const RMakeConst ARGS((void));
extern void RInitParam ARGS((Pointer p));
extern Param MakeParam ARGS((int paramkind, AttributeNumber paramid, Name paramname, ObjectId paramtype, List param_tlist));
extern void OutParam ARGS((StringInfo str, Param node));
extern bool EqualParam ARGS((Param a, Param b));
extern bool CopyParam ARGS((Param from, Param *to, char *(*alloc)()));
extern Param IMakeParam ARGS((int paramkind, AttributeNumber paramid, Name paramname, ObjectId paramtype, List param_tlist));
extern Param RMakeParam ARGS((void));
extern void RInitFunc ARGS((Pointer p));
extern Func MakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex, int funcsize, FunctionCachePtr func_fcache, List func_tlist, List func_planlist));
extern void OutFunc ARGS((StringInfo str, Func node));
extern bool EqualFunc ARGS((Func a, Func b));
extern bool CopyFunc ARGS((Func from, Func *to, char *(*alloc)()));
extern Func IMakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex, int funcsize, FunctionCachePtr func_fcache, List func_tlist, List func_planlist));
extern Func RMakeFunc ARGS((void));
extern void RInitArray ARGS((Pointer p));
extern Array MakeArray ARGS((ObjectId arrayelemtype, int arrayelemlength, bool arrayelembyval, int arrayndim, IntArray arraylow, IntArray arrayhigh, int arraylen));
extern void OutArray ARGS((StringInfo str, Array node));
extern bool EqualArray ARGS((Array a, Array b));
extern bool CopyArray ARGS((Array from, Array *to, char *(*alloc)()));
extern Array IMakeArray ARGS((ObjectId arrayelemtype, int arrayelemlength, bool arrayelembyval, int arrayndim, IntArray arraylow, IntArray arrayhigh, int arraylen));
extern Array RMakeArray ARGS((void));
extern void RInitArrayRef ARGS((Pointer p));
extern ArrayRef MakeArrayRef ARGS((int refattrlength, int refelemlength, ObjectId refelemtype, bool refelembyval, LispValue refupperindexpr, LispValue reflowerindexpr, LispValue refexpr, LispValue refassgnexpr));
extern void OutArrayRef ARGS((StringInfo str, ArrayRef node));
extern bool EqualArrayRef ARGS((ArrayRef a, ArrayRef b));
extern bool CopyArrayRef ARGS((ArrayRef from, ArrayRef *to, char *(*alloc)()));
extern ArrayRef IMakeArrayRef ARGS((int refattrlength, int refelemlength, ObjectId refelemtype, bool refelembyval, LispValue refupperindexpr, LispValue reflowerindexpr, LispValue refexpr, LispValue refassgnexpr));
extern ArrayRef RMakeArrayRef ARGS((void));

#endif /* primnodesIncluded */
