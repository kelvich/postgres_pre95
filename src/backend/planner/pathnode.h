/* $Header$ */

#include "c.h"
#include "pg_lisp.h"
extern bool path_is_cheaper ARGS((Path path1, Path path2));
extern Path cheaper_path ARGS((Path path1, Path path2));
extern Path set_cheapest ARGS((Rel parent_rel, List pathlist));
extern LispValue add_pathlist ARGS((LispValue parent_rel, LispValue unique_paths, LispValue new_paths));
extern LispValue better_path ARGS((LispValue new_path, LispValue unique_paths));
extern Path create_seqscan_path ARGS((LispValue rel));
extern IndexPath create_index_path ARGS((LispValue rel, LispValue index, LispValue restriction_clauses, LispValue is_join_scan));
extern Path create_nestloop_path ARGS((LispValue joinrel, LispValue outer_rel, LispValue outer_path, LispValue inner_path, LispValue keys));
extern MergePath create_mergesort_path ARGS((LispValue joinrel, LispValue outersize, LispValue innersize, LispValue outerwidth, LispValue innerwidth, LispValue outer_path, LispValue inner_path, LispValue keys, LispValue order, LispValue mergeclauses, LispValue outersortkeys, LispValue innersortkeys));
extern HashPath create_hashjoin_path ARGS((LispValue joinrel, LispValue outersize, LispValue innersize, LispValue outerwidth, LispValue innerwidth, LispValue outer_path, LispValue inner_path, LispValue keys, LispValue operator, LispValue hashclauses, LispValue outerkeys, LispValue innerkeys));
