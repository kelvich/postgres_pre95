/* $Header$ */
extern bool ExecQualClause ARGS((List clause, ExprContext econtext));
extern bool ExecQual ARGS((List qual, ExprContext econtext));
extern HeapTuple ExecTargetList ARGS((List targetlist, int nodomains, AttributePtr targettype, Pointer values, ExprContext econtext));
extern Const ExecMakeVarConst ARGS((TupleTableSlot slot, AttributeNumber attnum, AttributePtr tuple_type, Buffer buffer, ExprContext econtext));
extern Const ExecEvalVar ARGS((Expr variable, ExprContext econtext));
extern Const ExecEvalParam ARGS((Expr expression, ExprContext econtext));
extern Const ExecMakeFunctionResult ARGS((ObjectId functionOid, ObjectId returnType, List arguments, ExprContext econtext));
extern Const ExecEvalOper ARGS((List opClause, ExprContext econtext));
extern Const ExecEvalFunc ARGS((Expr funcClause, ExprContext econtext));
extern Const ExecEvalNot ARGS((List notclause, ExprContext econtext));
extern Const ExecEvalOr ARGS((List orExpr, ExprContext econtext));
extern Const ExecEvalExpr ARGS((Node expression, ExprContext econtext));
