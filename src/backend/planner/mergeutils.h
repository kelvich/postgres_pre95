/* ----------------------------------------------------------------
 *   FILE
 *	mergeutils.h
 *
 *   DESCRIPTION
 *	prototypes for mergeutils.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef mergeutilsIncluded		/* include this file only once */
#define mergeutilsIncluded	1

extern LispValue group_clauses_by_order ARGS((LispValue clauseinfo_list, LispValue inner_relid));
extern int dump_rel ARGS((Rel rel));
extern MInfo match_order_mergeinfo ARGS((MergeOrder ordering, List mergeinfo_list));

#endif /* mergeutilsIncluded */
