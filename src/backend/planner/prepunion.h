/* ----------------------------------------------------------------
 *   FILE
 *	prepunion.h
 *
 *   DESCRIPTION
 *	prototypes for prepunion.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef prepunionIncluded		/* include this file only once */
#define prepunionIncluded	1

extern LispValue find_all_inheritors ARGS((LispValue unexamined_relids, LispValue examined_relids));
extern int first_matching_rt_entry ARGS((LispValue rangetable, LispValue flag));
extern Append plan_union_queries ARGS((Index rt_index, int flag, LispValue root, LispValue tlist, LispValue qual, LispValue rangetable));
extern LispValue plan_union_query ARGS((LispValue relids, Index rt_index, LispValue rt_entry, LispValue root, LispValue tlist, LispValue qual, LispValue rangetable, int flag));
extern LispValue new_rangetable_entry ARGS((ObjectId new_relid, LispValue old_entry));
extern LispValue subst_rangetable ARGS((LispValue root, Index index, LispValue new_entry));
extern LispValue fix_parsetree_attnums ARGS((Index rt_index, ObjectId old_relid, ObjectId new_relid, LispValue parsetree));
extern List fix_rangetable ARGS((List rangetable, int index, List new_entry));
extern TL fix_targetlist ARGS((TL oringtlist, TL tlist));
extern Append make_append ARGS((List unionplans, Index rt_index, List union_rt_entries, List tlist));

#endif /* prepunionIncluded */
