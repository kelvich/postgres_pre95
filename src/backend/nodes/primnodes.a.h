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
extern void set_varno ARGS((Var node, Index value));
extern Index get_varno ARGS((Var node));
extern void set_varattno ARGS((Var node, AttributeNumber value));
extern AttributeNumber get_varattno ARGS((Var node));
extern void set_vartype ARGS((Var node, ObjectId value));
extern ObjectId get_vartype ARGS((Var node));
extern void set_vardotfields ARGS((Var node, List value));
extern List get_vardotfields ARGS((Var node));
extern void set_vararraylist ARGS((Var node, Index value));
extern Index get_vararraylist ARGS((Var node));
extern void set_varid ARGS((Var node, List value));
extern List get_varid ARGS((Var node));
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
extern Resdom MakeResdom ARGS((AttributeNumber resno, ObjectId restype, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop, int resjunk));
extern void PrintResdom ARGS((FILE *fp, Resdom node));
extern bool EqualResdom ARGS((Resdom a, Resdom b));
extern Expr MakeExpr ARGS((int resno));
extern void PrintExpr ARGS((FILE *fp, Expr node));
extern bool EqualExpr ARGS((Expr a, Expr b));
extern Var MakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List vardotfields, List vararraylist, List varid));
extern void PrintVar ARGS((FILE *fp, Var node));
extern bool EqualVar ARGS((Var a, Var b));
extern bool EqualArray ARGS((Array a, Array b));
extern Oper MakeOper ARGS((ObjectId opno, ObjectId opid, bool oprelationlevel, ObjectId opresulttype));
extern void PrintOper ARGS((FILE *fp, Oper node));
extern bool EqualOper ARGS((Oper a, Oper b));
extern Const MakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull));
extern void PrintConst ARGS((FILE *fp, Const node));
extern bool EqualConst ARGS((Const a, Const b));
extern Param MakeParam ARGS((int paramkind, AttributeNumber paramid, Name paramname, ObjectId paramtype));
extern void PrintParam ARGS((FILE *fp, Param node));
extern bool EqualParam ARGS((Param a, Param b));
extern Func MakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex));
extern void PrintFunc ARGS((FILE *fp, Func node));
extern bool EqualFunc ARGS((Func a, Func b));
