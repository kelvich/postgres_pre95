/* $Header: */

extern List handleunion ARGS((List root, List rangetable, List tlist, List qual));
extern List SplitTlistQual ARGS(( List root, List rangetable,List tlist, List qual));
extern List SplitTlist ARGS((List unionlist, List tlist));
extern List find_union_vars ARGS((List tlist));
extern List find_union_sets ARGS((List tlist));
extern List collect_union_sets ARGS((List tlist, List qual));
extern List find_qual_union_sets ARGS((List qual));
extern List flatten_union_list ARGS((List ulist));
extern List remove_subsets ARGS((List usets));
extern List SplitQual ARGS((List ulist, List uqual));
extern void match_union_clause ARGS((List unionlist, List leftop, List rightop));
extern List find_matching_union_qual ARGS((List ulist, List qual));     

