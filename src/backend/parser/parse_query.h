/* $Header$ */
extern notify_parser();

extern int MAKE_RESDOM,MAKE_VAR,MAKE_ADT,MAKE_CONST,MAKE_PARAM,MAKE_OP;

extern LispValue MakeRoot();
extern LispValue ModifyRuleQuery();
extern LispValue MakeRangeTableEntry();
extern LispValue ExpandAll();
extern LispValue MakeTimeRange();

extern int RangeTablePosn();

extern LispValue make_op();
extern LispValue make_const();
extern LispValue make_adt();
extern LispValue make_var();
extern LispValue lispMakeParam();
extern LispValue lispMakeResdom();
extern LispValue lispMakeConst();
extern LispValue lispMakeOp();
extern LispValue make_var();

extern LispValue SkipForwardToFromList();
extern LispValue SkipBackToTlist();
extern LispValue SkipForwardPastFromList();
