/* $Header$ */

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"

extern LispValue tlistentry_member ARGS((Var var, LispValue targetlist));
extern Expr matching_tlvar ARGS((Var var, List targetlist));
extern LispValue add_tl_element ARGS((Rel rel, Var var, LispValue joinlist));
extern TL create_tl_element ARGS((Var var, Resdom resdomno, List joinlist));
extern LispValue get_actual_tlist ARGS((LispValue tlist));
extern Resdom tlist_member ARGS((Var var, LispValue tlist));
extern LispValue match_varid ARGS((Var varid, LispValue tlist));
extern LispValue new_unsorted_tlist ARGS((LispValue targetlist));
extern LispValue targetlist_resdom_numbers ARGS((LispValue targetlist));
extern LispValue copy_vars ARGS((LispValue target, LispValue source));
extern LispValue flatten_tlist ARGS((LispValue tlist));
extern LispValue flatten_tlist_vars ARGS((LispValue full_tlist, LispValue flat_tlist));
extern Resdom tlist_resdom ARGS((LispValue tlist, Resdom resnode));

