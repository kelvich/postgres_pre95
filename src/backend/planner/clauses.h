#include "c.h"
#include "pg_lisp.h"
#include "relation.h"
#include "parse.h"

extern LispValue clause_head ARGS((LispValue clause));
extern LispValue clause_type ARGS((LispValue clause));
extern LispValue clause_args ARGS((LispValue clause));
extern LispValue clause_subclauses ARGS((LispValue type, LispValue clause));
extern bool is_opclause ARGS((LispValue clause));
extern LispValue make_opclause ARGS((LispValue op, LispValue leftop, LispValue rightop));
extern LispValue get_opargs ARGS((LispValue clause));
extern LispValue get_op ARGS((LispValue clause));
extern Var get_leftop ARGS((LispValue clause));
extern Var get_rightop ARGS((LispValue clause));
extern bool is_funcclause ARGS((LispValue clause));
extern LispValue make_funcclause ARGS((LispValue func, LispValue funcargs));
extern LispValue get_function ARGS((LispValue func));
extern LispValue get_funcargs ARGS((LispValue func));
extern bool or_clause ARGS((LispValue clause));
extern LispValue make_orclause ARGS((LispValue orclauses));
extern LispValue get_orclauseargs ARGS((LispValue orclause));
extern bool not_clause ARGS((LispValue clause));
extern LispValue make_notclause ARGS((LispValue notclause));
extern LispValue get_notclausearg ARGS((LispValue notclause));
extern bool and_clause ARGS((LispValue clause));
extern LispValue make_andclause ARGS((LispValue andclauses));
extern LispValue get_andclauseargs ARGS((LispValue andclause));

#define make_clause make_opclause

