/* $Header$ */

#include "nodes/pg_lisp.h"
extern LispValue prune_joinrels ARGS((LispValue rel_list));
extern LispValue prune_joinrel ARGS((Rel rel, LispValue other_rels));
extern void prune_rel_paths ARGS((LispValue rel_list));
extern Path prune_rel_path ARGS((Rel rel, Path unordered_path));
extern LispValue prune_oldrels ARGS((LispValue old_rels));
extern LispValue merge_joinrels ARGS((LispValue rel_list1,LispValue rel_list2));
