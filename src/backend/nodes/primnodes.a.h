/* $Header$ */
extern void RInitResdom ARGS((Pointer p));
extern Resdom MakeResdom ARGS((AttributeNumber resno, ObjectId restype, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop, int resjunk));
extern void OutResdom ARGS((StringInfo str, Resdom node));
extern bool EqualResdom ARGS((Resdom a, Resdom b));
extern bool CopyResdom ARGS((Resdom from, Resdom *to, int alloc));
extern Resdom IMakeResdom ARGS((AttributeNumber resno, ObjectId restype, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop, int resjunk));
extern void RInitExpr ARGS((Pointer p));
extern Expr MakeExpr ARGS((int resno));
extern void OutExpr ARGS((StringInfo str, Expr node));
extern bool EqualExpr ARGS((Expr a, Expr b));
extern bool CopyExpr ARGS((Expr from, Expr *to, int alloc));
extern void RInitVar ARGS((Pointer p));
extern Var MakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List vardotfields, List vararraylist, List varid, Pointer varslot));
extern void OutVar ARGS((StringInfo str, Var node));
extern bool EqualVar ARGS((Var a, Var b));
extern bool CopyVar ARGS((Var from, Var *to, int alloc));
extern Var IMakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List vardotfields, List vararraylist, List varid, Pointer varslot));
extern void RInitOper ARGS((Pointer p));
extern Oper MakeOper ARGS((ObjectId opno, ObjectId opid, bool oprelationlevel, ObjectId opresulttype, int opsize, FunctionCachePtr op_fcache));
extern void OutOper ARGS((StringInfo str, Oper node));
extern bool EqualOper ARGS((Oper a, Oper b));
extern bool CopyOper ARGS((Oper from, Oper *to, int alloc));
extern Oper IMakeOper ARGS((ObjectId opno, ObjectId opid, bool oprelationlevel, ObjectId opresulttype, int opsize, FunctionCachePtr op_fcache));
extern void RInitConst ARGS((Pointer p));
extern Const MakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull, bool constbyval));
extern void OutConst ARGS((StringInfo str, Const node));
extern bool EqualConst ARGS((Const a, Const b));
extern bool CopyConst ARGS((Const from, Const *to, int alloc));
extern Const IMakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull, bool constbyval));
extern void RInitParam ARGS((Pointer p));
extern Param MakeParam ARGS((int paramkind, AttributeNumber paramid, Name paramname, ObjectId paramtype));
extern void OutParam ARGS((StringInfo str, Param node));
extern bool EqualParam ARGS((Param a, Param b));
extern bool CopyParam ARGS((Param from, Param *to, int alloc));
extern Param IMakeParam ARGS((int paramkind, AttributeNumber paramid, Name paramname, ObjectId paramtype));
extern void RInitFunc ARGS((Pointer p));
extern Func MakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex, int funcsize, FunctionCachePtr func_fcache));
extern void OutFunc ARGS((StringInfo str, Func node));
extern bool EqualFunc ARGS((Func a, Func b));
extern bool CopyFunc ARGS((Func from, Func *to, int alloc));
extern Func IMakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex, int funcsize, FunctionCachePtr func_fcache));
extern void RInitArray ARGS((Pointer p));
extern Array MakeArray ARGS((ObjectId arrayelemtype, int arrayelemlength, bool arrayelembyval, int arraylow, int arrayhigh, int arraylen));
extern void OutArray ARGS((StringInfo str, Array node));
extern bool EqualArray ARGS((Array a, Array b));
extern bool CopyArray ARGS((Array from, Array *to, int alloc));
extern Array IMakeArray ARGS((ObjectId arrayelemtype, int arrayelemlength, bool arrayelembyval, int arraylow, int arrayhigh, int arraylen));
