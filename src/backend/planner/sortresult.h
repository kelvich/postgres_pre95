/* $Header$ */

#include "pg_lisp.h"
#include "c.h"

extern LispValue relation_sortkeys ARGS((LispValue tlist));
extern LispValue sort_list_car ARGS((LispValue list));
extern void *sort_relation_paths ARGS((LispValue pathlist, LispValue sortkeys, LispValue rel, LispValue width));
extern Plan sort_level_result ARGS((LispValue plan, LispValue numkeys));
