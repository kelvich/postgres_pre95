/* $Header$ */
extern void set_resno ARGS((Resdom node, AttributeNumber value));
extern AttributeNumber get_resno ARGS((Resdom node));
extern void set_restype ARGS((Resdom node, ObjectId value));
extern ObjectId get_restype ARGS((Resdom node));
extern void set_reslen ARGS((Resdom node, Size value));
extern Size get_reslen ARGS((Resdom node));
extern void set_resname ARGS((Resdom node, Name value));
extern Name get_resname ARGS((Resdom node));
extern void set_reskey ARGS((Resdom node, Index value));
extern Index get_reskey ARGS((Resdom node));
extern void set_reskeyop ARGS((Resdom node, OperatorTupleForm value));
extern OperatorTupleForm get_reskeyop ARGS((Resdom node));
extern void set_resjunk ARGS((Resdom node, int value));
extern int get_resjunk ARGS((Resdom node));
extern void set_varno ARGS((Var node, Index value));
extern Index get_varno ARGS((Var node));
extern void set_varattno ARGS((Var node, AttributeNumber value));
extern AttributeNumber get_varattno ARGS((Var node));
extern void set_vartype ARGS((Var node, ObjectId value));
extern ObjectId get_vartype ARGS((Var node));
extern void set_vardotfields ARGS((Var node, List value));
extern List get_vardotfields ARGS((Var node));
extern void set_vararraylist ARGS((Var node, List value));
extern List get_vararraylist ARGS((Var node));
extern void set_varid ARGS((Var node, List value));
extern List get_varid ARGS((Var node));
extern void set_varslot ARGS((Var node, Pointer value));
extern Pointer get_varslot ARGS((Var node));
extern void set_opno ARGS((Oper node, ObjectId value));
extern ObjectId get_opno ARGS((Oper node));
extern void set_opid ARGS((Oper node, ObjectId value));
extern ObjectId get_opid ARGS((Oper node));
extern void set_oprelationlevel ARGS((Oper node, bool value));
extern bool get_oprelationlevel ARGS((Oper node));
extern void set_opresulttype ARGS((Oper node, ObjectId value));
extern ObjectId get_opresulttype ARGS((Oper node));
extern void set_opsize ARGS((Oper node, int value));
extern int get_opsize ARGS((Oper node));
extern void set_op_fcache ARGS((Oper node, FunctionCachePtr value));
extern FunctionCachePtr get_op_fcache ARGS((Oper node));
extern void set_consttype ARGS((Const node, ObjectId value));
extern ObjectId get_consttype ARGS((Const node));
extern void set_constlen ARGS((Const node, Size value));
extern Size get_constlen ARGS((Const node));
extern void set_constvalue ARGS((Const node, Datum value));
extern Datum get_constvalue ARGS((Const node));
extern void set_constisnull ARGS((Const node, bool value));
extern bool get_constisnull ARGS((Const node));
extern void set_constbyval ARGS((Const node, bool value));
extern bool get_constbyval ARGS((Const node));
extern void set_paramkind ARGS((Param node, int value));
extern int get_paramkind ARGS((Param node));
extern void set_paramid ARGS((Param node, AttributeNumber value));
extern AttributeNumber get_paramid ARGS((Param node));
extern void set_paramname ARGS((Param node, Name value));
extern Name get_paramname ARGS((Param node));
extern void set_paramtype ARGS((Param node, ObjectId value));
extern ObjectId get_paramtype ARGS((Param node));
extern void set_funcid ARGS((Func node, ObjectId value));
extern ObjectId get_funcid ARGS((Func node));
extern void set_functype ARGS((Func node, ObjectId value));
extern ObjectId get_functype ARGS((Func node));
extern void set_funcisindex ARGS((Func node, bool value));
extern bool get_funcisindex ARGS((Func node));
extern void set_funcsize ARGS((Func node, int value));
extern int get_funcsize ARGS((Func node));
extern void set_func_fcache ARGS((Func node, FunctionCachePtr value));
extern FunctionCachePtr get_func_fcache ARGS((Func node));
extern void set_arrayelemtype ARGS((Array node, ObjectId value));
extern ObjectId get_arrayelemtype ARGS((Array node));
extern void set_arrayelemlength ARGS((Array node, int value));
extern int get_arrayelemlength ARGS((Array node));
extern void set_arrayelembyval ARGS((Array node, bool value));
extern bool get_arrayelembyval ARGS((Array node));
extern void set_arraylow ARGS((Array node, int value));
extern int get_arraylow ARGS((Array node));
extern void set_arrayhigh ARGS((Array node, int value));
extern int get_arrayhigh ARGS((Array node));
extern void set_arraylen ARGS((Array node, int value));
extern int get_arraylen ARGS((Array node));
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
