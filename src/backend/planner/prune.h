#include "pg_lisp.h"
extern LispValue prune_joinrels ARGS((LispValue rel_list));
extern LispValue prune_joinrel ARGS((LispValue rel, LispValue other_rels));
extern void *prune_rel_paths ARGS((LispValue rel_list));
extern Path prune_rel_path ARGS((LispValue rel, LispValue unordered_path));
