/* $Header$ */

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"

extern LispValue tlistentry_member ARGS((LispValue var, LispValue targetlist, int test));
extern Expr matching_tlvar ARGS((Var var, List targetlist, int test));
extern LispValue add_tl_element ARGS((LispValue rel, LispValue var, LispValue joinlist));
extern TL create_tl_element ARGS((Var var, Resdom resdomno, List joinlist));
extern LispValue get_actual_tlist ARGS((LispValue tlist));
extern Resdom tlist_member ARGS((LispValue var, LispValue tlist, LispValue dots, LispValue key, int test));
extern LispValue match_varid ARGS((LispValue varid, LispValue tlist, LispValue key));
extern LispValue new_unsorted_tlist ARGS((LispValue targetlist));
extern LispValue targetlist_resdom_numbers ARGS((LispValue targetlist));
extern LispValue copy_vars ARGS((LispValue target, LispValue source));
extern LispValue flatten_tlist ARGS((LispValue tlist));
extern LispValue flatten_tlist_vars ARGS((LispValue full_tlist, LispValue flat_tlist));
extern Resdom tlist_resdom ARGS((LispValue tlist, Resdom resnode));

