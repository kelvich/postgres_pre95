/* $Header$ */

#include "tmp/c.h"
#include "nodes/pg_lisp.h"

extern bool path_is_cheaper ARGS((Path path1, Path path2));
extern Path cheaper_path ARGS((Path path1, Path path2));
extern Path set_cheapest ARGS((Rel parent_rel, List pathlist));
extern LispValue add_pathlist ARGS((Rel parent_rel, LispValue unique_paths, LispValue new_paths));
extern LispValue better_path ARGS((Path new_path, LispValue unique_paths));
extern Path create_seqscan_path ARGS((Rel rel));
extern IndexPath create_index_path ARGS((Rel rel, Rel index, LispValue restriction_clauses, bool is_join_scan));
extern JoinPath create_nestloop_path ARGS((Rel joinrel, Rel outer_rel, Path outer_path, Path inner_path, LispValue keys));
extern MergePath create_mergesort_path ARGS((Rel joinrel, Count outersize, Count innersize, Count outerwidth, Count innerwidth, Path outer_path, Path inner_path, LispValue keys, LispValue order, LispValue mergeclauses, LispValue outersortkeys, LispValue innersortkeys));
extern HashPath create_hashjoin_path ARGS((Rel joinrel, Count outersize, Count innersize, Count outerwidth, Count innerwidth, Path outer_path, Path inner_path, LispValue keys, ObjectId operator, LispValue hashclauses, LispValue outerkeys, LispValue innerkeys));
