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
extern void set_ecxt_scantuple ARGS((ExprContext node, List value));
extern List get_ecxt_scantuple ARGS((ExprContext node));
extern void set_ecxt_scantype ARGS((ExprContext node, AttributePtr value));
extern AttributePtr get_ecxt_scantype ARGS((ExprContext node));
extern void set_ecxt_scan_buffer ARGS((ExprContext node, Buffer value));
extern Buffer get_ecxt_scan_buffer ARGS((ExprContext node));
extern void set_ecxt_innertuple ARGS((ExprContext node, List value));
extern List get_ecxt_innertuple ARGS((ExprContext node));
extern void set_ecxt_innertype ARGS((ExprContext node, AttributePtr value));
extern AttributePtr get_ecxt_innertype ARGS((ExprContext node));
extern void set_ecxt_inner_buffer ARGS((ExprContext node, Buffer value));
extern Buffer get_ecxt_inner_buffer ARGS((ExprContext node));
extern void set_ecxt_outertuple ARGS((ExprContext node, List value));
extern List get_ecxt_outertuple ARGS((ExprContext node));
extern void set_ecxt_outertype ARGS((ExprContext node, AttributePtr value));
extern AttributePtr get_ecxt_outertype ARGS((ExprContext node));
extern void set_ecxt_outer_buffer ARGS((ExprContext node, Buffer value));
extern Buffer get_ecxt_outer_buffer ARGS((ExprContext node));
extern void set_ecxt_relation ARGS((ExprContext node, Relation value));
extern Relation get_ecxt_relation ARGS((ExprContext node));
extern void set_ecxt_relid ARGS((ExprContext node, Index value));
extern Index get_ecxt_relid ARGS((ExprContext node));
extern ParamListInfo get_ecxt_param_list_info ARGS((ExprContext node));
extern void set_ecxt_param_list_info ARGS((ExprContext node, ParamListInfo value));
extern void set_varno ARGS((Var node, Index value));
extern Index get_varno ARGS((Var node));
extern void set_varattno ARGS((Var node, AttributeNumber value));
extern AttributeNumber get_varattno ARGS((Var node));
extern void set_vartype ARGS((Var node, ObjectId value));
extern ObjectId get_vartype ARGS((Var node));
extern void set_vardotfields ARGS((Var node, List value));
extern List get_vardotfields ARGS((Var node));
extern void set_vararrayindex ARGS((Var node, Index value));
extern Index get_vararrayindex ARGS((Var node));
extern void set_varid ARGS((Var node, List value));
extern List get_varid ARGS((Var node));
extern void set_opno ARGS((Oper node, ObjectId value));
extern ObjectId get_opno ARGS((Oper node));
extern void set_oprelationlevel ARGS((Oper node, bool value));
extern bool get_oprelationlevel ARGS((Oper node));
extern void set_opresulttype ARGS((Oper node, ObjectId value));
extern ObjectId get_opresulttype ARGS((Oper node));
extern void set_consttype ARGS((Const node, ObjectId value));
extern ObjectId get_consttype ARGS((Const node));
extern void set_constlen ARGS((Const node, Size value));
extern Size get_constlen ARGS((Const node));
extern void set_constvalue ARGS((Const node, Datum value));
extern Datum get_constvalue ARGS((Const node));
extern void set_constisnull ARGS((Const node, bool value));
extern bool get_constisnull ARGS((Const node));
extern void set_boolvalue ARGS((Bool node, bool value));
extern bool get_boolvalue ARGS((Bool node));
extern void set_paramid ARGS((Param node, int32 value));
extern int32 get_paramid ARGS((Param node));
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
extern Resdom MakeResdom ARGS((AttributeNumber resno, ObjectId restype, Size reslen, Name resname, Index reskey, OperatorTupleForm reskeyop));
extern void PrintResdom ARGS((Resdom node));
extern bool EqualResdom ARGS((Resdom a, Resdom b));
extern Expr MakeExpr ARGS((int resno));
extern void PrintExpr ARGS((Expr node));
extern bool EqualExpr ARGS((Expr a, Expr b));
extern ExprContext MakeExprContext ARGS((List ecxt_scantuple, AttributePtr ecxt_scantype, Buffer ecxt_scan_buffer, List ecxt_innertuple, AttributePtr ecxt_innertype, Buffer ecxt_inner_buffer, List ecxt_outertuple, AttributePtr ecxt_outertype, Buffer ecxt_outer_buffer, Relation ecxt_relation, Index ecxt_relid));
extern void PrintExprContext ARGS((ExprContext node));
extern bool EqualExprContext ARGS((ExprContext a, ExprContext b));
extern Var MakeVar ARGS((Index varno, AttributeNumber varattno, ObjectId vartype, List vardotfields, Index vararrayindex, List varid));
extern void PrintVar ARGS((Var node));
extern bool EqualVar ARGS((Var a, Var b));
extern Oper MakeOper ARGS((ObjectId opno, bool oprelationlevel, ObjectId opresulttype));
extern void PrintOper ARGS((Oper node));
extern bool EqualOper ARGS((Oper a, Oper b));
extern Const MakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull));
extern void PrintConst ARGS((Const node));
extern bool EqualConst ARGS((Const a, Const b));
extern Bool MakeBool ARGS((bool boolvalue));
extern void PrintBool ARGS((Bool node));
extern bool EqualBool ARGS((Bool a, Bool b));
extern Param MakeParam ARGS((int32 paramid, Name paramname, ObjectId paramtype));
extern void PrintParam ARGS((Param node));
extern bool EqualParam ARGS((Param a, Param b));
extern Func MakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex));
extern void PrintFunc ARGS((Func node));
extern bool EqualFunc ARGS((Func a, Func b));
