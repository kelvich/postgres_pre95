/* ----------------------------------------------------------------
 *   FILE
 *	preptlist.h
 *
 *   DESCRIPTION
 *	prototypes for preptlist.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef preptlistIncluded		/* include this file only once */
#define preptlistIncluded	1

extern List preprocess_targetlist ARGS((List tlist, int command_type, LispValue result_relation, List range_table));
extern LispValue expand_targetlist ARGS((List tlist, ObjectId relid, int command_type, Index result_relation));
extern LispValue replace_matching_resname ARGS((LispValue new_tlist, LispValue old_tlist));
extern LispValue new_relation_targetlist ARGS((ObjectId relid, Index rt_index, NodeTag node_type));

#endif /* preptlistIncluded */
