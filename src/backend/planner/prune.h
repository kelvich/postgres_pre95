/* ----------------------------------------------------------------
 *   FILE
 *	prune.h
 *
 *   DESCRIPTION
 *	prototypes for prune.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef pruneIncluded		/* include this file only once */
#define pruneIncluded	1

extern LispValue prune_joinrels ARGS((LispValue rel_list));
extern LispValue prune_joinrel ARGS((Rel rel, List other_rels));
extern void prune_rel_paths ARGS((List rel_list));
extern bool mergepath_contain_redundant_sorts ARGS((MergePath path));
extern bool path_contain_rotated_mergepaths ARGS((Path p1, Path p2));
extern bool path_contain_redundant_indexscans ARGS((Path path, bool order_expected));
extern void test_prune_unnecessary_paths ARGS((Rel rel));
extern Path prune_rel_path ARGS((Rel rel, Path unorderedpath));
extern LispValue merge_joinrels ARGS((LispValue rel_list1, LispValue rel_list2));
extern LispValue prune_oldrels ARGS((LispValue old_rels));

#endif /* pruneIncluded */
