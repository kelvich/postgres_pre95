/* $Header$ */

#include "primnodes.h"
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
extern LispValue get_varno ARGS((Var node));
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

extern Expr 
MakeExpr ARGS((int resno));
extern Var 
MakeVar ARGS((Index varno, AttributeNumber varattno,
	      ObjectId vartype, List vardotfields,
	      Index vararrayindex, List varid));

extern Oper MakeOper ARGS((ObjectId opno, bool oprelationlevel, ObjectId opresulttype));
extern Const MakeConst ARGS((ObjectId consttype, Size constlen, Datum constvalue, bool constisnull));
extern Param MakeParam ARGS((int32 paramid, Name paramname, ObjectId paramtype));
extern Func MakeFunc ARGS((ObjectId funcid, ObjectId functype, bool funcisindex));
