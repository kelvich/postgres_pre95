/* $Header$ */
extern LispValue ModifyQueryTree ARGS((LispValue query, LispValue priority, LispValue ruletag));
extern Name VarnoGetRelname ARGS((int vnum));
extern LispValue MakeRoot ARGS((int NumLevels , LispValue query_name , LispValue result, LispValue rtable , LispValue priority , LispValue ruleinfo , LispValue unique_flag , LispValue sort_clause , LispValue targetlist ));
extern LispValue MakeRangeTableEntry ARGS((Name relname , List options , Name refname ));
LispValue MakeTargetList ARGS((void));
extern LispValue ExpandAll ARGS((Name relname, int *this_resno));
extern LispValue MakeTimeRange ARGS((LispValue datestring1, LispValue datestring2, int timecode));
extern LispValue make_op ARGS((LispValue op, LispValue ltree, LispValue rtree));
extern LispValue make_var  ARGS((Name relname, Name attrname));
SkipForwardToFromList ARGS((void));
LispValue SkipBackToTlist ARGS((void));
LispValue SkipForwardPastFromList ARGS((void));
StripRangeTable ARGS((void));
extern LispValue make_const ARGS((LispValue value));
extern LispValue make_param ARGS((int paramKind, char * relationName, char *attrName));
extern LispValue HandleNestedDots ARGS((List dots));
extern LispValue setup_tlist ARGS((Func func, int attno, ObjectID typeid));

/* defined in gram.y, used in ylib.c and gram.y */
extern int NumLevels;

/* useful macros */
#define ISCOMPLEX(type) (typeid_get_relid((ObjectId)type) ? true : false)
