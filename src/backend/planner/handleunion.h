/* ----------------------------------------------------------------
 *   FILE
 *	handleunion.h
 *
 *   DESCRIPTION
 *	prototypes for handleunion.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef handleunionIncluded		/* include this file only once */
#define handleunionIncluded	1

extern List handleunion ARGS((List root, List rangetable, List tlist, List qual));
extern List SplitTlistQual ARGS((List root, List rangetable, List tlist, List qual));
extern List SplitTlist ARGS((List unionlist, List tlists));
extern void split_tlexpr ARGS((List clauses, List varnum));
extern List collect_union_sets ARGS((List tlist, List qual));
extern List collect_tlist_uset ARGS((List args));
extern List find_qual_union_sets ARGS((List qual));
extern List flatten_union_list ARGS((List ulist));
extern List remove_subsets ARGS((List usets));
extern List SplitQual ARGS((List ulist, List uquals));
extern List find_matching_union_qual ARGS((List ulist, List qual));
extern void match_union_clause ARGS((List unionlist, List *leftarg, List *rightarg));
extern List find_union_vars ARGS((List rangetable));

#endif /* handleunionIncluded */
