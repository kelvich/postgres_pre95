/* ----------------------------------------------------------------
 *   FILE
 *	joinpath.h
 *
 *   DESCRIPTION
 *	prototypes for joinpath.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef joinpathIncluded		/* include this file only once */
#define joinpathIncluded	1

extern void find_all_join_paths ARGS((LispValue joinrels, LispValue previous_level_rels, int nest_level));
extern Path best_innerjoin ARGS((LispValue join_paths, LispValue outer_relid));
extern LispValue sort_inner_and_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, List mergeinfo_list));
extern List match_unsorted_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, List outerpath_list, Path cheapest_inner, Path best_innerjoin, List mergeinfo_list));
extern LispValue match_unsorted_inner ARGS((Rel joinrel, Rel outerrel, Rel innerrel, List innerpath_list, List mergeinfo_list));
extern bool EnoughMemoryForHashjoin ARGS((Rel hashrel));
extern LispValue hash_inner_and_outer ARGS((Rel joinrel, Rel outerrel, Rel innerrel, LispValue hashinfo_list));
extern LispValue form_relid ARGS((LispValue relids));

#endif /* joinpathIncluded */
