/* ----------------------------------------------------------------
 *   FILE
 *	pathnode.h
 *
 *   DESCRIPTION
 *	prototypes for pathnode.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef pathnodeIncluded		/* include this file only once */
#define pathnodeIncluded	1

extern bool path_is_cheaper ARGS((Path path1, Path path2));
extern Path cheaper_path ARGS((Path path1, Path path2));
extern Path set_cheapest ARGS((Rel parent_rel, List pathlist));
extern LispValue add_pathlist ARGS((Rel parent_rel, List unique_paths, List new_paths));
extern LispValue better_path ARGS((Path new_path, List unique_paths));
extern Path create_seqscan_path ARGS((Rel rel));
extern IndexPath create_index_path ARGS((Rel rel, Rel index, LispValue restriction_clauses, bool is_join_scan));
extern JoinPath create_nestloop_path ARGS((Rel joinrel, Rel outer_rel, Path outer_path, Path inner_path, List keys));
extern MergePath create_mergesort_path ARGS((Rel joinrel, Count outersize, Count innersize, Count outerwidth, Count innerwidth, Path outer_path, Path inner_path, LispValue keys, LispValue order, LispValue mergeclauses, LispValue outersortkeys, LispValue innersortkeys));
extern HashPath create_hashjoin_path ARGS((Rel joinrel, Count outersize, Count innersize, Count outerwidth, Count innerwidth, Path outer_path, Path inner_path, LispValue keys, ObjectId operator, LispValue hashclauses, LispValue outerkeys, LispValue innerkeys));

#endif /* pathnodeIncluded */
