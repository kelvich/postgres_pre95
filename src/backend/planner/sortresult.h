/* $Header$ */

#include "tmp/c.h"
#include "nodes/pg_lisp.h"

extern LispValue relation_sortkeys ARGS((LispValue tlist));
extern LispValue sort_list_car ARGS((LispValue list));
extern void sort_relation_paths ARGS((LispValue pathlist, LispValue sortkeys, Rel rel, int width));
extern Plan sort_level_result ARGS((Plan plan, int numkeys));
