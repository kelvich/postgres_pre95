/* $Header$ */
extern bool ExecQualClause ARGS((List clause, ExprContext econtext));
extern bool ExecQual ARGS((List qual, ExprContext econtext));
extern HeapTuple ExecTargetList ARGS((List targetlist, int nodomains, AttributePtr targettype, Pointer values, ExprContext econtext));
extern Datum ExecMakeVarConst ARGS((TupleTableSlot slot, AttributeNumber attnum, AttributePtr tuple_type, Buffer buffer, ExprContext econtext));
extern Datum ExecEvalVar ARGS((Expr variable, ExprContext econtext));
extern Datum ExecEvalParam ARGS((Expr expression, ExprContext econtext));
extern Datum ExecMakeFunctionResult ARGS((ObjectId functionOid, ObjectId returnType, List arguments, ExprContext econtext));
extern Datum ExecEvalOper ARGS((List opClause, ExprContext econtext));
extern Datum ExecEvalFunc ARGS((Expr funcClause, ExprContext econtext));
extern Datum ExecEvalNot ARGS((List notclause, ExprContext econtext));
extern Datum ExecEvalOr ARGS((List orExpr, ExprContext econtext));
extern Datum ExecEvalExpr ARGS((Node expression, ExprContext econtext, Boolean *isNull));
